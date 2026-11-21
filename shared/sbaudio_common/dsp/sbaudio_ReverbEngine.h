#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

/*  Aura reverb engine.

    Signal flow (per channel pair):
      input -> low cut (HPF 12 dB/oct) -> bandwidth (LPF 12 dB/oct, set by Tone)
            -> pre-delay line -> early reflections (multi-tap, per algorithm)
                              -> input diffusion (4 allpasses per channel)
                              -> 8-line feedback delay network
                                   * modulated delay reads (chorused tail)
                                   * in-loop allpass per line (echo density)
                                   * three-band decay (bass / mid / high RT60)
                                   * Hadamard mixing matrix
            -> early + late, optional colour EQ -> 100 % wet output

    Everything is recomputed per 32-sample sub-block from smoothed targets, so all
    controls can be moved and automated without clicks. Changing algorithm fades the
    output out, clears the network and fades back in.
*/
namespace aura
{
    // Order is stored in projects/presets – only append new algorithms.
    enum Algo { Ambience = 0, Room, Chamber, Hall, Plate, Vintage, numAlgos };

    inline const juce::StringArray algoNames { "Ambience", "Room", "Chamber", "Hall", "Plate", "Vintage" };

    inline const char* algoDescription (int a)
    {
        switch (a)
        {
            case Ambience: return "Kort, tett rom uten tydelig hale. Gir nærhet og luft uten at det høres ut som reverb.";
            case Room:     return "Naturlig rom med tydelige tidlige refleksjoner. Plasserer kilden i et ekte rom.";
            case Chamber:  return "Ekkokammer: tett og jevn, varmere enn en hall. Klassisk på vokal.";
            case Hall:     return "Stor konsertsal med myk oppbygging, bred stereo og fyldig bunn.";
            case Plate:    return "Platereverb: umiddelbar, tett og lys. Uten tidlige refleksjoner, legger seg pent bak vokalen.";
            case Vintage:  return "Tidlig digital 80-tallsreverb: mørk, mellomtonefyldig og litt kornete, med tydelig modulasjon.";
            default:       return "";
        }
    }

    struct AlgoConfig
    {
        float lineScale;     // FDN delay-length multiplier
        float diffusion;     // input diffusion allpass coefficient
        float diffScale;     // input diffusion length multiplier
        float loopDiff;      // in-loop allpass coefficient
        float erLevel;       // early reflections level
        float lateLevel;     // late tail level
        float erScale;       // early reflection time spread
        float bassMult;      // RT60 multiplier below bassXover
        float bassXover;     // Hz
        float hfXover;       // Hz, damping crossover
        float hfDark, hfMid, hfBright;  // HF RT60 ratio at Tone -100 / 0 / +100
        float bandwidth;     // Hz, input LPF at Tone 0
        float bandwidthMax;  // Hz, upper limit for the input LPF
        float modDepthMs;    // modulation depth at Mod 100 %
        float modRate;       // Hz, base LFO rate
        float outGain;       // level calibration
        float colourDb;      // mid-range colour (Vintage)
    };

    inline const AlgoConfig& config (int a)
    {
        static const AlgoConfig cfg[numAlgos] = {
            //  line  diff  dScl  loop  er    late  erSc  bass  bX     hfX     dark  mid   brt   bw       bwMax    mod   rate  out    col
            {   0.22f,0.60f,0.45f,0.30f,1.00f,0.55f,0.35f,1.00f,200.0f,4500.0f,0.30f,0.60f,0.90f,13000.0f,20000.0f,0.12f,0.90f,0.63f, 0.0f }, // Ambience
            {   0.38f,0.62f,0.65f,0.40f,0.85f,0.80f,0.60f,1.00f,200.0f,3800.0f,0.30f,0.55f,0.88f,11000.0f,20000.0f,0.18f,0.70f,0.64f, 0.0f }, // Room
            {   0.56f,0.66f,0.85f,0.50f,0.55f,1.00f,0.90f,1.10f,220.0f,3500.0f,0.28f,0.55f,0.85f,10000.0f,18000.0f,0.30f,0.60f,0.62f, 0.0f }, // Chamber
            {   1.00f,0.70f,1.00f,0.55f,0.35f,1.00f,1.40f,1.25f,250.0f,3000.0f,0.25f,0.50f,0.80f, 9000.0f,18000.0f,0.45f,0.45f,0.87f, 0.0f }, // Hall
            {   0.55f,0.75f,0.55f,0.65f,0.00f,1.00f,0.00f,0.85f,300.0f,6000.0f,0.40f,0.70f,0.95f,14000.0f,20000.0f,0.30f,0.80f,0.67f, 0.0f }, // Plate
            {   0.80f,0.50f,1.00f,0.35f,0.25f,1.00f,1.00f,1.10f,250.0f,2500.0f,0.25f,0.45f,0.70f, 7500.0f,10500.0f,0.65f,0.35f,0.78f, 2.5f }, // Vintage
        };
        return cfg[juce::jlimit (0, numAlgos - 1, a)];
    }

