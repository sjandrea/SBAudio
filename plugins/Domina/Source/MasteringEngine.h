#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

/*  Domina – one-knob-style mastering chain (in the spirit of LANDR's mastering plugin).

    Signal path (base rate unless noted):
      input -> auto-EQ (10 bands, fitted from the analysis against a style target)
            -> tone (low shelf / mid / presence / high shelf)
            -> 3-band compressor (LR4 crossovers 160 Hz / 2.5 kHz, thresholds relative to the analysed band levels)
            -> de-esser (dynamic bell at 7.5 kHz)
            -> stereo width (M/S, optional mono bass)
            -> drive (gain that puts the result on the loudness target)
            -> [4x oversampled] saturation -> lookahead true-peak limiter -> ceiling
            -> level-matched compare (A/B against the delayed input)

    Analysis ("ANALYSER"):
      phase 1 – listens to 8 s of audio: 1/3-octave spectrum, mid/side power, integrated loudness (BS.1770).
      phase 2 – listens to the result twice (2 x 3 s) and corrects the remaining loudness error (limiter, saturation …).
                The second pass uses the measured slope of the first, since the limiter swallows part of any gain change.
    Everything derived from the analysis (EQ, compressor references, drive) is recomputed whenever a control
    changes, so the style or loudness target can be changed afterwards without analysing again. */
namespace domina
{
    using juce::jlimit;
    using juce::jmax;
    using juce::jmin;

    constexpr double pi = juce::MathConstants<double>::pi;

    constexpr int numEqBands   = 10;
    constexpr int numToneBands = 4;
    constexpr int numSpecBands = 31;     // 1/3 octave, 20 Hz – 20 kHz
    constexpr int numCompBands = 3;
    constexpr float xover1 = 160.0f, xover2 = 2500.0f;

    constexpr float phase1Seconds = 8.0f;
    constexpr float phase2SettleSeconds = 0.75f;
    constexpr float phase2Seconds = 3.0f;           // per refining pass (two passes)
    constexpr float phase1Share = 0.65f;           // share of the progress bar used by phase 1
    constexpr float activeThresholdDb = -50.0f;    // blocks quieter than this don't count in the analysis

    inline float specBandFreq (int b) { return 1000.0f * std::pow (2.0f, (float) (b - 17) / 3.0f); }

    enum Style { Warm = 0, Balanced = 1, Open = 2 };
    inline const juce::StringArray styleNames { "Varm", "Balansert", juce::String (juce::CharPointer_UTF8 ("\xc3\x85" "pen")) };
    inline constexpr float loudnessPresets[] = { -14.0f, -11.0f, -8.0f };   // LAV / MIDDELS / HØY
    inline constexpr float minTargetLufs = -18.0f, maxTargetLufs = -6.0f;

    //==========================================================================
    // Biquads (RBJ cookbook), double precision, transposed direct form II.
    struct Coeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        double magSq (double f, double fs) const
        {
            const double w = 2.0 * pi * jlimit (1.0, fs * 0.4999, f) / fs;
            const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
            const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
            const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
            return (nr * nr + ni * ni) / jmax (1.0e-30, dr * dr + di * di);
        }
        double magDb (double f, double fs) const { return 10.0 * std::log10 (jmax (1.0e-30, magSq (f, fs))); }

