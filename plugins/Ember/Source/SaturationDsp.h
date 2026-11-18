#pragma once

#include <juce_dsp/juce_dsp.h>

// Ember DSP: multiband saturation.
//   input -> oversampling -> Linkwitz-Riley crossover (up to 6 bands, phase-aligned with all-passes)
//         -> per band: feedback, drive, saturation style, level compensation, tone, dynamics, mix, level
//         -> sum -> downsampling -> output
namespace ember
{
    constexpr int maxBands = 6;
    constexpr int maxSplits = maxBands - 1;

    // NB: the order is stored in projects/presets (parameter value) – only append new styles at the end.
    enum Style
    {
        CleanTube = 0, WarmTube, SmoothTube,
        Tape, HotTape,
        Transformer, Console,
        SoftClip, HardClip, Diode,
        CrunchAmp, Fuzz,
        Rectify, Foldback, LoFi, Breakup,
        numStyles
    };

    inline const juce::StringArray styleNames {
        "Clean Tube", "Warm Tube", "Smooth Tube",
        "Tape", "Hot Tape",
        "Transformer", "Console",
        "Soft Clip", "Hard Clip", "Diode",
        "Crunch Amp", "Fuzz",
        "Rectify", "Foldback", "Lo-Fi", "Breakup" };

    /** Menu groups: heading + first style index (styles are contiguous). */
    struct StyleGroup { const char* name; int first, count; };
    inline const StyleGroup styleGroups[] = {
        { "TUBE", CleanTube, 3 }, { "TAPE", Tape, 2 }, { "TRANSFORMER / KONSOLL", Transformer, 2 },
        { "CLIPPING", SoftClip, 3 }, { "AMP", CrunchAmp, 2 }, { "EFFEKT", Rectify, 4 } };

    /** Pre-gain at 100 % drive, per style (dB). */
    inline constexpr float styleMaxDriveDb[numStyles] = {
        18.0f, 24.0f, 30.0f,
        24.0f, 30.0f,
        24.0f, 20.0f,
        30.0f, 30.0f, 30.0f,
        40.0f, 46.0f,
        24.0f, 30.0f, 24.0f, 24.0f };

    inline float driveGain (int style, float drive01) noexcept
    {
        return juce::Decibels::decibelsToGain (styleMaxDriveDb[juce::jlimit (0, numStyles - 1, style)] * drive01);
    }

    //==========================================================================
    namespace curves
    {
        /** tanh with a bias point: even harmonics. Unity slope at 0. */
        inline float asymTanh (float u, float bias) noexcept
        {
            const float tb = std::tanh (bias);
            return (std::tanh (u + bias) - tb) / (1.0f - tb * tb);
        }

        /** Triode-like: hard-ish positive clipping, soft negative. Unity slope at 0. */
        inline float triode (float u) noexcept
        {
            return u >= 0.0f ? std::tanh (u) : u / (1.0f - 0.5f * u);
        }

        inline float cubicSoft (float u) noexcept
        {
            if (u > 1.5f)  return 1.0f;
            if (u < -1.5f) return -1.0f;
            return u - (4.0f / 27.0f) * u * u * u;
        }

        inline float diode (float u) noexcept
        {
            return u >= 0.0f ? 1.0f - std::exp (-u) : (std::exp (0.6f * u) - 1.0f) / 0.6f;
        }

        inline float console (float u) noexcept
        {
            constexpr float hp = juce::MathConstants<float>::halfPi, b = 0.04f;
            return std::sin (juce::jlimit (-hp, hp, u + b)) - std::sin (b);
        }

        inline float fuzz (float u) noexcept
        {
            constexpr float b = 0.5f;
            static const float tb = std::tanh (b), norm = 4.0f * (1.0f - tb * tb);
            return (std::tanh (4.0f * u + b) - tb) / norm;
        }

        /** Crossover distortion: a soft dead zone around zero that widens with drive. */
        inline float breakup (float u, float drive01) noexcept
        {
            const float d = 0.15f * drive01;
            return d < 1.0e-4f ? std::tanh (u) : std::tanh (u - d * std::tanh (u / d));
        }

