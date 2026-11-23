#pragma once

#include "PluginProcessor.h"

// Built-in presets. Plain parameter values; parameters not listed use their default.
// Input assumes a track peaking around -10 to -6 dBFS (the threshold is fixed at -20 dBFS peak):
// adjust Input first if your level is very different, then Output.
// Ratio index: 0 1:1, 1 2:1, 2 3:1, 3 4:1, 4 6:1, 5 10:1, 6 20:1, 7 Nuke, 8 Opto.   Dist: 0 Clean, 1 Dist 2, 2 Dist 3.
namespace stressulf
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        struct Flags { bool hp = false, band = false, ahp = false, sm = false; };
        auto P = [] (const char* name, float in, int rat, float att, float rel, int ds, Flags f,
                     float out, float mx = 100.0f)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { input, in }, { ratio, (float) rat }, { attack, att }, { release, rel }, { dist, (float) ds },
                  { detHp, f.hp ? 1.0f : 0.0f }, { detBand, f.band ? 1.0f : 0.0f }, { audioHp, f.ahp ? 1.0f : 0.0f },
                  { smash, f.sm ? 1.0f : 0.0f }, { output, out }, { mix, mx } } };
        };
        const Flags none, hp { true }, hpBand { true, true }, hpAhp { true, false, true }, sm { false, false, false, true },
                    smHp { true, false, false, true };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                    input  ratio att     rel     dist flags   out    mix
            P ("Vokal – Jevn lead",                     -2.0f, 3,    3.0f,  250.0f, 0,  hp,      2.0f),
            P ("Vokal – Pop foran i miksen",             2.0f, 4,    1.0f,  150.0f, 1,  hp,      0.0f),
            P ("Vokal – Rock med bitt",                  6.0f, 4,    0.5f,  100.0f, 2,  hp,     -3.0f),
            P ("Vokal – Myk opto",                       0.0f, 8,   10.0f,  300.0f, 1,  hp,      2.0f),
            P ("Vokal – De-ess og jevn",                -2.0f, 3,    1.0f,  200.0f, 0,  hpBand,  1.0f),
            P ("Vokal – Rap tett",                       4.0f, 5,    0.3f,   80.0f, 1,  hp,     -2.0f),
            P ("Vokal – Parallell knusing",             10.0f, 6,    0.2f,   60.0f, 2,  smHp,   -8.0f, 35.0f),

            P ("Trommer – Bus lim",                     -4.0f, 1,   10.0f,  200.0f, 1,  hp,      1.0f),
            P ("Trommer – Rom knust",                   12.0f, 7,    0.1f,   80.0f, 2,  sm,    -10.0f),
            P ("Trommer – Parallell smash",             12.0f, 6,    0.1f,   60.0f, 2,  sm,    -10.0f, 30.0f),
            P ("Skarptromme – Punch",                    0.0f, 4,   12.0f,  120.0f, 1,  none,    0.0f),
            P ("Basstromme – Tett",                      0.0f, 3,    8.0f,   90.0f, 1,  none,    0.0f),

            P ("Bass – Jevn og varm",                    0.0f, 3,    5.0f,  300.0f, 1,  none,    1.0f),
            P ("Bass – Tett DI",                         4.0f, 4,    1.0f,  150.0f, 2,  none,   -2.0f),
            P ("Akustisk gitar – Jevn strumming",       -2.0f, 2,    8.0f,  300.0f, 0,  hpAhp,   2.0f),
            P ("Elgitar – Sustain",                      6.0f, 5,    2.0f,  500.0f, 2,  hp,     -4.0f),
            P ("Synth – Pump",                           6.0f, 5,    0.5f,  120.0f, 0,  none,   -3.0f),

            P ("Farge – Bare varme (1:1 + Dist 2)",      0.0f, 0,    5.0f,  300.0f, 1,  none,    0.0f),
            P ("Farge – Tape-kant (1:1 + Dist 3)",       4.0f, 0,    5.0f,  300.0f, 2,  none,   -4.0f),
            P ("Begrenser – Nuke",                       0.0f, 7,    0.05f, 100.0f, 0,  none,    0.0f),
        };
    }
}
