#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default.
// Threshold assumes a recorded track peaking around -10 to -6 dBFS – adjust Threshold first,
// then Gain, if your level is very different.
// Mode: 0 = Opto, 1 = Manual, 2 = Opto + Manual.   HPF: 0 = off, 1 = 60, 2 = 100, 3 = 150 Hz.
namespace optotube
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, float thr, float rat, int md, float att, float rel,
                     int hp, float tb, float gn, float mx = 100.0f)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { threshold, thr }, { ratio, rat }, { mode, (float) md }, { attack, att }, { release, rel },
                  { hpf, (float) hp }, { tube, tb }, { gain, gn }, { mix, mx } } };
        };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                       thr    ratio mode att    rel     hpf tube gain  mix
            // ---- Male vocal ----
            P ("Vokal mann – Jevn og varm",               -22.0f, 3.0f, 0,  10.0f, 300.0f, 1, 35.0f, 5.0f),
            P ("Vokal mann – Tett pop/rock",              -26.0f, 4.5f, 2,   5.0f, 120.0f, 2, 40.0f, 8.0f),
            P ("Vokal mann – Myk ballade",                -18.0f, 2.0f, 0,  10.0f, 300.0f, 1, 25.0f, 3.0f),
            P ("Vokal mann – Lead med kant",              -24.0f, 4.0f, 1,  15.0f, 250.0f, 2, 60.0f, 6.0f),
            P ("Vokal mann – Rap og tale",                -24.0f, 6.0f, 2,   3.0f,  80.0f, 2, 30.0f, 7.0f),
            P ("Vokal mann – Parallell tykkelse",         -34.0f, 8.0f, 1,   2.0f, 150.0f, 2, 60.0f, 8.0f, 45.0f),
            P ("Vokal mann – Nær og intim",               -20.0f, 2.5f, 2,  20.0f, 400.0f, 1, 20.0f, 4.0f),

            // ---- Acoustic guitar ----
            P ("Akustisk gitar – Fingerspill naturlig",   -18.0f, 2.0f, 0,  10.0f, 300.0f, 2, 15.0f, 2.0f),
            P ("Akustisk gitar – Strumming jevn",         -24.0f, 3.0f, 1,  20.0f, 200.0f, 3, 20.0f, 4.0f),
            P ("Akustisk gitar – Varm opto",              -24.0f, 3.0f, 0,  10.0f, 300.0f, 2, 50.0f, 4.0f),
            P ("Akustisk gitar – Bakgrunn i miks",        -30.0f, 5.0f, 2,  10.0f, 150.0f, 3, 30.0f, 7.0f),
            P ("Akustisk gitar – Parallell glans",        -36.0f, 10.0f, 1,  1.0f, 100.0f, 3, 40.0f, 6.0f, 40.0f),
            P ("Akustisk gitar – Perkusjon og anslag",    -22.0f, 4.0f, 1,  35.0f, 120.0f, 3, 25.0f, 4.0f),
        };
    }
}
