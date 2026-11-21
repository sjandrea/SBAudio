#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default.
// Profile: 0 = Universell, 1 = Vokal, 2 = Akustisk gitar, 3 = Tale, 4 = Mix / master.
// Presets never touch the learned curve – press LEARN on the track to get the static correction.
namespace foss
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, int prof, float rec, float tm, float bi, float br, float bs,
                     float lo = 20.0f, float hi = 20000.0f, float corr = 100.0f, float mx = 100.0f)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { profile, (float) prof }, { recover, rec }, { tame, tm }, { bias, bi }, { brighten, br }, { bass, bs },
                  { lowFreq, lo }, { highFreq, hi }, { correction, corr }, { mix, mx } } };
        };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                     prof rec    tame   bias    bright  bass    low     high     corr
            // ---- Vocal ----
            P ("Vokal – Klar og åpen",                   1,  40.0f, 35.0f,  10.0f,  15.0f,  0.0f,  80.0f),
            P ("Vokal – Tem harde toner",                1,  15.0f, 65.0f, -40.0f,   0.0f,  0.0f, 300.0f),
            P ("Vokal – Varm og nær",                    1,  30.0f, 40.0f,   0.0f, -15.0f, 20.0f,  70.0f),
            P ("Vokal – Fram i miksen",                  1,  55.0f, 45.0f,  20.0f,  20.0f,-10.0f, 100.0f),
            P ("Vokal – Bare learn (statisk)",           1,   0.0f,  0.0f,   0.0f,   0.0f,  0.0f,  60.0f),
            // ---- Acoustic guitar ----
            P ("Akustisk gitar – Naturlig balanse",      2,  30.0f, 30.0f,   0.0f,   0.0f,  0.0f,  60.0f),
            P ("Akustisk gitar – Glans og luft",         2,  45.0f, 30.0f,  20.0f,  30.0f,-15.0f,  80.0f),
            P ("Akustisk gitar – Tem boom og klang",     2,  15.0f, 60.0f, -30.0f,   0.0f,-20.0f,  60.0f, 6000.0f),
            P ("Akustisk gitar – Bakgrunn i miks",       2,  20.0f, 45.0f, -10.0f,  10.0f,-35.0f, 120.0f),
            // ---- Speech ----
            P ("Tale – Podkast",                         3,  35.0f, 50.0f,   0.0f,  10.0f,  0.0f,  70.0f),
            P ("Tale – Rydd opp i rom",                  3,  10.0f, 70.0f, -40.0f,   0.0f,-10.0f,  90.0f),
            // ---- Mix / master ----
            P ("Mix – Lett touch",                       4,  15.0f, 15.0f,   0.0f,   0.0f,  0.0f,  20.0f, 20000.0f, 50.0f),
            P ("Mix – Balanse",                          4,  30.0f, 30.0f,   0.0f,   0.0f,  0.0f,  20.0f, 20000.0f, 75.0f),
            P ("Mix – Åpne toppen",                      4,  35.0f, 20.0f,  20.0f,  25.0f,  0.0f,  20.0f, 20000.0f, 60.0f),
            P ("Mix – Rydd i mellomtonen",               4,  10.0f, 50.0f, -30.0f,   0.0f,  0.0f, 150.0f, 5000.0f, 60.0f),
        };
    }
}