        inline float smoothTubeBias (float env) noexcept { return 0.2f + 0.25f * juce::jmin (env, 2.0f); }

        /** Memoryless approximation of every style (used for the level-compensation table).
            env = average |u| (for styles whose bias follows the level). */
        inline float staticShape (int style, float u, float drive01, float env) noexcept
        {
            switch (style)
            {
                case CleanTube:   return asymTanh (u, 0.12f);
                case WarmTube:    return asymTanh (u, 0.35f);
                case SmoothTube:  { const float b = smoothTubeBias (env); const float tb = std::tanh (b);
                                    return (triode (u + b) - triode (b)) / (1.0f - tb * tb); }
                case Tape:        return std::tanh (u);
                case HotTape:     return asymTanh (u, 0.1f);
                case Transformer: return cubicSoft (u);
                case Console:     return console (u);
                case SoftClip:    return cubicSoft (u);
                case HardClip:    return juce::jlimit (-1.0f, 1.0f, u);
                case Diode:       return diode (u);
                case CrunchAmp:   return 0.5f * std::tanh (2.0f * asymTanh (u, 0.25f));
                case Fuzz:        return fuzz (u);
                case Rectify:     { const float t = std::tanh (u); return t + 0.7f * std::abs (t); }
                case Foldback:    return std::sin (u);
                case LoFi:        { const float L = std::exp2 (12.0f - 9.0f * drive01);
                                    return std::round (std::tanh (u) * L) / L; }
                case Breakup:     return breakup (u, drive01);
                default:          return u;
            }
        }
    }

    /** Static output compensation so a typical band level (-14 dBFS sine peak) keeps its RMS level
        when Drive is turned up. Interpolated over drive. */
    class CompensationTable
    {
    public:
        static constexpr int steps = 40;

