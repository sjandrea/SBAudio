#pragma once

#include "PluginProcessor.h"

// Built-in PrismEQ presets in the shared sbaudio::FactoryPreset format.
// Bands not listed stay unused; parameters not listed use their default.
namespace prism
{
namespace factorydetail
{
    struct B
    {
        int type; float freq, gain, q;
        int slope = 3;
        bool dyn = false; float range = -6.0f, thr = -30.0f, att = 5.0f, rel = 80.0f;
        bool sc = false;
        int place = Stereo;
    };

    inline B cut (int type, float f, int slope, float q = 0.71f) { B b { type, f, 0.0f, q }; b.slope = slope; return b; }
    inline B dynBand (int type, float f, float gain, float q, float range, float thr, float att, float rel, bool sc = false)
    {
        B b { type, f, gain, q };
        b.dyn = true; b.range = range; b.thr = thr; b.att = att; b.rel = rel; b.sc = sc;
        return b;
    }

    struct Factory { const char* name; std::vector<B> bands; };

    // Slope index: 0 = 6, 1 = 12, 2 = 18, 3 = 24 dB/oct
    inline const std::vector<Factory>& list()
    {
        static const std::vector<Factory> list {
            { "Standard (flat)", {} },

            { "Vokal – Ren og klar", {
                cut (LowCut, 80.0f, 3),
                { Bell, 300.0f, -2.5f, 1.0f },
                { Bell, 3000.0f, 2.0f, 0.9f },
                { HighShelf, 11000.0f, 2.5f, 0.71f },
                dynBand (Bell, 6800.0f, 0.0f, 3.0f, -6.0f, -32.0f, 1.0f, 60.0f) } },

            { "Vokal – Varm og nær", {
                cut (LowCut, 70.0f, 2),
                { LowShelf, 180.0f, 1.5f, 0.71f },
                dynBand (Bell, 3200.0f, 0.0f, 1.4f, -4.0f, -30.0f, 3.0f, 100.0f),
                { HighShelf, 12000.0f, 1.5f, 0.71f } } },

            { "Vokal – Luft og nærvær", {
                cut (LowCut, 90.0f, 3),
                { Bell, 4500.0f, 2.5f, 0.7f },
                { HighShelf, 14000.0f, 4.0f, 0.6f },
                dynBand (Bell, 7000.0f, 0.0f, 3.5f, -5.0f, -34.0f, 1.0f, 60.0f) } },

            { "Vokal – De-esser", {
                cut (LowCut, 60.0f, 1),
                dynBand (Bell, 6500.0f, 0.0f, 2.5f, -9.0f, -36.0f, 0.5f, 50.0f) } },

            { "Vokal – Proximity-kontroll", {
                cut (LowCut, 70.0f, 2),
                dynBand (LowShelf, 220.0f, 0.0f, 0.71f, -6.0f, -26.0f, 10.0f, 150.0f) } },

            { "Vokal – Telefon", {
                cut (LowCut, 500.0f, 3, 1.2f),
                cut (HighCut, 3500.0f, 3, 1.2f),
                { Bell, 1500.0f, 4.0f, 1.2f } } },

            { "Akustisk gitar – Klar og luftig", {
                cut (LowCut, 90.0f, 2),
                { Bell, 220.0f, -3.0f, 1.2f },
                { Bell, 5000.0f, 2.0f, 0.8f },
                { HighShelf, 12000.0f, 2.5f, 0.71f } } },

            { "Akustisk gitar – Slank i miks", {
                cut (LowCut, 140.0f, 3),
                { Bell, 250.0f, -3.5f, 1.1f },
                { Bell, 3500.0f, 1.5f, 0.9f },
                { HighShelf, 10000.0f, 1.5f, 0.71f } } },

            { "Akustisk gitar – Temme boom", {
                cut (LowCut, 60.0f, 1),
                dynBand (Bell, 140.0f, 0.0f, 1.4f, -6.0f, -24.0f, 8.0f, 120.0f),
                dynBand (Bell, 2500.0f, 0.0f, 2.0f, -3.0f, -30.0f, 3.0f, 80.0f) } },

            { "Akustisk gitar – Varm fingerspill", {
                cut (LowCut, 60.0f, 1),
                { LowShelf, 150.0f, 1.5f, 0.71f },
                { Bell, 1000.0f, -1.5f, 1.0f },
                dynBand (Bell, 3000.0f, 0.0f, 2.0f, -3.0f, -32.0f, 2.0f, 70.0f),
                { HighShelf, 9000.0f, 1.0f, 0.71f } } },

            { "Akustisk gitar – Plass til vokal (sidechain)", {
                cut (LowCut, 100.0f, 2),
                dynBand (Bell, 2500.0f, 0.0f, 0.8f, -4.0f, -30.0f, 10.0f, 150.0f, true),
                { HighShelf, 10000.0f, 1.0f, 0.71f } } },
        };
        return list;
    }

}

inline std::vector<sbaudio::FactoryPreset> factoryPresets()
{
    std::vector<sbaudio::FactoryPreset> result;
    for (const auto& f : factorydetail::list())
    {
        sbaudio::FactoryPreset p { juce::String (juce::CharPointer_UTF8 (f.name)), {} };
        int b = 0;
        for (const auto& s : f.bands)
        {
            auto set = [&] (const char* name, float v) { p.values.push_back ({ ParamIDs::band (b, name), v }); };
            set ("type", (float) s.type);
            set ("freq", s.freq);
            set ("gain", s.gain);
            set ("q", s.q);
            set ("slope", (float) s.slope);
            set ("place", (float) s.place);
            set ("dyn", s.dyn ? 1.0f : 0.0f);
            set ("range", s.range);
            set ("thr", s.thr);
            set ("att", s.att);
            set ("rel", s.rel);
            set ("sc", s.sc ? 1.0f : 0.0f);
            set ("on", 1.0f);
            set ("used", 1.0f);
            ++b;
        }
        result.push_back (std::move (p));
    }
    return result;
}
}
