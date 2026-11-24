#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include <array>

// Dovregubben delay engine.
//
// Per channel:  in ─► (+) ─► delay line ─► read (mod / reverse) ─► EQ ─► mode colour ─► diffusion ─► pitch/ghost ─► wet
//                      ▲                                                                                          │
//                      └──────────────────────────────── feedback (own or other channel) ◄───────────────────────┘
// Everything after the read is inside the loop, so every repeat is filtered, saturated, diffused and shifted again.
namespace dovre
{
    enum Mode  { Tape, HiFi, BBD, Digital, Pitch, Reverse, Ghost, numModes };
    enum Style { Single, Dual, PingPong, numStyles };

    inline const juce::StringArray modeNames  { "Tape", "HiFi", "BBD", "Digital", "Pitch", "Reverse", "Ghost" };
    inline const juce::StringArray styleNames { "Single", "Dual", "Ping-Pong" };

    inline const char* modeDescription (int m)
    {
        switch (m)
        {
            case Tape:    return "Båndekko: varm metning, wow og flutter, mørkere for hver repetisjon. Tid-endringer gir tape-pitch.";
            case HiFi:    return "Moderne, ren og lys delay med litt mykhet i sløyfen. AGE gir varme.";
            case BBD:     return "Analog «bucket brigade»-delay: smal båndbredde, mørk og myk. AGE gjør den mørkere og mer skitten.";
            case Digital: return "Helt ren digital delay. AGE gir tidlig digital lyd med lavere bitdybde og samplerate.";
            case Pitch:   return "Pitch-shifter i sløyfen. DETUNE gir dobling (L opp, R ned), PITCH stiger/synker for hver repetisjon.";
            case Reverse: return "Spiller bitene baklengs. DELAY bestemmer lengden på hver bit.";
            case Ghost:   return "Frekvensskift og diffusjon i sløyfen: spøkelsesaktige, svevende ekko. PITCH/DETUNE styrer skiftet.";
            default:      return "";
        }
    }

    // Tempo-synced note values, in quarter notes.
    inline const juce::StringArray noteNames { "1/64", "1/32T", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8",
                                               "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1/1" };
    inline constexpr float noteBeats[] { 0.0625f, 1.0f / 12.0f, 0.125f, 1.0f / 6.0f, 0.1875f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f,
                                         2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 8.0f / 3.0f, 3.0f, 4.0f };

    inline constexpr float maxDelayMs = 4000.0f;
    inline constexpr float pitchWindowS = 0.04f;       // pitch shifter grain for transposing; half of it is latency, compensated in the line
    inline constexpr float detuneWindowS = 0.022f;     // shorter grain when only detuning (doubling): tighter timing

    struct Settings
    {
        int mode = Tape, style = Dual;
        float delayMsL = 375.0f, delayMsR = 375.0f;
        float feedback = 0.3f;        // 0..1
        float mix = 0.25f;            // 0..1
        float width = 1.0f;           // 0..1
        float age = 0.25f;            // 0..1
        float diffusion = 0.0f;       // 0..1
        float diffSize = 0.5f;        // 0..1
        float modRate = 0.5f;         // Hz
        float modDepth = 0.2f;        // 0..1
        float lowCutHz = 100.0f, highCutHz = 8000.0f;
        float pitchSt = 0.0f, detuneCents = 0.0f;
        float duck = 0.0f;            // 0..1
    };

    inline float delayRatioR (float ratioPct) { return ratioPct * 0.01f; }

