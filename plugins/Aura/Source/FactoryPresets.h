#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default.
// Aura is always 100 % wet – use it on an aux/bus (send) in Logic.
namespace aura
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, Algo a, float pre, float dec, float sz, float tn, float lc, float md)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { algo, (float) a }, { preDelay, pre }, { decay, dec }, { size, sz },
                  { tone, tn }, { lowCut, lc }, { mod, md } } };
        };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                         type      pre    decay  size   tone   lowcut mod
            // ---- Vocal ----
            P ("Vokal – Plate klassisk",                   Plate,    30.0f, 1.8f,  55.0f,  10.0f, 150.0f, 25.0f),
            P ("Vokal – Plate lys pop",                    Plate,    15.0f, 1.4f,  45.0f,  35.0f, 200.0f, 20.0f),
            P ("Vokal – Plate lang ballade",               Plate,    45.0f, 3.0f,  70.0f,   0.0f, 180.0f, 30.0f),
            P ("Vokal – Vintage 80-tall",                  Vintage,  40.0f, 2.2f,  60.0f, -10.0f, 150.0f, 40.0f),
            P ("Vokal – Vintage mørk og tett",             Vintage,  20.0f, 1.5f,  45.0f, -35.0f, 180.0f, 30.0f),
            P ("Vokal – Hall moderne pop",                 Hall,     25.0f, 2.2f,  60.0f,  15.0f, 200.0f, 30.0f),
            P ("Vokal – Hall stor ballade",                Hall,     60.0f, 3.4f,  80.0f,  -5.0f, 180.0f, 40.0f),
            P ("Vokal – Episk hale",                       Hall,     90.0f, 6.5f, 100.0f, -20.0f, 250.0f, 55.0f),
            P ("Vokal – Kammer varm",                      Chamber,  20.0f, 1.5f,  50.0f, -15.0f, 150.0f, 20.0f),
            P ("Vokal – Kammer bred bakgrunnsvokal",       Chamber,  30.0f, 2.0f,  65.0f,   0.0f, 200.0f, 50.0f),
            P ("Vokal – Rom naturlig",                     Room,     10.0f, 0.8f,  40.0f,   0.0f, 150.0f, 15.0f),
            P ("Vokal – Rom med pre-delay",                Room,     80.0f, 0.7f,  35.0f,   5.0f, 200.0f, 15.0f),
            P ("Vokal – Ambience nærhet",                  Ambience,  0.0f, 0.45f, 30.0f,   5.0f, 200.0f, 10.0f),
            P ("Vokal – Rap og tale tørr",                 Ambience,  5.0f, 0.3f,  20.0f,  10.0f, 250.0f,  5.0f),

            // ---- Acoustic guitar ----
            P ("Akustisk gitar – Rom treverk",             Room,      5.0f, 0.9f,  45.0f,  -5.0f, 120.0f, 15.0f),
            P ("Akustisk gitar – Nær ambience",            Ambience,  0.0f, 0.35f, 30.0f,   0.0f, 100.0f, 10.0f),
            P ("Akustisk gitar – Kammer",                  Chamber,  15.0f, 1.6f,  55.0f,   0.0f, 120.0f, 20.0f),
            P ("Akustisk gitar – Konserthall",             Hall,     30.0f, 2.6f,  70.0f, -10.0f, 100.0f, 30.0f),
            P ("Akustisk gitar – Fingerspill stor hall",   Hall,     45.0f, 4.0f,  85.0f, -15.0f, 120.0f, 45.0f),
            P ("Akustisk gitar – Plate glans",             Plate,    20.0f, 1.6f,  50.0f,  25.0f, 150.0f, 25.0f),
            P ("Akustisk gitar – Strumming kort plate",    Plate,    10.0f, 0.9f,  35.0f,  10.0f, 200.0f, 15.0f),
            P ("Akustisk gitar – Vintage plate",           Vintage,  25.0f, 1.8f,  55.0f,  -5.0f, 120.0f, 30.0f),
            P ("Akustisk gitar – Svevende ambient",        Hall,     90.0f, 8.0f, 100.0f, -25.0f, 200.0f, 80.0f),
        };
    }
}
