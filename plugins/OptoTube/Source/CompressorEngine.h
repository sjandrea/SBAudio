#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace optotube
{
    enum Mode { Opto = 0, Manual = 1, OptoManual = 2 };

    struct Settings
    {
        float thresholdDb = -20.0f;
        float ratio       = 3.0f;
        float attackMs    = 10.0f;   // manual stage only
        float releaseMs   = 300.0f;  // manual stage only
        int   mode        = Opto;
        float hpfHz       = 0.0f;    // detector high-pass, 0 = off
        float tube        = 0.3f;    // 0..1
        float gainDb      = 0.0f;    // make-up on the compressed signal
        float mix         = 1.0f;    // 0..1 (parallel compression)
    };

    /** Second-order Butterworth high-pass (RBJ), used only in the detector. */
    struct HighPass
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;

        void set (double fs, double hz)
        {
            const double w = juce::MathConstants<double>::twoPi * hz / fs;
            const double c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.70710678);
            const double a0 = 1.0 + alpha;
            b0 = (float) ((1.0 + c) * 0.5 / a0);
            b1 = (float) (-(1.0 + c) / a0);
            b2 = b0;
            a1 = (float) (-2.0 * c / a0);
            a2 = (float) ((1.0 - alpha) / a0);
        }
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0.0f; }
    };

    /**
        Stereo-linked compressor combining two detector/ballistics models:

        - Opto (LA-2A-like): program-dependent attack around 10 ms and a two-stage
          release – about half of the gain reduction recovers in ~60 ms, the rest
          slowly (0.4–3 s). The slow part gets longer the longer and harder the
          signal has been compressed (the "memory" of an optical cell).
        - Manual (CL 1B-like): attack and release set by the user.
        - Opto + Manual: both run in parallel and the larger gain reduction wins,
          so the manual stage can catch fast peaks while the opto stage rides the level.

        After the gain stage comes a tube-style saturator (asymmetric soft curve =
        mostly 2nd harmonic), make-up gain and a dry/wet mix.
    */
    class CompressorEngine
    {
    public:
        void prepare (double sampleRate, int /*maxBlock*/)
        {
            fs = sampleRate;
            coefRms     = coef (0.004);
            coefOptoRel = coef (0.060);
            coefMemUp   = coef (2.0);
            coefMemDown = coef (6.0);
            coefDc      = coef (1.0 / (juce::MathConstants<double>::twoPi * 8.0));
            gainSm.reset (sampleRate, 0.03);
            mixSm.reset (sampleRate, 0.03);
            tubeSm.reset (sampleRate, 0.05);
            reset();
        }

        void reset()
        {
            ms = 0.0f;
            optoFast = optoSlow = memory = manualEnv = 0.0f;
            for (auto& h : hpf) h.reset();
            dcX = {}; dcY = {};
            lastHpf = -1.0f;
            firstBlock = true;
        }

        /** Largest gain reduction (dB, positive) during the last block. */
        float getBlockMaxGrDb() const { return blockMaxGr; }

        void process (juce::AudioBuffer<float>& buffer, const Settings& s)
        {
            const int numCh = juce::jmin (buffer.getNumChannels(), 2);
            const int n = buffer.getNumSamples();
            if (numCh == 0 || n == 0)
                return;

            if (std::abs (s.hpfHz - lastHpf) > 0.01f)
            {
                lastHpf = s.hpfHz;
                if (s.hpfHz > 0.0f)
                    for (auto& h : hpf) h.set (fs, s.hpfHz);
            }

            if (firstBlock)
            {
                gainSm.setCurrentAndTargetValue (s.gainDb);
                mixSm.setCurrentAndTargetValue (s.mix);
                tubeSm.setCurrentAndTargetValue (s.tube);
                firstBlock = false;
            }
            gainSm.setTargetValue (s.gainDb);
            mixSm.setTargetValue (s.mix);
            tubeSm.setTargetValue (s.tube);

            const float ratio = juce::jmax (1.0f, s.ratio);
            const float slope = 1.0f - 1.0f / ratio;
            const float cAtt = coef (s.attackMs * 0.001);
            const float cRel = coef (s.releaseMs * 0.001);
            const bool useOpto   = s.mode == Opto || s.mode == OptoManual;
            const bool useManual = s.mode == Manual || s.mode == OptoManual;
            const bool hp = s.hpfHz > 0.0f;

            blockMaxGr = 0.0f;
            float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

            for (int i = 0; i < n; ++i)
            {
                // ---- detector: stereo-linked RMS (~4 ms), optionally high-passed ----
                float peak2 = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    const float d = hp ? hpf[(size_t) c].process (ch[c][i]) : ch[c][i];
                    peak2 = juce::jmax (peak2, d * d);
                }
                ms += coefRms * (peak2 - ms);
                const float levelDb = 10.0f * std::log10 (ms + 1.0e-12f) + 3.01f; // sine RMS -> peak dBFS

                // ---- static curves (opto has a wider, softer knee) ----
                const float over = levelDb - s.thresholdDb;
                float gr = 0.0f;

                if (useOpto)
                {
                    const float t = staticCurve (over, slope, 10.0f);
                    // program-dependent attack: bigger jumps -> faster
                    const float tauA = 0.010f / (1.0f + 0.12f * juce::jmax (0.0f, t - optoFast));
                    const float cA = coef (juce::jmax (0.0015f, tauA));

                    optoFast += (t > optoFast ? cA : coefOptoRel) * (t - optoFast);

                    // memory: grows while compressing hard, decays slowly when idle
                    if (t > 1.0f)
                        memory += coefMemUp * juce::jmin (1.0f, t / 8.0f) * (1.0f - memory);
                    else
                        memory -= coefMemDown * memory;

                    const float cSlowRel = coef (0.4 + 2.6 * memory);
                    optoSlow += (t > optoSlow ? cA : cSlowRel) * (t - optoSlow);

                    gr = 0.5f * (optoFast + optoSlow);
                }

                if (useManual)
                {
                    const float t = staticCurve (over, slope, 6.0f);
                    manualEnv += (t > manualEnv ? cAtt : cRel) * (t - manualEnv);
                    gr = juce::jmax (gr, manualEnv);
                }

                if (! useOpto)  { optoFast = optoSlow = 0.0f; memory = 0.0f; }
                if (! useManual) manualEnv = 0.0f;

                blockMaxGr = juce::jmax (blockMaxGr, gr);

                // ---- gain, tube, make-up, mix ----
                const float g = juce::Decibels::decibelsToGain (-gr);
                const float makeup = juce::Decibels::decibelsToGain (gainSm.getNextValue());
                const float mix = mixSm.getNextValue();
                const float tube = tubeSm.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    const float dry = ch[c][i];
                    const float wet = saturate (dry * g, tube, c) * makeup;
                    ch[c][i] = dry + mix * (wet - dry);
                }
            }
        }

        /** Gain reduction (dB, positive) for a level 'over' dB above threshold. slope = 1 - 1/ratio. */
        static float staticCurve (float over, float slope, float knee) noexcept
        {
            if (2.0f * over <= -knee) return 0.0f;
            if (2.0f * over >= knee)  return slope * over;
            const float x = over + knee * 0.5f;
            return slope * x * x / (2.0f * knee);
        }

    private:
        /** Asymmetric tube-style curve, unity gain for small signals, followed by a DC blocker. */
        float saturate (float x, float amount, int c) noexcept
        {
            const float drive = 1.0f + 2.0f * amount;
            const float bias  = 0.25f * amount;
            const float tb    = std::tanh (bias);
            const float norm  = drive * (1.0f - tb * tb);
            const float y     = (std::tanh (drive * x + bias) - tb) / norm;
            const float mixed = x + amount * (y - x);

            // DC blocker (the bias produces a small offset)
            const float out = mixed - dcX[(size_t) c] + (1.0f - coefDc) * dcY[(size_t) c];
            dcX[(size_t) c] = mixed;
            dcY[(size_t) c] = out;
            return out;
        }

        float coef (double tauSeconds) const
        {
            return (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0e-5, tauSeconds) * fs)));
        }

        double fs = 48000.0;
        float coefRms = 0, coefOptoRel = 0, coefMemUp = 0, coefMemDown = 0, coefDc = 0;
        float ms = 0, optoFast = 0, optoSlow = 0, memory = 0, manualEnv = 0, blockMaxGr = 0;
        std::array<HighPass, 2> hpf;
        std::array<float, 2> dcX {}, dcY {};
        float lastHpf = -1.0f;
        bool firstBlock = true;
        juce::SmoothedValue<float> gainSm, mixSm, tubeSm;
    };
}