    inline constexpr int numLines = 8;
    inline constexpr float lineMs[numLines]     = { 31.7f, 37.3f, 41.9f, 47.9f, 53.3f, 59.1f, 66.7f, 73.1f };
    inline constexpr float loopApMs[numLines]   = { 3.1f, 4.3f, 5.3f, 6.7f, 7.1f, 8.9f, 9.7f, 11.3f };
    inline constexpr float diffMsL[4]           = { 1.31f, 2.17f, 3.71f, 5.29f };
    inline constexpr float diffMsR[4]           = { 1.49f, 2.41f, 3.43f, 5.67f };

    inline constexpr int numTaps = 12;
    inline constexpr float tapMsL[numTaps] = { 4.3f, 7.9f, 11.2f, 15.8f, 19.4f, 23.9f, 29.3f, 34.1f, 41.0f, 47.7f, 55.6f, 63.2f };
    inline constexpr float tapMsR[numTaps] = { 5.1f, 8.8f, 12.9f, 14.6f, 21.3f, 26.2f, 31.0f, 37.8f, 43.5f, 50.9f, 58.1f, 67.0f };
    inline constexpr float tapSign[numTaps] = { 1, -1, 1, 1, -1, 1, -1, -1, 1, -1, 1, -1 };

    inline float tapGain (int k) { return std::pow (1.0f - (float) k / 14.0f, 1.6f) * 0.55f; }

    /** Size 0..1 -> delay multiplier 0.5..1.5 (exponential). */
    inline float sizeMultiplier (float size01) { return 0.5f * std::pow (3.0f, juce::jlimit (0.0f, 1.0f, size01)); }

    /** Tone -1..+1 -> HF RT60 ratio. */
    inline float hfRatio (const AlgoConfig& c, float tone)
    {
        return tone < 0.0f ? juce::jmap (-tone, c.hfMid, c.hfDark) : juce::jmap (tone, c.hfMid, c.hfBright);
    }

    /** Tone -1..+1 -> input bandwidth in Hz. */
    inline float bandwidthHz (const AlgoConfig& c, float tone)
    {
        return juce::jlimit (1500.0f, c.bandwidthMax, c.bandwidth * std::pow (2.0f, tone * 1.3f));
    }

    struct Settings
    {
        int algo = Hall;
        float preDelayMs = 20.0f;
        float decayS = 2.0f;
        float size = 0.5f;     // 0..1
        float tone = 0.0f;     // -1..+1
        float lowCutHz = 100.0f;
        float mod = 0.3f;      // 0..1
    };

    //==========================================================================
    class DelayLine
    {
    public:
        void setMaxSamples (int n)
        {
            int sz = 1;
            while (sz < n + 4) sz <<= 1;
            buf.assign ((size_t) sz, 0.0f);
            mask = sz - 1;
            pos = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float x) noexcept { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }

        /** Integer read: d = 0 is the sample just pushed. */
        float at (int d) const noexcept { return buf[(size_t) ((pos - d) & mask)]; }

        /** Fractional read with 4-point Hermite interpolation (d >= 1). */
        float read (float d) const noexcept
        {
            const int i = (int) d;
            const float f = d - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

    private:
        std::vector<float> buf;
        int mask = 0, pos = 0;
    };

