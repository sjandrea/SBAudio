#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include <algorithm>
#include <array>
#include <functional>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"
#include "Wavetables.h"

/*  Nordlys synth engine.

    Patch  = 4 layers (A–D) that play together on every note.
    Layer  = oscillator (Analog / Wavetable / FM / Granular / Noise, up to 7 unison voices)
             -> drive -> multimode filter (TPT state variable) -> amp envelope -> level/pan.
             Each layer has its own filter envelope and two LFOs.
    Mod    = 8 matrix slots: source × via × amount -> destination, for all layers or one layer.
    Orb    = global X/Y macro with optional automatic circling motion, usable as mod sources.
    Arp    = tempo-synced arpeggiator in front of the voices.
    FX     = chorus -> ping-pong delay -> reverb (shared aura::Engine) -> master.

    Modulation runs at control rate (every 16 samples); oscillators, filters and envelopes
    run per sample. MIDI and arpeggiator events are sample accurate.
*/
namespace nordlys
{
    //==========================================================================
    struct EnvSettings { float a = 5.0f, d = 600.0f, s = 0.8f, r = 400.0f; }; // ms, ms, 0..1, ms

    struct LayerSettings
    {
        bool on = false;
        float level = 0.8f, pan = 0.0f, pitch = 0.0f; // pitch in semitones (oct + coarse + fine)
        int osc = oscAnalog, wave = 0;
        float shape = 0.66f, sym = 0.5f;
        int uni = 1;
        float detune = 15.0f, spread = 0.5f;
        float gsizeMs = 120.0f, gdens = 6.0f, gspray = 0.2f, gpitch = 8.0f, gshim = 0.0f;
        int ftype = fltLP24;
        float cutoff = 8000.0f, reso = 0.1f, fenv = 0.0f, fkey = 0.5f, drive = 0.0f;
        EnvSettings amp, filt { 5.0f, 800.0f, 0.3f, 600.0f };
        float lfoRate[2] { 4.0f, 0.3f };
        int lfoShape[2] { lfoSine, lfoTriangle };
        float lfoFade[2] { 0.0f, 0.0f };
    };

    struct SlotSettings { int src = srcOff, via = srcOff, dst = dstOff, layer = 0; float amt = 0.0f; };

    struct Settings
    {
        std::array<LayerSettings, numLayers> layers;
        std::array<SlotSettings, numSlots> slots;
        float masterDb = -6.0f;
        int polyMode = modePoly;
        float glideMs = 0.0f, bendRange = 2.0f, velSens = 0.6f;
        float orbX = 0.0f, orbY = 0.0f, orbRate = 0.0f, orbDepth = 0.0f;
        bool arpOn = false;
        int arpMode = arpUp, arpRate = 5, arpOct = 1;
        float arpGate = 0.6f;
        float choMix = 0.0f, choRate = 0.4f, choDepth = 0.4f;
        float dlyMix = 0.0f, dlyFb = 0.35f, dlyTone = 6000.0f;
        int dlyTime = 7;
        float revMix = 0.25f, revDecay = 3.5f, revTone = 0.0f;
        int revType = 3;
    };

    struct HostTime { double bpm = 120.0; double ppq = 0.0; bool playing = false; bool hasPpq = false; };

