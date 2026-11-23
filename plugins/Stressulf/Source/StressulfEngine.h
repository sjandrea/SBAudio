#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// Stressulf compressor engine.
//
// Character compressor in the spirit of the classic "distortion-capable" studio compressors:
//  - Input drives the signal into a FIXED threshold (no threshold knob), Output sets the level.
//  - Ratio selector 1:1 .. 20:1, NUKE (near-brickwall) and OPTO (program-dependent, slow release).
//  - Program-dependent release: sustained gain reduction makes the release slower.
//  - Dist 2 (even harmonics) / Dist 3 (odd harmonics) colour that grows with gain reduction.
//  - Detector HP (less pumping from lows) and BAND emphasis (6 kHz, works like a de-esser).
//  - Audio HP (80 Hz) in the audio path, stereo link, SMASH (all-buttons style over-compression).
// Everything non-linear runs at 2x oversampling.
namespace stressulf
{
    enum RatioMode { r1 = 0, r2, r3, r4, r6, r10, r20, nuke, opto };
    enum DistMode  { clean = 0, dist2, dist3 };

    inline const juce::StringArray ratioNames { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "Nuke", "Opto" };
    inline const juce::StringArray distNames  { "Clean", "Dist 2", "Dist 3" };

    inline constexpr float thresholdDb = -20.0f;   // fixed; Input drives into it

    struct Settings
    {
        float inputDb = 0.0f, outputDb = 0.0f;
        float attackMs = 5.0f, releaseMs = 300.0f;
        int ratio = r4, dist = clean;
        bool detHp = false, detBand = false, audioHp = false, link = true, smash = false;
        float mix = 1.0f;
    };

    struct CurveShape { float ratio, kneeDb; };

    inline CurveShape curveFor (int ratio, bool smash)
    {
        if (smash) return { 20.0f, 1.0f };
        switch (ratio)
        {
            case r1:   return { 1.0f, 0.0f };
            case r2:   return { 2.0f, 12.0f };   // very soft knee at the low ratios
            case r3:   return { 3.0f, 10.0f };
            case r4:   return { 4.0f, 8.0f };
            case r6:   return { 6.0f, 6.0f };
            case r10:  return { 10.0f, 4.0f };
            case r20:  return { 20.0f, 2.0f };
            case nuke: return { 60.0f, 0.5f };
            case opto: return { 6.0f, 12.0f };
            default:   return { 4.0f, 8.0f };
        }
    }

    /** Static gain reduction (positive dB) for a detector level in dBFS. */
    inline float staticGr (float levelDb, int ratio, bool smash)
    {
        const auto c = curveFor (ratio, smash);
        const float slope = 1.0f - 1.0f / c.ratio;
        const float over = levelDb - thresholdDb;
        float gr;
        if (c.kneeDb < 0.01f)                 gr = over > 0.0f ? over * slope : 0.0f;
        else if (over <= -0.5f * c.kneeDb)    gr = 0.0f;
        else if (over >=  0.5f * c.kneeDb)    gr = over * slope;
        else { const float t = over + 0.5f * c.kneeDb; gr = slope * t * t / (2.0f * c.kneeDb); }
        return smash ? gr * 1.2f : gr;        // SMASH over-compresses: louder in = quieter out
    }