        CompensationTable()
        {
            constexpr int n = 512;
            constexpr float amp = 0.2f;
            for (int s = 0; s < numStyles; ++s)
                for (int d = 0; d <= steps; ++d)
                {
                    const float d01 = (float) d / (float) steps;
                    const float g = driveGain (s, d01);
                    double sumIn = 0, sumOut = 0, mean = 0;
                    std::array<float, n> y {};
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = amp * std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) n);
                        y[(size_t) i] = curves::staticShape (s, g * x, d01, g * amp * 0.6366f);
                        mean += y[(size_t) i];
                        sumIn += x * x;
                    }
                    mean /= n;
                    for (auto v : y)
                        sumOut += (v - mean) * (v - mean);
                    const double c = sumOut > 1.0e-12 ? std::sqrt (sumIn / sumOut) : 1.0;
                    table[(size_t) s][(size_t) d] = (float) juce::jlimit (0.01, 4.0, c);
                }
        }

        float get (int style, float drive01) const noexcept
        {
            const float p = juce::jlimit (0.0f, 1.0f, drive01) * (float) steps;
            const int i = juce::jmin (steps - 1, (int) p);
            const float t = p - (float) i;
            const auto& row = table[(size_t) juce::jlimit (0, numStyles - 1, style)];
            return row[(size_t) i] + t * (row[(size_t) i + 1] - row[(size_t) i]);
        }

    private:
        std::array<std::array<float, steps + 1>, numStyles> table {};
    };

    //==========================================================================
    struct BandSettings
    {
        int style = WarmTube;
        float drive = 0.25f;    // 0..1
        float dynamics = 0.0f;  // -1..1
        float tone = 0.0f;      // -1..1
        float feedback = 0.0f;  // 0..1
        float fbFreq = 1000.0f; // Hz
        float mix = 1.0f;       // 0..1
        float levelDb = 0.0f;
        bool on = true, mute = false;
    };

    struct Settings
    {
        float inputDb = 0.0f, outputDb = 0.0f, mix = 1.0f;
        int quality = 1;        // 0 = 2x, 1 = 4x, 2 = 8x
        bool autoGain = true;
        int numBands = 1;
        std::array<float, maxSplits> splits { 120.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f };
        std::array<BandSettings, maxBands> bands {};
        int soloMask = 0;
        bool holdDry = false;   // GUI is restructuring bands: play dry until it is done
    };

    /** Makes crossover frequencies strictly increasing (at least 1/4 octave apart). */
    inline std::array<float, maxSplits> sanitiseSplits (const std::array<float, maxSplits>& in, int numBands)
    {
        auto out = in;
        const int n = juce::jlimit (0, maxSplits, numBands - 1);
        float lo = 20.0f;
        for (int i = 0; i < n; ++i)
        {
            out[(size_t) i] = juce::jlimit (lo, 20000.0f, out[(size_t) i]);
            lo = out[(size_t) i] * 1.19f;
        }
        for (int i = n - 1; i >= 0; --i) // keep room at the top end
        {
            const float hi = 20000.0f / std::pow (1.19f, (float) (n - 1 - i));
            out[(size_t) i] = juce::jmin (out[(size_t) i], hi);
        }
        return out;
    }

    //==========================================================================
    class Engine
    {
    public:
        Engine()
        {
            using OS = juce::dsp::Oversampling<float>;
            for (int q = 0; q < 3; ++q)
                os[(size_t) q] = std::make_unique<OS> (2, (size_t) q + 1, OS::filterHalfBandPolyphaseIIR, true, true);
        }

        void prepare (double sampleRate, int maxBlockSize)
        {
            baseRate = sampleRate;
            maxBlock = juce::jmax (1, maxBlockSize);
            for (auto& o : os)
            {
                o->initProcessing ((size_t) maxBlock);
                o->reset();
            }
            dryOs.setSize (2, maxBlock * 8, false, true, false);
            inGain.reset (sampleRate, 0.03);
            outGain.reset (sampleRate, 0.03);
            setQuality (currentQuality, true);
        }

        /** Latency in samples at the base rate (for the current quality). */
        int getLatency() const { return (int) std::round (os[(size_t) currentQuality]->getLatencyInSamples()); }

        bool isFullyDry() const noexcept { return topoFade <= 0.0f; }

        /** Peak levels and meters (written here, read by the processor). */
        float lastInPeak[2] {}, lastOutPeak[2] {};

        /** Processes buffer in place. numSamples must be <= maxBlock. */
        void process (juce::AudioBuffer<float>& buffer, int start, int numSamples, const Settings& s)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            if (numCh == 0 || numSamples <= 0)
                return;

            // quality change: this block fades out on the old setting, the next one fades in on the new
            int fade = 0; // -1 = out, +1 = in
            const int wantedQuality = juce::jlimit (0, 2, s.quality);
            if (qualityFadeInPending)
            {
                setQuality (pendingQuality, false);
                qualityFadeInPending = false;
                fade = 1;
            }
            else if (wantedQuality != currentQuality)
            {
                pendingQuality = wantedQuality;
                qualityFadeInPending = true;
                fade = -1;
            }

            inGain.setTargetValue (juce::Decibels::decibelsToGain (s.inputDb));
            outGain.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));

            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c, start);
                auto sm = inGain;
                for (int i = 0; i < numSamples; ++i)
                    d[i] *= sm.getNextValue();
                lastInPeak[c] = juce::FloatVectorOperations::findMaximum (d, numSamples);
                lastInPeak[c] = juce::jmax (lastInPeak[c], -juce::FloatVectorOperations::findMinimum (d, numSamples));
            }
            inGain.skip (numSamples);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh,
                                                (size_t) start, (size_t) numSamples);
            auto& o = *os[(size_t) currentQuality];
            auto up = o.processSamplesUp (block);
            const int n = (int) up.getNumSamples();

            for (int c = 0; c < numCh; ++c)
                dryOs.copyFrom (c, 0, up.getChannelPointer ((size_t) c), n);

            processOversampled (up, numCh, n, s);
            o.processSamplesDown (block);

            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c, start);
                auto sm = outGain;
                for (int i = 0; i < numSamples; ++i)
                {
                    float v = d[i] * sm.getNextValue();
                    if (fade != 0)
                    {
                        const float t = (float) i / (float) juce::jmax (1, numSamples - 1);
                        v *= fade > 0 ? t : 1.0f - t;
                    }
                    d[i] = v;
                }
                lastOutPeak[c] = juce::jmax (juce::FloatVectorOperations::findMaximum (d, numSamples),
                                             -juce::FloatVectorOperations::findMinimum (d, numSamples));
            }
            outGain.skip (numSamples);
        }

    private:
        //======================================================================
        struct ChannelState
        {
            float dcX = 0, dcY = 0;
            float emph = 0, deemph = 0, hfLoss = 0;
            float lpA = 0, lpB = 0, hp = 0;
            float tone = 0;
            float envIn = 0, envOut = 0, biasEnv = 0;
            float ic1 = 0, ic2 = 0, yPrev = 0;
            float holdVal = 0;
            int holdCount = 0;
        };

        /** Values smoothed per oversampled sample. */
        struct Smoothed
        {
            float g = 1, comp = 1, mix = 1, level = 1, gl = 1, gh = 1, fb = 0, dyn = 0;
        };

        struct Band
        {
            int style = WarmTube, pendingStyle = WarmTube;
            float styleFade = 1.0f;
            Smoothed cur, target;
            float drive01 = 0.25f;
            // per sub-block coefficients
            float aTone = 0.1f, fbG = 0.1f, fbK = 1.0f, fbH = 1.0f, aLoss = 1.0f, lofiL = 4096.0f;
            int holdN = 1;
            std::array<ChannelState, 2> ch {};
            bool first = true, snap = false;
        };

        void setQuality (int q, bool force)
        {
            if (q == currentQuality && ! force)
                return;
            currentQuality = q;
            os[(size_t) q]->reset();
            osFactor = 1 << (q + 1);
            osRate = baseRate * osFactor;

            juce::dsp::ProcessSpec spec { osRate, (juce::uint32) (maxBlock * osFactor), 2 };
            for (auto& f : xover) { f.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); f.prepare (spec); }
            for (auto& row : ap)
                for (auto& f : row) { f.setType (juce::dsp::LinkwitzRileyFilterType::allpass); f.prepare (spec); }

            // per-rate constants
            const auto onePole = [this] (float hz) { return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) osRate); };
            aEmph    = onePole (2500.0f);
            aTrans   = onePole (120.0f);
            aAmpHp   = onePole (150.0f);
            aAmpLp   = onePole (6000.0f);
            aCab     = onePole (5000.0f);
            aFuzzLp  = onePole (4500.0f);
            aBias    = onePole (8.0f);
            dcR      = std::exp (-juce::MathConstants<float>::twoPi * 8.0f / (float) osRate);
            envAtt   = onePole (1.0f / (juce::MathConstants<float>::twoPi * 0.003f));
            envRel   = onePole (1.0f / (juce::MathConstants<float>::twoPi * 0.060f));
            smoothA  = onePole (1.0f / (juce::MathConstants<float>::twoPi * 0.015f));
            fadeStep = 1.0f / (0.005f * (float) osRate);

            resetBands();
            haveSplits = false;
        }

        void resetBands()
        {
            for (auto& f : xover) f.reset();
            for (auto& row : ap) for (auto& f : row) f.reset();
            for (auto& b : bands)
            {
                b.ch = {};
                b.first = true;
                b.styleFade = 1.0f;
            }
        }

        void updateSplits (const Settings& s, int numSub)
        {
            const auto target = sanitiseSplits (s.splits, activeBands);
            // glide in the log domain (~25 ms)
            const float a = haveSplits ? juce::jmin (1.0f, (float) numSub / (0.025f * (float) osRate)) : 1.0f;
            for (int i = 0; i < activeBands - 1; ++i)
            {
                auto& f = splitHz[(size_t) i];
                f = haveSplits ? f * std::pow (target[(size_t) i] / f, a) : target[(size_t) i];
                xover[(size_t) i].setCutoffFrequency (f);
                for (int k = 0; k < i; ++k)          // band k (< i) needs the all-pass of split i
                    ap[(size_t) k][(size_t) i].setCutoffFrequency (f);
            }
            haveSplits = true;
        }

        void updateBandTargets (const Settings& s)
        {
            const bool anySolo = (s.soloMask & ((1 << activeBands) - 1)) != 0;
            for (int k = 0; k < activeBands; ++k)
            {
                auto& b = bands[(size_t) k];
                const auto& p = s.bands[(size_t) k];
                const int style = juce::jlimit (0, numStyles - 1, p.style);
                if (b.first)
                {
                    b.style = b.pendingStyle = style;
                    b.styleFade = 1.0f;
                }
                else if (style != b.pendingStyle)
                    b.pendingStyle = style;

                b.drive01 = juce::jlimit (0.0f, 1.0f, p.drive);
                auto& t = b.target;
                t.g    = driveGain (b.style, b.drive01);
                t.comp = s.autoGain ? compTable.get (b.style, b.drive01) : 1.0f;
                t.mix  = p.on ? juce::jlimit (0.0f, 1.0f, p.mix) * juce::jlimit (0.0f, 1.0f, s.mix) : 0.0f;
                const bool silent = p.mute || (anySolo && (s.soloMask & (1 << k)) == 0);
                t.level = silent ? 0.0f : (p.on ? juce::Decibels::decibelsToGain (p.levelDb) : 1.0f);
                t.gh   = juce::Decibels::decibelsToGain (juce::jlimit (-1.0f, 1.0f, p.tone) * 6.0f);
                t.gl   = 1.0f / t.gh;
                t.fb   = juce::jlimit (0.0f, 1.0f, p.feedback) * 0.95f;
                t.dyn  = juce::jlimit (-1.0f, 1.0f, p.dynamics);

                // tone pivot at the band's centre
                const float lo = k == 0 ? 20.0f : splitHz[(size_t) k - 1];
                const float hi = k == activeBands - 1 ? 20000.0f : splitHz[(size_t) k];
                const float pivot = juce::jlimit (60.0f, 12000.0f, std::sqrt (lo * hi));
                b.aTone = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * pivot / (float) osRate);

                // feedback band-pass (TPT state-variable filter, Q = 2)
                const float ff = juce::jlimit (20.0f, 20000.0f, p.fbFreq);
                b.fbG = std::tan (juce::MathConstants<float>::pi * juce::jmin (ff, 0.45f * (float) osRate) / (float) osRate);
                b.fbK = 1.0f / 2.0f;
                b.fbH = 1.0f / (1.0f + b.fbK * b.fbG + b.fbG * b.fbG);

                b.holdN = juce::jmax (1, (int) std::round ((float) osFactor * (1.0f + 5.0f * b.drive01 * b.drive01)));

                b.aLoss = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (20000.0f - 9000.0f * b.drive01) / (float) osRate);
                b.lofiL = std::exp2 (12.0f - 9.0f * b.drive01);

                if (b.first || b.snap)
                {
                    if (b.first) b.cur = t;
                    else { b.cur.g = t.g; b.cur.comp = t.comp; }
                    b.first = b.snap = false;
                }
            }
        }

        void processOversampled (juce::dsp::AudioBlock<float>& up, int numCh, int n, const Settings& s)
        {
            const int wanted = juce::jlimit (1, maxBands, s.numBands);

            constexpr int sub = 32;
            for (int s0 = 0; s0 < n; s0 += sub)
            {
                const int len = juce::jmin (sub, n - s0);

                // restructure while fully dry
                if (topoFade <= 0.0f && wanted != activeBands)
                {
                    activeBands = wanted;
                    resetBands();
                    haveSplits = false;
                }

                updateSplits (s, len);
                updateBandTargets (s);

                for (int i = s0; i < s0 + len; ++i)
                {
                    const bool dryNow = s.holdDry || wanted != activeBands;
                    topoFade = dryNow ? juce::jmax (0.0f, topoFade - fadeStep) : juce::jmin (1.0f, topoFade + fadeStep);

                    for (int k = 0; k < activeBands; ++k)
                        stepBand (bands[(size_t) k]);

                    for (int c = 0; c < numCh; ++c)
                    {
                        auto* d = up.getChannelPointer ((size_t) c);
                        const float x = d[i];
                        float part[maxBands];

                        if (activeBands == 1)
                            part[0] = x;
                        else
                        {
                            float rest = x;
                            for (int k = 0; k < activeBands - 1; ++k)
                            {
                                float lo, hi;
                                xover[(size_t) k].processSample (c, rest, lo, hi);
                                for (int j = k + 1; j < activeBands - 1; ++j)
                                    lo = ap[(size_t) k][(size_t) j].processSample (c, lo);
                                part[k] = lo;
                                rest = hi;
                            }
                            part[activeBands - 1] = rest;
                        }

                        float sum = 0.0f;
                        for (int k = 0; k < activeBands; ++k)
                            sum += processBand (bands[(size_t) k], bands[(size_t) k].ch[(size_t) c], part[k]);

                        const float dry = dryOs.getSample (c, i);
                        d[i] = dry + topoFade * (sum - dry);
                    }
                }
            }

            for (auto& b : bands)
                for (auto& st : b.ch)
                    for (auto* v : { &st.dcX, &st.dcY, &st.emph, &st.deemph, &st.hfLoss, &st.lpA, &st.lpB, &st.hp,
                                     &st.tone, &st.envIn, &st.envOut, &st.ic1, &st.ic2, &st.yPrev })
                        juce::dsp::util::snapToZero (*v);
            for (auto& f : xover) f.snapToZero();
            for (auto& row : ap) for (auto& f : row) f.snapToZero();
        }

        void stepBand (Band& b) noexcept
        {
            auto& c = b.cur;
            const auto& t = b.target;
            const float a = smoothA;
            c.g     += a * (t.g - c.g);
            c.comp  += a * (t.comp - c.comp);
            c.mix   += a * (t.mix - c.mix);
            c.level += a * (t.level - c.level);
            c.gl    += a * (t.gl - c.gl);
            c.gh    += a * (t.gh - c.gh);
            c.fb    += a * (t.fb - c.fb);
            c.dyn   += a * (t.dyn - c.dyn);

            // style change: fade the wet part out, switch, fade back in
            if (b.pendingStyle != b.style)
            {
                b.styleFade -= fadeStep;
                if (b.styleFade <= 0.0f)
                {
                    b.styleFade = 0.0f;
                    b.style = b.pendingStyle;
                    for (auto& st : b.ch)
                    {
                        const float e1 = st.envIn, e2 = st.envOut;
                        st = {};
                        st.envIn = e1; st.envOut = e2;
                    }
                    b.snap = true; // jump to the new style's drive/compensation (wet is silent now)
                }
            }
            else if (b.styleFade < 1.0f)
                b.styleFade = juce::jmin (1.0f, b.styleFade + fadeStep);
        }

        float processBand (const Band& b, ChannelState& s, float x) noexcept
        {
            const auto& p = b.cur;
            using namespace curves;

            // resonant feedback around the saturator
            float fbIn = 0.0f;
            if (p.fb > 1.0e-4f)
            {
                const float hp = (s.yPrev - (b.fbK + b.fbG) * s.ic1 - s.ic2) * b.fbH;
                const float bp = b.fbG * hp + s.ic1;
                s.ic1 = b.fbG * hp + bp;
                const float lp = b.fbG * bp + s.ic2;
                s.ic2 = b.fbG * bp + lp;
                fbIn = p.fb * std::tanh (bp * b.fbK);
            }

            const float u = p.g * x + fbIn;
            float y;

            switch (b.style)
            {
                case CleanTube:  y = asymTanh (u, 0.12f); break;
                case WarmTube:   y = asymTanh (u, 0.35f); break;
                case SmoothTube:
                {
                    s.biasEnv += aBias * (std::abs (u) - s.biasEnv);
                    const float bias = smoothTubeBias (s.biasEnv);
                    const float tb = std::tanh (bias);
                    y = (triode (u + bias) - triode (bias)) / (1.0f - tb * tb);
                    break;
                }
                case Tape:
                case HotTape:
                {
                    const float k = b.style == Tape ? 1.0f : 2.0f;
                    s.emph += aEmph * (u - s.emph);
                    const float e = u + k * (u - s.emph);                       // pre-emphasis
                    float t = b.style == Tape ? std::tanh (e) : asymTanh (e, 0.1f);
                    s.deemph += aEmph * (t - s.deemph);
                    t = s.deemph + (t - s.deemph) / (1.0f + k);                 // de-emphasis
                    s.hfLoss += b.aLoss * (t - s.hfLoss);
                    y = s.hfLoss;
                    break;
                }
                case Transformer:
                {
                    s.lpA += aTrans * (u - s.lpA);
                    const float lo = s.lpA, hi = u - lo;
                    y = std::tanh (1.8f * lo) / 1.8f + cubicSoft (hi);
                    break;
                }
                case Console:    y = console (u); break;
                case SoftClip:   y = cubicSoft (u); break;
                case HardClip:   y = juce::jlimit (-1.0f, 1.0f, u); break;
                case Diode:      y = diode (u); break;
                case CrunchAmp:
                {
                    s.hp += aAmpHp * (u - s.hp);
                    const float st1 = asymTanh (u - s.hp, 0.25f);
                    s.lpA += aAmpLp * (st1 - s.lpA);
                    const float st2 = 0.5f * std::tanh (2.0f * s.lpA);
                    s.lpB += aCab * (st2 - s.lpB);
                    y = s.lpB;
                    break;
                }
                case Fuzz:
                {
                    s.lpA += aFuzzLp * (fuzz (u) - s.lpA);
                    y = s.lpA;
                    break;
                }
                case Rectify:    { const float t = std::tanh (u); y = t + 0.7f * std::abs (t); break; }
                case Foldback:   y = std::sin (u); break;
                case LoFi:
                {
                    if (--s.holdCount <= 0)
                    {
                        s.holdVal = std::round (std::tanh (u) * b.lofiL) / b.lofiL;
                        s.holdCount = b.holdN;
                    }
                    y = s.holdVal;
                    break;
                }
                case Breakup:    y = breakup (u, b.drive01); break;
                default:         y = u; break;
            }

            // DC blocker (asymmetric styles create DC)
            const float yb = y - s.dcX + dcR * s.dcY;
            s.dcX = y;
            s.dcY = yb;
            s.yPrev = yb;

            float wet = yb * p.comp;

            // tone: tilt around the band centre
            s.tone += b.aTone * (wet - s.tone);
            wet = s.tone * p.gl + (wet - s.tone) * p.gh;

            // dynamics: >0 gives back the dynamics the saturation took, <0 squashes further
            if (std::abs (p.dyn) > 1.0e-3f)
            {
                const float ax = std::abs (x), aw = std::abs (wet);
                s.envIn  += (ax > s.envIn  ? envAtt : envRel) * (ax - s.envIn);
                s.envOut += (aw > s.envOut ? envAtt : envRel) * (aw - s.envOut);
                const float r = (s.envIn + 1.0e-5f) / (s.envOut + 1.0e-5f);
                wet *= juce::jlimit (0.25f, 4.0f, std::exp (p.dyn * std::log (r)));
            }

            const float out = x + p.mix * b.styleFade * (wet - x);
            return out * p.level;
        }

        //======================================================================
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> os;
        juce::AudioBuffer<float> dryOs;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain { 1.0f }, outGain { 1.0f };

        std::array<juce::dsp::LinkwitzRileyFilter<float>, maxSplits> xover;
        std::array<std::array<juce::dsp::LinkwitzRileyFilter<float>, maxSplits>, maxSplits> ap; // [band][split]
        std::array<float, maxSplits> splitHz { 120.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f };
        bool haveSplits = false;

        std::array<Band, maxBands> bands;
        CompensationTable compTable;

        double baseRate = 44100.0, osRate = 176400.0;
        int maxBlock = 512, osFactor = 4, currentQuality = 1;
        int activeBands = 1;
        float topoFade = 1.0f;
        int pendingQuality = 1;
        bool qualityFadeInPending = false;

        float aEmph = 0, aTrans = 0, aAmpHp = 0, aAmpLp = 0, aCab = 0, aFuzzLp = 0, aBias = 0, dcR = 0.999f;
        float envAtt = 0, envRel = 0, smoothA = 0, fadeStep = 0.001f;
    };
}