    //==========================================================================
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void set (double fs, double f, double q, bool highpass)
        {
            f = juce::jlimit (5.0, fs * 0.45, f);
            const double w = juce::MathConstants<double>::twoPi * f / fs;
            const double cw = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
            const double bb0 = highpass ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5;
            const double bb1 = highpass ? -(1.0 + cw) : (1.0 - cw);
            b0 = (float) (bb0 / a0); b1 = (float) (bb1 / a0); b2 = b0;
            a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - al) / a0);
        }
        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0; }
    };

    /** Power-of-two circular buffer with fractional (Hermite) reads. */
    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void prepare (int minSize)
        {
            int n = 1;
            while (n < minSize) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) noexcept { buf[(size_t) (w & mask)] = x; ++w; }

        /** d = samples back from the most recent pushed sample (d >= 1). */
        float read (float d) const noexcept
        {
            d = juce::jlimit (3.0f, (float) (mask - 4), d);
            const float rp = (float) (w - 1) - d + 1.0f;   // position of the wanted sample
            const float fl = std::floor (rp);
            const int i = (int) fl;
            const float f = rp - fl;
            const float x0 = buf[(size_t) ((i - 1) & mask)], x1 = buf[(size_t) (i & mask)];
            const float x2 = buf[(size_t) ((i + 1) & mask)], x3 = buf[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (x2 - x0);
            const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
            const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            return ((c3 * f + c2) * f + c1) * f + x1;
        }
    };

    /** Schroeder allpass with a smoothly variable (linear-interpolated) length. */
    struct Allpass
    {
        DelayLine line;
        float len = 100.0f;
        float process (float x, float targetLen, float g) noexcept
        {
            len += 0.0005f * (targetLen - len);
            const float d = line.read (len);
            const float v = x + g * d;
            line.push (v);
            return d - g * v;
        }
    };

    /** Two crossfading grains (Hann windows, half a grain apart). */
    struct PitchShifter
    {
        DelayLine line;
        float W = 2000.0f, phase = 0.0f;

        void prepare (double fs) { W = (float) (fs * pitchWindowS); line.prepare ((int) (W * 2.0f) + 8); phase = 0.0f; }
        void reset() { line.reset(); phase = 0.0f; }

        float process (float x, float ratio) noexcept
        {
            line.push (x);
            phase += (1.0f - ratio) / W;
            phase -= std::floor (phase);
            float p2 = phase + 0.5f;
            p2 -= std::floor (p2);
            const float g1 = std::sin (juce::MathConstants<float>::pi * phase);
            const float g2 = std::sin (juce::MathConstants<float>::pi * p2);
            return line.read (1.0f + phase * W) * g1 * g1 + line.read (1.0f + p2 * W) * g2 * g2;
        }
    };

    /** Frequency shifter: 90° allpass network (Niemitalo) + quadrature oscillator. */
    struct FreqShifter
    {
        struct AP
        {
            float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            float process (float x) noexcept { const float y = a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
        };
        std::array<AP, 4> A, B;
        float bDelay = 0.0f;
        double ph = 0.0;

        FreqShifter()
        {
            const float ca[] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            const float cb[] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int i = 0; i < 4; ++i) { A[(size_t) i].a = ca[i] * ca[i]; B[(size_t) i].a = cb[i] * cb[i]; }
        }
        void reset() { for (auto& s : A) s = { s.a }; for (auto& s : B) s = { s.a }; bDelay = 0.0f; ph = 0.0; }

        float process (float x, float shiftHz, double fs) noexcept
        {
            float a = x, b = x;
            for (auto& s : A) a = s.process (a);
            for (auto& s : B) b = s.process (b);
            const float im = bDelay;
            bDelay = b;
            ph += juce::MathConstants<double>::twoPi * (double) shiftHz / fs;
            if (ph > juce::MathConstants<double>::twoPi) ph -= juce::MathConstants<double>::twoPi;
            if (ph < 0.0) ph += juce::MathConstants<double>::twoPi;
            return a * (float) std::cos (ph) - im * (float) std::sin (ph);
        }
    };

    //==========================================================================
    class Engine
    {
    public:
        void prepare (double sampleRate)
        {
            fs = sampleRate;
            const int lineLen = (int) (fs * (2.0 * maxDelayMs * 0.001 + 0.1)) + 16;  // reverse reads up to 2× delay
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.line.prepare (lineLen);
                for (auto& ap : ch.ap) ap.line.prepare ((int) (fs * 0.06) + 8);
                ch.shifter.prepare (fs);
            }
            reset();
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.line.reset();
                for (auto& ap : ch.ap) ap.line.reset();
                ch.shifter.reset();
                ch.fshift.reset();
                for (auto* f : { &ch.hp, &ch.lp, &ch.lp2, &ch.lp3, &ch.dcHp }) f->reset();
                ch.y = 0.0f;
                ch.lfo = c * 0.25f;
                ch.rnd = ch.rndTarget = 0.0f;
                ch.revPhase = c * 0.37f;
                ch.hold = 0.0f; ch.holdCount = 0.0f;
                ch.delay = -1.0f;
            }
            env = 0.0f; duckGain = 1.0f;
            shiftMix = 0.0f;
            sm = {};
            smValid = false;
            counter = 0;
        }

        /** buf: numCh (1 or 2) channels, processed in place. */
        void process (float* const* io, int numIn, int numOut, int n, const Settings& s)
        {
            if (! smValid) { sm = s; smValid = true; updateFilters(); }

            const float tDelay = s.mode == Tape ? 0.30f : (s.mode == BBD ? 0.18f : 0.08f);
            const float aDelay = 1.0f - std::exp (-1.0f / (float) (fs * tDelay));
            const float aParam = 1.0f - std::exp (-1.0f / (float) (fs * 0.03));
            const float aEnvAtt = 1.0f - std::exp (-1.0f / (float) (fs * 0.002));
            const float aEnvRel = 1.0f - std::exp (-1.0f / (float) (fs * 0.25));
            const float aDuck = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
            const float aShift = 1.0f - std::exp (-1.0f / (float) (fs * 0.05));

            const bool stereoOut = numOut > 1;
            const int style = stereoOut ? s.style : Single;
            const bool pitchMode = s.mode == Pitch, ghost = s.mode == Ghost, reverse = s.mode == Reverse;

            const float fsMs = (float) fs * 0.001f;
            float targetL = s.delayMsL * fsMs;
            float targetR = (style == Single ? s.delayMsL : s.delayMsR) * fsMs;

            // pitch shifting ratios: L up, R down by detune
            const float centsL = s.pitchSt * 100.0f + s.detuneCents;
            const float centsR = s.pitchSt * 100.0f - s.detuneCents;
            const float ratioL = std::exp2 (centsL / 1200.0f), ratioR = std::exp2 (centsR / 1200.0f);
            const bool shifting = pitchMode && (std::abs (centsL) > 0.3f || std::abs (centsR) > 0.3f);
            const float grain = (float) fs * (std::abs (s.pitchSt) < 0.5f ? detuneWindowS : pitchWindowS);
            for (auto& ch : chans) ch.shifter.W = grain;
            const float shiftLatency = grain * 0.5f;

            // ghost: frequency shift in Hz, opposite directions L/R
            const float ghostHz = 0.6f + s.pitchSt * 6.0f + s.detuneCents * 0.1f;

            // modulation depth in samples (+ tape wow/flutter)
            const float modSamples = s.modDepth * s.modDepth * 10.0f * fsMs;
            const float wowSamples = s.mode == Tape ? s.age * 1.4f * fsMs : 0.0f;
            const float flutSamples = s.mode == Tape ? s.age * 0.07f * fsMs : (s.mode == BBD ? s.age * 0.03f * fsMs : 0.0f);
            const float lfoInc = s.modRate / (float) fs;
            const float wowInc = 0.55f / (float) fs, flutInc = 6.3f / (float) fs;

            // character
            float drive = 1.0f;
            switch (s.mode)
            {
                case Tape:  drive = 1.0f + 3.5f * s.age; break;
                case HiFi:  drive = 1.0f + 1.2f * s.age; break;
                case BBD:   drive = 1.0f + 2.0f * s.age; break;
                case Pitch: case Reverse: case Ghost: drive = 1.0f + 1.0f * s.age; break;
                default: break;
            }
            const float noiseAmt = s.mode == Tape ? s.age * s.age * 0.0012f : (s.mode == BBD ? s.age * s.age * 0.0009f : 0.0f);
            const bool crush = s.mode == Digital && s.age > 0.005f;
            const float levels = std::exp2 (16.0f - 11.0f * s.age);
            const float holdLen = 1.0f + s.age * 5.0f * (float) (fs / 48000.0);

            const float diffTarget = ghost ? juce::jmax (0.4f, s.diffusion) : s.diffusion;
            const float apScale = (0.15f + 1.6f * s.diffSize) * fsMs;
            static constexpr float apMs[2][4] { { 4.7f, 7.3f, 11.1f, 16.9f }, { 5.3f, 8.1f, 12.3f, 15.7f } };

            for (int i = 0; i < n; ++i)
            {
                if ((counter++ & 31) == 0)
                {
                    sm.lowCutHz += 0.25f * (s.lowCutHz - sm.lowCutHz);
                    sm.highCutHz += 0.25f * (s.highCutHz - sm.highCutHz);
                    sm.age += 0.25f * (s.age - sm.age);
                    curMode = s.mode;
                    updateFilters();
                }
                sm.mix += aParam * (s.mix - sm.mix);
                sm.feedback += aParam * (s.feedback - sm.feedback);
                sm.width += aParam * (s.width - sm.width);
                sm.diffusion += aParam * (diffTarget - sm.diffusion);
                shiftMix += aShift * ((shifting ? 1.0f : 0.0f) - shiftMix);

                const float inL = io[0][i];
                const float inR = numIn > 1 ? io[1][i] : inL;

                // ducking envelope on the dry input
                const float lvl = juce::jmax (std::abs (inL), std::abs (inR));
                env += (lvl > env ? aEnvAtt : aEnvRel) * (lvl - env);
                const float envDb = 20.0f * std::log10 (env + 1.0e-6f);
                const float duckTarget = 1.0f - 0.93f * s.duck * juce::jlimit (0.0f, 1.0f, (envDb + 48.0f) / 36.0f);
                duckGain += aDuck * (duckTarget - duckGain);

                float wet[2] {};
                const int nCh = style == Single ? 1 : 2;
                for (int c = 0; c < nCh; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    float target = c == 0 ? targetL : targetR;
                    if (pitchMode) target -= shiftLatency * shiftMix;
                    target = juce::jmax (2.0f, target);
                    if (ch.delay < 0.0f) ch.delay = target;
                    ch.delay += aDelay * (target - ch.delay);

                    // LFO + smoothed random + tape wow/flutter
                    ch.lfo += lfoInc; ch.lfo -= std::floor (ch.lfo);
                    ch.wow += wowInc; ch.wow -= std::floor (ch.wow);
                    ch.flut += flutInc * (c == 0 ? 1.0f : 1.07f); ch.flut -= std::floor (ch.flut);
                    if (--ch.rndCount <= 0.0f)
                    {
                        ch.rndTarget = random.nextFloat() * 2.0f - 1.0f;
                        ch.rndCount = (float) fs / juce::jmax (0.2f, s.modRate * 2.0f);
                    }
                    ch.rnd += (s.modRate * 4.0f / (float) fs) * (ch.rndTarget - ch.rnd);
                    const float twoPi = juce::MathConstants<float>::twoPi;
                    const float mod = modSamples * (0.75f * std::sin (twoPi * ch.lfo) + 0.25f * ch.rnd)
                                    + wowSamples * (0.7f * std::sin (twoPi * ch.wow) + 0.3f * ch.rnd)
                                    + flutSamples * std::sin (twoPi * ch.flut);

                    float y;
                    if (reverse)
                    {
                        const float S = juce::jmax (64.0f, ch.delay);
                        ch.revPhase += 1.0f / S;
                        ch.revPhase -= std::floor (ch.revPhase);
                        float p2 = ch.revPhase + 0.5f; p2 -= std::floor (p2);
                        const float g1 = std::sin (juce::MathConstants<float>::pi * ch.revPhase);
                        const float g2 = std::sin (juce::MathConstants<float>::pi * p2);
                        y = ch.line.read (2.0f + 2.0f * S * ch.revPhase + mod * 0.2f) * g1 * g1
                          + ch.line.read (2.0f + 2.0f * S * p2 + mod * 0.2f) * g2 * g2;
                    }
                    else
                    {
                        y = ch.line.read (juce::jmax (2.0f, ch.delay + mod));
                    }

                    // EQ in the loop
                    y = ch.hp.process (y);
                    y = ch.lp.process (y);
                    if (curMode != Digital) y = ch.lp2.process (y);
                    if (curMode == BBD) y = ch.lp3.process (y);

                    // colour
                    if (curMode == Digital)
                    {
                        if (crush)
                        {
                            ch.holdCount -= 1.0f;
                            if (ch.holdCount <= 0.0f) { ch.hold = std::round (y * levels) / levels; ch.holdCount += holdLen; }
                            y = ch.hold;
                        }
                        y = juce::jlimit (-4.0f, 4.0f, y);
                    }
                    else
                    {
                        y = std::tanh (y * drive) / drive;
                    }
                    if (noiseAmt > 0.0f)
                        y += noiseAmt * (random.nextFloat() * 2.0f - 1.0f);

                    // diffusion
                    if (sm.diffusion > 0.001f)
                    {
                        const float g = 0.45f + 0.25f * sm.diffusion;
                        float d = y;
                        for (int k = 0; k < 4; ++k)
                            d = ch.ap[(size_t) k].process (d, apMs[c][k] * apScale, g);
                        y += sm.diffusion * (d - y);
                    }

                    // pitch / ghost
                    if (pitchMode && shiftMix > 0.0005f)
                    {
                        const float shifted = ch.shifter.process (y, c == 0 ? ratioL : ratioR);
                        y = shiftMix >= 0.9995f ? shifted : y + shiftMix * (shifted - y);
                    }
                    else if (pitchMode)
                    {
                        ch.shifter.process (y, 1.0f);
                        ch.shifter.phase = 0.0f;   // restart cleanly the next time detune is used
                    }
                    if (ghost)
                        y = ch.fshift.process (y, c == 0 ? ghostHz : -ghostHz, fs);

                    y = ch.dcHp.process (y);
                    ch.y = y;
                    wet[c] = y;
                }

                // write the lines
                const float fb = sm.feedback;
                if (style == Single)
                {
                    chans[0].line.push (0.5f * (inL + inR) + fb * chans[0].y);
                    wet[1] = wet[0];
                }
                else if (style == Dual)
                {
                    chans[0].line.push (inL + fb * chans[0].y);
                    chans[1].line.push (inR + fb * chans[1].y);
                }
                else
                {
                    chans[0].line.push (0.5f * (inL + inR) + fb * chans[1].y);
                    chans[1].line.push (fb * chans[0].y);
                }

                // width, ducking, mix
                const float m = 0.5f * (wet[0] + wet[1]), sd = 0.5f * (wet[0] - wet[1]) * sm.width;
                const float wl = (m + sd) * duckGain, wr = (m - sd) * duckGain;
                const float dryG = juce::jmin (1.0f, 2.0f * (1.0f - sm.mix)), wetG = juce::jmin (1.0f, 2.0f * sm.mix);

                if (stereoOut)
                {
                    io[0][i] = inL * dryG + wl * wetG;
                    io[1][i] = inR * dryG + wr * wetG;
                }
                else
                {
                    io[0][i] = inL * dryG + 0.5f * (wl + wr) * wetG;
                }
            }
        }

    private:
        void updateFilters()
        {
            const float a = sm.age;
            float modeLp = 20000.0f;
            switch (curMode)
            {
                case Tape:    modeLp = 15000.0f - 10500.0f * a; break;
                case HiFi:    modeLp = 18000.0f - 6000.0f * a; break;
                case BBD:     modeLp = 8500.0f - 6000.0f * a; break;
                case Pitch:
                case Reverse: modeLp = 19000.0f - 12000.0f * a; break;
                case Ghost:   modeLp = 7500.0f - 3500.0f * a; break;
                default: break;
            }
            for (auto& ch : chans)
            {
                ch.hp.set (fs, sm.lowCutHz, 0.707, true);
                ch.lp.set (fs, sm.highCutHz, 0.707, false);
                ch.lp2.set (fs, modeLp, 0.6, false);
                ch.lp3.set (fs, modeLp * 1.15f, 0.9, false);
                ch.dcHp.set (fs, 12.0, 0.6, true);
            }
        }

        struct Channel
        {
            DelayLine line;
            std::array<Allpass, 4> ap;
            PitchShifter shifter;
            FreqShifter fshift;
            Biquad hp, lp, lp2, lp3, dcHp;
            float y = 0.0f, delay = -1.0f;
            float lfo = 0.0f, wow = 0.0f, flut = 0.0f, rnd = 0.0f, rndTarget = 0.0f, rndCount = 0.0f;
            float revPhase = 0.0f, hold = 0.0f, holdCount = 0.0f;
        };

        double fs = 48000.0;
        std::array<Channel, 2> chans;
        Settings sm;            // smoothed copy
        bool smValid = false;
        int curMode = Tape;
        float env = 0.0f, duckGain = 1.0f, shiftMix = 0.0f;
        unsigned counter = 0;
        juce::Random random { 0x5b0d };
    };
}