    /** RBJ biquad (transposed direct form II), double coefficients for low-frequency precision. */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        inline float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void highPass (double sr, double f, double q)
        {
            const double w = juce::MathConstants<double>::twoPi * f / sr, cw = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = b0; a1 = -2 * cw / a0; a2 = (1 - al) / a0;
        }
        void peak (double sr, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = juce::MathConstants<double>::twoPi * f / sr, cw = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al / A;
            b0 = (1 + al * A) / a0; b1 = -2 * cw / a0; b2 = (1 - al * A) / a0; a1 = -2 * cw / a0; a2 = (1 - al / A) / a0;
        }
    };

    class Engine
    {
    public:
        void prepare (double sampleRate, int maxBlock, int numChannels)
        {
            sr = sampleRate;
            channels = juce::jlimit (1, 2, numChannels);
            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            oversampler->initProcessing ((size_t) maxBlock);
            oversampler->reset();
            latency = (int) std::lround (oversampler->getLatencyInSamples());

            osr = sr * 2.0;
            for (auto& c : ch)
            {
                c = {};
                c.audioHp.highPass (sr, 80.0, 0.707);
                c.detHp.highPass (osr, 100.0, 0.6);
                c.detBand.peak (osr, 6000.0, 1.2, 12.0);
                c.dryDelay.assign ((size_t) latency + 1, 0.0f);
            }
            dcCoef = (float) (1.0 - juce::MathConstants<double>::twoPi * 10.0 / osr);
            grSlowCoef = coef (0.5);

            inGain.reset (sr, 0.03);  outGain.reset (sr, 0.03);  mixS.reset (sr, 0.03);
            inGain.setCurrentAndTargetValue (1.0f); outGain.setCurrentAndTargetValue (1.0f); mixS.setCurrentAndTargetValue (1.0f);
            dry.setSize (channels, maxBlock);
            inArr.assign ((size_t) maxBlock, 1.0f);
            blockMaxGr = 0.0f;
        }

        int getLatency() const noexcept { return latency; }
        float getBlockMaxGrDb() const noexcept { return blockMaxGr; }

        void process (juce::AudioBuffer<float>& buffer, const Settings& s)
        {
            const int n = buffer.getNumSamples();
            const int nc = juce::jmin (channels, buffer.getNumChannels());
            blockMaxGr = 0.0f;
            if (n == 0 || nc == 0) return;
            if (n > dry.getNumSamples())   // host exceeded the announced block size
            {
                for (int start = 0; start < n; start += dry.getNumSamples())
                {
                    juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), nc, start, juce::jmin (dry.getNumSamples(), n - start));
                    const float keep = blockMaxGr;
                    process (sub, s);
                    blockMaxGr = juce::jmax (keep, blockMaxGr);
                }
                return;
            }

            inGain.setTargetValue (juce::Decibels::decibelsToGain (s.inputDb));
            outGain.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
            mixS.setTargetValue (s.mix);

            // ---- base rate: keep dry, audio HP, input gain ------------------------------
            for (int i = 0; i < n; ++i) inArr[(size_t) i] = inGain.getNextValue();
            for (int c = 0; c < nc; ++c)
            {
                dry.copyFrom (c, 0, buffer, c, 0, n);
                auto* d = buffer.getWritePointer (c);
                auto& st = ch[(size_t) c];
                for (int i = 0; i < n; ++i)
                {
                    float x = s.audioHp ? st.audioHp.process (d[i]) : d[i];
                    d[i] = x * inArr[(size_t) i];
                }
            }

            // ---- 2x oversampled: detector, gain computer, distortion --------------------
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
            auto os = oversampler->processSamplesUp (block);
            const int osN = (int) os.getNumSamples();

            const bool isOpto = s.ratio == opto && ! s.smash;
            const float attMs = s.smash ? juce::jmin (s.attackMs, 0.8f) : isOpto ? 10.0f : s.attackMs;
            const float attC = coef (attMs * 0.001);
            const float drive = distDrive (s);
            const bool linked = s.link && nc == 2;

            float* p[2] = { os.getChannelPointer (0), nc > 1 ? os.getChannelPointer (1) : nullptr };
            for (int j = 0; j < osN; ++j)
            {
                float lvl[2] = { 0.0f, 0.0f };
                for (int c = 0; c < nc; ++c)
                {
                    auto& st = ch[(size_t) c];
                    float d = p[c][j];
                    if (s.detHp)   d = st.detHp.process (d);
                    if (s.detBand) d = st.detBand.process (d);
                    lvl[c] = std::abs (d);
                }
                if (linked) lvl[0] = lvl[1] = juce::jmax (lvl[0], lvl[1]);

                for (int c = 0; c < nc; ++c)
                {
                    auto& st = ch[(size_t) c];
                    const float target = staticGr (juce::Decibels::gainToDecibels (lvl[c], -120.0f), s.ratio, s.smash);

                    // program-dependent release: the longer and deeper it compresses, the slower it lets go
                    st.grSlow += (st.gr - st.grSlow) * (1.0f - grSlowCoef);
                    float relMs;
                    if (isOpto)        relMs = 60.0f + 120.0f * st.grSlow;
                    else if (s.smash)  relMs = juce::jmin (s.releaseMs, 120.0f) * (1.0f + 0.05f * st.grSlow);
                    else               relMs = s.releaseMs * juce::jmin (3.0f, 1.0f + 0.12f * st.grSlow);

                    const float c0 = target > st.gr ? attC : coef (relMs * 0.001);
                    st.gr = target + (st.gr - target) * c0;

                    float y = p[c][j] * juce::Decibels::decibelsToGain (-st.gr);
                    if (drive > 0.0f)
                        y = shape (y, drive * (1.0f + st.gr / 12.0f), s.dist == dist3 ? dist3 : dist2);
                    // DC blocker (even-harmonic shaping is asymmetric)
                    const float dc = y - st.dcX + dcCoef * st.dcY;
                    st.dcX = y; st.dcY = dc;
                    p[c][j] = dc;
                    blockMaxGr = juce::jmax (blockMaxGr, st.gr);
                }
            }
            oversampler->processSamplesDown (block);

            // ---- base rate: parallel mix (latency-aligned dry), output -------------------
            const int size = latency + 1;
            for (int i = 0; i < n; ++i)
            {
                const float m = mixS.getNextValue(), g = outGain.getNextValue();
                for (int c = 0; c < nc; ++c)
                {
                    auto& st = ch[(size_t) c];
                    const int rp = (st.wp + 1) % size;
                    st.dryDelay[(size_t) st.wp] = dry.getSample (c, i);
                    const float dr = st.dryDelay[(size_t) rp];
                    auto* d = buffer.getWritePointer (c);
                    d[i] = (dr * (1.0f - m) + d[i] * m) * g;
                }
                for (int c = 0; c < nc; ++c) ch[(size_t) c].wp = (ch[(size_t) c].wp + 1) % size;
            }
        }

        /** Saturation curve with slope 1 at zero, so quiet signals pass unchanged. */
        static float shape (float x, float drive, int mode) noexcept
        {
            const float k = 1.0f + 3.0f * drive;
            if (mode == dist3)
                return std::tanh (k * x) / k;                                  // odd harmonics
            constexpr float b = 0.35f;                                          // asymmetric -> even harmonics
            const float tb = std::tanh (b);
            return (std::tanh (k * x + b) - tb) / (k * (1.0f - tb * tb));
        }

        /** Amount of colour for the settings (0 = none). */
        static float distDrive (const Settings& s) noexcept
        {
            float d = s.dist == dist2 ? 0.35f : s.dist == dist3 ? 0.55f : 0.0f;
            if (s.ratio == r1) d += 0.12f;         // 1:1 is used as a pure "warmth" box
            if (s.smash)       d += 0.25f;
            return d;
        }

    private:
        float coef (double seconds) const { return (float) std::exp (-1.0 / (juce::jmax (1.0e-5, seconds) * osr)); }

        struct Chan
        {
            Biquad audioHp, detHp, detBand;
            float gr = 0.0f, grSlow = 0.0f, dcX = 0.0f, dcY = 0.0f;
            std::vector<float> dryDelay;
            int wp = 0;
        };

        double sr = 44100.0, osr = 88200.0;
        int channels = 2, latency = 0;
        std::array<Chan, 2> ch;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain, outGain;
        juce::SmoothedValue<float> mixS;
        juce::AudioBuffer<float> dry;
        std::vector<float> inArr;
        float dcCoef = 0.999f, grSlowCoef = 0.999f, blockMaxGr = 0.0f;
    };
}