    //==========================================================================
    struct Rng
    {
        uint32_t s = 0x9e3779b9u;
        void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; }
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
        float bi()  { return uni() * 2.0f - 1.0f; }                             // -1..1
    };

    inline float equalPowerL (float pan) { return std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2; }
    inline float equalPowerR (float pan) { return std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * juce::MathConstants<float>::sqrt2; }

    //==========================================================================
    class Envelope
    {
    public:
        enum Stage { Idle, Attack, Decay, Sustain, Release };

        void set (const EnvSettings& e, double fs)
        {
            const double f = fs;
            aInc  = (float) (1.0 / (std::max (0.5, (double) e.a) * 0.001 * f));
            dCoef = (float) std::exp (-6.9 / (std::max (1.0, (double) e.d) * 0.001 * f));
            rCoef = (float) std::exp (-9.2 / (std::max (1.0, (double) e.r) * 0.001 * f));
            sus   = juce::jlimit (0.0f, 1.0f, e.s);
        }

        void noteOn()  { stage = Attack; }   // starts from the current level: no clicks on retrigger
        void noteOff() { if (stage != Idle) stage = Release; }
        void kill()    { stage = Idle; level = 0.0f; }
        bool isIdle() const { return stage == Idle; }
        bool isReleasing() const { return stage == Release; }
        float getLevel() const { return level; }

        float next()
        {
            switch (stage)
            {
                case Attack:
                    level += aInc;
                    if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                    break;
                case Decay:
                    level = sus + (level - sus) * dCoef;
                    if (std::abs (level - sus) < 1.0e-4f) stage = Sustain;
                    break;
                case Sustain:
                    level += (sus - level) * 0.002f;
                    break;
                case Release:
                    level *= rCoef;
                    if (level < 1.0e-5f) { level = 0.0f; stage = Idle; }
                    break;
                case Idle:
                default: break;
            }
            return level;
        }

    private:
        Stage stage = Idle;
        float level = 0.0f, aInc = 0.01f, dCoef = 0.999f, rCoef = 0.999f, sus = 1.0f;
    };

    //==========================================================================
    class Lfo
    {
    public:
        void start (int shape, float fadeMs, Rng& rng)
        {
            phase = (shape == lfoSH || shape == lfoDrift) ? rng.uni() : 0.0f;
            held = rng.bi(); from = held; to = rng.bi();
            fade = fadeMs > 1.0f ? 0.0f : 1.0f;
            value = compute (shape);
        }

        /** Current value (-1..1, faded in) – call advance() after reading. */
        float get() const { return value * fade; }

        void advance (float rateHz, int shape, float fadeMs, int samples, double fs, Rng& rng)
        {
            phase += rateHz * (float) samples / (float) fs;
            if (phase >= 1.0f)
            {
                phase -= std::floor (phase);
                held = rng.bi();
                from = to; to = rng.bi();
            }
            if (fade < 1.0f)
                fade = fadeMs > 1.0f ? std::min (1.0f, fade + (float) samples / (fadeMs * 0.001f * (float) fs)) : 1.0f;
            value = compute (shape);
        }

    private:
        float compute (int shape) const
        {
            using MC = juce::MathConstants<float>;
            switch (shape)
            {
                case lfoSine:     return std::sin (phase * MC::twoPi);
                case lfoTriangle: return phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f;
                case lfoSaw:      return 1.0f - 2.0f * phase;
                case lfoSquare:   return phase < 0.5f ? 1.0f : -1.0f;
                case lfoSH:       return held;
                case lfoDrift:
                default:          return from + (to - from) * (0.5f - 0.5f * std::cos (phase * MC::pi));
            }
        }

        float phase = 0.0f, value = 0.0f, held = 0.0f, from = 0.0f, to = 0.0f, fade = 1.0f;
    };

    //==========================================================================
    /** Topology-preserving-transform state variable filter (Zavalishin / Simper). */
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() { ic1 = ic2 = 0.0f; }

        struct Coefs { float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f; };
        static Coefs make (float g, float k)
        {
            Coefs c;
            c.k = k;
            c.a1 = 1.0f / (1.0f + g * (g + k));
            c.a2 = g * c.a1;
            c.a3 = g * c.a2;
            return c;
        }

        /** Returns lp, bp, hp through references. */
        inline void tick (float v0, const Coefs& c, float& lp, float& bp, float& hp)
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - c.k * v1 - v2;
        }
    };

    //==========================================================================
    struct Grain
    {
        bool active = false;
        float phase = 0.0f, ratio = 1.0f, inc = 0.0f, t = 0.0f, tInc = 0.0f, gl = 1.0f, gr = 1.0f, frac = 0.0f;
        const float* tA = nullptr;
        const float* tB = nullptr;
    };

    struct LayerVoice
    {
        bool active = false;
        float phase[maxUnison] {}, modPhase[maxUnison] {};
        float inc[maxUnison] {}, uniL[maxUnison] {}, uniR[maxUnison] {};
        int uni = 1;
        const float* tA = nullptr;
        const float* tB = nullptr;
        float frac = 0.0f, warp = 0.5f, fmRatio = 1.0f, fmIndex = 0.0f;
        // granular
        Grain grains[maxGrains];
        float grainTimer = 0.0f, inc0 = 0.0f;
        // noise
        float noiseLp[2] {}, noiseCoef = 1.0f;
        // filter
        Svf svf[2][2];
        Svf::Coefs c1, c2;
        float driveGain = 1.0f, driveComp = 1.0f;
        // amp
        float gainL = 0.0f, gainR = 0.0f, targetL = 0.0f, targetR = 0.0f;
        Envelope amp, filt;
        Lfo lfo[2];
        float mod[numDests] {};
    };

    struct Voice
    {
        bool active = false, keyDown = false, fromArp = false;
        int note = 60;
        float velocity = 1.0f, random = 0.0f;
        float semis = 60.0f, targetSemis = 60.0f, glideStep = 0.0f;
        uint64_t age = 0;
        Rng rng;
        LayerVoice layers[numLayers];
    };

    //==========================================================================
    class StereoDelay
    {
    public:
        void prepare (double sampleRate)
        {
            fs = sampleRate;
            int size = 1;
            while (size < (int) (fs * 4.2)) size <<= 1;
            bufL.assign ((size_t) size, 0.0f);
            bufR.assign ((size_t) size, 0.0f);
            mask = size - 1;
            w = 0; lpL = lpR = 0.0f; time = -1.0f;
        }

        void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); lpL = lpR = 0.0f; }

        void process (float* L, float* R, int n, float targetSamples, float fb, float mix, float toneHz)
        {
            targetSamples = juce::jlimit (16.0f, (float) mask - 4.0f, targetSamples);
            if (time < 0.0f) time = targetSamples;
            const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / (float) fs);
            for (int i = 0; i < n; ++i)
            {
                time += (targetSamples - time) * 0.0004f;
                const float rp = (float) w - time;
                const int i0 = (int) std::floor (rp);
                const float fr = rp - (float) i0;
                const int a = i0 & mask, b = (i0 + 1) & mask;
                const float dl = bufL[(size_t) a] + fr * (bufL[(size_t) b] - bufL[(size_t) a]);
                const float dr = bufR[(size_t) a] + fr * (bufR[(size_t) b] - bufR[(size_t) a]);
                lpL += c * (dl - lpL);
                lpR += c * (dr - lpR);
                const float in = 0.5f * (L[i] + R[i]);
                bufL[(size_t) w] = in + fb * lpR;
                bufR[(size_t) w] = fb * lpL;
                w = (w + 1) & mask;
                L[i] += mix * lpL;
                R[i] += mix * lpR;
            }
        }

    private:
        double fs = 44100.0;
        std::vector<float> bufL, bufR;
        int mask = 0, w = 0;
        float lpL = 0.0f, lpR = 0.0f, time = -1.0f;
    };

    //==========================================================================
    class Arpeggiator
    {
    public:
        struct Held { int note; float vel; int order; };

        std::function<void (int, float)> noteOn;
        std::function<void (int)> noteOff;

        void reset() { held.clear(); releaseAll(); running = false; counter = 0; }
        bool hasNotes() const { return ! held.empty(); }

        void add (int note, float vel, bool hostPlaying, double stepBeats)
        {
            for (auto& h : held) if (h.note == note) return;
            const bool wasEmpty = held.empty();
            held.push_back ({ note, vel, counter++ });
            if (wasEmpty)
            {
                step = 0;
                running = true;
                if (hostPlaying) nextStep = std::ceil (beat / stepBeats - 1.0e-6) * stepBeats;
                else             { beat = 0.0; nextStep = 0.0; }
                offAt = -1.0;
            }
        }

        void remove (int note)
        {
            held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
            if (held.empty()) { releaseAll(); running = false; }
        }

        void stop() { held.clear(); releaseAll(); running = false; }

        /** Re-align to the host transport (called at block start while it plays). */
        void sync (double ppq, double stepBeats)
        {
            beat = ppq;
            if (running && (nextStep > beat + stepBeats * 1.01 || nextStep < beat - stepBeats * 0.01))
            {
                releaseAll();
                nextStep = std::ceil (beat / stepBeats - 1.0e-6) * stepBeats;
            }
        }

        /** Samples until the next arp event (or a large number). */
        int samplesUntilNext (double beatsPerSample) const
        {
            if (! running) return 1 << 30;
            double t = nextStep;
            if (! playing.empty() && offAt >= 0.0) t = std::min (t, offAt);
            const double d = (t - beat) / beatsPerSample;
            return d <= 0.5 ? 0 : (int) std::ceil (d - 0.5);
        }

        void advance (int samples, double beatsPerSample) { beat += samples * beatsPerSample; }

        void fire (const Settings& s, double beatsPerSample, Rng& rng)
        {
            const double stepBeats = arpRateBeats[juce::jlimit (0, 7, s.arpRate)];
            const double eps = beatsPerSample * 0.5;
            if (! playing.empty() && offAt >= 0.0 && offAt <= beat + eps)
            {
                releaseAll();
                offAt = -1.0;
            }
            if (running && nextStep <= beat + eps)
            {
                releaseAll();
                playStep (s, rng);
                offAt = nextStep + stepBeats * juce::jlimit (0.05, 1.0, (double) s.arpGate) - beatsPerSample * 2.0;
                nextStep += stepBeats;
                ++step;
            }
        }

        double beat = 0.0;

    private:
        void releaseAll()
        {
            for (int n : playing) if (noteOff) noteOff (n);
            playing.clear();
        }

        void playStep (const Settings& s, Rng& rng)
        {
            if (held.empty()) return;
            const int oct = juce::jlimit (1, 4, s.arpOct);
            std::vector<Held> base (held.begin(), held.end());
            if (s.arpMode != arpPlayed)
                std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
            else
                std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });

            if (s.arpMode == arpChord)
            {
                const int o = step % oct;
                for (auto& h : base) trigger (h.note + 12 * o, h.vel);
                return;
            }

            seq.clear();
            for (int o = 0; o < oct; ++o)
                for (auto& h : base) seq.push_back ({ h.note + 12 * o, h.vel, 0 });
            const int len = (int) seq.size();
            int idx = 0;
            switch (s.arpMode)
            {
                case arpDown:   idx = len - 1 - step % len; break;
                case arpUpDown:
                {
                    if (len < 2) { idx = 0; break; }
                    const int period = 2 * len - 2, k = step % period;
                    idx = k < len ? k : period - k;
                    break;
                }
                case arpRandom: idx = (int) (rng.next() % (uint32_t) len); break;
                case arpUp:
                case arpPlayed:
                default:        idx = step % len; break;
            }
            trigger (seq[(size_t) idx].note, seq[(size_t) idx].vel);
        }

        void trigger (int note, float vel)
        {
            if (note > 127) return;
            playing.push_back (note);
            if (noteOn) noteOn (note, vel);
        }

        std::vector<Held> held, seq;
        std::vector<int> playing;
        double nextStep = 0.0, offAt = -1.0;
        int step = 0, counter = 0;
        bool running = false;
    };

    //==========================================================================
    class Engine
    {
    public:
        static constexpr int ctrlBlock = 16;

        Engine()
        {
            held.reserve (128);
            arp.noteOn  = [this] (int n, float v) { synthNoteOn (n, v, true); };
            arp.noteOff = [this] (int n) { synthNoteOff (n, true); };
        }

        void prepare (double sampleRate, int maxBlock)
        {
            fs = sampleRate;
            capacity = std::max (64, maxBlock);
            scratchR.assign ((size_t) capacity, 0.0f);
            wet.setSize (2, capacity, false, true, true);
            delay.prepare (fs);
            reverb.prepare (fs);
            chorus.prepare ({ fs, (juce::uint32) capacity, 2 });
            chorus.setCentreDelay (8.0f);
            chorus.setFeedback (0.15f);
            smoothCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) fs));
            masterGain = -1.0f;
            reset();
        }

        void reset()
        {
            for (auto& v : voices) { v.active = false; for (auto& l : v.layers) { l.active = false; l.amp.kill(); l.filt.kill(); } }
            arp.reset();
            held.clear();
            for (auto& st : sustained) st = false;
            for (auto& k : keyHeld) k = false;
            sustainPedal = false;
            delay.reset();
            reverb.reset();
            chorus.reset();
            ctrlCountdown = 0;
        }

        /** Renders one block. Output is cleared and overwritten. */
        void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& settings, const HostTime& host)
        {
            s = &settings;
            const int n = out.getNumSamples();
            out.clear();
            if (out.getNumChannels() < 1 || n <= 0) return;
            float* L = out.getWritePointer (0);
            float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
            if (R == nullptr)
            {
                if ((int) scratchR.size() < n) scratchR.resize ((size_t) n);   // only if the host exceeds the prepared block size
                std::fill (scratchR.begin(), scratchR.begin() + n, 0.0f);
                R = scratchR.data();
            }

            // arpeggiator on/off
            if (settings.arpOn != arpWasOn)
            {
                arp.stop();
                arpWasOn = settings.arpOn;
            }
            const double bpm = juce::jlimit (20.0, 400.0, host.bpm > 0.0 ? host.bpm : 120.0);
            const double bps = bpm / 60.0 / fs;
            const double stepBeats = arpRateBeats[juce::jlimit (0, 7, settings.arpRate)];
            hostPlaying = host.playing && host.hasPpq;
            if (settings.arpOn && hostPlaying) arp.sync (host.ppq, stepBeats);
            currentBpm = bpm;

            auto it = midi.begin();
            int pos = 0;
            while (pos < n)
            {
                while (it != midi.end() && (*it).samplePosition <= pos)
                {
                    handleMidi ((*it).getMessage(), stepBeats);
                    ++it;
                }
                if (settings.arpOn && arp.samplesUntilNext (bps) == 0)
                {
                    arp.fire (settings, bps, arpRng);
                    if (arp.samplesUntilNext (bps) == 0) arp.advance (1, bps); // safety: never stall
                    continue;
                }
                int end = n;
                if (it != midi.end()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
                if (settings.arpOn) end = std::min (end, pos + std::max (1, arp.samplesUntilNext (bps)));
                render (L + pos, R + pos, end - pos);
                if (settings.arpOn) arp.advance (end - pos, bps);
                pos = end;
            }
            while (it != midi.end()) { handleMidi ((*it).getMessage(), stepBeats); ++it; }

            applyFx (L, R, n);

            if (out.getNumChannels() == 1)
                for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

            float pk = 0.0f;
            for (int c = 0; c < out.getNumChannels(); ++c) pk = std::max (pk, out.getMagnitude (c, 0, n));
            float cur = outPeak.load (std::memory_order_relaxed);
            while (pk > cur && ! outPeak.compare_exchange_weak (cur, pk, std::memory_order_relaxed)) {}
        }

        int countActiveVoices() const
        {
            int c = 0;
            for (auto& v : voices) c += v.active ? 1 : 0;
            return c;
        }

        // Live values for the GUI
        std::atomic<float> orbXOut { 0.0f }, orbYOut { 0.0f }, outPeak { 0.0f };
        std::atomic<int> activeVoices { 0 };

    private:
        //======================================================================
        void handleMidi (const juce::MidiMessage& m, double stepBeats)
        {
            if (m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                const float vel = m.getFloatVelocity();
                keyHeld[note] = true;
                sustained[note] = false;
                if (s->arpOn) arp.add (note, vel, hostPlaying, stepBeats);
                else          synthNoteOn (note, vel, false);
            }
            else if (m.isNoteOff())
            {
                const int note = m.getNoteNumber();
                keyHeld[note] = false;
                if (sustainPedal) sustained[note] = true;
                else              releaseNote (note);
            }
            else if (m.isPitchWheel())
                bend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
            else if (m.isChannelPressure())
                aftertouch = (float) m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch())
                aftertouch = (float) m.getAfterTouchValue() / 127.0f;
            else if (m.isController())
            {
                const int cc = m.getControllerNumber(), val = m.getControllerValue();
                if (cc == 1) modWheelTarget = (float) val / 127.0f;
                else if (cc == 64)
                {
                    const bool down = val >= 64;
                    if (sustainPedal && ! down)
                        for (int k = 0; k < 128; ++k)
                            if (sustained[k]) { sustained[k] = false; if (! keyHeld[k]) releaseNote (k); }
                    sustainPedal = down;
                }
                else if (cc == 120 || cc == 123)
                    allNotesOff (cc == 120);
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                allNotesOff (m.isAllSoundOff());
        }

        void releaseNote (int note)
        {
            arp.remove (note);
            synthNoteOff (note, false);
        }

        void allNotesOff (bool hard)
        {
            arp.stop();
            held.clear();
            for (int k = 0; k < 128; ++k) { sustained[k] = false; keyHeld[k] = false; }
            for (auto& v : voices)
                for (auto& l : v.layers)
                {
                    if (hard) { l.amp.kill(); l.active = false; }
                    else      { l.amp.noteOff(); l.filt.noteOff(); }
                }
            if (hard) for (auto& v : voices) v.active = false;
        }

        //======================================================================
        void synthNoteOn (int note, float vel, bool fromArp)
        {
            if (s->polyMode != modePoly)
            {
                held.erase (std::remove (held.begin(), held.end(), note), held.end());
                held.push_back (note);
                auto& v = voices[0];
                const bool legato = s->polyMode == modeLegato && v.active && v.keyDown;
                for (int i = 1; i < maxVoices; ++i) if (voices[(size_t) i].active) releaseVoice (voices[(size_t) i]);
                if (legato)
                {
                    v.note = note;
                    setGlide (v, (float) note, true);
                }
                else
                    startVoice (v, note, vel, v.active, fromArp);
                v.keyDown = true;
                return;
            }

            Voice* target = nullptr;
            for (auto& v : voices)                                   // same note re-struck: reuse it
                if (v.active && v.note == note && v.fromArp == fromArp) { target = &v; break; }
            if (target == nullptr)
                for (auto& v : voices) if (! v.active) { target = &v; break; }
            if (target == nullptr)                                   // steal: oldest released, else oldest
            {
                for (auto& v : voices)
                    if (! v.keyDown && (target == nullptr || v.age < target->age)) target = &v;
                if (target == nullptr)
                    for (auto& v : voices)
                        if (target == nullptr || v.age < target->age) target = &v;
            }
            startVoice (*target, note, vel, target->active, fromArp);
            target->keyDown = true;
        }

        void synthNoteOff (int note, bool fromArp)
        {
            if (s->polyMode != modePoly)
            {
                held.erase (std::remove (held.begin(), held.end(), note), held.end());
                auto& v = voices[0];
                if (! v.active || v.note != note) return;
                if (! held.empty())
                {
                    v.note = held.back();
                    if (s->polyMode == modeLegato) setGlide (v, (float) v.note, true);
                    else                           startVoice (v, v.note, v.velocity, true, fromArp);
                }
                else
                    releaseVoice (v);
                return;
            }
            for (auto& v : voices)
                if (v.active && v.keyDown && v.note == note && v.fromArp == fromArp)
                    releaseVoice (v);
        }

        void releaseVoice (Voice& v)
        {
            v.keyDown = false;
            for (auto& l : v.layers) { l.amp.noteOff(); l.filt.noteOff(); }
        }

        void setGlide (Voice& v, float targetNote, bool allowGlide)
        {
            v.targetSemis = targetNote;
            if (allowGlide && s->glideMs > 1.0f)
                v.glideStep = (v.targetSemis - v.semis) / (s->glideMs * 0.001f * (float) fs);
            else
            {
                v.semis = v.targetSemis;
                v.glideStep = 0.0f;
            }
        }

        void startVoice (Voice& v, int note, float vel, bool wasActive, bool fromArp)
        {
            v.rng.seed ((uint32_t) (++voiceCounter * 2654435761u) ^ (uint32_t) note);
            v.note = note;
            v.velocity = vel;
            v.random = v.rng.bi();
            v.age = voiceCounter;
            v.fromArp = fromArp;

            if (s->glideMs > 1.0f && lastNote >= 0)
            {
                if (! wasActive || s->polyMode == modePoly) v.semis = (float) lastNote;
                setGlide (v, (float) note, true);
            }
            else
                setGlide (v, (float) note, false);
            lastNote = note;

            for (int li = 0; li < numLayers; ++li)
            {
                auto& lv = v.layers[li];
                const auto& L = s->layers[(size_t) li];
                if (! L.on)
                {
                    if (lv.active) { lv.amp.noteOff(); lv.filt.noteOff(); }   // stolen voice: let the old layer fade out
                    continue;
                }

                const bool fresh = ! wasActive || ! lv.active;
                if (fresh)
                {
                    for (int u = 0; u < maxUnison; ++u) { lv.phase[u] = v.rng.uni(); lv.modPhase[u] = 0.0f; }
                    if (L.osc == oscFM) for (int u = 0; u < maxUnison; ++u) lv.phase[u] = 0.0f;
                    for (auto& g : lv.grains) g.active = false;
                    for (auto& ch : lv.svf) for (auto& f : ch) f.reset();
                    lv.noiseLp[0] = lv.noiseLp[1] = 0.0f;
                    lv.gainL = lv.gainR = 0.0f;
                    lv.amp.kill(); lv.filt.kill();
                }
                lv.grainTimer = 0.0f;
                lv.active = true;
                lv.amp.set (L.amp, fs);
                lv.filt.set (L.filt, fs);
                lv.amp.noteOn();
                lv.filt.noteOn();
                for (int k = 0; k < 2; ++k) lv.lfo[k].start (L.lfoShape[k], L.lfoFade[k], v.rng);
                updateLayer (v, li, 0);
                lv.gainL = fresh ? lv.targetL : lv.gainL;
                lv.gainR = fresh ? lv.targetR : lv.gainR;
            }
            v.active = true;
        }

        //======================================================================
        void updateGlobalControl()
        {
            modWheel += (modWheelTarget - modWheel) * 0.2f;
            orbPhase += s->orbRate * (float) ctrlBlock / (float) fs;
            orbPhase -= std::floor (orbPhase);
            const float a = orbPhase * juce::MathConstants<float>::twoPi;
            orbXv = juce::jlimit (-1.0f, 1.0f, s->orbX + s->orbDepth * std::cos (a));
            orbYv = juce::jlimit (-1.0f, 1.0f, s->orbY + s->orbDepth * std::sin (a));
            orbXOut.store (orbXv, std::memory_order_relaxed);
            orbYOut.store (orbYv, std::memory_order_relaxed);
        }

        /** Control-rate update of one layer of one voice. samples = how far LFOs advance. */
        void updateLayer (Voice& v, int li, int samples)
        {
            auto& lv = v.layers[li];
            const auto& L = s->layers[(size_t) li];

            lv.amp.set (L.amp, fs);
            lv.filt.set (L.filt, fs);

            // sources
            float src[numSources] {};
            src[srcLfo1] = lv.lfo[0].get();
            src[srcLfo2] = lv.lfo[1].get();
            src[srcFiltEnv] = lv.filt.getLevel();
            src[srcAmpEnv] = lv.amp.getLevel();
            src[srcModWheel] = modWheel;
            src[srcVelocity] = v.velocity;
            src[srcAftertouch] = aftertouch;
            src[srcKey] = ((float) v.note - 60.0f) / 60.0f;
            src[srcRandom] = v.random;
            src[srcOrbX] = orbXv;
            src[srcOrbY] = orbYv;

            float* mod = lv.mod;
            std::fill (mod, mod + numDests, 0.0f);
            for (const auto& sl : s->slots)
            {
                if (sl.src <= srcOff || sl.dst <= dstOff || sl.src >= numSources || sl.dst >= numDests) continue;
                if (sl.layer != 0 && sl.layer - 1 != li) continue;
                const float via = (sl.via > srcOff && sl.via < numSources) ? src[sl.via] : 1.0f;
                mod[sl.dst] += sl.amt * src[sl.src] * via;
            }

            for (int k = 0; k < 2; ++k)
            {
                const float rate = L.lfoRate[k] * std::exp2 (juce::jlimit (-4.0f, 4.0f, mod[k == 0 ? dstLfo1Rate : dstLfo2Rate] * 4.0f));
                if (samples > 0) lv.lfo[k].advance (rate, L.lfoShape[k], L.lfoFade[k], samples, fs, v.rng);
            }

            // pitch
            const float pitch = v.semis + L.pitch + bend * s->bendRange + mod[dstPitch] * 24.0f + mod[dstFine];
            const float inc0 = (float) (440.0 * std::exp2 ((pitch - 69.0f) / 12.0f) / fs);
            lv.inc0 = std::min (inc0, 0.45f);

            const float shape = juce::jlimit (0.0f, 1.0f, L.shape + mod[dstShape]);
            const float sym = juce::jlimit (0.0f, 1.0f, L.sym + mod[dstSym]);
            const auto& bank = WaveBank::get();

            if (L.osc == oscAnalog || L.osc == oscWavetable || L.osc == oscFM)
            {
                const int n = juce::jlimit (1, maxUnison, L.uni);
                lv.uni = n;
                const float det = std::max (0.0f, L.detune + mod[dstDetune] * 100.0f);
                const float norm = 1.0f / std::sqrt ((float) n);
                float maxInc = 0.0f;
                for (int u = 0; u < n; ++u)
                {
                    const float x = n > 1 ? (float) u / (float) (n - 1) * 2.0f - 1.0f : 0.0f;
                    lv.inc[u] = std::min (0.45f, lv.inc0 * std::exp2 (x * det / 1200.0f));
                    maxInc = std::max (maxInc, lv.inc[u]);
                    const float p = x * L.spread;
                    lv.uniL[u] = equalPowerL (p) * norm;
                    lv.uniR[u] = equalPowerR (p) * norm;
                }
                if (L.osc == oscFM)
                {
                    lv.fmRatio = fmRatios[fmRatioIndex (sym)];
                    lv.fmIndex = juce::jlimit (0.0f, 12.0f, shape * 10.0f) / juce::MathConstants<float>::twoPi;
                }
                else
                {
                    const int b = L.osc == oscAnalog ? 0 : juce::jlimit (0, WaveBank::numBanks - 1, L.wave);
                    lv.warp = juce::jmap (sym, 0.06f, 0.94f);
                    const float warpFactor = 0.5f / std::min (lv.warp, 1.0f - lv.warp);
                    const int mip = WaveBank::mipFor (maxInc * warpFactor);
                    const int nf = bank.numFrames (b);
                    const float fp = shape * (float) (nf - 1);
                    const int f0 = std::min ((int) fp, nf - 1), f1 = std::min (f0 + 1, nf - 1);
                    lv.tA = bank.table (b, f0, mip);
                    lv.tB = bank.table (b, f1, mip);
                    lv.frac = fp - (float) f0;
                }
            }
            else if (L.osc == oscGranular)
            {
                for (auto& g : lv.grains)
                    if (g.active) g.inc = std::min (0.45f, lv.inc0 * g.ratio);
            }
            else // noise
            {
                const float hz = 150.0f * std::exp2 (shape * 7.2f);
                lv.noiseCoef = std::min (1.0f, 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) fs));
            }

            // filter
            if (L.ftype != fltOff)
            {
                const float oct = L.fenv * 5.0f * lv.filt.getLevel() + L.fkey * ((float) v.note - 60.0f) / 12.0f + mod[dstCutoff] * 6.0f;
                const float fc = juce::jlimit (20.0f, 0.45f * (float) fs, L.cutoff * std::exp2 (juce::jlimit (-12.0f, 12.0f, oct)));
                const float res = juce::jlimit (0.0f, 1.0f, L.reso + mod[dstReso]);
                const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) fs);
                lv.c1 = Svf::make (g, 2.0f - 1.96f * res);
                lv.c2 = Svf::make (g, 1.4142f);
            }
            lv.driveGain = 1.0f + L.drive * 8.0f;
            lv.driveComp = 1.0f / (1.0f + L.drive * 2.5f);

            // level / pan
            const float vel = 1.0f - s->velSens + s->velSens * v.velocity * v.velocity;
            const float gain = L.level * L.level * vel * juce::jlimit (0.0f, 2.0f, 1.0f + mod[dstLevel]) * 0.35f;
            const float pan = juce::jlimit (-1.0f, 1.0f, L.pan + mod[dstPan]);
            lv.targetL = gain * equalPowerL (pan);
            lv.targetR = gain * equalPowerR (pan);
        }

        //======================================================================
        void render (float* L, float* R, int n)
        {
            int pos = 0;
            while (pos < n)
            {
                if (ctrlCountdown <= 0)
                {
                    updateGlobalControl();
                    for (auto& v : voices)
                    {
                        if (! v.active) continue;
                        if (v.glideStep != 0.0f)
                        {
                            v.semis += v.glideStep * (float) ctrlBlock;
                            if ((v.glideStep > 0.0f && v.semis >= v.targetSemis) || (v.glideStep < 0.0f && v.semis <= v.targetSemis))
                            { v.semis = v.targetSemis; v.glideStep = 0.0f; }
                        }
                        for (int li = 0; li < numLayers; ++li)
                            if (v.layers[li].active) updateLayer (v, li, ctrlBlock);
                    }
                    ctrlCountdown = ctrlBlock;
                }
                const int len = std::min (n - pos, ctrlCountdown);
                for (auto& v : voices)
                {
                    if (! v.active) continue;
                    bool any = false;
                    for (int li = 0; li < numLayers; ++li)
                    {
                        auto& lv = v.layers[li];
                        if (! lv.active) continue;
                        renderLayer (v, lv, s->layers[(size_t) li], L + pos, R + pos, len);
                        if (lv.amp.isIdle()) lv.active = false;
                        else any = true;
                    }
                    if (! any) v.active = false;
                }
                ctrlCountdown -= len;
                pos += len;
            }
            activeVoices.store (countActiveVoices(), std::memory_order_relaxed);
        }

        void spawnGrain (Voice& v, LayerVoice& lv, const LayerSettings& L)
        {
            Grain* g = nullptr;
            for (auto& gr : lv.grains) if (! gr.active) { g = &gr; break; }
            const float dens = juce::jlimit (1.0f, 16.0f, L.gdens + lv.mod[dstGrainDensity] * 8.0f);
            const float sizeMs = juce::jlimit (5.0f, 2000.0f, L.gsizeMs * std::exp2 (juce::jlimit (-3.0f, 3.0f, lv.mod[dstGrainSize] * 3.0f)));
            const float sizeS = sizeMs * 0.001f * (float) fs;
            lv.grainTimer += sizeS / dens * (0.6f + 0.8f * v.rng.uni());
            if (g == nullptr) return;

            auto& rng = v.rng;
            float semis = L.gpitch * 0.01f * rng.bi();
            const float r = rng.uni();
            if (r < L.gshim * 0.30f) semis += 12.0f;
            else if (r < L.gshim * 0.45f) semis += 7.0f;
            else if (r < L.gshim * 0.55f) semis += 19.0f;
            else if (r < L.gshim * 0.60f) semis -= 12.0f;

            g->ratio = std::exp2 (semis / 12.0f);
            g->inc = std::min (0.45f, lv.inc0 * g->ratio);
            g->phase = rng.uni();
            g->t = 0.0f;
            g->tInc = 1.0f / std::max (16.0f, sizeS * (0.8f + 0.4f * rng.uni()));
            const float pan = L.spread * rng.bi();
            const float norm = 1.3f / std::sqrt (dens);
            g->gl = equalPowerL (pan) * norm;
            g->gr = equalPowerR (pan) * norm;

            const auto& bank = WaveBank::get();
            const int b = juce::jlimit (0, WaveBank::numBanks - 1, L.wave);
            const float pos = juce::jlimit (0.0f, 1.0f, juce::jlimit (0.0f, 1.0f, L.shape + lv.mod[dstShape]) + L.gspray * 0.5f * rng.bi());
            const int nf = bank.numFrames (b);
            const float fp = pos * (float) (nf - 1);
            const int f0 = std::min ((int) fp, nf - 1), f1 = std::min (f0 + 1, nf - 1);
            const int mip = WaveBank::mipFor (g->inc * 1.5f);
            g->tA = bank.table (b, f0, mip);
            g->tB = bank.table (b, f1, mip);
            g->frac = fp - (float) f0;
            g->active = true;
        }

        void renderLayer (Voice& v, LayerVoice& lv, const LayerSettings& Ls, float* outL, float* outR, int n)
        {
            const auto& bank = WaveBank::get();
            const int osc = Ls.osc;
            const int ft = Ls.ftype;
            const bool drive = Ls.drive > 0.001f;

            for (int i = 0; i < n; ++i)
            {
                float sl = 0.0f, sr = 0.0f;
                switch (osc)
                {
                    case oscAnalog:
                    case oscWavetable:
                    {
                        if (lv.tA == nullptr || lv.tB == nullptr) break;   // osc type just changed: tables set at next control update
                        const float w = lv.warp, fr = lv.frac;
                        const bool warped = std::abs (w - 0.5f) > 0.001f;
                        for (int u = 0; u < lv.uni; ++u)
                        {
                            float ph = lv.phase[u] + lv.inc[u];
                            ph -= (float) (int) ph;
                            lv.phase[u] = ph;
                            if (warped) ph = ph < w ? 0.5f * ph / w : 0.5f + 0.5f * (ph - w) / (1.0f - w);
                            const float a = WaveBank::read (lv.tA, ph);
                            const float x = fr > 1.0e-4f ? a + fr * (WaveBank::read (lv.tB, ph) - a) : a;
                            sl += x * lv.uniL[u];
                            sr += x * lv.uniR[u];
                        }
                        break;
                    }
                    case oscFM:
                    {
                        for (int u = 0; u < lv.uni; ++u)
                        {
                            float ph = lv.phase[u] + lv.inc[u];
                            ph -= (float) (int) ph;
                            lv.phase[u] = ph;
                            float mp = lv.modPhase[u] + lv.inc[u] * lv.fmRatio;
                            mp -= (float) (int) mp;
                            lv.modPhase[u] = mp;
                            const float x = bank.sine (ph + lv.fmIndex * bank.sine (mp));
                            sl += x * lv.uniL[u];
                            sr += x * lv.uniR[u];
                        }
                        break;
                    }
                    case oscGranular:
                    {
                        lv.grainTimer -= 1.0f;
                        if (lv.grainTimer <= 0.0f)
                            spawnGrain (v, lv, Ls);
                        for (auto& g : lv.grains)
                        {
                            if (! g.active) continue;
                            g.phase += g.inc;
                            g.phase -= (float) (int) g.phase;
                            const float a = WaveBank::read (g.tA, g.phase);
                            const float x = g.frac > 1.0e-4f ? a + g.frac * (WaveBank::read (g.tB, g.phase) - a) : a;
                            const float sw = bank.sine (g.t * 0.5f);
                            const float y = x * sw * sw;
                            sl += y * g.gl;
                            sr += y * g.gr;
                            g.t += g.tInc;
                            if (g.t >= 1.0f) g.active = false;
                        }
                        break;
                    }
                    default: // noise
                    {
                        const float m = v.rng.bi();
                        const float l = m + Ls.spread * (v.rng.bi() - m);
                        const float r = m + Ls.spread * (v.rng.bi() - m);
                        lv.noiseLp[0] += lv.noiseCoef * (l - lv.noiseLp[0]);
                        lv.noiseLp[1] += lv.noiseCoef * (r - lv.noiseLp[1]);
                        sl = lv.noiseLp[0] * 0.6f;
                        sr = lv.noiseLp[1] * 0.6f;
                        break;
                    }
                }

                if (drive)
                {
                    sl = std::tanh (sl * lv.driveGain) * lv.driveComp * 1.6f;
                    sr = std::tanh (sr * lv.driveGain) * lv.driveComp * 1.6f;
                }

                if (ft != fltOff)
                {
                    float x[2] = { sl, sr };
                    for (int c = 0; c < 2; ++c)
                    {
                        float lp, bp, hp;
                        lv.svf[c][0].tick (x[c], lv.c1, lp, bp, hp);
                        switch (ft)
                        {
                            case fltLP24: { float lp2, bp2, hp2; lv.svf[c][1].tick (lp, lv.c2, lp2, bp2, hp2); x[c] = lp2; break; }
                            case fltLP12: x[c] = lp; break;
                            case fltBP:   x[c] = bp * lv.c1.k; break;
                            case fltHP:   x[c] = hp; break;
                            case fltNotch:
                            default:      x[c] = lp + hp; break;
                        }
                    }
                    sl = x[0]; sr = x[1];
                }

                lv.filt.next();
                const float env = lv.amp.next();
                lv.gainL += (lv.targetL - lv.gainL) * smoothCoef;
                lv.gainR += (lv.targetR - lv.gainR) * smoothCoef;
                outL[i] += sl * env * lv.gainL;
                outR[i] += sr * env * lv.gainR;
            }
        }

        //======================================================================
        void applyFx (float* L, float* R, int n)
        {
            const float target = juce::Decibels::decibelsToGain (s->masterDb, -80.0f);
            if (masterGain < 0.0f) masterGain = target;

            aura::Settings rs;
            rs.algo = juce::jlimit (0, aura::numAlgos - 1, s->revType);
            rs.preDelayMs = 20.0f;
            rs.decayS = s->revDecay;
            rs.size = juce::jlimit (0.3f, 1.0f, 0.35f + s->revDecay / 12.0f);
            rs.tone = s->revTone;
            rs.lowCutHz = 120.0f;
            rs.mod = 0.5f;

            const double delayBeats = delayTimeBeats[juce::jlimit (0, 10, s->dlyTime)];
            const float delaySamples = (float) (delayBeats * 60.0 / currentBpm * fs);

            for (int start = 0; start < n; start += capacity)
            {
                const int len = std::min (capacity, n - start);
                float* l = L + start;
                float* r = R + start;

                if (s->choMix > 0.001f)
                {
                    chorus.setRate (s->choRate);
                    chorus.setDepth (s->choDepth);
                    chorus.setMix (s->choMix * 0.5f);
                    float* chans[] = { l, r };
                    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
                    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
                }

                if (s->dlyMix > 0.001f || delayActive)
                {
                    delay.process (l, r, len, delaySamples, s->dlyFb, s->dlyMix, s->dlyTone);
                    delayActive = s->dlyMix > 0.001f;
                }

                const float mix = s->revMix;
                if (mix > 0.001f || reverbActive)
                {
                    wet.copyFrom (0, 0, l, len);
                    wet.copyFrom (1, 0, r, len);
                    reverb.process (wet.getArrayOfWritePointers(), 2, 2, len, rs);
                    const float* wl = wet.getReadPointer (0);
                    const float* wr = wet.getReadPointer (1);
                    const float dryG = 1.0f - 0.3f * mix;
                    for (int i = 0; i < len; ++i)
                    {
                        l[i] = l[i] * dryG + wl[i] * mix;
                        r[i] = r[i] * dryG + wr[i] * mix;
                    }
                    reverbActive = mix > 0.001f;
                }

                for (int i = 0; i < len; ++i)
                {
                    masterGain += (target - masterGain) * smoothCoef;
                    l[i] *= masterGain;
                    r[i] *= masterGain;
                }
            }
        }

        //======================================================================
        const Settings* s = nullptr;
        double fs = 44100.0, currentBpm = 120.0;
        int capacity = 512;
        std::array<Voice, maxVoices> voices;
        std::vector<int> held;
        bool keyHeld[128] {}, sustained[128] {};
        bool sustainPedal = false, hostPlaying = false, arpWasOn = false, delayActive = false, reverbActive = false;
        float bend = 0.0f, aftertouch = 0.0f, modWheel = 0.0f, modWheelTarget = 0.0f;
        float orbPhase = 0.0f, orbXv = 0.0f, orbYv = 0.0f;
        float smoothCoef = 0.01f, masterGain = -1.0f;
        int ctrlCountdown = 0, lastNote = -1;
        uint64_t voiceCounter = 0;
        Rng arpRng;
        Arpeggiator arp;
        StereoDelay delay;
        aura::Engine reverb;
        juce::dsp::Chorus<float> chorus;
        juce::AudioBuffer<float> wet;
        std::vector<float> scratchR;
    };
}