    /** Schroeder allpass with a fixed integer delay. */
    struct Allpass
    {
        DelayLine line;
        int len = 1;
        void prepare (int maxLen) { line.setMaxSamples (maxLen + 1); }
        float process (float x, float g) noexcept
        {
            const float d = line.at (len - 1);
            const float v = x - g * d;
            line.push (v);
            return d + g * v;
        }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0.0f; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        {
            b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
            a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
        }
        void lowpass (double fs, double f, double q = 0.7071)
        {
            const double w = juce::MathConstants<double>::twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void highpass (double fs, double f, double q = 0.7071)
        {
            const double w = juce::MathConstants<double>::twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void peak (double fs, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
            const double c = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
    };

    //==========================================================================
    class Engine
    {
    public:
        static constexpr int subBlock = 32;
        static constexpr float maxPreDelayMs = 250.0f;

        void prepare (double sampleRate)
        {
            fs = sampleRate;
            const float msToS = (float) fs * 0.001f;

            // pre-delay + early reflections share one line per channel
            const int preMax = (int) std::ceil ((maxPreDelayMs + 70.0f * 1.4f * 1.5f + 10.0f) * msToS);
            pre[0].setMaxSamples (preMax);
            pre[1].setMaxSamples (preMax);

            for (int i = 0; i < numLines; ++i)
            {
                lines[i].setMaxSamples ((int) std::ceil ((lineMs[i] * 1.5f + 3.0f) * msToS) + 8);
                loopAp[i].prepare ((int) std::ceil (loopApMs[i] * msToS) + 2);
            }
            for (int k = 0; k < 4; ++k)
            {
                diffL[k].prepare ((int) std::ceil (diffMsL[k] * 1.2f * msToS) + 2);
                diffR[k].prepare ((int) std::ceil (diffMsR[k] * 1.2f * msToS) + 2);
            }
            erSmear[0].prepare ((int) std::ceil (6.0f * msToS) + 2);
            erSmear[1].prepare ((int) std::ceil (6.0f * msToS) + 2);

            initialised = false;
            reset();
        }

        void reset()
        {
            for (auto& p : pre) p.clear();
            for (auto& l : lines) l.clear();
            for (auto& a : loopAp) a.line.clear();
            for (auto& a : diffL) a.line.clear();
            for (auto& a : diffR) a.line.clear();
            for (auto& a : erSmear) a.line.clear();
            for (auto& b : lowCut) b.reset();
            for (auto& b : bandLp) b.reset();
            for (auto& b : colour) b.reset();
            std::fill (std::begin (lpBass), std::end (lpBass), 0.0f);
            std::fill (std::begin (lpHigh), std::end (lpHigh), 0.0f);
        }

        /** Processes in place. numIn: 1 or 2 input channels, numOut: 1 or 2 output channels. */
        void process (float* const* ch, int numIn, int numOut, int n, const Settings& target)
        {
            if (! initialised)
            {
                applyAlgo (target.algo);
                sm = target;
                preA = std::max (1.0f, juce::jlimit (0.0f, maxPreDelayMs, target.preDelayMs) * (float) fs * 0.001f);
                preXfading = false;
                initialised = true;
                fade = 1.0f;
                fadeDir = 0;
            }

            for (int start = 0; start < n; start += subBlock)
            {
                const int len = std::min (subBlock, n - start);
                updateSubBlock (target, len);

                for (int i = start; i < start + len; ++i)
                {
                    const float inL = ch[0][i];
                    const float inR = numIn > 1 ? ch[1][i] : inL;
                    float l, r;
                    tick (inL, inR, l, r);

                    fade = juce::jlimit (0.0f, 1.0f, fade + (float) fadeDir * fadeStep);
                    l *= fade; r *= fade;

                    if (numOut > 1) { ch[0][i] = l; ch[1][i] = r; }
                    else            { ch[0][i] = 0.5f * (l + r); }
                }

                // algorithm switch: once faded out, clear the network and fade back in
                if (fadeDir < 0 && fade <= 0.0f)
                {
                    reset();
                    applyAlgo (pendingAlgo);
                    fadeDir = 1;
                }
                else if (fadeDir > 0 && fade >= 1.0f)
                    fadeDir = 0;
            }
        }

        int getAlgo() const { return algo; }

    private:
        void applyAlgo (int a)
        {
            algo = juce::jlimit (0, numAlgos - 1, a);
            cfg = &config (algo);
            const float msToS = (float) fs * 0.001f;
            for (int k = 0; k < 4; ++k)
            {
                diffL[k].len = std::max (1, (int) std::lround (diffMsL[k] * cfg->diffScale * msToS));
                diffR[k].len = std::max (1, (int) std::lround (diffMsR[k] * cfg->diffScale * msToS));
            }
            for (int i = 0; i < numLines; ++i)
                loopAp[i].len = std::max (1, (int) std::lround (loopApMs[i] * std::sqrt (cfg->lineScale) * msToS));
            erSmear[0].len = std::max (1, (int) std::lround (3.3f * msToS));
            erSmear[1].len = std::max (1, (int) std::lround (4.1f * msToS));

            if (cfg->colourDb != 0.0f)
                for (auto& b : colour) b.peak (fs, 900.0, 0.7, cfg->colourDb);
            for (auto& b : colour) b.reset();
            filtersDirty = true;
        }

        void updateSubBlock (const Settings& t, int len)
        {
            if (t.algo != algo && fadeDir >= 0)
            {
                pendingAlgo = t.algo;
                fadeDir = -1;
            }
            else if (fadeDir < 0)
                pendingAlgo = t.algo;

            // one-pole smoothing towards the targets (~80 ms)
            const float a = 1.0f - std::exp (-(float) len / (0.08f * (float) fs));
            const float aSize = 1.0f - std::exp (-(float) len / (0.5f * (float) fs));
            auto toward = [] (float& v, float target, float coeff) { v += (target - v) * coeff; };
            toward (sm.decayS, t.decayS, a);
            toward (sm.size, t.size, aSize);
            toward (sm.mod, t.mod, a);
            const float oldTone = sm.tone, oldCut = sm.lowCutHz;
            toward (sm.tone, t.tone, a);
            toward (sm.lowCutHz, t.lowCutHz, a);

            const float msToS = (float) fs * 0.001f;
            const auto& c = *cfg;

            // filters (only when they actually move)
            if (filtersDirty || std::abs (sm.tone - oldTone) > 1.0e-4f || std::abs (sm.lowCutHz - oldCut) > 1.0e-3f)
            {
                const double nyq = 0.45 * fs;
                for (auto& b : lowCut) b.highpass (fs, juce::jlimit (10.0, nyq, (double) sm.lowCutHz));
                for (auto& b : bandLp) b.lowpass (fs, std::min ((double) bandwidthHz (c, sm.tone), nyq));
                filtersDirty = false;
            }

            // pre-delay: jumping read position is crossfaded (moving it would pitch-shift and click)
            const float preTarget = std::max (1.0f, juce::jlimit (0.0f, maxPreDelayMs, t.preDelayMs) * msToS);
            if (! preXfading && std::abs (preTarget - preA) > 0.5f)
            {
                preB = preTarget;
                preXf = 0.0f;
                preXfading = true;
            }
            const float sizeMul = sizeMultiplier (sm.size);
            erTimeMul = c.erScale * sizeMul * msToS;

            // FDN delays and per-line decay gains
            const float rt = std::max (0.1f, sm.decayS);
            const float hfR = hfRatio (c, sm.tone);
            for (int i = 0; i < numLines; ++i)
            {
                baseLen[i] = std::max (8.0f, lineMs[i] * c.lineScale * sizeMul * msToS);
                const float loopSamples = baseLen[i] + (float) loopAp[i].len;
                auto g = [&] (float rt60) { return std::min (0.9995f, std::pow (10.0f, -3.0f * loopSamples / (rt60 * (float) fs))); };
                gMid[i]  = g (rt);
                gBass[i] = g (rt * c.bassMult);
                gHigh[i] = g (rt * hfR);
            }
            aBass = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * c.bassXover / (float) fs);
            aHigh = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * std::min (c.hfXover, 0.4f * (float) fs) / (float) fs);

            modDepth = c.modDepthMs * sm.mod * msToS;
            const float rateScale = 0.6f + 0.8f * sm.mod;
            for (int i = 0; i < numLines; ++i)
                lfoInc[i] = juce::MathConstants<float>::twoPi * c.modRate * rateScale * (0.71f + 0.083f * (float) i) / (float) fs;

            // output level: calibrated so a long hall is not much louder than a short room
            outGain = c.outGain * std::pow (std::max (0.2f, rt) / 2.0f, -0.35f);

            fadeStep = 1.0f / (0.02f * (float) fs);
            preXfStep = 1.0f / (0.03f * (float) fs);
        }

        static void hadamard8 (float* x) noexcept
        {
            for (int h = 1; h < 8; h <<= 1)
                for (int i = 0; i < 8; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j] = a + b;
                        x[j + h] = a - b;
                    }
            constexpr float norm = 0.35355339059f; // 1/sqrt(8)
            for (int i = 0; i < 8; ++i) x[i] *= norm;
        }

