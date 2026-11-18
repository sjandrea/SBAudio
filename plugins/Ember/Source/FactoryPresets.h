#pragma once

#include "PluginProcessor.h"

// Built-in Ember presets (vocal). Values are plain parameter values; parameters not listed use their default.
// The presets assume a vocal track peaking around -10 to -6 dBFS. Much hotter/quieter: adjust Input first.
namespace ember
{
namespace factorydetail
{
    struct B
    {
        int style; float drive;
        float dyn = 0.0f, tone = 0.0f, mix = 100.0f, level = 0.0f;
        float fb = 0.0f, fbFreq = 1000.0f;
        bool on = true, mute = false;
    };

    inline B off()                 { B b { WarmTube, 0.0f }; b.on = false; return b; }
    inline B muted()               { B b { WarmTube, 0.0f }; b.mute = true; return b; }
    inline B withFb (B b, float fb, float f) { b.fb = fb; b.fbFreq = f; return b; }

    struct Global { float input = 0.0f, output = 0.0f, mix = 100.0f; };

    inline sbaudio::FactoryPreset make (const char* name, std::vector<B> bands, std::vector<float> splits, Global g = {})
    {
        using namespace ParamIDs;
        sbaudio::FactoryPreset p { juce::String (juce::CharPointer_UTF8 (name)), {} };
        auto& v = p.values;
        v.push_back ({ ParamIDs::bands, (float) bands.size() });
        v.push_back ({ input, g.input });
        v.push_back ({ output, g.output });
        v.push_back ({ mix, g.mix });
        for (size_t i = 0; i < splits.size(); ++i)
            v.push_back ({ split ((int) i), splits[i] });
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto& b = bands[i];
            const int k = (int) i;
            v.push_back ({ band (k, "style"), (float) b.style });
            v.push_back ({ band (k, "drive"), b.drive });
            v.push_back ({ band (k, "dyn"), b.dyn });
            v.push_back ({ band (k, "tone"), b.tone });
            v.push_back ({ band (k, "mix"), b.mix });
            v.push_back ({ band (k, "level"), b.level });
            v.push_back ({ band (k, "fb"), b.fb });
            v.push_back ({ band (k, "fbfreq"), b.fbFreq });
            v.push_back ({ band (k, "on"), b.on ? 1.0f : 0.0f });
            v.push_back ({ band (k, "mute"), b.mute ? 1.0f : 0.0f });
        }
        return p;
    }
}

    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace factorydetail;
        //                                  style        drive  dyn   tone   mix   level
        return {
            sbaudio::FactoryPreset { "Standard", {} },

            // ---- Én bånd: karakter på hele vokalen ----
            make ("Vokal – Varm rørglød",          { { WarmTube,    30.0f,  20.0f, -10.0f } }, {}),
            make ("Vokal – Subtil tape-lim",       { { Tape,        35.0f,   0.0f, -15.0f,  85.0f } }, {}),
            make ("Vokal – Analog konsoll",        { { Console,     45.0f,  10.0f,   0.0f } }, {}),
            make ("Vokal – Myk ballade",           { { CleanTube,   35.0f,  35.0f, -10.0f } }, {}),
            make ("Vokal – Parallell grit",        { { Diode,       60.0f,   0.0f, -20.0f,  30.0f } }, {}),

            // ---- Flerbånd: metning der den hjelper, ren der den ikke gjør det ----
            make ("Vokal – Tetthet i mellomtonen", { { CleanTube,   10.0f },
                                                     { WarmTube,    45.0f,  30.0f },
                                                     { Tape,        15.0f,   0.0f, -20.0f } }, { 250.0f, 4000.0f }),
            make ("Vokal – Luft og glans",         { off(),
                                                     { Tape,        55.0f,  10.0f,  20.0f,  60.0f,  1.0f } }, { 6000.0f }),
            make ("Vokal – Nærvær 2–5 kHz",        { off(),
                                                     { WarmTube,    40.0f,  20.0f,   0.0f,  70.0f },
                                                     off() }, { 1800.0f, 5500.0f }),
            make ("Vokal – Varm bunn, ren topp",   { { SmoothTube,  40.0f,  10.0f, -10.0f },
                                                     off() }, { 800.0f }),
            make ("Vokal – De-harsh",              { off(),
                                                     { WarmTube,    20.0f },
                                                     { Tape,        30.0f, -40.0f, -30.0f } }, { 200.0f, 5000.0f }),
            make ("Vokal – Pop moderne",           { { Transformer, 15.0f },
                                                     { Console,     30.0f },
                                                     { WarmTube,    40.0f,  20.0f },
                                                     { Tape,        35.0f,   0.0f,  10.0f,  70.0f } }, { 150.0f, 1200.0f, 7000.0f }),
            make ("Vokal – Rap og hip-hop tett",   { { Transformer, 30.0f },
                                                     { Console,     50.0f, -20.0f },
                                                     { Tape,        25.0f } }, { 200.0f, 3000.0f }),
            make ("Vokal – Bakgrunnsvokal tykk",   { { Transformer, 20.0f },
                                                     { WarmTube,    45.0f,   0.0f, -15.0f,  70.0f } }, { 300.0f }),
            make ("Vokal – Rock-kant",             { { WarmTube,    30.0f },
                                                     { CrunchAmp,   35.0f,  20.0f, -20.0f,  45.0f } }, { 1000.0f }),

            // ---- Effekt ----
            make ("Vokal – Distort refreng",       { { Fuzz,        45.0f,  40.0f, -30.0f,  35.0f } }, {}),
            make ("Vokal – Megafon",               { muted(),
                                                     { CrunchAmp,   50.0f,   0.0f,   0.0f, 100.0f,  4.0f },
                                                     muted() }, { 600.0f, 3500.0f }),
            make ("Vokal – Lo-fi radio",           { muted(),
                                                     { LoFi,        45.0f,   0.0f, -20.0f, 100.0f,  3.0f },
                                                     muted() }, { 400.0f, 4000.0f }),
            make ("Vokal – Spøkelsesresonans",     { withFb ({ WarmTube, 40.0f, 0.0f, 0.0f, 40.0f }, 60.0f, 1200.0f) }, {}),
            make ("Vokal – Knust og ødelagt",      { { Breakup,     50.0f,  30.0f, -10.0f,  45.0f } }, {}),
        };
    }
}