        static Coeffs norm (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }
    };

    enum class FilterType { Bell, LowShelf, HighShelf, HighPass, LowPass, BandPass, AllPass };

    inline Coeffs design (FilterType t, double fs, double f, double q, double db = 0.0)
    {
        f = jlimit (5.0, fs * 0.45, f);
        const double A = std::pow (10.0, db / 40.0);
        const double w0 = 2.0 * pi * f / fs, cs = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        switch (t)
        {
            case FilterType::Bell:
                return Coeffs::norm (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
            case FilterType::LowShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                return Coeffs::norm (A * ((A + 1) - (A - 1) * cs + sq), 2 * A * ((A - 1) - (A + 1) * cs),
                                     A * ((A + 1) - (A - 1) * cs - sq), (A + 1) + (A - 1) * cs + sq,
                                     -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sq);
            }
            case FilterType::HighShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                return Coeffs::norm (A * ((A + 1) + (A - 1) * cs + sq), -2 * A * ((A - 1) + (A + 1) * cs),
                                     A * ((A + 1) + (A - 1) * cs - sq), (A + 1) - (A - 1) * cs + sq,
                                     2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sq);
            }
            case FilterType::HighPass:
                return Coeffs::norm ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
            case FilterType::LowPass:
                return Coeffs::norm ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
            case FilterType::BandPass:
                return Coeffs::norm (alpha, 0, -alpha, 1 + alpha, -2 * cs, 1 - alpha);
            case FilterType::AllPass:
                return Coeffs::norm (1 - alpha, -2 * cs, 1 + alpha, 1 + alpha, -2 * cs, 1 - alpha);
        }
        return {};
    }

    struct Biquad
    {
        Coeffs c;
        double z1 = 0.0, z2 = 0.0;

        inline float process (float xf) noexcept
        {
            const double x = xf;
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
        void reset() noexcept { z1 = z2 = 0.0; }
    };

    //==========================================================================
    // EQ band layout.
    struct BandDef { float freq, q; FilterType type; };

    inline constexpr BandDef corrBands[numEqBands] = {
        { 31.5f, 1.0f, FilterType::Bell }, { 63.0f, 1.4f, FilterType::Bell }, { 125.0f, 1.4f, FilterType::Bell },
        { 250.0f, 1.4f, FilterType::Bell }, { 500.0f, 1.4f, FilterType::Bell }, { 1000.0f, 1.4f, FilterType::Bell },
        { 2000.0f, 1.4f, FilterType::Bell }, { 4000.0f, 1.4f, FilterType::Bell }, { 8000.0f, 1.4f, FilterType::Bell },
        { 12000.0f, 0.7f, FilterType::HighShelf } };
    inline constexpr int corrEvalBand[numEqBands] = { 2, 5, 8, 11, 14, 17, 20, 23, 26, 29 };

    // LAV, MID, PRESENCE, HØY
    inline constexpr BandDef toneBands[numToneBands] = {
        { 100.0f, 0.7f, FilterType::LowShelf }, { 900.0f, 0.7f, FilterType::Bell },
        { 4000.0f, 0.9f, FilterType::Bell }, { 9000.0f, 0.7f, FilterType::HighShelf } };

    inline Coeffs designBand (const BandDef& d, double fs, double db) { return design (d.type, fs, d.freq, d.q, db); }

    //==========================================================================
    // Tonal target for a mastered mix (1/3-octave band levels, pink noise = flat) and the style tilt.
    inline float targetDb (int style, float f)
    {
        struct Pt { float f, db; };
        static const Pt curve[] = { { 20, -2 }, { 50, 4 }, { 100, 4 }, { 200, 2 }, { 500, 0 }, { 1000, -1.5f },
                                    { 2000, -3.5f }, { 4000, -6 }, { 8000, -9.5f }, { 12000, -12.5f },
                                    { 16000, -17 }, { 20000, -23 } };
        constexpr int n = (int) (sizeof (curve) / sizeof (curve[0]));
        float db = curve[n - 1].db;
        if (f <= curve[0].f) db = curve[0].db;
        else
            for (int i = 1; i < n; ++i)
                if (f <= curve[i].f)
                {
                    const float t = std::log (f / curve[i - 1].f) / std::log (curve[i].f / curve[i - 1].f);
                    db = curve[i - 1].db + t * (curve[i].db - curve[i - 1].db);
                    break;
                }

        const float tilt = style == Warm ? 1.0f : style == Open ? -1.0f : 0.0f;
        return db + tilt * jlimit (-3.0f, 3.0f, -0.7f * std::log2 (f / 800.0f));
    }

    inline float styleTimeFactor (int style) { return style == Warm ? 1.3f : style == Open ? 0.8f : 1.0f; }
    inline float styleEven (int style)       { return style == Warm ? 0.15f : style == Open ? 0.03f : 0.08f; }
    inline float styleReleaseMs (int style)  { return style == Warm ? 180.0f : style == Open ? 80.0f : 120.0f; }

    inline float compCurve (float over, float slope, float knee) noexcept
    {
        if (2.0f * over <= -knee) return 0.0f;
        if (2.0f * over >= knee)  return slope * over;
        const float x = over + knee * 0.5f;
        return slope * x * x / (2.0f * knee);
    }

    inline float toDb (double power) { return (float) (10.0 * std::log10 (jmax (1.0e-20, power))); }

    //==========================================================================
    /** BS.1770 K-weighting + momentary/short-term loudness in 100 ms steps. */
    class LoudnessMeter
    {
    public:
        void prepare (double fs)
        {
            {   // stage 1: high shelf
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                          2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            {   // stage 2: high pass
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
                hp = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            sub = jmax (1, (int) std::lround (fs * 0.1));
            for (auto* b : { &s1[0], &s1[1] }) b->c = shelf;
            for (auto* b : { &s2[0], &s2[1] }) b->c = hp;
            reset();
        }

        void reset()
        {
            for (auto& b : s1) b.reset();
            for (auto& b : s2) b.reset();
            blocks.fill (0.0);
            acc = 0.0; n = 0; write = 0; filled = 0;
        }

        /** Returns true when a new 100 ms step completed (momentary energy updated). */
        inline bool push (float l, float r) noexcept
        {
            const float kl = s2[0].process (s1[0].process (l));
            const float kr = s2[1].process (s1[1].process (r));
            acc += (double) kl * kl + (double) kr * kr;
            if (++n < sub)
                return false;
            blocks[(size_t) write] = acc / (double) sub;
            write = (write + 1) % numBlocks;
            filled = jmin (filled + 1, numBlocks);
            acc = 0.0; n = 0;
            return true;
        }

        double energy (int steps) const noexcept   // mean of the last `steps` 100 ms blocks
        {
            const int m = jmin (steps, filled);
            if (m == 0) return 0.0;
            double s = 0.0;
            for (int i = 1; i <= m; ++i)
                s += blocks[(size_t) ((write - i + numBlocks) % numBlocks)];
            return s / (double) m;
        }
        double momentaryEnergy() const noexcept { return energy (4); }
        static float lufs (double e) noexcept { return e <= 1.0e-12 ? -100.0f : (float) (-0.691 + 10.0 * std::log10 (e)); }
        float momentary() const noexcept  { return lufs (energy (4)); }
        float shortTerm() const noexcept  { return lufs (energy (30)); }
        Coeffs shelfCoeffs() const { return shelf; }
        Coeffs hpCoeffs() const    { return hp; }

    private:
        static constexpr int numBlocks = 30;
        Coeffs shelf, hp;
        std::array<Biquad, 2> s1, s2;
        std::array<double, numBlocks> blocks {};
        double acc = 0.0;
        int n = 0, sub = 4800, write = 0, filled = 0;
    };

    /** Gated integrated loudness (BS.1770) from 400 ms block energies, histogram based (no allocation). */
    class Integrator
    {
    public:
        void reset() { counts.fill (0); sums.fill (0.0); }

        void add (double e) noexcept
        {
            const float l = LoudnessMeter::lufs (e);
            if (l < -70.0f) return;
            const int b = jlimit (0, numBins - 1, (int) ((l + 70.0f) * 10.0f));
            ++counts[(size_t) b];
            sums[(size_t) b] += e;
        }

        int numBlocks() const noexcept
        {
            int c = 0;
            for (auto v : counts) c += v;
            return c;
        }

        float integrated() const noexcept
        {
            double s = 0.0; int c = 0;
            for (int b = 0; b < numBins; ++b) { s += sums[(size_t) b]; c += counts[(size_t) b]; }
            if (c == 0) return -100.0f;
            const float rel = LoudnessMeter::lufs (s / c) - 10.0f;
            const int b0 = jlimit (0, numBins - 1, (int) ((rel + 70.0f) * 10.0f));
            s = 0.0; c = 0;
            for (int b = b0; b < numBins; ++b) { s += sums[(size_t) b]; c += counts[(size_t) b]; }
            return c == 0 ? -100.0f : LoudnessMeter::lufs (s / c);
        }

    private:
        static constexpr int numBins = 800;   // -70 .. +10 LUFS in 0.1 LU
        std::array<int, numBins> counts {};
        std::array<double, numBins> sums {};
    };

    //==========================================================================
    /** Lookahead peak limiter: sliding minimum + release + box smoothing (never overshoots). */
    class Limiter
    {
    public:
        void prepare (int lookahead, double sampleRate)
        {
            W = lookahead + 1;
            D = lookahead;
            fs = sampleRate;
            dl.assign ((size_t) D, 0.0f);
            dr.assign ((size_t) D, 0.0f);
            box.assign ((size_t) W, 1.0f);
            dqVal.assign ((size_t) W + 2, 1.0f);
            dqIdx.assign ((size_t) W + 2, 0);
            reset();
        }

        void reset()
        {
            std::fill (dl.begin(), dl.end(), 0.0f);
            std::fill (dr.begin(), dr.end(), 0.0f);
            std::fill (box.begin(), box.end(), 1.0f);
            boxSum = (double) W; boxPos = 0; dPos = 0;
            head = tail = count = 0; idx = 0;
            rel = 1.0f;
        }

        void setRelease (float ms) { relCoef = (float) (1.0 - std::exp (-1.0 / (jmax (1.0f, ms) * 0.001 * fs))); }

        inline float process (float& l, float& r, float ceiling) noexcept
        {
            const float p = jmax (std::abs (l), std::abs (r));
            const float g = p > ceiling ? ceiling / p : 1.0f;

            // sliding minimum over the last W samples (monotonic queue)
            const int cap = (int) dqVal.size();
            while (count > 0 && dqVal[(size_t) ((tail - 1 + cap) % cap)] >= g) { tail = (tail - 1 + cap) % cap; --count; }
            dqVal[(size_t) tail] = g; dqIdx[(size_t) tail] = idx; tail = (tail + 1) % cap; ++count;
            while (dqIdx[(size_t) head] <= idx - W) { head = (head + 1) % cap; --count; }
            const float h = dqVal[(size_t) head];
            ++idx;
            if (idx > (1 << 30)) rebase();

            if (h < rel) rel = h;
            else         rel += (h - rel) * relCoef;

            boxSum += (double) rel - (double) box[(size_t) boxPos];
            box[(size_t) boxPos] = rel;
            if (++boxPos >= W)
            {
                boxPos = 0;
                boxSum = 0.0;
                for (auto v : box) boxSum += v;
            }
            const float gain = (float) (boxSum / (double) W);

            const float ol = dl[(size_t) dPos], orr = dr[(size_t) dPos];
            dl[(size_t) dPos] = l; dr[(size_t) dPos] = r;
            if (++dPos >= D) dPos = 0;
            l = ol * gain;
            r = orr * gain;
            return gain;
        }

    private:
        void rebase()
        {
            const int cap = (int) dqVal.size();
            for (int i = 0, p = head; i < count; ++i, p = (p + 1) % cap) dqIdx[(size_t) p] -= (1 << 29);
            idx -= (1 << 29);
        }

        int W = 2, D = 1;
        double fs = 44100.0;
        std::vector<float> dl, dr, box, dqVal;
        std::vector<int> dqIdx;
        double boxSum = 0.0;
        int boxPos = 0, dPos = 0, head = 0, tail = 0, count = 0, idx = 0;
        float rel = 1.0f, relCoef = 0.001f;
    };

    //==========================================================================
    struct Settings
    {
        int style = Balanced;
        float targetLufs = -11.0f;
        std::array<float, numToneBands> tone {};   // dB: low, mid, presence, high
        float correction = 0.7f;                   // auto-EQ amount 0..1
        float compression = 0.4f, deess = 0.2f, saturation = 0.2f;
        float width = 1.0f;                        // 0..2
        bool monoBass = false;
        float ceilingDb = -1.0f;
        bool compare = false;
    };

    /** The analysis result. Everything else is derived from it and the current settings. */
    struct AnalysisData
    {
        bool valid = false;
        float inLufs = -100.0f;                     // integrated loudness of the analysed input
        float midPower = 0.0f, sidePower = 0.0f;    // mean square of M = (L+R)/2 and S = (L-R)/2
        float residual = 0.0f;                      // loudness correction measured in phase 2 (dB)
        std::array<float, numSpecBands> bandDb {};  // mid signal power per 1/3 octave (dB, calibrated)

        juce::String toString() const
        {
            juce::StringArray s;
            s.add ("1");
            s.add (valid ? "1" : "0");
            for (float v : { inLufs, midPower, sidePower, residual }) s.add (juce::String (v, 6));
            for (float v : bandDb) s.add (juce::String (v, 3));
            return s.joinIntoString (";");
        }

        static AnalysisData fromString (const juce::String& str)
        {
            AnalysisData d;
            juce::StringArray s;
            s.addTokens (str, ";", "");
            if (s.size() != 6 + numSpecBands || s[0] != "1")
                return d;
            d.valid = s[1] == "1";
            d.inLufs = s[2].getFloatValue();
            d.midPower = s[3].getFloatValue();
            d.sidePower = s[4].getFloatValue();
            d.residual = s[5].getFloatValue();
            for (int b = 0; b < numSpecBands; ++b) d.bandDb[(size_t) b] = s[6 + b].getFloatValue();
            d.valid = d.valid && d.inLufs > -70.0f && d.midPower > 0.0f;
            return d;
        }
    };

    enum AnalysisState { Idle = 0, Listening = 1, Refining = 2 };

    /** Lock-free values shared with the GUI and the processor. */
    struct SharedState
    {
        // meters (written by the audio thread)
        std::atomic<float> inShort { -100.0f }, outShort { -100.0f }, outMomentary { -100.0f }, outIntegrated { -100.0f };
        std::atomic<float> truePeakDb { -100.0f };          // hold since reset
        std::atomic<float> limiterGrDb { 0.0f };            // max since last read (GUI resets to -1)
        std::array<std::atomic<float>, numCompBands> compGrDb {};
        std::atomic<float> deessGrDb { 0.0f };
        std::atomic<float> driveDb { 0.0f }, matchDb { 0.0f };
        std::array<std::atomic<float>, numEqBands> corrDb {};   // current auto-EQ gains (targets)
        std::atomic<int> displaySerial { 0 };

        // analysis
        std::atomic<int> analysisState { Idle };
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> waitingForAudio { false };
        std::atomic<int> command { 0 };          // 1 = start/restart analysis, 2 = cancel, 3 = reset integrated/peak

        // analysis data (for saving with the project); version bumps on every change
        std::atomic<bool> dataValid { false };
        std::atomic<float> dataInLufs { -100.0f }, dataMid { 0.0f }, dataSide { 0.0f }, dataResidual { 0.0f };
        std::array<std::atomic<float>, numSpecBands> dataBandDb {};
        std::atomic<int> dataVersion { 0 };
        std::atomic<int> loadVersion { 0 };      // bumped by the processor after restoring a project
        std::atomic<bool> autoStartDone { false };

        sbaudio::AnalyzerRing preRing, postRing;

        void storeData (const AnalysisData& d)
        {
            dataInLufs.store (d.inLufs); dataMid.store (d.midPower); dataSide.store (d.sidePower); dataResidual.store (d.residual);
            for (int b = 0; b < numSpecBands; ++b) dataBandDb[(size_t) b].store (d.bandDb[(size_t) b]);
            dataValid.store (d.valid);
            dataVersion.fetch_add (1);
        }

        AnalysisData loadData() const
        {
            AnalysisData d;
            d.valid = dataValid.load(); d.inLufs = dataInLufs.load(); d.midPower = dataMid.load();
            d.sidePower = dataSide.load(); d.residual = dataResidual.load();
            for (int b = 0; b < numSpecBands; ++b) d.bandDb[(size_t) b] = dataBandDb[(size_t) b].load();
            return d;
        }
    };

    inline void storeMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    //==========================================================================
    /** Values derived from the analysis + settings. Pure function – also used by the GUI. */
    struct Derived
    {
        std::array<float, numEqBands> corrDb {};
        std::array<float, numCompBands> refDb { -24.0f, -20.0f, -30.0f };
        float driveDb = 0.0f;
        float eqDeltaDb = 0.0f;
    };

    inline void computeCorrectionCurve (const AnalysisData& d, int style, float amount, float* out /* numSpecBands */)
    {
        float maxL = -200.0f;
        for (auto v : d.bandDb) maxL = jmax (maxL, v);

        std::array<float, numSpecBands> diff {}, valid {};
        float sum = 0.0f, wsum = 0.0f;
        for (int b = 0; b < numSpecBands; ++b)
        {
            const float f = specBandFreq (b);
            diff[(size_t) b] = targetDb (style, f) - d.bandDb[(size_t) b];
            const float t = jlimit (0.0f, 1.0f, (d.bandDb[(size_t) b] - (maxL - 50.0f)) / 12.0f);
            valid[(size_t) b] = t * t * (3.0f - 2.0f * t);
            if (f >= 100.0f && f <= 8000.0f) { sum += valid[(size_t) b] * diff[(size_t) b]; wsum += valid[(size_t) b]; }
        }
        const float offset = wsum > 0.0f ? sum / wsum : 0.0f;

        std::array<float, numSpecBands> c {};
        for (int b = 0; b < numSpecBands; ++b)
        {
            float v = diff[(size_t) b] - offset;
            v = v > 0.0f ? v * valid[(size_t) b] : v * (0.5f + 0.5f * valid[(size_t) b]);
            c[(size_t) b] = v;
        }
        // gaussian smoothing across bands (sigma 1.3 bands)
        for (int b = 0; b < numSpecBands; ++b)
        {
            float s = 0.0f, w = 0.0f;
            for (int o = -4; o <= 4; ++o)
            {
                const int i = b + o;
                if (i < 0 || i >= numSpecBands) continue;
                const float wi = std::exp (-0.5f * (float) (o * o) / (1.3f * 1.3f));
                s += wi * c[(size_t) i]; w += wi;
            }
            float v = (s / w) * amount * 0.6f;
            const float f = specBandFreq (b);
            float maxBoost = f < 35.0f ? 0.0f : f > 15000.0f ? 2.0f : 4.0f;
            out[b] = jlimit (-6.0f, maxBoost, v);
        }
    }

    /** Fits the 10 auto-EQ filters to the correction curve. */
    inline void fitCorrection (const float* curve, double fs, std::array<float, numEqBands>& gains)
    {
        for (int i = 0; i < numEqBands; ++i) gains[(size_t) i] = curve[corrEvalBand[i]];
        std::array<Coeffs, numEqBands> cs;
        for (int it = 0; it < 8; ++it)
        {
            for (int i = 0; i < numEqBands; ++i) cs[(size_t) i] = designBand (corrBands[i], fs, gains[(size_t) i]);
            for (int i = 0; i < numEqBands; ++i)
            {
                const double f = specBandFreq (corrEvalBand[i]);
                double resp = 0.0;
                for (auto& c : cs) resp += c.magDb (f, fs);
                gains[(size_t) i] = jlimit (-8.0f, 8.0f, gains[(size_t) i] + 0.8f * (curve[corrEvalBand[i]] - (float) resp));
            }
        }
    }

    inline Derived derive (const AnalysisData& d, const Settings& s, double fs)
    {
        Derived r;
        if (! d.valid)
            return r;

        std::array<float, numSpecBands> curve {};
        computeCorrectionCurve (d, s.style, s.correction, curve.data());
        fitCorrection (curve.data(), fs, r.corrDb);

        // combined EQ response at the analysis bands
        std::array<Coeffs, numEqBands + numToneBands> cs;
        for (int i = 0; i < numEqBands; ++i)   cs[(size_t) i] = designBand (corrBands[i], fs, r.corrDb[(size_t) i]);
        for (int i = 0; i < numToneBands; ++i) cs[(size_t) (numEqBands + i)] = designBand (toneBands[i], fs, s.tone[(size_t) i]);

        LoudnessMeter km;
        km.prepare (fs);
        const auto ks = km.shelfCoeffs(), kh = km.hpCoeffs();

        const double sideRatio = d.midPower > 0.0f ? (double) d.sidePower / (double) d.midPower : 0.0;
        double before = 0.0, after = 0.0;
        std::array<double, numCompBands> bandPow {};
        for (int b = 0; b < numSpecBands; ++b)
        {
            const double f = specBandFreq (b);
            const double p = std::pow (10.0, d.bandDb[(size_t) b] / 10.0);
            double h = 1.0;
            for (auto& c : cs) h *= c.magSq (f, fs);
            const double k = ks.magSq (f, fs) * kh.magSq (f, fs);
            before += p * k;
            after += p * k * h;
            bandPow[(size_t) (f < xover1 ? 0 : f < xover2 ? 1 : 2)] += p * h;
        }
        r.eqDeltaDb = before > 0.0 ? (float) (10.0 * std::log10 (jmax (1.0e-12, after / before))) : 0.0f;

        const double w2 = (double) s.width * s.width;
        const float widthDelta = (float) (10.0 * std::log10 ((1.0 + w2 * sideRatio) / (1.0 + sideRatio)));
        for (int i = 0; i < numCompBands; ++i)
            r.refDb[(size_t) i] = toDb (bandPow[(size_t) i] * (1.0 + w2 * sideRatio));

        r.driveDb = jlimit (-12.0f, 24.0f, s.targetLufs - (d.inLufs + r.eqDeltaDb + widthDelta) + d.residual);
        return r;
    }

    //==========================================================================
    class Engine
    {
    public:
        explicit Engine (SharedState& s) : shared (s) {}

        int getLatency() const { return latency; }

        void prepare (double sampleRate, int maxBlock)
        {
            fs = sampleRate;
            maxBlockSize = jmax (32, maxBlock);

            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            oversampler->initProcessing ((size_t) maxBlockSize);
            oversampler->reset();
            const int osLatency = (int) std::lround (oversampler->getLatencyInSamples());
            const double fsOs = fs * 4.0;

            const int lookBase = jmax (1, (int) std::lround (0.001 * fs));   // 1 ms lookahead
            limiter.prepare (lookBase * 4, fsOs);
            latency = osLatency + lookBase;

            work.setSize (2, maxBlockSize, false, true, false);
            dry.setSize (2, maxBlockSize, false, true, false);
            for (auto& d : dryDelay) d.assign ((size_t) latency, 0.0f);   // latency >= 1 (lookahead)
            dryPos = 0;

            inMeter.prepare (fs);
            outMeter.prepare (fs);
            outIntegrator.reset();

            // analysis FFT (fine resolution so the lowest 1/3-octave bands get bins)
            const int order = fs <= 50000.0 ? 14 : fs <= 100000.0 ? 15 : 16;
            fftN = 1 << order;
            fft = std::make_unique<juce::dsp::FFT> (order);
            fftRing.assign ((size_t) fftN, 0.0f);
            fftBuf.assign ((size_t) fftN * 2, 0.0f);
            binAcc.assign ((size_t) fftN / 2 + 1, 0.0);
            fftWindow.resize ((size_t) fftN);
            for (int n = 0; n < fftN; ++n)
                fftWindow[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) fftN);

            // crossovers
            for (int c = 0; c < 2; ++c)
            {
                auto& x = xo[(size_t) c];
                for (auto* b : { &x.lp1a, &x.lp1b }) b->c = design (FilterType::LowPass, fs, xover1, 0.7071);
                for (auto* b : { &x.hp1a, &x.hp1b }) b->c = design (FilterType::HighPass, fs, xover1, 0.7071);
                for (auto* b : { &x.lp2a, &x.lp2b }) b->c = design (FilterType::LowPass, fs, xover2, 0.7071);
                for (auto* b : { &x.hp2a, &x.hp2b }) b->c = design (FilterType::HighPass, fs, xover2, 0.7071);
                x.ap2.c = design (FilterType::AllPass, fs, xover2, 0.7071);
                sideHp.c = design (FilterType::HighPass, fs, 120.0, 0.7071);
            }
            sibDetect.c = design (FilterType::BandPass, fs, 7500.0, 1.4);

            driveGain.reset (fs, 0.25);
            resetState();
            settingsKnown = false;
            snapDrive = true;
        }

        void reset() { resetState(); }

        /** Host bypass: keep the latency so nothing jumps in time. */
        void processBypassed (juce::AudioBuffer<float>& buffer)
        {
            const int nch = jmin (2, buffer.getNumChannels());
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                for (int c = 0; c < nch; ++c)
                {
                    auto& d = dryDelay[(size_t) c];
                    const float in = buffer.getSample (c, i);
                    buffer.setSample (c, i, d[(size_t) dryPos]);
                    d[(size_t) dryPos] = in;
                }
                if (++dryPos >= (int) dryDelay[0].size()) dryPos = 0;
            }
        }

        void process (juce::AudioBuffer<float>& buffer, const Settings& s)
        {
            const int total = buffer.getNumSamples();
            const int nch = jmin (2, buffer.getNumChannels());
            if (nch == 0) return;

            handleCommands();
            if (! settingsKnown || needsRecalc (s))
                recalc (s);
            last = s;
            settingsKnown = true;

            for (int start = 0; start < total; start += maxBlockSize)
            {
                const int n = jmin (maxBlockSize, total - start);
                float* io[2] = { buffer.getWritePointer (0, start), buffer.getWritePointer (nch > 1 ? 1 : 0, start) };
                processChunk (io, nch, n, s);
            }
            shared.displaySerial.fetch_add (1);
        }

    private:
        struct Crossover { Biquad lp1a, lp1b, hp1a, hp1b, lp2a, lp2b, hp2a, hp2b, ap2; };
        struct Comp
        {
            float env = 0.0f, gr = 0.0f;
            float msCoef = 0.01f, att = 0.01f, rel = 0.001f;
            float thr = 0.0f, slope = 0.0f, makeup = 0.0f;
        };

        static float coef (double ms, double rate) { return (float) (1.0 - std::exp (-1.0 / (jmax (0.01, ms) * 0.001 * rate))); }

        void resetState()
        {
            for (auto& ch : eq) for (auto& b : ch) b.reset();
            for (auto& x : xo) for (auto* b : { &x.lp1a, &x.lp1b, &x.hp1a, &x.hp1b, &x.lp2a, &x.lp2b, &x.hp2a, &x.hp2b, &x.ap2 }) b->reset();
            for (auto& b : deessBell) b.reset();
            sideHp.reset(); sibDetect.reset();
            for (auto& c : comp) { c.env = 0.0f; c.gr = 0.0f; }
            sibEnv = fullEnv = 0.0f; deessGr = 0.0f;
            for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
            dryPos = 0;
            if (oversampler) oversampler->reset();
            limiter.reset();
            dcX = { 0, 0 }; dcY = { 0, 0 };
            inMeter.reset(); outMeter.reset();
            for (auto& v : eqCur) v = 999.0f;   // force coefficient update
            for (auto& v : toneCur) v = 999.0f;
            firstBlock = true;
        }

        //----------------------------------------------------------------------
        void handleCommands()
        {
            const int lv = shared.loadVersion.load();
            if (lv != seenLoadVersion)
            {
                seenLoadVersion = lv;
                data = shared.loadData();
                state = Idle;
                shared.analysisState.store (Idle);
                settingsKnown = false;
                snapDrive = true;
            }

            switch (shared.command.exchange (0))
            {
                case 1: startAnalysis(); break;
                case 2:
                    if (state != Idle) { state = Idle; shared.analysisState.store (Idle); shared.progress.store (0.0f); }
                    break;
                case 3:
                    outIntegrator.reset();
                    shared.truePeakDb.store (-100.0f);
                    break;
                default: break;
            }
        }

        void startAnalysis()
        {
            state = Listening;
            activeSamples = 0;
            std::fill (binAcc.begin(), binAcc.end(), 0.0);
            fftFrames = 0; fftWrite = 0; fftSinceHop = 0; fftFilled = 0;
            msMid = msSide = 0.0; msCount = 0;
            anIntegrator.reset();
            shared.autoStartDone.store (true);
            shared.analysisState.store (Listening);
            shared.progress.store (0.0f);
        }

        void finishPhase1()
        {
            AnalysisData d = data;
            d.inLufs = anIntegrator.integrated();
            d.midPower = (float) (msMid / jmax<double> (1.0, (double) msCount));
            d.sidePower = (float) (msSide / jmax<double> (1.0, (double) msCount));
            d.residual = 0.0f;

            // spectrum -> 1/3-octave band power, calibrated to the time-domain mean square of the mid signal
            const double df = fs / (double) fftN;
            std::array<double, numSpecBands> bandPow {};
            double totalPow = 0.0;
            for (int b = 0; b < numSpecBands; ++b)
            {
                const double fc = specBandFreq (b), lo = fc * std::pow (2.0, -1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
                const int k0 = jmax (1, (int) std::floor (lo / df - 0.5)), k1 = jmin (fftN / 2, (int) std::ceil (hi / df + 0.5));
                double s = 0.0;
                for (int k = k0; k <= k1; ++k)
                {
                    const double bl = ((double) k - 0.5) * df, bh = ((double) k + 0.5) * df;
                    const double ov = jmax (0.0, jmin (hi, bh) - jmax (lo, bl)) / df;
                    s += binAcc[(size_t) k] * ov;
                }
                bandPow[(size_t) b] = s;
                totalPow += s;
            }
            const double scale = totalPow > 0.0 ? (double) d.midPower / totalPow : 0.0;
            for (int b = 0; b < numSpecBands; ++b)
                d.bandDb[(size_t) b] = toDb (bandPow[(size_t) b] * scale);

            d.valid = d.inLufs > -60.0f && d.midPower > 1.0e-9f && fftFrames > 0;
            if (! d.valid)
            {
                state = Idle;
                shared.analysisState.store (Idle);
                shared.progress.store (0.0f);
                return;
            }

            data = d;
            shared.storeData (data);
            recalc (last);

            state = Refining;
            refinePass = 0;
            activeSamples = 0;
            refineIntegrator.reset();
            shared.analysisState.store (Refining);
        }

        void finishRefinePass()
        {
            const float measured = refineIntegrator.integrated();
            const float error = last.targetLufs - measured;
            if (measured > -60.0f)
            {
                float step = error;
                if (refinePass == 1)
                {
                    // how much of the first correction actually came out
                    const float slope = std::abs (firstStep) > 0.2f ? (measured - firstMeasured) / firstStep : 1.0f;
                    step = error / jlimit (0.25f, 1.0f, slope);
                }
                step = jlimit (-6.0f, 6.0f, step);
                data.residual = jlimit (-8.0f, 8.0f, data.residual + step);
                firstStep = step;
                firstMeasured = measured;
                shared.storeData (data);
                recalc (last);
            }

            if (refinePass == 0 && measured > -60.0f && std::abs (error) > 0.1f)
            {
                refinePass = 1;
                activeSamples = 0;
                refineIntegrator.reset();
                return;
            }
            state = Idle;
            shared.analysisState.store (Idle);
            shared.progress.store (1.0f);
            // start the integrated/true-peak readout from the finished master
            outIntegrator.reset();
            shared.truePeakDb.store (-100.0f);
        }

        //----------------------------------------------------------------------
        bool needsRecalc (const Settings& s) const
        {
            auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f; };
            bool toneChanged = false;
            for (int i = 0; i < numToneBands; ++i) toneChanged = toneChanged || differs (s.tone[(size_t) i], last.tone[(size_t) i]);
            return s.style != last.style || toneChanged || differs (s.targetLufs, last.targetLufs)
                || differs (s.correction, last.correction) || differs (s.compression, last.compression) || differs (s.width, last.width);
        }

        void recalc (const Settings& s)
        {
            derived = derive (data, s, fs);
            for (int i = 0; i < numEqBands; ++i) shared.corrDb[(size_t) i].store (derived.corrDb[(size_t) i]);

            const float a = jlimit (0.0f, 1.0f, s.compression);
            const float ratio = 1.0f + 3.0f * a;
            const float tf = styleTimeFactor (s.style);
            constexpr float attMs[] = { 25.0f, 12.0f, 5.0f }, relMs[] = { 220.0f, 150.0f, 90.0f }, avgMs[] = { 20.0f, 8.0f, 3.0f };
            for (int b = 0; b < numCompBands; ++b)
            {
                auto& c = comp[(size_t) b];
                c.slope = a < 0.001f ? 0.0f : 1.0f - 1.0f / ratio;
                c.thr = derived.refDb[(size_t) b] + 3.0f - 11.0f * a;
                c.makeup = 0.6f * compCurve (derived.refDb[(size_t) b] + 2.0f - c.thr, c.slope, compKnee);
                c.msCoef = coef (avgMs[b], fs);
                c.att = coef (attMs[b] * tf, fs);
                c.rel = coef (relMs[b] * tf, fs);
            }
            limiter.setRelease (styleReleaseMs (s.style));
            driveTarget = derived.driveDb;
            if (snapDrive)   // after prepare / project load: start at the right level instead of fading
            {
                driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (driveTarget));
                snapDrive = false;
            }
        }

        //----------------------------------------------------------------------
        void processChunk (float* io[2], int nch, int n, const Settings& s)
        {
            float* L = work.getWritePointer (0);
            float* R = work.getWritePointer (1);
            float* dL = dry.getWritePointer (0);
            float* dR = dry.getWritePointer (1);

            // ---- input: copy, dry delay, meters, analysis
            float peak = 0.0f;
            const int dSize = (int) dryDelay[0].size();
            for (int i = 0; i < n; ++i)
            {
                const float l = io[0][i], r = nch > 1 ? io[1][i] : l;
                L[i] = l; R[i] = r;
                peak = jmax (peak, std::abs (l), std::abs (r));
                dL[i] = dryDelay[0][(size_t) dryPos]; dR[i] = dryDelay[1][(size_t) dryPos];
                dryDelay[0][(size_t) dryPos] = l; dryDelay[1][(size_t) dryPos] = r;
                if (++dryPos >= dSize) dryPos = 0;
            }
            const bool active = juce::Decibels::gainToDecibels (peak, -100.0f) > activeThresholdDb;

            if (! data.valid && state == Idle && active && ! shared.autoStartDone.load())
                startAnalysis();

            for (int i = 0; i < n; ++i)
            {
                if (inMeter.push (L[i], R[i]) && state == Listening && active)
                    anIntegrator.add (inMeter.momentaryEnergy());
            }

            if (state == Listening)
            {
                shared.waitingForAudio.store (! active);
                if (active)
                {
                    for (int i = 0; i < n; ++i)
                    {
                        const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]);
                        msMid += (double) m * m; msSide += (double) sd * sd; ++msCount;
                        fftRing[(size_t) fftWrite] = m;
                        fftWrite = (fftWrite + 1) % fftN;
                        fftFilled = jmin (fftFilled + 1, fftN);
                        if (++fftSinceHop >= fftN / 2 && fftFilled == fftN)
                        {
                            fftSinceHop = 0;
                            runFft();
                        }
                    }
                    activeSamples += n;
                    const float t = (float) activeSamples / (float) (phase1Seconds * fs);
                    shared.progress.store (phase1Share * jmin (1.0f, t));
                    if (t >= 1.0f)
                        finishPhase1();
                }
            }

            pushMono (shared.preRing, L, R, n);

            // ---- base-rate chain in sub-blocks (coefficients are updated between them)
            const float eqAlpha = 1.0f - std::exp (-32.0f / (0.05f * (float) fs));
            for (int pos = 0; pos < n; pos += 32)
            {
                const int m = jmin (32, n - pos);
                updateEq (s, firstBlock ? 1.0f : eqAlpha);
                firstBlock = false;
                processEq (L + pos, R + pos, m);
                processComp (L + pos, R + pos, m);
                processDeess (L + pos, R + pos, m, s);
                processWidthDrive (L + pos, R + pos, m, s);
            }

            // ---- oversampled: saturation + limiter
            const float ceiling = juce::Decibels::decibelsToGain (jlimit (-6.0f, 0.0f, s.ceilingDb));
            {
                juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, 0, (size_t) n);
                auto os = oversampler->processSamplesUp (block);
                float* oL = os.getChannelPointer (0);
                float* oR = os.getChannelPointer (1);
                const int N = (int) os.getNumSamples();

                const float k0 = satK, k1 = jmax (1.0e-4f, 0.55f * std::pow (jlimit (0.0f, 1.0f, s.saturation), 0.8f));
                const float even = styleEven (s.style);
                const float dcR = (float) (1.0 - 2.0 * pi * 5.0 / (fs * 4.0));
                float minGain = 1.0f, tp = 0.0f;
                for (int i = 0; i < N; ++i)
                {
                    const float k = jmax (1.0e-4f, k0 + (k1 - k0) * (float) i / (float) N);
                    float x[2] = { oL[i], oR[i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        // tanh(kx)/k has unity slope at 0: transparent at low level, rounds off peaks.
                        // The squared term adds even harmonics; the DC blocker removes its offset.
                        float y = std::tanh (k * x[c]) / k;
                        y += even * k * y * y;
                        const float o = y - dcX[(size_t) c] + dcR * dcY[(size_t) c];
                        dcX[(size_t) c] = y; dcY[(size_t) c] = o;
                        x[c] = o;
                    }

                    minGain = jmin (minGain, limiter.process (x[0], x[1], ceiling));
                    x[0] = jlimit (-ceiling, ceiling, x[0]);
                    x[1] = jlimit (-ceiling, ceiling, x[1]);
                    tp = jmax (tp, std::abs (x[0]), std::abs (x[1]));
                    oL[i] = x[0]; oR[i] = x[1];
                }
                satK = k1;
                oversampler->processSamplesDown (block);
                storeMax (shared.limiterGrDb, -juce::Decibels::gainToDecibels (minGain, -60.0f));
                storeMax (shared.truePeakDb, juce::Decibels::gainToDecibels (tp, -100.0f));
            }

            // ---- output: ceiling, meters, compare
            const float matchCoef = coef (300.0, fs), cmpCoef = coef (15.0, fs);
            const float cmpTarget = s.compare ? 1.0f : 0.0f;
            for (int i = 0; i < n; ++i)
            {
                L[i] = jlimit (-ceiling, ceiling, L[i]);
                R[i] = jlimit (-ceiling, ceiling, R[i]);
                if (outMeter.push (L[i], R[i]))
                    onOutputBlock (active);

                matchGain += (matchGainTarget - matchGain) * matchCoef;
                cmpMix += (cmpTarget - cmpMix) * cmpCoef;
                const float ol = L[i] + (dL[i] * matchGain - L[i]) * cmpMix;
                const float orr = R[i] + (dR[i] * matchGain - R[i]) * cmpMix;
                io[0][i] = nch > 1 ? ol : 0.5f * (ol + orr);
                if (nch > 1) io[1][i] = orr;
            }
            pushMono (shared.postRing, L, R, n);
            shared.driveDb.store (juce::Decibels::gainToDecibels (driveGain.getCurrentValue(), -100.0f));
        }

        void onOutputBlock (bool active)
        {
            const double e = outMeter.momentaryEnergy();
            outIntegrator.add (e);
            const float inS = inMeter.shortTerm(), outS = outMeter.shortTerm();
            shared.inShort.store (inS);
            shared.outShort.store (outS);
            shared.outMomentary.store (outMeter.momentary());
            shared.outIntegrated.store (outIntegrator.integrated());
            if (inS > -60.0f && outS > -60.0f)
            {
                matchGainTarget = juce::Decibels::decibelsToGain (jlimit (-30.0f, 30.0f, outS - inS));
                shared.matchDb.store (outS - inS);
            }

            if (state == Refining && active)
            {
                activeSamples += (int) std::lround (fs * 0.1);
                const float settle = phase2SettleSeconds * (float) fs;
                if ((float) activeSamples > settle)
                    refineIntegrator.add (e);
                const float t = ((float) activeSamples - settle) / (phase2Seconds * (float) fs);
                const float tt = 0.5f * ((float) refinePass + jlimit (0.0f, 1.0f, t));
                shared.progress.store (phase1Share + (1.0f - phase1Share) * tt);
                if (t >= 1.0f)
                    finishRefinePass();
            }
            if (state == Refining)
                shared.waitingForAudio.store (! active);
        }

        void runFft()
        {
            for (int i = 0; i < fftN; ++i)
                fftBuf[(size_t) i] = fftRing[(size_t) ((fftWrite + i) % fftN)] * fftWindow[(size_t) i];
            std::fill (fftBuf.begin() + fftN, fftBuf.end(), 0.0f);
            fft->performRealOnlyForwardTransform (fftBuf.data(), true);
            for (int k = 0; k <= fftN / 2; ++k)
            {
                const double re = fftBuf[(size_t) (2 * k)], im = fftBuf[(size_t) (2 * k + 1)];
                binAcc[(size_t) k] += re * re + im * im;
            }
            ++fftFrames;
        }

        static void pushMono (sbaudio::AnalyzerRing& ring, const float* l, const float* r, int n)
        {
            float tmp[256];
            for (int pos = 0; pos < n; pos += 256)
            {
                const int m = jmin (256, n - pos);
                for (int i = 0; i < m; ++i) tmp[i] = 0.5f * (l[pos + i] + r[pos + i]);
                ring.push (tmp, m);
            }
        }

        //----------------------------------------------------------------------
        void updateEq (const Settings& s, float alpha)
        {
            for (int i = 0; i < numEqBands; ++i)
            {
                const float target = derived.corrDb[(size_t) i];
                float& cur = eqCur[(size_t) i];
                const float next = cur > 900.0f ? target : cur + (target - cur) * alpha;
                if (std::abs (next - cur) > 0.002f || cur > 900.0f)
                {
                    cur = std::abs (next - target) < 0.002f ? target : next;
                    const auto c = designBand (corrBands[i], fs, cur);
                    eq[0][(size_t) i].c = c; eq[1][(size_t) i].c = c;
                }
            }
            for (int i = 0; i < numToneBands; ++i)
            {
                const float target = jlimit (-12.0f, 12.0f, s.tone[(size_t) i]);
                float& cur = toneCur[(size_t) i];
                const float next = cur > 900.0f ? target : cur + (target - cur) * alpha;
                if (std::abs (next - cur) > 0.002f || cur > 900.0f)
                {
                    cur = std::abs (next - target) < 0.002f ? target : next;
                    const auto c = designBand (toneBands[i], fs, cur);
                    eq[0][(size_t) (numEqBands + i)].c = c; eq[1][(size_t) (numEqBands + i)].c = c;
                }
            }
        }

        void processEq (float* L, float* R, int m)
        {
            for (int b = 0; b < numEqBands + numToneBands; ++b)
            {
                auto& f0 = eq[0][(size_t) b];
                auto& f1 = eq[1][(size_t) b];
                for (int i = 0; i < m; ++i) { L[i] = f0.process (L[i]); R[i] = f1.process (R[i]); }
            }
        }

        void processComp (float* L, float* R, int m)
        {
            float maxGr[numCompBands] = {};
            for (int i = 0; i < m; ++i)
            {
                float band[numCompBands][2];
                float x[2] = { L[i], R[i] };
                for (int c = 0; c < 2; ++c)
                {
                    auto& f = xo[(size_t) c];
                    const float low = f.ap2.process (f.lp1b.process (f.lp1a.process (x[c])));
                    const float rest = f.hp1b.process (f.hp1a.process (x[c]));
                    band[0][c] = low;
                    band[1][c] = f.lp2b.process (f.lp2a.process (rest));
                    band[2][c] = f.hp2b.process (f.hp2a.process (rest));
                }
                // LR4 low + high sum to a 2nd-order allpass, so low (with the f2 allpass) + mid + high is flat.
                float outL = 0.0f, outR = 0.0f;
                for (int b = 0; b < numCompBands; ++b)
                {
                    auto& cp = comp[(size_t) b];
                    float g = 1.0f;
                    if (cp.slope > 0.0f)
                    {
                        const float ms = 0.5f * (band[b][0] * band[b][0] + band[b][1] * band[b][1]);
                        cp.env += (ms - cp.env) * cp.msCoef;
                        const float lvl = 10.0f * std::log10 (cp.env + 1.0e-12f);
                        const float target = compCurve (lvl - cp.thr, cp.slope, compKnee);
                        cp.gr += (target - cp.gr) * (target > cp.gr ? cp.att : cp.rel);
                        g = juce::Decibels::decibelsToGain (cp.makeup - cp.gr);
                        maxGr[b] = jmax (maxGr[b], cp.gr);
                    }
                    else
                    {
                        cp.gr *= 0.999f; cp.env = 0.0f;
                    }
                    outL += band[b][0] * g;
                    outR += band[b][1] * g;
                }
                L[i] = outL; R[i] = outR;
            }
            for (int b = 0; b < numCompBands; ++b) storeMax (shared.compGrDb[(size_t) b], maxGr[b]);
        }

        void processDeess (float* L, float* R, int m, const Settings& s)
        {
            const float d = jlimit (0.0f, 1.0f, s.deess);
            const float sibAtt = coef (0.5, fs), sibRel = coef (40.0, fs), fullAtt = coef (10.0, fs), fullRel = coef (150.0, fs);
            const float grAtt = coef (1.0, fs), grRel = coef (80.0, fs);
            const float thr = -6.0f - 10.0f * d, maxCut = 2.0f + 8.0f * d;
            float peakGr = deessGr;
            for (int i = 0; i < m; ++i)
            {
                const float mid = 0.5f * (L[i] + R[i]);
                const float sb = sibDetect.process (mid);
                const float sms = sb * sb, fms = mid * mid;
                sibEnv += (sms - sibEnv) * (sms > sibEnv ? sibAtt : sibRel);
                fullEnv += (fms - fullEnv) * (fms > fullEnv ? fullAtt : fullRel);
                float target = 0.0f;
                if (d > 0.001f)
                {
                    const float sibDb = 10.0f * std::log10 (sibEnv + 1.0e-12f);
                    const float fullDb = 10.0f * std::log10 (fullEnv + 1.0e-12f);
                    if (sibDb > -60.0f)
                        target = jlimit (0.0f, maxCut, (sibDb - fullDb - thr) * 0.7f);
                }
                deessGr += (target - deessGr) * (target > deessGr ? grAtt : grRel);
                peakGr = jmax (peakGr, deessGr);
            }
            storeMax (shared.deessGrDb, deessGr);

            if (peakGr < 0.01f && deessBellDb < 0.01f)
            {
                deessBell[0].reset(); deessBell[1].reset();
                deessBellDb = 0.0f;
                return;
            }
            if (std::abs (peakGr - deessBellDb) > 0.01f)
            {
                deessBellDb = peakGr;
                const auto c = design (FilterType::Bell, fs, 7500.0, 1.2, -deessBellDb);
                deessBell[0].c = c; deessBell[1].c = c;
            }
            for (int i = 0; i < m; ++i) { L[i] = deessBell[0].process (L[i]); R[i] = deessBell[1].process (R[i]); }
        }

        void processWidthDrive (float* L, float* R, int m, const Settings& s)
        {
            const float wCoef = coef (50.0, fs);
            const float wTarget = jlimit (0.0f, 2.0f, s.width), mbTarget = s.monoBass ? 1.0f : 0.0f;
            driveGain.setTargetValue (juce::Decibels::decibelsToGain (driveTarget));
            for (int i = 0; i < m; ++i)
            {
                widthCur += (wTarget - widthCur) * wCoef;
                monoBassCur += (mbTarget - monoBassCur) * wCoef;
                const float mid = 0.5f * (L[i] + R[i]);
                float side = 0.5f * (L[i] - R[i]);
                const float hp = sideHp.process (side);
                side = (side + (hp - side) * monoBassCur) * widthCur;
                const float g = driveGain.getNextValue();
                L[i] = (mid + side) * g;
                R[i] = (mid - side) * g;
            }
        }

        //----------------------------------------------------------------------
        static constexpr float compKnee = 8.0f;

        SharedState& shared;
        double fs = 44100.0;
        int maxBlockSize = 512, latency = 0;

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        Limiter limiter;
        juce::AudioBuffer<float> work, dry;
        std::array<std::vector<float>, 2> dryDelay;
        int dryPos = 0;

        std::array<std::array<Biquad, numEqBands + numToneBands>, 2> eq;
        std::array<float, numEqBands> eqCur {};
        std::array<float, numToneBands> toneCur {};
        std::array<Crossover, 2> xo;
        std::array<Comp, numCompBands> comp;
        std::array<Biquad, 2> deessBell;
        Biquad sibDetect, sideHp;
        float sibEnv = 0.0f, fullEnv = 0.0f, deessGr = 0.0f, deessBellDb = 0.0f;
        float widthCur = 1.0f, monoBassCur = 0.0f;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain { 1.0f };
        float driveTarget = 0.0f;
        float satK = 1.0e-4f;
        std::array<float, 2> dcX {}, dcY {};
        float matchGain = 1.0f, matchGainTarget = 1.0f, cmpMix = 0.0f;
        bool firstBlock = true, snapDrive = true;

        LoudnessMeter inMeter, outMeter;
        Integrator outIntegrator, anIntegrator, refineIntegrator;

        // analysis
        AnalysisData data;
        Derived derived;
        Settings last;
        bool settingsKnown = false;
        int state = Idle, refinePass = 0;
        float firstStep = 0.0f, firstMeasured = 0.0f;
        int seenLoadVersion = 0;
        int activeSamples = 0;
        int fftN = 16384, fftWrite = 0, fftSinceHop = 0, fftFilled = 0, fftFrames = 0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> fftRing, fftBuf, fftWindow;
        std::vector<double> binAcc;
        double msMid = 0.0, msSide = 0.0;
        long long msCount = 0;
    };
}