        void readPre (float p, float& dl, float& dr, float& erL, float& erR) const noexcept
        {
            dl = pre[0].read (p);
            dr = pre[1].read (p);
            erL = erR = 0.0f;
            if (cfg->erLevel > 0.0f)
                for (int k = 0; k < numTaps; ++k)
                {
                    const float gk = tapGain (k) * tapSign[k];
                    erL += gk * pre[0].read (p + tapMsL[k] * erTimeMul);
                    erR += gk * pre[1].read (p + tapMsR[k] * erTimeMul);
                }
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            const auto& c = *cfg;

            // input filtering
            float xl = bandLp[0].process (lowCut[0].process (inL));
            float xr = bandLp[1].process (lowCut[1].process (inR));
            pre[0].push (xl);
            pre[1].push (xr);

            float dl, dr, erL, erR;
            readPre (preA, dl, dr, erL, erR);
            if (preXfading)
            {
                float bl, br, bel, ber;
                readPre (preB, bl, br, bel, ber);
                const float w = preXf;
                dl += (bl - dl) * w;  dr += (br - dr) * w;
                erL += (bel - erL) * w;  erR += (ber - erR) * w;
                preXf += preXfStep;
                if (preXf >= 1.0f) { preA = preB; preXfading = false; }
            }

            if (c.erLevel > 0.0f)
            {
                // a little cross-feed keeps the image coherent
                const float cl = erL, cr = erR;
                erL = erSmear[0].process (cl + 0.25f * cr, 0.5f);
                erR = erSmear[1].process (cr + 0.25f * cl, 0.5f);
            }

            // input diffusion
            float sl = dl, sr = dr;
            for (int k = 0; k < 4; ++k)
            {
                sl = diffL[k].process (sl, c.diffusion);
                sr = diffR[k].process (sr, c.diffusion);
            }

            // FDN
            float y[numLines];
            for (int i = 0; i < numLines; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] > juce::MathConstants<float>::twoPi) lfoPhase[i] -= juce::MathConstants<float>::twoPi;
                const float m = std::sin (lfoPhase[i]) * 0.7f + std::sin (2.17f * lfoPhase[i] + (float) i) * 0.3f;
                const float d = baseLen[i] + modDepth * (1.0f + m);
                float v = lines[i].read (d);
                v = loopAp[i].process (v, c.loopDiff);

