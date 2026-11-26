#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

/*  Foss – smart, adaptive tonal-balance EQ.

    Signal path (per hop, stereo-linked):
      STFT (sqrt-Hann, 75 % overlap)  ->  64 log-spaced bands (20 Hz – 20 kHz, ~1/6 octave)
      -> gain curve in dB =  adaptive   (Recover / Tame / Bias against a smoothed spectral envelope)
                           + correction (learned long-term spectrum vs. a target profile)
                           + brighten (tilt) + bass (low shelf)
      -> interpolated per bin, auto-gain, mix/delta in the frequency domain -> overlap-add.

    The gains are real and smooth across frequency, so the processing is zero-phase and
    mixing in the frequency domain equals a phase-aligned dry/wet mix. Latency = FFT size. */
namespace foss
{
    constexpr int numBands = 64;
    constexpr float minBandHz = 20.0f, maxBandHz = 20000.0f;
    constexpr float bandSpacingOct = 9.965784f / (float) (numBands - 1);   // log2(1000) / 63
    constexpr float learnSeconds = 10.0f;    // seconds of signal collected by Learn
    constexpr float minLearnSeconds = 2.0f;  // stopping earlier than this discards the result

    inline float bandFreq (int b) { return minBandHz * std::pow (maxBandHz / minBandHz, (float) b / (float) (numBands - 1)); }
    inline float freqToBandPos (float f)
    {
        return (float) (numBands - 1) * std::log (juce::jmax (1.0f, f) / minBandHz) / std::log (maxBandHz / minBandHz);
    }

    //==========================================================================
    // Target profiles for Learn. dB relative values per fractional-octave band (pink noise = flat).
    inline const juce::StringArray profileNames { "Universell", "Vokal", "Akustisk gitar", "Tale", "Mix / master" };
    constexpr int numProfiles = 5;

    struct Pt { float f, db; };

    inline float profileDb (int profile, float f)
    {
        static const std::vector<Pt> curves[numProfiles] = {
            { { 20, -3 }, { 60, 1.5f }, { 120, 1.5f }, { 300, 0.5f }, { 1000, -1.5f }, { 3000, -4 }, { 6000, -6.5f },
              { 10000, -9 }, { 16000, -14 }, { 20000, -19 } },
            { { 20, -30 }, { 80, -14 }, { 150, -3 }, { 300, 0.5f }, { 700, 0.5f }, { 1500, -1.5f }, { 3000, -3 },
              { 5000, -4.5f }, { 8000, -8 }, { 12000, -12 }, { 16000, -17 }, { 20000, -24 } },
            // acoustic guitar: natural body around 100–300 Hz (measured on real recordings), same top end as before
            { { 20, -24 }, { 50, -12 }, { 70, -5 }, { 100, 1.5f }, { 130, 4 }, { 170, 5 }, { 220, 4.5f }, { 300, 3 },
              { 400, 1.5f }, { 550, 0 }, { 800, -0.8f }, { 1500, -2.7f }, { 3000, -4.5f }, { 6000, -7 },
              { 10000, -10.5f }, { 16000, -16 }, { 20000, -22 } },
            { { 20, -34 }, { 90, -16 }, { 160, -4 }, { 350, 0 }, { 800, 0 }, { 1600, -1.5f }, { 3000, -3.5f },
              { 5000, -6 }, { 8000, -10 }, { 12000, -16 }, { 20000, -30 } },
            { { 20, -2 }, { 50, 4 }, { 100, 4 }, { 200, 2 }, { 500, 0 }, { 1000, -1.5f }, { 2000, -3.5f }, { 4000, -6 },
              { 8000, -9.5f }, { 12000, -12.5f }, { 16000, -17 }, { 20000, -23 } },
        };
        const auto& c = curves[juce::jlimit (0, numProfiles - 1, profile)];
        if (f <= c.front().f) return c.front().db;
        if (f >= c.back().f)  return c.back().db;
        for (size_t i = 1; i < c.size(); ++i)
            if (f <= c[i].f)
            {
                const float t = std::log (f / c[i - 1].f) / std::log (c[i].f / c[i - 1].f);
                return c[i - 1].db + t * (c[i].db - c[i - 1].db);
            }
        return c.back().db;
    }

