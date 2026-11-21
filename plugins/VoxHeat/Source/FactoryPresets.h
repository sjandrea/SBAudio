#pragma once

#include "PluginProcessor.h"

// Built-in vocal presets. Values are plain parameter values; parameters not listed use their default.
// Assumes a vocal recorded around -18 dBFS RMS (peaks -10 to -6 dBFS). With Auto Gain on (default)
// the output level stays close to the input, so presets can be compared directly.
// Character: 0 = TG (solid-state console, forward), 1 = Tube (valve, warm).
namespace voxheat
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, float drv, int chr, float dens, float tn, float mx = 100.0f,
                     float in = 0.0f, float out = 0.0f)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { input, in }, { drive, drv }, { character, (float) chr }, { density, dens },
                  { tone, tn }, { mix, mx }, { output, out }, { autogain, 1.0f } } };
        };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                       drive char dens  tone   mix   in    out
            // ---- Lead vocal ----
            P ("Vokal – Varm og nær",                      30.0f, 1, 35.0f, -1.0f),
            P ("Vokal – Pop glans",                        40.0f, 0, 45.0f,  2.0f,  85.0f),
            P ("Vokal – Moderne pop, tett",                50.0f, 0, 60.0f,  1.5f),
            P ("Vokal – Myk ballade",                      20.0f, 1, 20.0f, -1.5f),
            P ("Vokal – Soul og R&B",                      45.0f, 1, 40.0f, -0.5f,  90.0f),
            P ("Vokal – Rock med kant",                    70.0f, 0, 50.0f,  1.0f, 100.0f, 2.0f),
            P ("Vokal – Rap foran i miksen",               55.0f, 0, 65.0f,  1.0f, 100.0f, 2.0f),

            // ---- Voice type ----
            P ("Vokal mann – Dyp og varm",                 35.0f, 1, 35.0f,  0.5f),
            P ("Vokal mann – Presens og tyngde",           45.0f, 0, 50.0f,  1.0f),
            P ("Vokal kvinne – Luftig",                    30.0f, 0, 35.0f,  2.5f,  80.0f),
            P ("Vokal kvinne – Fyldig rør",                35.0f, 1, 40.0f, -1.0f,  90.0f),

            // ---- Special ----
            P ("Vokal – Bakgrunnskor",                     45.0f, 1, 55.0f, -2.0f),
            P ("Vokal – Parallell tykkelse",               85.0f, 1, 70.0f,  0.0f,  35.0f),
            P ("Vokal – Lo-fi radio",                      90.0f, 0, 40.0f, -3.0f, 100.0f, 4.0f, -2.0f),
            P ("Tale og podkast – Klar",                   25.0f, 0, 50.0f,  1.0f),
        };
    }
}