                // three-band decay
                lpBass[i] += aBass * (v - lpBass[i]);
                lpHigh[i] += aHigh * (v - lpHigh[i]);
                const float bass = lpBass[i], mid = lpHigh[i] - lpBass[i], high = v - lpHigh[i];
                y[i] = gBass[i] * bass + gMid[i] * mid + gHigh[i] * high;
            }

            float lateL = 0.0f, lateR = 0.0f;
            for (int i = 0; i < numLines; ++i)
            {
                lateL += outSignL[i] * y[i];
                lateR += outSignR[i] * y[i];
            }

            hadamard8 (y);
            for (int i = 0; i < numLines; ++i)
                lines[i].push (y[i] + inject[i] * ((i & 1) ? sr : sl));

            float l = (c.erLevel * erL + c.lateLevel * lateL * 0.5f) * outGain;
            float r = (c.erLevel * erR + c.lateLevel * lateR * 0.5f) * outGain;
            if (c.colourDb != 0.0f)
            {
                l = colour[0].process (l);
                r = colour[1].process (r);
            }
            outL = l;
            outR = r;
        }

        double fs = 48000.0;
        bool initialised = false;
        int algo = Hall, pendingAlgo = Hall;
        const AlgoConfig* cfg = &config (Hall);
        Settings sm;
        bool filtersDirty = true;

        DelayLine pre[2];
        DelayLine lines[numLines];
        Allpass loopAp[numLines];
        Allpass diffL[4], diffR[4], erSmear[2];
        Biquad lowCut[2], bandLp[2], colour[2];

        float preA = 1.0f, preB = 1.0f, preXf = 0.0f, preXfStep = 0.001f;
        bool preXfading = false;
        float erTimeMul = 1.0f, modDepth = 0.0f, outGain = 1.0f;
        float baseLen[numLines] {}, gMid[numLines] {}, gBass[numLines] {}, gHigh[numLines] {};
        float lpBass[numLines] {}, lpHigh[numLines] {};
        float aBass = 0.0f, aHigh = 0.0f;
        float lfoPhase[numLines] { 0.0f, 0.8f, 1.6f, 2.4f, 3.2f, 4.0f, 4.8f, 5.6f };
        float lfoInc[numLines] {};

        float fade = 1.0f, fadeStep = 0.001f;
        int fadeDir = 0;

        static constexpr float inject[numLines]   = { 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
        static constexpr float outSignL[numLines] = { 1, 1, -1, 1, -1, -1, 1, -1 };
        static constexpr float outSignR[numLines] = { 1, -1, 1, 1, -1, 1, -1, -1 };
    };
}