    inline float tiltDb (float brighten, float f)   // brighten -1..1, 1 dB/oct around 1 kHz at full
    {
        return juce::jlimit (-6.0f, 6.0f, brighten * std::log2 (f / 1000.0f));
    }

    inline float bassDb (float bass, float f)       // bass -1..1, ±6 dB low shelf around 120 Hz
    {
        const float x = f / 120.0f;
        return bass * 6.0f / (1.0f + x * x);
    }

    /** Smooth fade-in/-out (half an octave) outside the frequency range. */
    inline float rangeWeight (float f, float lowHz, float highHz)
    {
        auto ss = [] (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); };
        const float lo = lowHz <= minBandHz + 0.5f ? 1.0f : ss (std::log2 (f / lowHz) * 2.0f + 1.0f);
        const float hi = highHz >= maxBandHz - 1.0f ? 1.0f : ss (std::log2 (highHz / f) * 2.0f + 1.0f);
        return lo * hi;
    }

    /** Gaussian smoothing across bands (sigma in bands), edge-normalised. */
    inline void smoothBands (const float* in, float* out, float sigma)
    {
        const int r = juce::jmax (1, (int) std::ceil (sigma * 2.5f));
        for (int b = 0; b < numBands; ++b)
        {
            float sum = 0.0f, wsum = 0.0f;
            for (int o = -r; o <= r; ++o)
            {
                const int i = b + o;
                if (i < 0 || i >= numBands) continue;
                const float w = std::exp (-0.5f * (float) (o * o) / (sigma * sigma));
                sum += w * in[i];
                wsum += w;
            }
            out[b] = sum / wsum;
        }
    }

    inline float smoothstep (float edge0, float edge1, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    /** Static correction from a learned spectrum (dB, any offset) and a profile.

        Learn is meant to fix the broad tonal balance, not to flatten the character of the source:
          - the learned spectrum is viewed at ~1/2-octave resolution, so single notes, vowels and
            formants are not "corrected" away
          - deviations within ±tolerance dB of the profile are left alone
          - only part of the remaining deviation is corrected, and large corrections are soft-limited
            (boosts harder than cuts)
          - boosts are only applied where the source has real content, and fade out at the extremes
            (no lifting of rumble, pre-filtered lows or the noise floor up top)
        At Correction = 100 % a typical result stays within roughly ±3–5 dB. */
    struct CorrectionTuning
    {
        float viewSigma = 3.0f;        // bands (~0.47 oct) – smoothing of the learned spectrum
        float tolerance = 1.0f;        // dB left uncorrected
        float strength = 0.75f;        // fraction of the deviation beyond the tolerance
        float maxBoost = 5.0f, maxCut = 8.0f;  // soft limits (dB)
        float contentRange = 30.0f;    // boosts need content within this many dB of the spectral peak
        float outSigma = 2.5f;         // bands – smoothing of the final curve
    };

    inline void computeCorrection (const float* learnedDb, int profile, float* out, const CorrectionTuning& tu = {})
    {
        std::array<float, numBands> L {};
        smoothBands (learnedDb, L.data(), tu.viewSigma);
        float maxL = -200.0f;
        for (auto v : L) maxL = juce::jmax (maxL, v);

        std::array<float, numBands> diff {}, content {};
        std::array<float, numBands> mid {};   // no allocation: also called on the audio thread
        int numMid = 0;
        for (int b = 0; b < numBands; ++b)
        {
            const float f = bandFreq (b);
            diff[(size_t) b] = profileDb (profile, f) - L[(size_t) b];
            content[(size_t) b] = smoothstep (maxL - tu.contentRange, maxL - tu.contentRange + 12.0f, L[(size_t) b]);
            if (f >= 150.0f && f <= 5000.0f && content[(size_t) b] > 0.5f)
                mid[(size_t) numMid++] = diff[(size_t) b];
        }
        // level reference: median deviation where the source lives (robust against one odd region)
        float offset = 0.0f;
        if (numMid > 0)
        {
            std::nth_element (mid.begin(), mid.begin() + numMid / 2, mid.begin() + numMid);
            offset = mid[(size_t) (numMid / 2)];
        }

        std::array<float, numBands> c {};
        for (int b = 0; b < numBands; ++b)
        {
            const float f = bandFreq (b);
            const float d = diff[(size_t) b] - offset;
            const float mag = juce::jmax (0.0f, std::abs (d) - tu.tolerance) * tu.strength;
            if (d > 0.0f)
            {
                // boosts: soft-limited, only with content, faded out below ~100 Hz and above ~12 kHz
                const float edge = smoothstep (70.0f, 140.0f, f) * (1.0f - 0.6f * smoothstep (10000.0f, 16000.0f, f));
                c[(size_t) b] = tu.maxBoost * std::tanh (mag / tu.maxBoost) * content[(size_t) b] * edge;
            }
            else
                c[(size_t) b] = -tu.maxCut * std::tanh (mag / tu.maxCut);
        }
        smoothBands (c.data(), out, tu.outSigma);
    }

    //==========================================================================
    struct Settings
    {
        float recover = 0.3f, tame = 0.3f, bias = 0.0f;  // 0..1, 0..1, -1..1
        float brighten = 0.0f, bass = 0.0f;              // -1..1
        float correction = 1.0f;                         // 0..1.5
        int profile = 0;
        float lowHz = 20.0f, highHz = 20000.0f;
        float mix = 1.0f, outputDb = 0.0f;
        bool autoGain = true, delta = false, bypass = false;
    };

    /** Lock-free values shared with the GUI. */
    struct SharedState
    {
        std::array<std::atomic<float>, numBands> inDb {}, outDb {}, gainDb {}, staticDb {};
        std::array<std::atomic<float>, numBands> learnedDb {};   // normalised (max = 0 dB)
        std::atomic<bool> hasLearned { false };
        std::atomic<int> learnedVersion { 0 };     // bumped whenever learnedDb changes
        std::atomic<bool> learning { false };
        std::atomic<float> learnProgress { 0.0f };
        std::atomic<int> learnCommand { 0 };       // 1 = start, 2 = stop (finish), 3 = cancel
        std::atomic<float> autoGainDb { 0.0f };
        std::atomic<int> displaySerial { 0 };

        void setLearned (const float* db, bool has)
        {
            for (int b = 0; b < numBands; ++b) learnedDb[(size_t) b].store (has ? db[b] : 0.0f, std::memory_order_relaxed);
            hasLearned.store (has);
            learnedVersion.fetch_add (1);
        }
    };

    //==========================================================================
    class Engine
    {
    public:
        explicit Engine (SharedState& s) : shared (s) {}

        static int fftOrderFor (double sr) { return sr <= 50000.0 ? 11 : sr <= 100000.0 ? 12 : 13; }
        int getLatency() const { return N; }

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = juce::jlimit (1, 2, numChannels);
            const int order = fftOrderFor (sr);
            N = 1 << order;
            hop = N / 4;
            fft = std::make_unique<juce::dsp::FFT> (order);

            window.resize ((size_t) N);
            for (int n = 0; n < N; ++n)
                window[(size_t) n] = std::sqrt (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) N));
            olaScale = 0.5f;   // sum of Hann windows at 75 % overlap = 2

            for (int c = 0; c < 2; ++c)
            {
                inBuf[c].assign ((size_t) N, 0.0f);
                outBuf[c].assign ((size_t) N, 0.0f);
                frame[c].assign ((size_t) N * 2, 0.0f);
            }
            wpos = 0;
            hopCount = 0;

            const int nBins = N / 2 + 1;
            const float binHz = (float) sr / (float) N;
            power.assign ((size_t) nBins, 0.0f);
            binGain.assign ((size_t) nBins, 1.0f);
            binBand.resize ((size_t) nBins);
            binFrac.resize ((size_t) nBins);
            for (int k = 0; k < nBins; ++k)
            {
                const float pos = juce::jlimit (0.0f, (float) (numBands - 1) - 1.0e-4f, freqToBandPos ((float) k * binHz));
                binBand[(size_t) k] = (int) pos;
                binFrac[(size_t) k] = pos - (float) (int) pos;
            }

            // analysis weights: triangular in log-frequency, at least ~1.5 bins wide
            for (int b = 0; b < numBands; ++b)
            {
                auto& w = bandWeights[(size_t) b];
                w.clear();
                const float fc = bandFreq (b);
                const float halfOct = juce::jmax (bandSpacingOct, std::log2 ((fc + 1.5f * binHz) / fc));
                const float lo = fc * std::pow (2.0f, -halfOct), hi = fc * std::pow (2.0f, halfOct);
                const int k0 = juce::jmax (1, (int) std::floor (lo / binHz)), k1 = juce::jmin (nBins - 1, (int) std::ceil (hi / binHz));
                float wsum = 0.0f;
                for (int k = k0; k <= k1; ++k)
                {
                    const float d = std::abs (std::log2 (juce::jmax (1.0f, (float) k * binHz) / fc)) / halfOct;
                    if (d < 1.0f) { w.push_back ({ k, 1.0f - d }); wsum += 1.0f - d; }
                }
                if (w.empty()) { w.push_back ({ juce::jlimit (1, nBins - 1, juce::roundToInt (fc / binHz)), 1.0f }); wsum = 1.0f; }
                // mean power per bin x bandwidth (in bins) -> band energy (pink noise reads flat)
                const float bwBins = juce::jmax (1.0f, fc * (std::pow (2.0f, bandSpacingOct * 0.5f) - std::pow (2.0f, -bandSpacingOct * 0.5f)) / binHz);
                for (auto& p : w) p.second *= bwBins / wsum;
            }
            // full-scale sine ~ 0 dB: |X| peak = N/pi with sqrt-Hann, both channels averaged
            powerNorm = (juce::MathConstants<float>::pi / (float) N) * (juce::MathConstants<float>::pi / (float) N);

            const float dt = (float) hop / (float) sr;
            auto coef = [dt] (float tau) { return 1.0f - std::exp (-dt / tau); };
            aEnvAtt = coef (0.012f);  aEnvRel = coef (0.10f);
            aCut = coef (0.025f);     aCutRel = coef (0.18f);
            aBoost = coef (0.06f);    aBoostRel = coef (0.22f);
            aAuto = coef (0.6f);
            aDisp = coef (0.05f);     aDispRel = coef (0.35f);
            frameSeconds = dt;

            envDb.fill (-120.0f);
            adaptDb.fill (0.0f);
            dispIn.fill (-120.0f);
            dispOut.fill (-120.0f);
            autoDb = 0.0f;
            outGain.reset (sr, 0.05);
            outGain.setCurrentAndTargetValue (1.0f);
            maxLearnFrames = (int) std::ceil (learnSeconds / dt) + 4;
            learnStore.assign ((size_t) maxLearnFrames * numBands, 0.0f);
            learnLoud.assign ((size_t) maxLearnFrames, -200.0f);
            learnScratch.assign ((size_t) maxLearnFrames, 0.0f);
            learnFrames = 0;
            learnedVersionSeen = -1;

            for (int b = 0; b < numBands; ++b)
            {
                // rough loudness weighting (less weight to deep bass and extreme highs)
                const float f = bandFreq (b);
                const float lw = 1.0f / (1.0f + std::pow (120.0f / f, 2.0f)) / (1.0f + std::pow (f / 14000.0f, 2.0f));
                loudnessWeight[(size_t) b] = (double) lw * (f > 1500.0f && f < 6000.0f ? 1.4 : 1.0);
            }
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c)
            {
                std::fill (inBuf[c].begin(), inBuf[c].end(), 0.0f);
                std::fill (outBuf[c].begin(), outBuf[c].end(), 0.0f);
            }
            envDb.fill (-120.0f);
            adaptDb.fill (0.0f);
        }

        void process (juce::AudioBuffer<float>& buffer, const Settings& s)
        {
            settings = s;
            handleLearnCommand();

            const int n = buffer.getNumSamples();
            const int nch = juce::jmin (channels, buffer.getNumChannels());
            outGain.setTargetValue (s.bypass ? 1.0f : juce::Decibels::decibelsToGain (s.outputDb));

            float* data[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < nch; ++c)
                {
                    inBuf[c][(size_t) wpos] = data[c][i];
                    data[c][i] = outBuf[c][(size_t) wpos];
                    outBuf[c][(size_t) wpos] = 0.0f;
                }
                const float g = outGain.getNextValue();
                for (int c = 0; c < nch; ++c) data[c][i] *= g;

                wpos = (wpos + 1) % N;
                if (++hopCount == hop)
                {
                    hopCount = 0;
                    processFrame (nch);
                }
            }
        }

    private:
        //======================================================================
        void handleLearnCommand()
        {
            const int cmd = shared.learnCommand.exchange (0);
            if (cmd == 1)
            {
                learnFrames = 0;
                shared.learnProgress.store (0.0f);
                shared.learning.store (true);
            }
            else if (cmd == 2 && shared.learning.load())
                finishLearning();
            else if (cmd == 3)
                shared.learning.store (false);
        }

        /** Long-term spectrum of the collected frames.
            - relative gate: only frames within 20 dB of the loud parts (no reverb tails, breaths or bleed)
            - every frame contributes its spectral shape with equal weight, so a few loud notes
              do not dominate the result */
        void finishLearning()
        {
            shared.learning.store (false);
            const int n = learnFrames;
            if ((float) n * frameSeconds < minLearnSeconds)
                return;

            std::copy (learnLoud.begin(), learnLoud.begin() + n, learnScratch.begin());
            const int k90 = juce::jlimit (0, n - 1, (int) (0.9f * (float) n));
            std::nth_element (learnScratch.begin(), learnScratch.begin() + k90, learnScratch.begin() + n);
            const float gate = learnScratch[(size_t) k90] - 20.0f;

            std::array<double, numBands> acc {};
            int used = 0;
            for (int i = 0; i < n; ++i)
            {
                if (learnLoud[(size_t) i] < gate) continue;
                const float* e = &learnStore[(size_t) i * numBands];
                const double norm = std::pow (10.0, -0.1 * (double) learnLoud[(size_t) i]);
                for (int b = 0; b < numBands; ++b) acc[(size_t) b] += (double) e[b] * norm;
                ++used;
            }
            if (used == 0) return;

            std::array<float, numBands> db {};
            float maxDb = -200.0f;
            for (int b = 0; b < numBands; ++b)
            {
                db[(size_t) b] = 10.0f * std::log10 ((float) (acc[(size_t) b] / (double) used) + 1.0e-20f);
                maxDb = juce::jmax (maxDb, db[(size_t) b]);
            }
            for (auto& v : db) v = juce::jmax (-120.0f, v - maxDb);
            shared.setLearned (db.data(), true);
        }

        void processFrame (int nch)
        {
            const int nBins = N / 2 + 1;

            // --- analysis: windowed frames of the last N samples (oldest first) ---
            std::fill (power.begin(), power.end(), 0.0f);
            for (int c = 0; c < nch; ++c)
            {
                auto& f = frame[c];
                for (int n = 0; n < N; ++n)
                    f[(size_t) n] = inBuf[c][(size_t) ((wpos + n) % N)] * window[(size_t) n];
                std::fill (f.begin() + N, f.end(), 0.0f);
                fft->performRealOnlyForwardTransform (f.data(), false);
                for (int k = 0; k < nBins; ++k)
                {
                    const float re = f[(size_t) (2 * k)], im = f[(size_t) (2 * k + 1)];
                    power[(size_t) k] += (re * re + im * im) / (float) nch;
                }
            }

            std::array<float, numBands> bandDb {}, bandLin {};
            float maxBand = -200.0f, total = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                float e = 0.0f;
                for (auto& [k, w] : bandWeights[(size_t) b]) e += w * power[(size_t) k];
                e *= powerNorm;
                bandLin[(size_t) b] = e;
                total += e;
                bandDb[(size_t) b] = 10.0f * std::log10 (e + 1.0e-20f);
                maxBand = juce::jmax (maxBand, bandDb[(size_t) b]);
            }
            const float totalDb = 10.0f * std::log10 (total + 1.0e-20f);
            const bool signalPresent = totalDb > -65.0f;

            // --- learn: long-term average of frames with signal ---
            if (shared.learning.load (std::memory_order_relaxed) && signalPresent && learnFrames < maxLearnFrames)
            {
                double loud = 0.0;
                float* dst = &learnStore[(size_t) learnFrames * numBands];
                for (int b = 0; b < numBands; ++b)
                {
                    dst[b] = bandLin[(size_t) b];
                    loud += (double) bandLin[(size_t) b] * loudnessWeight[(size_t) b];
                }
                learnLoud[(size_t) learnFrames] = (float) (10.0 * std::log10 (loud + 1.0e-20));
                ++learnFrames;
                const float prog = (float) learnFrames * frameSeconds / learnSeconds;
                shared.learnProgress.store (juce::jmin (1.0f, prog));
                if (prog >= 1.0f) finishLearning();
            }

            // --- static curve: correction + brighten + bass (recomputed only when needed) ---
            updateStaticCurve();

            // --- adaptive (Recover / Tame) against a smoothed spectral envelope ---
            for (int b = 0; b < numBands; ++b)
            {
                const float x = juce::jmax (-120.0f, bandDb[(size_t) b]);
                float& e = envDb[(size_t) b];
                e += (x > e ? aEnvAtt : aEnvRel) * (x - e);
            }
            std::array<float, numBands> ref {}, target {};
            smoothBands (envDb.data(), ref.data(), 4.0f);   // ~0.6 octave

            float envMax = -200.0f;
            for (auto v : envDb) envMax = juce::jmax (envMax, v);

            const float recScale = juce::jlimit (0.0f, 2.0f, 1.0f + settings.bias);
            const float tameScale = juce::jlimit (0.0f, 2.0f, 1.0f - settings.bias);
            const float kR = juce::jmin (0.9f, settings.recover * 0.55f * recScale);
            const float kT = juce::jmin (0.95f, settings.tame * 0.85f * tameScale);
            const float maxBoost = 9.0f * juce::jmin (1.0f, settings.recover * recScale);
            const float maxCut = 12.0f * juce::jmin (1.0f, settings.tame * tameScale);

            for (int b = 0; b < numBands; ++b)
            {
                const float e = envDb[(size_t) b];
                const float d = e - ref[(size_t) b];
                // absolute and relative gate: never lift noise floor or silence
                const float gate = juce::jlimit (0.0f, 1.0f, (e - juce::jmax (-90.0f, envMax - 60.0f)) / 10.0f);
                float g = 0.0f;
                if (d < 0.0f) g = juce::jmin (maxBoost, -d * kR) * gate;
                else          g = -juce::jmin (maxCut, juce::jmax (0.0f, d - 1.0f) * kT);
                target[(size_t) b] = g * rangeW[(size_t) b];
            }
            smoothBands (target.data(), ref.data(), 0.7f);   // reuse ref as scratch

            for (int b = 0; b < numBands; ++b)
            {
                const float t = ref[(size_t) b];
                float& a = adaptDb[(size_t) b];
                const float coef = t < 0.0f ? (t < a ? aCut : aCutRel) : (t > a ? aBoost : aBoostRel);
                a += coef * (t - a);
                if (! signalPresent) a += aCutRel * (0.0f - a);   // relax towards flat in silence
            }

            std::array<float, numBands> totalGain {};
            for (int b = 0; b < numBands; ++b)
                totalGain[(size_t) b] = settings.bypass ? 0.0f
                                                       : juce::jlimit (-18.0f, 18.0f, adaptDb[(size_t) b] + staticDb[(size_t) b]);

            // --- auto gain: keep the (roughly loudness-weighted) level unchanged ---
            if (settings.autoGain && ! settings.bypass && signalPresent)
            {
                double before = 0.0, after = 0.0;
                for (int b = 0; b < numBands; ++b)
                {
                    const double w = (double) bandLin[(size_t) b] * loudnessWeight[(size_t) b];
                    before += w;
                    after += w * std::pow (10.0, (double) totalGain[(size_t) b] / 10.0);
                }
                const float want = before > 1.0e-12 ? juce::jlimit (-12.0f, 12.0f, (float) (-10.0 * std::log10 (after / before))) : autoDb;
                autoDb += aAuto * (want - autoDb);
            }
            else if (! settings.autoGain || settings.bypass)
                autoDb += aAuto * 4.0f * (0.0f - autoDb);
            shared.autoGainDb.store (autoDb, std::memory_order_relaxed);

            // --- per-band linear gain incl. mix / delta ---
            std::array<float, numBands> lin {};
            const float mix = settings.bypass ? 0.0f : settings.mix;
            for (int b = 0; b < numBands; ++b)
            {
                const float wet = juce::Decibels::decibelsToGain (totalGain[(size_t) b] + autoDb);
                lin[(size_t) b] = (settings.delta && ! settings.bypass) ? mix * (wet - 1.0f) : 1.0f + mix * (wet - 1.0f);
            }
            for (int k = 0; k < nBins; ++k)
            {
                const int b = binBand[(size_t) k];
                const float fr = binFrac[(size_t) k];
                binGain[(size_t) k] = lin[(size_t) b] + fr * (lin[(size_t) b + 1] - lin[(size_t) b]);
            }

            // --- synthesis ---
            const float scale = olaScale;   // JUCE's inverse transform is already normalised by 1/N
            for (int c = 0; c < nch; ++c)
            {
                auto& f = frame[c];
                for (int k = 0; k < nBins; ++k)
                {
                    f[(size_t) (2 * k)] *= binGain[(size_t) k];
                    f[(size_t) (2 * k + 1)] *= binGain[(size_t) k];
                }
                for (int k = 1; k < N / 2; ++k)    // mirrored negative frequencies
                {
                    f[(size_t) (2 * (N - k))] *= binGain[(size_t) k];
                    f[(size_t) (2 * (N - k) + 1)] *= binGain[(size_t) k];
                }
                fft->performRealOnlyInverseTransform (f.data());
                auto& o = outBuf[c];
                for (int n = 0; n < N; ++n)
                    o[(size_t) ((wpos + n) % N)] += f[(size_t) n] * window[(size_t) n] * scale;
            }

            // --- display ---
            for (int b = 0; b < numBands; ++b)
            {
                const float x = juce::jmax (-120.0f, bandDb[(size_t) b]);
                const float y = juce::jmax (-120.0f, x + juce::Decibels::gainToDecibels (std::abs (lin[(size_t) b]), -120.0f));
                dispIn[(size_t) b] += (x > dispIn[(size_t) b] ? aDisp : aDispRel) * (x - dispIn[(size_t) b]);
                dispOut[(size_t) b] += (y > dispOut[(size_t) b] ? aDisp : aDispRel) * (y - dispOut[(size_t) b]);
                shared.inDb[(size_t) b].store (dispIn[(size_t) b], std::memory_order_relaxed);
                shared.outDb[(size_t) b].store (dispOut[(size_t) b], std::memory_order_relaxed);
                // curve shape without auto gain (shown separately), mix applied
                const float shape = 1.0f + mix * (juce::Decibels::decibelsToGain (totalGain[(size_t) b]) - 1.0f);
                shared.gainDb[(size_t) b].store (juce::Decibels::gainToDecibels (shape, -60.0f), std::memory_order_relaxed);
                shared.staticDb[(size_t) b].store (staticDb[(size_t) b], std::memory_order_relaxed);
            }
            shared.displaySerial.fetch_add (1, std::memory_order_release);
        }

        void updateStaticCurve()
        {
            const int ver = shared.learnedVersion.load();
            using juce::exactlyEqual;
            const bool same = ver == learnedVersionSeen && settings.profile == cachedProfile
                              && exactlyEqual (settings.correction, cachedCorrection) && exactlyEqual (settings.brighten, cachedBrighten)
                              && exactlyEqual (settings.bass, cachedBass) && exactlyEqual (settings.lowHz, cachedLow)
                              && exactlyEqual (settings.highHz, cachedHigh);
            if (same) return;
            learnedVersionSeen = ver;
            cachedProfile = settings.profile;  cachedCorrection = settings.correction;
            cachedBrighten = settings.brighten; cachedBass = settings.bass;
            cachedLow = settings.lowHz;        cachedHigh = settings.highHz;

            std::array<float, numBands> corr {};
            if (shared.hasLearned.load())
            {
                std::array<float, numBands> l {};
                for (int b = 0; b < numBands; ++b) l[(size_t) b] = shared.learnedDb[(size_t) b].load (std::memory_order_relaxed);
                computeCorrection (l.data(), settings.profile, corr.data());
            }
            for (int b = 0; b < numBands; ++b)
            {
                const float f = bandFreq (b);
                rangeW[(size_t) b] = rangeWeight (f, settings.lowHz, settings.highHz);
                staticDb[(size_t) b] = corr[(size_t) b] * settings.correction * rangeW[(size_t) b]
                                       + tiltDb (settings.brighten, f) + bassDb (settings.bass, f);
            }
        }

        //======================================================================
        SharedState& shared;
        Settings settings;
        double sr = 48000.0;
        int channels = 2, N = 2048, hop = 512, wpos = 0, hopCount = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window, power, binGain, binFrac;
        std::vector<int> binBand;
        std::vector<float> inBuf[2], outBuf[2], frame[2];
        std::array<std::vector<std::pair<int, float>>, numBands> bandWeights;
        float olaScale = 0.5f, powerNorm = 1.0f, frameSeconds = 0.01f;

        float aEnvAtt = 0, aEnvRel = 0, aCut = 0, aCutRel = 0, aBoost = 0, aBoostRel = 0, aAuto = 0, aDisp = 0, aDispRel = 0;
        std::array<float, numBands> envDb {}, adaptDb {}, staticDb {}, rangeW {}, dispIn {}, dispOut {};
        std::array<double, numBands> loudnessWeight {};
        float autoDb = 0.0f;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain;

        std::vector<float> learnStore, learnLoud, learnScratch;   // preallocated in prepare()
        int learnFrames = 0, maxLearnFrames = 0;

        int learnedVersionSeen = -1, cachedProfile = -1;
        float cachedCorrection = -1, cachedBrighten = -9, cachedBass = -9, cachedLow = -1, cachedHigh = -1;
    };
}
