#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/*  Renske – repair chain DSP.

    Signal flow (fixed order, every module can be switched on/off):

        De-clip -> [click stage: De-click + De-crackle + Mouth de-click] -> De-hum -> De-plosive
                -> [spectral stage: De-bleed, De-noise, Dialogue isolate, De-reverb, Breath control,
                    De-ess, Rebalance] -> output

    The latency is constant (click stage 2*H + spectral stage N + L*hop) no matter which modules are on,
    so Logic's delay compensation never has to change while you work. */

namespace renske
{
    using juce::jlimit;
    using juce::jmax;
    using juce::jmin;

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (jmax (1.0e-9f, g)); }
    inline float powToDb (float p)   { return 10.0f * std::log10 (jmax (1.0e-20f, p)); }

    inline void storeMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    //==========================================================================
    /** Simple biquad (transposed direct form II). */
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

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
            a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
        }
        void setLowpass (double sr, double f, double q = 0.70710678)
        {
            const double w = juce::MathConstants<double>::twoPi * jlimit (5.0, sr * 0.45, f) / sr;
            const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setHighpass (double sr, double f, double q = 0.70710678)
        {
            const double w = juce::MathConstants<double>::twoPi * jlimit (5.0, sr * 0.45, f) / sr;
            const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setPeak (double sr, double f, double q, double gainDb)
        {
            const double w = juce::MathConstants<double>::twoPi * jlimit (5.0, sr * 0.45, f) / sr;
            const double A = std::pow (10.0, gainDb / 40.0), c = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
    };

    //==========================================================================
    /** Fixed-length delay for the dry signal (mix / "removed only"). */
    struct DelayLine
    {
        void prepare (int delay, int numCh)
        {
            d = delay;
            for (int c = 0; c < 2; ++c) buf[c].assign ((size_t) jmax (1, d), 0.0f);
            pos = 0;
            ch = numCh;
        }
        float process (int c, float x) noexcept
        {
            if (d <= 0) return x;
            const float y = buf[c][(size_t) pos];
            buf[c][(size_t) pos] = x;
            return y;
        }
        void advance() noexcept { if (d > 0 && ++pos >= d) pos = 0; }
        void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }

        std::vector<float> buf[2];
        int d = 0, pos = 0, ch = 2;
    };

    //==========================================================================
    // Settings for one block, filled by the processor from the parameters.
    struct Settings
    {
        // de-clip
        bool declipOn = false;   int declipMode = 0;  float declipThrDb = -0.5f;  float declipGainDb = -3.0f;
        // click stage
        bool clickOn = false;    float clickSens = 50.0f;  float clickWidthMs = 2.0f;
        bool crackleOn = false;  float crackleAmount = 40.0f;
        bool mouthOn = false;    float mouthSens = 50.0f;  float mouthWidthMs = 1.5f;
        // de-hum
        bool humOn = false;      int humBase = 0;  int humHarmonics = 6;  float humQ = 20.0f;  float humDepthDb = 30.0f;
        float humSlope = 30.0f;  bool humHpf = false;
        // de-plosive
        bool plosiveOn = false;  float plosiveSens = 50.0f;  float plosiveFreq = 150.0f;  float plosiveMaxDb = 18.0f;
        // spectral
        bool bleedOn = false;    float bleedAmount = 60.0f;  float bleedMaxDb = 20.0f;
        bool denoiseOn = false;  int denoiseSource = 0;  int denoiseType = 0;  float denoiseRedDb = 12.0f;
        float denoiseThrDb = 0.0f;  float denoiseSmooth = 40.0f;
        bool isolateOn = false;  float isolateAmount = 60.0f;  float isolateBgDb = -30.0f;
        bool reverbOn = false;   float reverbAmount = 50.0f;  float reverbTail = 1.0f;
        bool breathOn = false;   float breathSens = 50.0f;  float breathGainDb = -15.0f;
        bool essOn = false;      int essMode = 1;  float essThrDb = -30.0f;  float essFreq = 5500.0f;  float essMaxDb = 12.0f;
        bool rebalOn = false;    float rebalVox = 0.0f, rebalBass = 0.0f, rebalPerc = 0.0f, rebalOther = 0.0f;
        float rebalSep = 50.0f;
    };

    //==========================================================================
    /** Live values shared with the editor and the assistant. */
    struct Shared
    {
        enum Module { mDeclip, mClick, mCrackle, mMouth, mHum, mPlosive, mBleed, mDenoise, mIsolate, mReverb,
                      mBreath, mEss, mRebal, numModules };

        // activity per module (max since last GUI read; events per GUI read for repairs, dB for gain modules)
        std::array<std::atomic<float>, numModules> activity {};

        std::atomic<float> humFreq { 0.0f };       // detected / used fundamental

        // learned noise profile (bins at the current FFT size), written by audio thread on learn finish
        std::atomic<int> learnCommand { 0 };       // 1 = start, 2 = finish, 3 = cancel
        std::atomic<bool> learning { false };
        std::atomic<float> learnSeconds { 0.0f };
        std::atomic<int> profileVersion { 0 };     // bumped whenever profile changes (audio side)

        // spectrogram columns for the GUI
        static constexpr int numBands = 128;
        static constexpr int fifoSize = 64;
        juce::AbstractFifo fifo { fifoSize };
        std::array<std::array<float, numBands>, fifoSize> colsIn {}, colsOut {};

        // ---- assistant ----
        std::atomic<int> analyse { 0 };            // 1 = running
        std::atomic<float> anSeconds { 0.0f };
        std::atomic<int> anClips { 0 }, anClicks { 0 }, anCrackle { 0 }, anMouth { 0 }, anPlosives { 0 };
        std::atomic<int> anBreaths { 0 }, anFrames { 0 }, anSibFrames { 0 };
        std::atomic<float> anHumDb { 0.0f }, anHumFreq { 0.0f };
        std::atomic<float> anNoiseDb { -120.0f }, anSignalDb { -120.0f }, anReverb { 0.0f }, anPeak { 0.0f };
        std::atomic<float> anSibDb { -120.0f };

        void resetAnalysis()
        {
            anSeconds = 0; anClips = 0; anClicks = 0; anCrackle = 0; anMouth = 0; anPlosives = 0; anBreaths = 0;
            anFrames = 0; anSibFrames = 0; anHumDb = 0; anHumFreq = 0; anNoiseDb = -120; anSignalDb = -120;
            anReverb = 0; anPeak = 0; anSibDb = -120;
        }
    };

    //==========================================================================
    /** De-clip detection: finds runs of samples stuck at the clip level (at least 3 in a row on the
        same side). The click stage redraws them with least-squares AR interpolation, constrained to
        stay outside the clip level. */
    struct Declipper
    {
        float peakHold = 0.0f, peakDecay = 0.9999f;

        void prepare (double sr) { peakDecay = (float) std::exp (-1.0 / (3.0 * sr)); peakHold = 0.0f; }

        /** Calls repair (start, end, sign, level) for every clipped run starting in x[a, b). */
        template <typename Fn>
        int process (const float* x, int a, int b, int total, const Settings& s, Fn&& repair)
        {
            // clip level from the raw look-ahead [b, total) – the block itself may already hold
            // repaired peaks from the previous window
            float blockPeak = 0.0f;
            for (int n = b; n < total; ++n) blockPeak = jmax (blockPeak, std::abs (x[n]));
            peakHold = jmax (blockPeak, peakHold * std::pow (peakDecay, (float) (b - a)));
            const float level = s.declipMode == 0 ? peakHold * 0.995f : dbToGain (s.declipThrDb);
            if (level < 1.0e-4f) return 0;

            int runs = 0, n = a;
            while (n < b)
            {
                if (std::abs (x[n]) < level) { ++n; continue; }
                const float sign = x[n] > 0 ? 1.0f : -1.0f;
                int e = n;
                while (e < total - 64 && x[e] * sign >= level) ++e;
                if (e - n >= 3)
                {
                    ++runs;
                    repair (n, e, sign, level);
                }
                n = jmax (e, n + 1);
            }
            return runs;
        }
    };

    //==========================================================================
    /** Click stage: impulsive-noise detection with an LPC residual and least-squares autoregressive
        (LSAR) interpolation of the damaged samples. Works on windows of 3*H samples:
        [repaired history | block to repair | raw look-ahead]. Latency = 2*H.
        De-clip also runs here (it needs look-ahead and the same interpolator). */
    class ClickStage
    {
    public:
        static constexpr int order = 24;

        int getLatency() const { return 2 * H; }

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            H = sr <= 50000.0 ? 1024 : sr <= 100000.0 ? 2048 : 4096;
            channels = jlimit (1, 2, numChannels);
            for (int c = 0; c < 2; ++c)
            {
                win[c].assign ((size_t) (3 * H), 0.0f);
                ready[c].assign ((size_t) H, 0.0f);
                declip[c].prepare (sr);
            }
            resid.assign ((size_t) (3 * H), 0.0f);
            mark.assign ((size_t) (3 * H), 0);
            scratch.assign ((size_t) H, 0.0f);
            tmp.assign ((size_t) (3 * H), 0.0f);
            maxInterp = jmin (H / 2, (int) (sr * 0.012));
            band.assign ((size_t) maxInterp * (order + 1), 0.0);
            rhs.assign ((size_t) maxInterp, 0.0);
            fill = 0;
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c) { std::fill (win[c].begin(), win[c].end(), 0.0f); std::fill (ready[c].begin(), ready[c].end(), 0.0f); }
            fill = 0;
        }

        void process (juce::AudioBuffer<float>& buf, const Settings& s, Shared& sh)
        {
            const int n = buf.getNumSamples(), nch = jmin (channels, buf.getNumChannels());
            int i = 0;
            while (i < n)
            {
                const int todo = jmin (n - i, H - fill);
                for (int c = 0; c < nch; ++c)
                {
                    float* d = buf.getWritePointer (c, i);
                    float* w = win[c].data() + 2 * H + fill;
                    const float* r = ready[c].data() + fill;
                    for (int k = 0; k < todo; ++k) { w[k] = d[k]; d[k] = r[k]; }
                }
                fill += todo;
                i += todo;
                if (fill == H)
                {
                    for (int c = 0; c < nch; ++c) processWindow (c, s, sh);
                    for (int c = 0; c < nch; ++c)
                    {
                        auto& w = win[c];
                        std::copy (w.begin() + H, w.begin() + 2 * H, ready[c].begin());
                        std::copy (w.begin() + H, w.end(), w.begin());
                    }
                    fill = 0;
                }
            }
        }

    private:
        /** Autocorrelation LPC of the whole window into A[0..order] (A[0] = 1).
            With useMask, samples marked in 'mark' (clipped) are left out and each lag is rescaled
            by the number of valid pairs, so the model describes the undamaged signal. */
        bool computeLpc (const float* x, int total, bool useMask = false)
        {
            double r[order + 1] {};
            for (int n = 0; n < total; ++n)
            {
                const float wv = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) (total - 1));
                tmp[(size_t) n] = (useMask && mark[(size_t) n]) ? 0.0f : x[n] * wv;
            }
            for (int k = 0; k <= order; ++k)
            {
                double acc = 0;
                int valid = 0;
                for (int n = k; n < total; ++n)
                {
                    acc += (double) tmp[(size_t) n] * tmp[(size_t) (n - k)];
                    if (useMask && ! mark[(size_t) n] && ! mark[(size_t) (n - k)]) ++valid;
                }
                if (useMask) acc *= (double) (total - k) / (double) jmax (1, valid);
                const double lw = std::exp (-0.5 * std::pow (2.0 * juce::MathConstants<double>::pi * 40.0 * k / sr, 2.0));
                r[k] = acc * lw;
            }
            if (r[0] < 1.0e-12) return false;
            r[0] *= useMask ? 1.001 : 1.00001;
            return levinson (r, A);
        }

        void processWindow (int c, const Settings& s, Shared& sh)
        {
            float* x = win[c].data();
            const int total = 3 * H, a = H, b = 2 * H;
            const bool analysing = sh.analyse.load (std::memory_order_relaxed) != 0;
            const bool anyClick = s.clickOn || s.crackleOn || s.mouthOn;
            bool haveLpc = false;

            // ---- de-clip (always detects for the assistant, repairs only when on) ----
            numRuns = 0;
            const int clips = declip[c].process (x, a, b, total, s, [this] (int n0, int n1, float sign, float level)
            {
                if (numRuns < maxRuns) runs[numRuns++] = { n0, n1, sign, level };
            });
            if (s.declipOn && numRuns > 0)
            {
                // model from the unclipped samples only (every clipped sample in the window is masked)
                std::fill (mark.begin(), mark.end(), (char) 0);
                const float lvl = runs[0].level;
                for (int n = 0; n < total; ++n) if (std::abs (x[n]) >= lvl) mark[(size_t) n] = 1;
                haveLpc = computeLpc (x, total, true);
                for (int r = 0; r < numRuns; ++r)
                {
                    const auto& run = runs[r];
                    const int lo = run.start - 1, hi = run.end + 1;   // neighbours are often distorted too
                    // short runs: LSAR; long runs (or no model): smooth polynomial through both sides
                    if (hi - lo > 12 || ! haveLpc || ! lsar (x, lo, hi)) polyFill (x, run.start, run.end);
                    for (int n = run.start; n < run.end; ++n)
                        x[n] = run.sign * jlimit (run.level, run.level * 4.0f, x[n] * run.sign);   // stay outside the clip level
                }
            }
            if (s.declipOn) sh.activity[Shared::mDeclip].store (sh.activity[Shared::mDeclip].load() + (float) clips);
            if (analysing) sh.anClips += clips;

            if (! anyClick && ! analysing) return;

            // ---- LPC on the whole window (after de-clipping) ----
            if (! computeLpc (x, total)) return;

            // ---- residual over the block (+ margin into the look-ahead) ----
            const int e0 = a - 8, e1 = jmin (total, b + order + 8);
            for (int n = e0; n < e1; ++n)
            {
                double e = x[n];
                for (int k = 1; k <= order; ++k) e += A[k] * x[n - k];
                resid[(size_t) n] = (float) e;
            }

            // robust scale: median |e| of the block
            for (int n = 0; n < H; ++n) scratch[(size_t) n] = std::abs (resid[(size_t) (a + n)]);
            std::nth_element (scratch.begin(), scratch.begin() + H / 2, scratch.end());
            const float sigma = jmax (1.0e-7f, 1.4826f * scratch[(size_t) (H / 2)]);

            std::fill (mark.begin() + e0, mark.begin() + e1, (char) 0);
            const float msToS = (float) sr * 0.001f;
            const int markLo = a - 4, markHi = e1 - order - 2;

            // k = threshold in sigmas; hp = detect on the differentiated residual (bright, short mouth clicks).
            // Hits closer than 'width' are joined into one repair.
            auto detect = [&] (float k, float widthMs, bool hp, bool doMark) -> int
            {
                const float thr = k * sigma * (hp ? 1.3f : 1.0f);
                const int join = jmax (2, (int) (widthMs * msToS));
                int events = 0, lastHit = -100000;
                for (int n = a; n < b; ++n)
                {
                    const float v = hp ? resid[(size_t) n] - resid[(size_t) (n - 1)] : resid[(size_t) n];
                    if (std::abs (v) <= thr) continue;
                    const bool joined = n - lastHit <= join;
                    if (! joined) ++events;
                    if (doMark)
                    {
                        const int lo = jmax (markLo, (joined ? lastHit : n) - 2), hi = jmin (markHi, n + 3);
                        for (int m = lo; m <= hi; ++m) mark[(size_t) m] = 1;
                    }
                    lastHit = n;
                }
                return events;
            };

            // sensitivity 0..100 -> threshold in sigmas (the assistant uses the middle setting)
            const int nClick = (s.clickOn || analysing)
                ? detect (juce::jmap (s.clickOn ? s.clickSens : 50.0f, 0.0f, 100.0f, 14.0f, 4.5f), s.clickOn ? s.clickWidthMs : 2.0f, false, s.clickOn) : 0;
            const int nCrackle = (s.crackleOn || analysing)
                ? detect (juce::jmap (s.crackleOn ? s.crackleAmount : 40.0f, 0.0f, 100.0f, 7.0f, 3.0f), 0.25f, false, s.crackleOn) : 0;
            const int nMouth = (s.mouthOn || analysing)
                ? detect (juce::jmap (s.mouthOn ? s.mouthSens : 50.0f, 0.0f, 100.0f, 12.0f, 4.0f), s.mouthOn ? s.mouthWidthMs : 1.5f, true, s.mouthOn) : 0;

            if (analysing) { sh.anClicks += nClick; sh.anCrackle += nCrackle; sh.anMouth += nMouth; }
            if (! anyClick) return;
            auto addEvents = [&sh] (int m, int count) { sh.activity[(size_t) m].store (sh.activity[(size_t) m].load() + (float) count); };
            if (s.clickOn)   addEvents (Shared::mClick, nClick);
            if (s.crackleOn) addEvents (Shared::mCrackle, nCrackle);
            if (s.mouthOn)   addEvents (Shared::mMouth, nMouth);

            // ---- interpolate every marked run ----
            int n = markLo;
            while (n < markHi)
            {
                if (! mark[(size_t) n]) { ++n; continue; }
                int e = n;
                while (e < markHi && mark[(size_t) e] && e - n < maxInterp) ++e;
                lsar (x, n, e);
                n = e + 1;
            }
        }

        /** Least-squares cubic through K samples on each side of the gap x[a, e), evaluated in the gap. */
        static void polyFill (float* x, int a, int e)
        {
            const int m = e - a, K = jlimit (6, 32, m / 2);
            const double mid = 0.5 * (a + e - 1), half = 0.5 * (m + 2 * K);
            double M[4][5] {};
            auto add = [&] (int n)
            {
                const double t = (n - mid) / half;
                const double pw[4] = { 1.0, t, t * t, t * t * t };
                for (int i = 0; i < 4; ++i)
                {
                    for (int j = 0; j < 4; ++j) M[i][j] += pw[i] * pw[j];
                    M[i][4] += pw[i] * x[n];
                }
            };
            for (int n = a - K; n < a; ++n) add (n);
            for (int n = e; n < e + K; ++n) add (n);
            for (int i = 0; i < 4; ++i) M[i][i] += 1.0e-9;
            // Gaussian elimination with partial pivoting
            for (int i = 0; i < 4; ++i)
            {
                int piv = i;
                for (int r = i + 1; r < 4; ++r) if (std::abs (M[r][i]) > std::abs (M[piv][i])) piv = r;
                for (int cI = 0; cI < 5; ++cI) std::swap (M[i][cI], M[piv][cI]);
                if (std::abs (M[i][i]) < 1.0e-15) return;
                for (int r = 0; r < 4; ++r)
                    if (r != i)
                    {
                        const double f = M[r][i] / M[i][i];
                        for (int cI = i; cI < 5; ++cI) M[r][cI] -= f * M[i][cI];
                    }
            }
            double cf[4];
            for (int i = 0; i < 4; ++i) cf[i] = M[i][4] / M[i][i];
            for (int n = a; n < e; ++n)
            {
                const double t = (n - mid) / half;
                x[n] = (float) jlimit (-4.0, 4.0, cf[0] + t * (cf[1] + t * (cf[2] + t * cf[3])));
            }
        }

        /** Least-squares AR interpolation: replaces x[a, e) with the values that minimise the total
            prediction error of the AR model A over the gap (banded Cholesky, O(m * order^2)). */
        bool lsar (float* x, int a, int e)
        {
            constexpr int p = order;
            const int m = e - a;
            if (m <= 0 || m > maxInterp) return false;

            double cc[p + 1];
            for (int d = 0; d <= p; ++d)
            {
                double acc = 0;
                for (int k = 0; k + d <= p; ++k) acc += A[k] * A[k + d];
                cc[d] = acc;
            }

            // right-hand side: -sum_n A[n - i] * (prediction error of the known samples at n)
            std::fill (rhs.begin(), rhs.begin() + m, 0.0);
            for (int n = a; n < e + p; ++n)
            {
                double known = 0;
                for (int k = 0; k <= p; ++k)
                {
                    const int idx = n - k;
                    if (idx < a || idx >= e) known += A[k] * x[idx];
                }
                for (int k = 0; k <= p; ++k)
                {
                    const int idx = n - k;
                    if (idx >= a && idx < e) rhs[(size_t) (idx - a)] -= A[k] * known;
                }
            }

            // banded Cholesky: L[i][i - d] stored at band[i * (p + 1) + d]
            auto L = [this] (int i, int d) -> double& { return band[(size_t) i * (p + 1) + (size_t) d]; };
            const double reg = cc[0] * 1.0e-6;
            for (int i = 0; i < m; ++i)
            {
                for (int j = jmax (0, i - p); j <= i; ++j)
                {
                    double sum = cc[i - j] + (i == j ? reg : 0.0);
                    for (int k = jmax (0, i - p); k < j; ++k)
                        if (j - k <= p) sum -= L (i, i - k) * L (j, j - k);
                    if (i == j)
                    {
                        if (sum <= 0) return false;
                        L (i, 0) = std::sqrt (sum);
                    }
                    else L (i, i - j) = sum / L (j, 0);
                }
            }
            // forward / backward substitution
            for (int i = 0; i < m; ++i)
            {
                double sum = rhs[(size_t) i];
                for (int j = jmax (0, i - p); j < i; ++j) sum -= L (i, i - j) * rhs[(size_t) j];
                rhs[(size_t) i] = sum / L (i, 0);
            }
            for (int i = m - 1; i >= 0; --i)
            {
                double sum = rhs[(size_t) i];
                for (int j = i + 1; j <= jmin (m - 1, i + p); ++j) sum -= L (j, j - i) * rhs[(size_t) j];
                rhs[(size_t) i] = sum / L (i, 0);
            }
            for (int i = 0; i < m; ++i) x[a + i] = (float) jlimit (-4.0, 4.0, rhs[(size_t) i]);
            return true;
        }

        static bool levinson (const double* r, double* a)
        {
            double err = r[0];
            a[0] = 1.0;
            for (int i = 1; i <= order; ++i)
            {
                double acc = r[i];
                for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
                const double k = -acc / err;
                if (std::abs (k) >= 1.0) return false;
                double tmpA[order + 1];
                for (int j = 0; j <= i; ++j) tmpA[j] = a[j];
                for (int j = 1; j < i; ++j) a[j] = tmpA[j] + k * tmpA[i - j];
                a[i] = k;
                err *= (1.0 - k * k);
                if (err <= 0) return false;
            }
            return true;
        }

        double sr = 44100.0;
        int H = 1024, channels = 2, fill = 0, maxInterp = 512;
        std::vector<float> win[2], ready[2];
        std::vector<float> resid, scratch, tmp;
        std::vector<char> mark;
        std::vector<double> band, rhs;
        double A[order + 1] {};
        struct Run { int start, end; float sign, level; };
        static constexpr int maxRuns = 256;
        Run runs[maxRuns] {};
        int numRuns = 0;
        Declipper declip[2];
    };

    //==========================================================================
    /** De-hum: notch cascade on the mains fundamental and its harmonics. The fundamental is either fixed
        (50/60 Hz) or tracked automatically with Goertzel scans on a decimated copy of the signal. */
    class HumStage
    {
    public:
        static constexpr int maxHarm = 16;

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = jlimit (1, 2, numChannels);
            decim = jmax (1, (int) std::round (sr / 3000.0));
            dsr = sr / decim;
            for (auto& f : aa) { f.setLowpass (sr, 600.0); f.reset(); }
            dbuf.assign ((size_t) (dsr * 1.0), 0.0f);
            dpos = 0; dcount = 0; scanCount = 0;
            for (auto& ch : notch) for (auto& f : ch) f.reset();
            for (auto& f : hp) f.reset();
            freq = 50.0f; lastFreq = -1.0f; lastKey = {};
            wet.reset (sr, 0.05);
            wet.setCurrentAndTargetValue (0.0f);
        }

        float getFreq() const { return freq; }
        float getDetectedDb() const { return detDb; }
        float getDetectedFreq() const { return detFreq; }

        void process (juce::AudioBuffer<float>& buf, const Settings& s, Shared& sh)
        {
            const int n = buf.getNumSamples(), nch = jmin (channels, buf.getNumChannels());
            const bool analysing = sh.analyse.load (std::memory_order_relaxed) != 0;
            const bool needScan = (s.humOn && s.humBase == 0) || analysing;

            if (needScan)
            {
                const float* l = buf.getReadPointer (0);
                const float* r = buf.getReadPointer (nch > 1 ? 1 : 0);
                for (int i = 0; i < n; ++i)
                {
                    const float v = aa[1].process (aa[0].process (0.5f * (l[i] + r[i])));
                    if (++dcount >= decim)
                    {
                        dcount = 0;
                        dbuf[(size_t) dpos] = v;
                        if (++dpos >= (int) dbuf.size()) dpos = 0;
                        if (++scanCount >= (int) dbuf.size() / 2) { scanCount = 0; scan(); }
                    }
                }
            }

            if (s.humOn)
            {
                if (s.humBase == 1) freq = 50.0f;
                else if (s.humBase == 2) freq = 60.0f;
                else if (detDb > 6.0f) freq += 0.5f * (detFreq - freq);
            }
            sh.humFreq.store (freq, std::memory_order_relaxed);
            if (analysing) { sh.anHumDb.store (detDb); sh.anHumFreq.store (detFreq); }

            wet.setTargetValue (s.humOn ? 1.0f : 0.0f);
            if (! s.humOn && ! wet.isSmoothing() && wet.getCurrentValue() == 0.0f)
                return;

            updateFilters (s);

            for (int c = 0; c < nch; ++c)
            {
                float* d = buf.getWritePointer (c);
                auto w = wet;
                for (int i = 0; i < n; ++i)
                {
                    float y = d[i];
                    for (int k = 0; k < numNotches; ++k) y = notch[c][k].process (y);
                    if (s.humHpf) y = hp[c * 2 + 1].process (hp[c * 2].process (y));
                    const float g = w.getNextValue();
                    d[i] = d[i] + g * (y - d[i]);
                }
            }
            wet.skip (n);
            if (s.humOn) storeMax (sh.activity[Shared::mHum], s.humDepthDb);
        }

    private:
        void updateFilters (const Settings& s)
        {
            const auto key = std::array<float, 6> { freq, (float) s.humHarmonics, s.humQ, s.humDepthDb, s.humSlope, s.humHpf ? 1.0f : 0.0f };
            bool same = std::abs (key[0] - lastKey[0]) < 0.02f;
            for (size_t i = 1; i < key.size(); ++i) same = same && juce::exactlyEqual (key[i], lastKey[i]);
            if (same) return;
            lastKey = key;
            numNotches = 0;
            const int nh = jlimit (1, maxHarm, s.humHarmonics);
            for (int k = 1; k <= nh; ++k)
            {
                const double f = (double) freq * k;
                if (f > sr * 0.45) break;
                const double slope = nh > 1 ? (double) (k - 1) / (nh - 1) : 0.0;
                const double depth = s.humDepthDb * (1.0 - 0.8 * s.humSlope * 0.01 * slope);
                const double q = jmin (300.0, (double) s.humQ * k);   // same width in Hz on every harmonic
                for (int c = 0; c < 2; ++c) notch[c][numNotches].setPeak (sr, f, q, -depth);
                ++numNotches;
            }
            for (int c = 0; c < 2; ++c)
            {
                hp[c * 2].setHighpass (sr, freq * 1.3, 0.54);
                hp[c * 2 + 1].setHighpass (sr, freq * 1.3, 1.31);
            }
        }

        float goertzel (double f) const
        {
            const double w = juce::MathConstants<double>::twoPi * f / dsr, c = 2.0 * std::cos (w);
            double s1 = 0, s2 = 0;
            for (size_t i = 0; i < lin.size(); ++i)
            {
                const double s0 = lin[i] + c * s1 - s2;
                s2 = s1; s1 = s0;
            }
            return (float) (s1 * s1 + s2 * s2 - c * s1 * s2);
        }

        void scan()
        {
            // unwrap the ring (oldest first) and window it once
            const int n = (int) dbuf.size();
            lin.resize ((size_t) n);
            if ((int) hann.size() != n)
            {
                hann.resize ((size_t) n);
                for (int i = 0; i < n; ++i) hann[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1));
            }
            for (int i = 0; i < n; ++i) lin[(size_t) i] = dbuf[(size_t) ((dpos + i) % n)] * hann[(size_t) i];
            auto goertzel = [this] (const std::vector<float>&, int, double f, double) { return this->goertzel (f); };
            // harmonic sum around 50 and 60 Hz; compare with the level between the harmonics
            float best = 0.0f, bestF = 50.0f;
            for (double base : { 50.0, 60.0 })
                for (double f = base - 2.0; f <= base + 2.0; f += 0.1)
                {
                    float p = 0.0f;
                    for (int k = 1; k <= 3; ++k) p += goertzel (dbuf, dpos, f * (double) k, dsr);
                    if (p > best) { best = p; bestF = (float) f; }
                }
            float floor = 0.0f;
            for (int k = 1; k <= 3; ++k)
                floor += 0.5f * (goertzel (dbuf, dpos, bestF * ((float) k + 0.5f), dsr) + goertzel (dbuf, dpos, bestF * ((float) k - 0.5f), dsr));
            detFreq = bestF;
            detDb = powToDb (best) - powToDb (floor);
        }

        double sr = 44100.0, dsr = 3000.0;
        int channels = 2, decim = 16, dpos = 0, dcount = 0, scanCount = 0, numNotches = 0;
        Biquad aa[2];
        std::vector<float> dbuf, lin, hann;
        Biquad notch[2][maxHarm];
        Biquad hp[4];
        float freq = 50.0f, lastFreq = -1.0f, detDb = 0.0f, detFreq = 50.0f;
        std::array<float, 6> lastKey {};
        juce::SmoothedValue<float> wet;
    };

    //==========================================================================
    /** De-plosive: complementary split at the crossover frequency; when the low band suddenly
        dominates the rest of the signal (a "p" or "b" pop, wind), the low band is turned down fast. */
    class PlosiveStage
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = jlimit (1, 2, numChannels);
            for (auto& f : lp) f.reset();
            lowEnv = highEnv = 0.0f;
            gain = 1.0f;
            lastFreq = -1.0f;
            aAtt = (float) (1.0 - std::exp (-1.0 / (0.0007 * sr)));
            aRel = (float) (1.0 - std::exp (-1.0 / (0.060 * sr)));
            aHigh = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
            aGainDown = (float) (1.0 - std::exp (-1.0 / (0.0008 * sr)));
            aGainUp = (float) (1.0 - std::exp (-1.0 / (0.080 * sr)));
            inPlosive = false;
        }

        void process (juce::AudioBuffer<float>& buf, const Settings& s, Shared& sh)
        {
            const int n = buf.getNumSamples(), nch = jmin (channels, buf.getNumChannels());
            const bool analysing = sh.analyse.load (std::memory_order_relaxed) != 0;
            if (! s.plosiveOn && ! analysing) { gain = 1.0f; return; }

            if (std::abs (s.plosiveFreq - lastFreq) > 0.5f)
            {
                lastFreq = s.plosiveFreq;
                for (int c = 0; c < 2; ++c)
                {
                    lp[c * 2].setLowpass (sr, lastFreq);
                    lp[c * 2 + 1].setLowpass (sr, lastFreq);
                }
            }

            // ratio threshold: sensitivity 0..100 -> low band must be 18..3 dB above the rest
            const float ratio = dbToGain (juce::jmap (analysing && ! s.plosiveOn ? 50.0f : s.plosiveSens, 0.0f, 100.0f, 18.0f, 3.0f));
            const float minGain = dbToGain (-s.plosiveMaxDb);
            float maxGr = 0.0f;

            float* d0 = buf.getWritePointer (0);
            float* d1 = nch > 1 ? buf.getWritePointer (1) : nullptr;
            for (int i = 0; i < n; ++i)
            {
                const float l0 = lp[1].process (lp[0].process (d0[i]));
                const float l1 = d1 ? lp[3].process (lp[2].process (d1[i])) : l0;
                const float h0 = d0[i] - l0, h1 = d1 ? d1[i] - l1 : h0;
                const float lv = jmax (std::abs (l0), std::abs (l1));
                const float hv = jmax (std::abs (h0), std::abs (h1));
                lowEnv += (lv > lowEnv ? aAtt : aRel) * (lv - lowEnv);
                highEnv += aHigh * (hv - highEnv);

                float target = 1.0f;
                const float limit = highEnv * ratio + 0.003f;   // absolute floor ~ -50 dBFS
                const bool hit = lowEnv > limit;
                if (hit) target = jmax (minGain, limit / lowEnv);
                if (hit && ! inPlosive && analysing) sh.anPlosives++;
                inPlosive = hit;

                gain += (target < gain ? aGainDown : aGainUp) * (target - gain);
                if (s.plosiveOn)
                {
                    d0[i] = h0 + gain * l0;
                    if (d1) d1[i] = h1 + gain * l1;
                    maxGr = jmax (maxGr, -gainToDb (gain));
                }
            }
            if (s.plosiveOn) storeMax (sh.activity[Shared::mPlosive], maxGr);
        }

    private:
        double sr = 44100.0;
        int channels = 2;
        Biquad lp[4];
        float lowEnv = 0, highEnv = 0, gain = 1, lastFreq = -1;
        float aAtt = 0, aRel = 0, aHigh = 0, aGainDown = 0, aGainUp = 0;
        bool inPlosive = false;
    };

    //==========================================================================
    /** Spectral stage: one STFT (sqrt-Hann, 75 % overlap) shared by all spectral modules. Each module
        produces a gain per bin; the gains are multiplied and applied to all channels (linked), so the
        stereo image is kept. Frames are processed with L frames of look-ahead (needed for the
        harmonic/percussive split in Rebalance and for smoother de-noising). */
    class SpectralStage
    {
    public:
        static constexpr int L = 2;               // look-ahead frames
        static constexpr int hist = 2 * L + 1;

        int getLatency() const { return N + L * hop; }
        int getFftSize() const { return N; }

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = jlimit (1, 2, numChannels);
            const int ord = sr <= 50000.0 ? 11 : sr <= 100000.0 ? 12 : 13;
            N = 1 << ord;
            hop = N / 4;
            nBins = N / 2 + 1;
            fft = std::make_unique<juce::dsp::FFT> (ord);
            window.resize ((size_t) N);
            for (int n = 0; n < N; ++n)
                window[(size_t) n] = std::sqrt (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) N));

            for (int c = 0; c < 3; ++c)
            {
                inRing[c].assign ((size_t) N, 0.0f);
                for (auto& f : spec[c]) f.assign ((size_t) N * 2, 0.0f);
            }
            for (int c = 0; c < 2; ++c) outRing[c].assign ((size_t) N, 0.0f);
            frame.assign ((size_t) N * 2, 0.0f);
            for (auto& p : pow) p.assign ((size_t) nBins, 0.0f);
            for (auto& p : refPow) p.assign ((size_t) nBins, 0.0f);
            framesSeen = 0;
            pos = 0;
            hopCount = 0;

            const float dt = (float) hop / (float) sr;
            frameDt = dt;
            auto vec = [this] (float v) { return std::vector<float> ((size_t) nBins, v); };
            gain = vec (1.0f);  tmpG = vec (1.0f);
            dnGain = vec (1.0f); dnSmooth = vec (0.0f); dnMin = vec (1.0e-6f); dnMinTmp = vec (1.0e-6f);
            dnProfile = vec (0.0f); learnAcc.assign ((size_t) nBins, 0.0);
            isoGain = vec (1.0f); isoFloor = vec (1.0e-6f); isoSmooth = vec (0.0f);
            rvGain = vec (1.0f); for (auto& p : rvHist) p.assign ((size_t) nBins, 0.0f);
            blGain = vec (1.0f); blNum = vec (0.0f); blDen = vec (1.0e-12f);
            rbGain = vec (1.0f);
            profileValid = false;
            minFrames = 0;
            psdInit = false;
            psdSm = vec (0.0f);
            for (auto& sm : subMin) sm = vec (1.0e10f);
            subMinAll = vec (1.0e10f);
            dnMinTmp = vec (1.0e10f);
            dnMin = vec (1.0e10f);
            subSlot = 0;
            breathGain = 1.0f; voicePeak = 1.0e-6f; essGr = 0.0f;
            for (auto& f : fade) f = 0.0f;
            learnFrames = 0;
            rvPos = 0;
            medBuf.resize (32);
            // bands for the noise floor: ~1/6 octave, at least 6 bins wide
            floorEdge.clear();
            floorEdge.push_back (1);
            while (floorEdge.back() < nBins)
            {
                const int k = floorEdge.back();
                floorEdge.push_back (jmin (nBins, k + jmax (6, (int) ((float) k * 0.122f))));
            }
            numFloorBands = (int) floorEdge.size() - 1;
            floorBand.assign ((size_t) numFloorBands, 0.0f);
            medBuf2.assign ((size_t) nBins, 0.0f);
            binBandPos.assign ((size_t) nBins, 0.0f);
            for (int k = 0; k < nBins; ++k)
            {
                // fractional band index relative to band centres
                int b = 0;
                while (b < numFloorBands - 1 && floorEdge[(size_t) b + 1] <= k) ++b;
                const float c0 = 0.5f * (float) (floorEdge[(size_t) b] + floorEdge[(size_t) b + 1]);
                if ((float) k >= c0 && b < numFloorBands - 1)
                {
                    const float c1 = 0.5f * (float) (floorEdge[(size_t) b + 1] + floorEdge[(size_t) b + 2]);
                    binBandPos[(size_t) k] = (float) b + ((float) k - c0) / (c1 - c0);
                }
                else if (b > 0)
                {
                    const float cm = 0.5f * (float) (floorEdge[(size_t) b - 1] + floorEdge[(size_t) b]);
                    binBandPos[(size_t) k] = (float) (b - 1) + ((float) k - cm) / (c0 - cm);
                }
                else binBandPos[(size_t) k] = 0.0f;
            }
            noiseHistDb.clear();

            // band mapping for the spectrogram (log spaced 30 Hz .. 20 kHz)
            const float binHz = (float) sr / (float) N;
            for (int b = 0; b <= Shared::numBands; ++b)
            {
                const float f = 30.0f * std::pow (20000.0f / 30.0f, (float) b / Shared::numBands);
                bandEdge[(size_t) b] = jlimit (1, nBins - 1, (int) std::round (f / binHz));
            }
            powerNorm = (juce::MathConstants<float>::pi / (float) N) * (juce::MathConstants<float>::pi / (float) N);
        }

        void reset()
        {
            for (int c = 0; c < 3; ++c) std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
            for (int c = 0; c < 2; ++c) std::fill (outRing[c].begin(), outRing[c].end(), 0.0f);
            framesSeen = 0;
        }

        /** Noise profile as dB per bin (for state saving / display). */
        std::vector<float> getProfile() const { return profileValid ? dnProfile : std::vector<float>(); }
        void setProfile (const std::vector<float>& p)
        {
            if ((int) p.size() == nBins) { dnProfile = p; profileValid = true; }
        }
        bool hasProfile() const { return profileValid; }

        void process (juce::AudioBuffer<float>& buf, const float* const* side, int numSide, const Settings& s, Shared& sh)
        {
            settings = s;
            handleLearn (sh);
            const int n = buf.getNumSamples(), nch = jmin (channels, buf.getNumChannels());
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < nch; ++c)
                {
                    float* d = buf.getWritePointer (c);
                    inRing[c][(size_t) pos] = d[i];
                    d[i] = outRing[c][(size_t) pos];
                    outRing[c][(size_t) pos] = 0.0f;
                }
                float sc = 0.0f;
                for (int c = 0; c < numSide; ++c) sc += side[c][i];
                inRing[2][(size_t) pos] = numSide > 0 ? sc / (float) numSide : 0.0f;
                hasSide = numSide > 0;
                if (++pos >= N) pos = 0;
                if (++hopCount >= hop) { hopCount = 0; processFrame (nch, sh); }
            }
        }

        const std::vector<float>& currentProfileDb() const { return dnProfile; }


    private:
        //----------------------------------------------------------------------
        void handleLearn (Shared& sh)
        {
            const int cmd = sh.learnCommand.exchange (0);
            if (cmd == 1) { learnFrames = 0; std::fill (learnAcc.begin(), learnAcc.end(), 0.0); sh.learning = true; }
            else if (cmd == 3) sh.learning = false;
            else if (cmd == 2 && sh.learning.load())
            {
                sh.learning = false;
                if (learnFrames > 4)
                {
                    for (int k = 0; k < nBins; ++k) dnProfile[(size_t) k] = (float) (learnAcc[(size_t) k] / learnFrames);
                    profileValid = true;
                    sh.profileVersion++;
                }
            }
            sh.learnSeconds.store ((float) learnFrames * frameDt);
        }

        int ringIndex (int framesBack) const { return ((framesSeen - 1 - framesBack) % hist + hist) % hist; }

        void processFrame (int nch, Shared& sh)
        {
            // ---- analysis of the newest frame ----
            const int slot = framesSeen % hist;
            const int nAna = hasSide ? 3 : nch;
            for (int c = 0; c < 3; ++c)
            {
                if (c >= nch && c != 2) continue;
                if (c == 2 && ! hasSide) continue;
                auto& f = spec[c][(size_t) slot];
                for (int n = 0; n < N; ++n) f[(size_t) n] = inRing[c][(size_t) ((pos + n) % N)] * window[(size_t) n];
                std::fill (f.begin() + N, f.end(), 0.0f);
                fft->performRealOnlyForwardTransform (f.data(), true);
            }
            juce::ignoreUnused (nAna);
            {
                auto& p = pow[(size_t) slot];
                auto& rp = refPow[(size_t) slot];
                for (int k = 0; k < nBins; ++k)
                {
                    float acc = 0.0f;
                    for (int c = 0; c < nch; ++c)
                    {
                        const auto& f = spec[c][(size_t) slot];
                        acc += f[(size_t) (2 * k)] * f[(size_t) (2 * k)] + f[(size_t) (2 * k + 1)] * f[(size_t) (2 * k + 1)];
                    }
                    p[(size_t) k] = acc / (float) nch * powerNorm;
                    if (hasSide)
                    {
                        const auto& f = spec[2][(size_t) slot];
                        rp[(size_t) k] = (f[(size_t) (2 * k)] * f[(size_t) (2 * k)] + f[(size_t) (2 * k + 1)] * f[(size_t) (2 * k + 1)]) * powerNorm;
                    }
                    else rp[(size_t) k] = 0.0f;
                }
            }
            ++framesSeen;
            if (framesSeen <= L) return;

            // ---- process the centre frame (L frames old) ----
            const int cs = ringIndex (L);
            const auto& P = pow[(size_t) cs];
            const auto& s = settings;
            const float binHz = (float) sr / (float) N;
            const bool analysing = sh.analyse.load (std::memory_order_relaxed) != 0;

            std::fill (gain.begin(), gain.end(), 1.0f);

            auto fadeTo = [this] (int m, bool on)
            {
                fade[m] += ((on ? 1.0f : 0.0f) - fade[m]) * 0.25f;
                if (! on && fade[m] < 0.002f) fade[m] = 0.0f;
                return fade[m];
            };
            auto applyGain = [&] (const std::vector<float>& g, float f)
            {
                for (int k = 0; k < nBins; ++k) gain[(size_t) k] *= 1.0f + f * (g[(size_t) k] - 1.0f);
            };
            auto meanReduction = [&] (const std::vector<float>& g, float f)
            {
                double acc = 0, w = 0;
                for (int k = 2; k < nBins; k += 2) { const double e = P[(size_t) k]; acc += e * (1.0f + f * (g[(size_t) k] - 1.0f)) * (1.0f + f * (g[(size_t) k] - 1.0f)); w += e; }
                return w > 1.0e-14 ? (float) jmax (0.0, -10.0 * std::log10 (jmax (1.0e-10, acc / w))) : 0.0f;
            };

            // frame level (100 Hz .. 10 kHz)
            double frameE = 0;
            for (int k = (int) (100 / binHz); k < jmin (nBins, (int) (10000 / binHz)); ++k) frameE += P[(size_t) k];
            const float frameDb = powToDb ((float) frameE);

            // minimum-statistics noise floor (used by adaptive de-noise, isolate and the assistant)
            updateNoiseFloor (P);

            // learn
            if (sh.learning.load (std::memory_order_relaxed))
            {
                for (int k = 0; k < nBins; ++k) learnAcc[(size_t) k] += P[(size_t) k];
                ++learnFrames;
            }

            // ---- De-bleed ----
            if (const float f = fadeTo (Shared::mBleed, s.bleedOn && hasSide); f > 0.0f)
            {
                const auto& R = refPow[(size_t) cs];
                const float strength = s.bleedAmount * 0.02f;   // 0..2
                const float floorG = dbToGain (-s.bleedMaxDb);
                for (int k = 0; k < nBins; ++k)
                {
                    const float x = std::sqrt (P[(size_t) k]), r = std::sqrt (R[(size_t) k]);
                    if (R[(size_t) k] > 1.0e-10f)
                    {
                        blNum[(size_t) k] += 0.02f * (x * r - blNum[(size_t) k]);
                        blDen[(size_t) k] += 0.02f * (R[(size_t) k] - blDen[(size_t) k]);
                    }
                    const float h = jlimit (0.0f, 4.0f, blNum[(size_t) k] / jmax (1.0e-12f, blDen[(size_t) k]));
                    const float b = strength * h * h * R[(size_t) k];
                    const float g = jmax (floorG, 1.0f - b / jmax (1.0e-14f, P[(size_t) k]));
                    blGain[(size_t) k] += (g > blGain[(size_t) k] ? 0.3f : 0.7f) * (g - blGain[(size_t) k]);
                }
                applyGain (blGain, f);
                storeMax (sh.activity[Shared::mBleed], meanReduction (blGain, f));
            }

            // ---- De-noise ----
            if (const float f = fadeTo (Shared::mDenoise, s.denoiseOn); f > 0.0f)
            {
                const bool learned = s.denoiseSource == 1 && profileValid;
                const float thr = dbToGain (s.denoiseThrDb) * dbToGain (s.denoiseThrDb);   // power
                const float floorG = dbToGain (-s.denoiseRedDb);
                const float tSm = s.denoiseSmooth * 0.01f;
                const float aUp = 1.0f - 0.85f * tSm, aDown = 1.0f - 0.6f * tSm;
                // observed power: a little smoothing in time, then across frequency (wider in the highs).
                // Smoothing the power (not the gain) keeps tones and their skirts intact while isolated
                // noise peaks are averaged away (less "musical noise").
                for (int k = 0; k < nBins; ++k) dnSmooth[(size_t) k] += 0.6f * (P[(size_t) k] - dnSmooth[(size_t) k]);
                smoothPower (dnSmooth, powSm, s.denoiseSmooth);
                for (int k = 0; k < nBins; ++k)
                {
                    const float fr = (float) k * binHz;
                    float w = 1.0f;   // threshold weighting per type
                    if (s.denoiseType == 1)      // voice: protect 100 Hz .. 4 kHz, be harder outside
                        w = fr < 80.0f ? 2.0f : fr < 4000.0f ? 0.8f : fr < 8000.0f ? 1.0f : 1.6f;
                    else if (s.denoiseType == 2) // guitar: protect body and harmonics, hiss above 6 kHz
                        w = fr < 70.0f ? 1.5f : fr < 5000.0f ? 0.75f : 1.3f;
                    // over-subtraction factor 2.5 at 0 dB threshold
                    const float noise = 2.5f * (learned ? dnProfile[(size_t) k] : noiseFloor (k)) * thr * w;
                    const float g = jlimit (0.0f, 1.0f, 1.0f - noise / jmax (1.0e-20f, powSm[(size_t) k]));
                    tmpG[(size_t) k] = floorG + (1.0f - floorG) * std::sqrt (g);
                }
                for (int k = 0; k < nBins; ++k)
                {
                    const float g = tmpG[(size_t) k];
                    dnGain[(size_t) k] += (g > dnGain[(size_t) k] ? aUp : aDown) * (g - dnGain[(size_t) k]);
                }
                applyGain (dnGain, f);
                storeMax (sh.activity[Shared::mDenoise], meanReduction (dnGain, f));
            }

            // ---- Dialogue isolate ----
            if (const float f = fadeTo (Shared::mIsolate, s.isolateOn); f > 0.0f)
            {
                const float sep = s.isolateAmount * 0.01f;
                const float bg = dbToGain (s.isolateBgDb);
                for (int k = 0; k < nBins; ++k) isoSmooth[(size_t) k] += 0.5f * (P[(size_t) k] - isoSmooth[(size_t) k]);
                smoothPower (isoSmooth, powSm, 30.0f);
                for (int k = 0; k < nBins; ++k)
                {
                    const float fr = (float) k * binHz;
                    // speech band weighting
                    const float band = fr < 70.0f ? 0.0f : fr < 140.0f ? (fr - 70.0f) / 70.0f
                                     : fr < 7000.0f ? 1.0f : fr < 12000.0f ? 1.0f - (fr - 7000.0f) / 5000.0f : 0.0f;
                    const float snrDb = powToDb (powSm[(size_t) k]) - powToDb (noiseFloor (k));
                    // presence: sigmoid around 4..14 dB above the floor depending on separation
                    const float centre = 4.0f + 10.0f * sep, width = 4.0f - 2.5f * sep;
                    const float pres = 1.0f / (1.0f + std::exp (-(snrDb - centre) / width));
                    tmpG[(size_t) k] = bg + (1.0f - bg) * pres * band;
                }
                for (int k = 0; k < nBins; ++k)
                {
                    const float g = tmpG[(size_t) k];
                    isoGain[(size_t) k] += (g > isoGain[(size_t) k] ? 0.8f : 0.35f) * (g - isoGain[(size_t) k]);
                }
                applyGain (isoGain, f);
                storeMax (sh.activity[Shared::mIsolate], meanReduction (isoGain, f));
            }

            // ---- De-reverb (late reverberation estimate) ----
            {
                // keep a history of power spectra for the late-reverb model (also for the assistant)
                rvPos = (rvPos + 1) % rvLen;
                std::copy (P.begin(), P.end(), rvHist[(size_t) rvPos].begin());
            }
            if (const float f = fadeTo (Shared::mReverb, s.reverbOn); f > 0.0f)
            {
                const int D = jlimit (1, rvLen - 1, (int) std::round (0.05f / frameDt));   // 50 ms gap
                const auto& Pd = rvHist[(size_t) ((rvPos - D + rvLen) % rvLen)];
                const float t60 = jmax (0.1f, s.reverbTail);
                const float decay = std::exp (-2.0f * 6.908f * (float) D * frameDt / t60);
                const float strength = s.reverbAmount * 0.02f;
                const float floorG = dbToGain (-6.0f - 0.24f * s.reverbAmount);
                smoothPower (P, powSm, 25.0f);
                smoothPower (Pd, powSm2, 25.0f);
                for (int k = 0; k < nBins; ++k)
                {
                    const float late = strength * decay * powSm2[(size_t) k];
                    tmpG[(size_t) k] = jmax (floorG, 1.0f - late / jmax (1.0e-20f, powSm[(size_t) k]));
                }
                for (int k = 0; k < nBins; ++k)
                {
                    const float g = tmpG[(size_t) k];
                    rvGain[(size_t) k] += (g > rvGain[(size_t) k] ? 0.7f : 0.5f) * (g - rvGain[(size_t) k]);
                }
                applyGain (rvGain, f);
                storeMax (sh.activity[Shared::mReverb], meanReduction (rvGain, f));
            }

            // ---- Breath control ----
            {
                // spectral flatness 500 Hz .. 8 kHz
                double lg = 0, ar = 0; int cnt = 0;
                for (int k = (int) (500 / binHz); k < jmin (nBins, (int) (8000 / binHz)); ++k)
                {
                    const double v = P[(size_t) k] + 1.0e-20;
                    lg += std::log (v); ar += v; ++cnt;
                }
                const float flat = cnt > 0 ? (float) (std::exp (lg / cnt) / (ar / cnt)) : 0.0f;
                voicePeak = jmax ((float) frameE, voicePeak * std::exp (-frameDt / 3.0f));
                const float relDb = frameDb - powToDb (voicePeak);
                const float sens = (s.breathOn ? s.breathSens : 50.0f) * 0.01f;
                const float floorDb = powToDb (noiseFloor (nBins / 8) * (float) nBins) + 3.0f;
                const bool breath = flat > (0.45f - 0.25f * sens) && relDb < -(8.0f + 10.0f * (1.0f - sens))
                                    && relDb > -50.0f && frameDb > floorDb;
                if (analysing && breath) sh.anBreaths++;
                const float target = breath ? dbToGain (s.breathGainDb) : 1.0f;
                breathGain += (target < breathGain ? 0.35f : 0.25f) * (target - breathGain);
                if (const float f = fadeTo (Shared::mBreath, s.breathOn); f > 0.0f)
                {
                    const float g = 1.0f + f * (breathGain - 1.0f);
                    for (auto& v : gain) v *= g;
                    storeMax (sh.activity[Shared::mBreath], -gainToDb (g));
                }
            }

            // ---- De-ess ----
            {
                const int k0 = jlimit (1, nBins - 1, (int) (s.essFreq / binHz));
                const int k1 = jmin (nBins, (int) (16000.0f / binHz));
                double es = 0;
                for (int k = k0; k < k1; ++k) es += P[(size_t) k];
                const float esDb = powToDb ((float) es);
                if (analysing)
                {
                    const int a0 = (int) (5000 / binHz), a1 = jmin (nBins, (int) (10000 / binHz));
                    double e2 = 0; for (int k = a0; k < a1; ++k) e2 += P[(size_t) k];
                    if (frameE > 1.0e-9 && e2 / frameE > 0.35) sh.anSibFrames++;
                    storeMax (sh.anSibDb, powToDb ((float) e2));
                }
                const float over = esDb - s.essThrDb;
                const float target = over > 0 ? jmin (s.essMaxDb, over * 0.75f) : 0.0f;
                essGr += (target > essGr ? 0.85f : 0.3f) * (target - essGr);
                if (const float f = fadeTo (Shared::mEss, s.essOn); f > 0.0f && essGr > 0.01f)
                {
                    double meanP = es / jmax (1, k1 - k0);
                    const int kfade = jmax (1, k0 / 4);   // ~1/3 octave fade-in below the cutoff
                    for (int k = jmax (1, k0 - kfade); k < nBins; ++k)
                    {
                        float w = k < k0 ? (float) (k - (k0 - kfade)) / (float) kfade : 1.0f;
                        if (s.essMode == 1)   // spectral: louder bins get more reduction
                            w *= jlimit (0.2f, 1.6f, std::sqrt ((float) (P[(size_t) k] / jmax (1.0e-20, meanP))));
                        gain[(size_t) k] *= 1.0f + f * (dbToGain (-essGr * w) - 1.0f);
                    }
                    storeMax (sh.activity[Shared::mEss], essGr * f);
                }
            }

            // ---- Rebalance (harmonic/percussive + centre extraction) ----
            if (const float f = fadeTo (Shared::mRebal, s.rebalOn); f > 0.0f)
            {
                const float p = 1.0f + 3.0f * s.rebalSep * 0.01f;
                const float gV = dbToGain (s.rebalVox), gB = dbToGain (s.rebalBass), gP = dbToGain (s.rebalPerc), gO = dbToGain (s.rebalOther);
                const auto& Lsp = spec[0][(size_t) cs];
                const auto& Rsp = spec[nch > 1 ? 1 : 0][(size_t) cs];
                for (int k = 0; k < nBins; ++k)
                {
                    // harmonic: median over time
                    float t[hist];
                    for (int j = 0; j < hist; ++j) t[j] = pow[(size_t) j][(size_t) k];
                    std::nth_element (t, t + L, t + hist);
                    const float Hm = t[L];
                    // percussive: median over frequency (17 bins)
                    int cntF = 0;
                    for (int j = k - 8; j <= k + 8; ++j) if (j >= 0 && j < nBins) medBuf[(size_t) cntF++] = P[(size_t) j];
                    std::nth_element (medBuf.begin(), medBuf.begin() + cntF / 2, medBuf.begin() + cntF);
                    const float Pm = medBuf[(size_t) (cntF / 2)];
                    const float hp_ = std::pow (Hm + 1.0e-20f, p * 0.5f), pp = std::pow (Pm + 1.0e-20f, p * 0.5f);
                    const float mH = hp_ / (hp_ + pp), mP = 1.0f - mH;

                    // centre: same level and phase in L and R
                    const float lr = Lsp[(size_t) (2 * k)], li = Lsp[(size_t) (2 * k + 1)];
                    const float rr = Rsp[(size_t) (2 * k)], ri = Rsp[(size_t) (2 * k + 1)];
                    const float ml = std::sqrt (lr * lr + li * li), mr = std::sqrt (rr * rr + ri * ri);
                    float centre = 1.0f;
                    if (nch > 1)
                    {
                        const float pan = 1.0f - std::abs (ml - mr) / jmax (1.0e-12f, ml + mr);
                        const float cph = (lr * rr + li * ri) / jmax (1.0e-12f, ml * mr);
                        centre = std::pow (jmax (0.0f, pan * cph), 1.0f + 2.0f * s.rebalSep * 0.01f);
                    }
                    const float fr = (float) k * binHz;
                    const float low = fr < 120.0f ? 1.0f : fr < 300.0f ? 1.0f - (fr - 120.0f) / 180.0f : 0.0f;
                    const float voxBand = fr < 100.0f ? 0.0f : fr < 200.0f ? (fr - 100.0f) / 100.0f
                                        : fr < 8000.0f ? 1.0f : fr < 14000.0f ? 1.0f - (fr - 8000.0f) / 6000.0f : 0.0f;
                    const float mB = mH * low;
                    const float mV = mH * (1.0f - low) * centre * voxBand;
                    const float mPc = mP;
                    const float mO = jmax (0.0f, 1.0f - mB - mV - mPc);
                    const float g = mB * gB + mV * gV + mPc * gP + mO * gO;
                    rbGain[(size_t) k] += 0.6f * (g - rbGain[(size_t) k]);
                }
                applyGain (rbGain, f);
                storeMax (sh.activity[Shared::mRebal], jmax (std::abs (s.rebalVox), std::abs (s.rebalBass),
                                                             std::abs (s.rebalPerc), std::abs (s.rebalOther)) * f);
            }

            // ---- assistant: noise floor, signal level, reverb ----
            if (analysing)
            {
                sh.anFrames++;
                double nf = 0;
                for (int k = (int) (100 / binHz); k < jmin (nBins, (int) (10000 / binHz)); ++k) nf += noiseFloor (k);
                sh.anNoiseDb.store (powToDb ((float) nf));
                storeMax (sh.anSignalDb, frameDb);
                // reverb: how much energy is left 50 ms after a frame that dropped by > 10 dB
                const int D = jlimit (1, rvLen - 1, (int) std::round (0.05f / frameDt));
                double prev = 0;
                const auto& Pd = rvHist[(size_t) ((rvPos - D + rvLen) % rvLen)];
                for (int k = (int) (300 / binHz); k < jmin (nBins, (int) (5000 / binHz)); ++k) prev += Pd[(size_t) k];
                if (prev > frameE * 20.0 && prev > 1.0e-7)
                {
                    const float ratioDb = powToDb ((float) frameE) - powToDb ((float) prev);   // e.g. -20 dB after 50 ms
                    const float r = jlimit (0.0f, 1.0f, (ratioDb + 40.0f) / 30.0f);           // dry ~ -40 dB -> 0, wet ~ -10 dB -> 1
                    sh.anReverb.store (sh.anReverb.load() * 0.97f + r * 0.03f);
                }
            }

            // ---- synthesis ----
            for (int c = 0; c < nch; ++c)
            {
                const auto& src = spec[c][(size_t) cs];
                std::copy (src.begin(), src.end(), frame.begin());
                for (int k = 0; k < nBins; ++k)
                {
                    frame[(size_t) (2 * k)] *= gain[(size_t) k];
                    frame[(size_t) (2 * k + 1)] *= gain[(size_t) k];
                }
                fft->performRealOnlyInverseTransform (frame.data());
                auto& o = outRing[c];
                for (int n = 0; n < N; ++n)
                    o[(size_t) ((pos + n) % N)] += frame[(size_t) n] * window[(size_t) n] * 0.5f;
            }

            // ---- spectrogram column ----
            pushColumn (P, sh);
        }

        void updateNoiseFloor (const std::vector<float>& P)
        {
            // minimum statistics on a smoothed spectrum: minimum over the last U sub-windows of ~0.4 s,
            // so the floor follows the noise both down (at once) and up (after ~1.6 s).
            // Digital silence is ignored so it does not pull the floor to zero.
            double total = 0;
            for (int k = 1; k < nBins; k += 4) total += P[(size_t) k];
            if (total < 1.0e-14 || framesSeen * hop < N + L * hop) return;   // silence / start-up frames
            const int subFrames = jmax (4, (int) (0.5f / frameDt));
            if (! psdInit) { psdSm = P; psdInit = true; }
            for (int k = 0; k < nBins; ++k)
            {
                const float v = (psdSm[(size_t) k] += 0.2f * (P[(size_t) k] - psdSm[(size_t) k]));
                dnMinTmp[(size_t) k] = jmin (dnMinTmp[(size_t) k], v);
            }
            // noise is smooth across frequency, sustained tones are narrow peaks: per ~1/6-octave band take
            // the 25th percentile of the time-minimum, so a held note is not mistaken for noise
            for (int b = 0; b < numFloorBands; ++b)
            {
                const int k0 = floorEdge[(size_t) b], k1 = floorEdge[(size_t) b + 1];
                int cnt = 0;
                for (int k = k0; k < k1; ++k) medBuf2[(size_t) cnt++] = jmin (subMinAll[(size_t) k], dnMinTmp[(size_t) k]);
                const int q = cnt / 4;
                std::nth_element (medBuf2.begin(), medBuf2.begin() + q, medBuf2.begin() + cnt);
                floorBand[(size_t) b] = std::log (jmax (1.0e-20f, medBuf2[(size_t) q]));
            }
            for (int k = 0; k < nBins; ++k)
            {
                // linear interpolation (in dB) between band centres
                const float bandPos = binBandPos[(size_t) k];
                const int b0 = jlimit (0, numFloorBands - 1, (int) bandPos), b1 = jmin (numFloorBands - 1, b0 + 1);
                const float fr = jlimit (0.0f, 1.0f, bandPos - (float) b0);
                dnMin[(size_t) k] = std::exp (floorBand[(size_t) b0] + fr * (floorBand[(size_t) b1] - floorBand[(size_t) b0]));
            }
            if (++minFrames >= subFrames)
            {
                minFrames = 0;
                subSlot = (subSlot + 1) % numSub;
                std::copy (dnMinTmp.begin(), dnMinTmp.end(), subMin[(size_t) subSlot].begin());
                for (int k = 0; k < nBins; ++k)
                {
                    float m = 1.0e10f;
                    for (auto& sm : subMin) m = jmin (m, sm[(size_t) k]);
                    subMinAll[(size_t) k] = m;
                    dnMinTmp[(size_t) k] = 1.0e10f;
                }
            }
        }

        /** Estimated mean noise power in bin k (minimum statistics with bias compensation). */
        float noiseFloor (int k) const { return dnMin[(size_t) k] * minBias; }

        /** Moving average of a power spectrum across frequency; the width grows with frequency
            (~1/25 octave at 0 %, ~1/3 octave at 100 %). */
        void smoothPower (const std::vector<float>& src, std::vector<float>& dst, float amount)
        {
            const float oct = 0.04f + 0.3f * amount * 0.01f;
            const float fac = std::pow (2.0f, oct * 0.5f) - 1.0f;
            if ((int) prefix.size() != nBins + 1) prefix.resize ((size_t) nBins + 1);
            if ((int) dst.size() != nBins) dst.resize ((size_t) nBins);
            prefix[0] = 0.0;
            for (int k = 0; k < nBins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + src[(size_t) k];
            dst[0] = src[0];
            for (int k = 1; k < nBins; ++k)
            {
                const int w = jmin (64, (int) ((float) k * fac));
                const int a = jmax (1, k - w), b = jmin (nBins - 1, k + w);
                dst[(size_t) k] = (float) ((prefix[(size_t) b + 1] - prefix[(size_t) a]) / (b - a + 1));
            }
        }

        void pushColumn (const std::vector<float>& P, Shared& sh)
        {
            int s1, n1, s2, n2;
            sh.fifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 + n2 < 1) return;
            const int idx = n1 > 0 ? s1 : s2;
            auto& ci = sh.colsIn[(size_t) idx];
            auto& co = sh.colsOut[(size_t) idx];
            for (int b = 0; b < Shared::numBands; ++b)
            {
                const int k0 = bandEdge[(size_t) b], k1 = jmax (k0 + 1, bandEdge[(size_t) b + 1]);
                float mi = 0, mo = 0;
                for (int k = k0; k < k1 && k < nBins; ++k)
                {
                    mi = jmax (mi, P[(size_t) k]);
                    mo = jmax (mo, P[(size_t) k] * gain[(size_t) k] * gain[(size_t) k]);
                }
                ci[(size_t) b] = powToDb (mi);
                co[(size_t) b] = powToDb (mo);
            }
            sh.fifo.finishedWrite (1);
        }

        double sr = 44100.0;
        int channels = 2, N = 2048, hop = 512, nBins = 1025, pos = 0, hopCount = 0, framesSeen = 0;
        float frameDt = 0.01f, powerNorm = 1.0f;
        bool hasSide = false;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window, frame;
        std::vector<float> inRing[3], outRing[2];
        std::array<std::vector<float>, hist> spec[3];
        std::array<std::vector<float>, hist> pow, refPow;
        std::vector<float> gain, tmpG, powSm, powSm2;
        std::vector<double> prefix;
        int numFloorBands = 0;
        std::vector<int> floorEdge;
        std::vector<float> floorBand, binBandPos, medBuf2;
        Settings settings;

        // module state
        std::vector<float> dnGain, dnSmooth, dnMin, dnMinTmp, dnProfile, psdSm;
        bool psdInit = false;
        static constexpr int numSub = 4;
        std::array<std::vector<float>, numSub> subMin;
        std::vector<float> subMinAll;
        int subSlot = 0;
    public:
        float minBias = 4.0f;
    private:
        std::vector<double> learnAcc;
        int learnFrames = 0, minFrames = 0;
        bool profileValid = false;
        std::vector<float> isoGain, isoFloor, isoSmooth;
        static constexpr int rvLen = 32;
        std::array<std::vector<float>, rvLen> rvHist;
        int rvPos = 0;
        std::vector<float> rvGain, blGain, blNum, blDen, rbGain;
        std::vector<float> medBuf;
        std::vector<float> noiseHistDb;
        float breathGain = 1.0f, voicePeak = 1.0e-6f, essGr = 0.0f;
        float fade[Shared::numModules] {};
        std::array<int, Shared::numBands + 1> bandEdge {};
    };
}
