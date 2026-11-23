#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default.
// Presets never touch the analysis – load one after analysing and the loudness target still holds.
// Style: 0 = Varm, 1 = Balansert, 2 = Åpen.
namespace domina
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, int st, float lufs, float lo, float md, float pr, float hi,
                     float comp, float ds, float sat, float wd, bool mono = false, float ceil = -1.0f, float aeq = 70.0f)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { style, (float) st }, { loudness, lufs }, { low, lo }, { mid, md }, { presence, pr }, { high, hi },
                  { compression, comp }, { deess, ds }, { saturation, sat }, { width, wd },
                  { monoBass, mono ? 1.0f : 0.0f }, { ceiling, ceil }, { autoEq, aeq } } };
        };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                     st  LUFS    low   mid   pres  high  comp  dess  sat   width mono
            P ("Strømming – −14 LUFS",                   1, -14.0f, 0.0f, 0.0f, 0.0f, 0.0f, 35.0f, 20.0f, 15.0f, 100.0f),
            P ("Pop – Blank og moderne",                 2,  -9.0f, 0.5f, 0.0f, 0.5f, 1.0f, 45.0f, 30.0f, 20.0f, 115.0f, true),
            P ("Rock – Tett og fet",                     1,  -9.0f, 0.5f, 0.5f, 0.5f, 0.0f, 55.0f, 20.0f, 35.0f, 110.0f),
            P ("Hip-hop – Tung bunn",                    0,  -8.0f, 1.5f, 0.0f, 0.5f, 0.0f, 45.0f, 25.0f, 30.0f, 105.0f, true),
            P ("EDM – Maks trykk",                       2,  -7.0f, 1.0f, -0.5f, 0.0f, 1.0f, 55.0f, 20.0f, 40.0f, 120.0f, true),
            P ("Akustisk – Naturlig",                    1, -14.0f, 0.0f, 0.0f, 0.0f, 0.5f, 25.0f, 25.0f, 10.0f, 100.0f),
            P ("Singer/songwriter – Varm og nær",        0, -12.0f, 0.5f, 0.0f, 0.5f, -0.5f, 35.0f, 35.0f, 20.0f, 95.0f),
            P ("Jazz – Dynamisk",                        0, -16.0f, 0.0f, 0.0f, 0.0f, 0.0f, 15.0f, 15.0f, 10.0f, 100.0f),
            P ("Klassisk / film – Luftig",               2, -18.0f, 0.0f, 0.0f, 0.0f, 0.5f, 10.0f, 10.0f, 0.0f, 105.0f),
            P ("Lo-fi – Mykt og varmt",                  0, -12.0f, 1.0f, 0.5f, -1.0f, -2.0f, 40.0f, 20.0f, 55.0f, 90.0f),
            P ("Metal – Tett og aggressiv",              1,  -8.0f, 0.5f, -1.0f, 1.0f, 0.5f, 60.0f, 25.0f, 45.0f, 110.0f, true),
            P ("Podkast / tale",                         1, -16.0f, -1.0f, 0.5f, 1.0f, 0.0f, 50.0f, 45.0f, 10.0f, 100.0f, true),
            P ("Demo – Høyt og trygt",                   1, -10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 40.0f, 25.0f, 25.0f, 100.0f, false, -1.0f),
            P ("Vinyl-forberedelse",                     0, -14.0f, 0.0f, 0.0f, 0.0f, -0.5f, 30.0f, 40.0f, 15.0f, 90.0f, true, -2.0f),
        };
    }
}
