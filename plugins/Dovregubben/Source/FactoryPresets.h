#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default.
namespace dovre
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        // note indices in noteNames
        enum { n16 = 5, n8T = 6, n16D = 7, n8 = 8, n4T = 9, n8D = 10, n4 = 11, n4D = 13, n2 = 14 };
        // delay: > 0 = milliseconds, <= 0 = synced note (-index)
        auto P = [] (const char* name, Mode m, Style st, float delayOrNote, float rat, float fb, float mx, float wd, float ag,
                     float df, float dsz, float rate, float depth, float lo, float hi, float pi, float dt, float dk)
        {
            const bool synced = delayOrNote <= 0.0f;
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)),
                { { mode, (float) m }, { style, (float) st }, { sync, synced ? 1.0f : 0.0f },
                  { delay, synced ? 375.0f : delayOrNote }, { note, synced ? -delayOrNote : 10.0f },
                  { ratio, rat }, { feedback, fb }, { mix, mx }, { width, wd }, { age, ag },
                  { diffusion, df }, { diffSize, dsz }, { modRate, rate }, { modDepth, depth },
                  { lowCut, lo }, { highCut, hi }, { pitch, pi }, { detune, dt }, { duck, dk } } };
        };
        auto N = [] (int idx) { return -(float) idx; };

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            //  name                                         mode     style     delay      R%    fb   mix  width age  diff size rate  depth lowcut highcut pitch detune duck
            // ---- Doubling ----
            P ("Dobling – Klassisk ADT",                   Pitch,   Dual,     24.0f,     133,   0,  50, 100,   0,   0, 50, 0.60f, 18,  120, 12000,  0,   7,   0),
            P ("Dobling – Tett vokaldobling",              Pitch,   Dual,     18.0f,     150,   0,  45, 100,   0,   0, 50, 0.45f, 12,  150, 10000,  0,   5,   0),
            P ("Dobling – Bred stereo-vokal",              Pitch,   Dual,     22.0f,     145,   0,  55, 100,   0,   0, 50, 0.80f, 22,  150, 11000,  0,  11,   0),
            P ("Dobling – Tape-dobbel (ADT)",              Tape,    Dual,     28.0f,     125,   0,  45, 100,  30,   0, 50, 0.70f, 28,  100,  9000,  0,   0,   0),
            P ("Dobling – Bakgrunnsvokal-kor",             Pitch,   Dual,     30.0f,     140,   0,  55, 100,   0,  15, 30, 1.10f, 35,  180,  9000,  0,  16,   0),
            P ("Dobling – Oktav over (subtil)",            Pitch,   Dual,     25.0f,     120,   0,  25, 100,   0,  10, 30, 0.50f, 10,  300,  9000, 12,   0,   0),
            P ("Dobling – Lo-fi BBD-dobbel",               BBD,     Dual,     20.0f,     160,   0,  45, 100,  35,   0, 50, 0.90f, 30,  150,  8000,  0,   0,   0),
            P ("Dobling – Akustisk gitar dobbeltspor",     Pitch,   Dual,     26.0f,     135,   0,  50, 100,   0,   0, 50, 0.50f, 25,  150, 12000,  0,   9,   0),
            P ("Dobling – Akustisk gitar bred",            HiFi,    Dual,     14.0f,     171,   0,  50, 100,   0,   0, 50, 0.30f, 10,  200, 14000,  0,   0,   0),
            P ("Dobling – Gitar og vokal lim",             Pitch,   Dual,     20.0f,     140,   0,  35, 100,   0,   0, 50, 0.50f, 12,  180,  9000,  0,   6,   0),

            // ---- Acoustic guitar ----
            P ("Akustisk gitar – Slapback rom",            Tape,    Dual,     95.0f,     110,  10,  20, 100,  20,   0, 50, 0.50f, 15,  150,  7000,  0,   0,   0),
            P ("Akustisk gitar – Strumming 1/8 tape",      Tape,    Dual,     N (n8),    100,  25,  18, 100,  30,   0, 50, 0.50f, 15,  250,  5000,  0,   0,  40),
            P ("Akustisk gitar – Fingerspill 1/4 ping-pong", HiFi,  PingPong, N (n4),    100,  30,  20, 100,  10,  20, 50, 0.40f, 15,  200,  6000,  0,   0,  30),
            P ("Akustisk gitar – Prikket 1/8 bred",        HiFi,    PingPong, N (n8D),   100,  35,  20, 100,  10,   0, 50, 0.40f, 10,  200,  7000,  0,   0,  25),
            P ("Akustisk gitar – Diffust rom-ekko",        HiFi,    Dual,     180.0f,    130,  35,  22, 100,  15,  70, 60, 0.40f, 20,  200,  6000,  0,   0,  20),
            P ("Akustisk gitar – Vintage BBD",             BBD,     Dual,     320.0f,    100,  35,  20, 100,  40,   0, 50, 0.60f, 25,  150,  6000,  0,   0,  25),
            P ("Akustisk gitar – Shimmer-ekko",            Pitch,   PingPong, N (n4),    100,  45,  25, 100,   0,  60, 70, 0.30f, 15,  300,  9000, 12,   0,  30),
            P ("Akustisk gitar – Reverse svell",           Reverse, Dual,     N (n2),    100,  30,  25, 100,  10,  30, 50, 0.40f, 10,  200,  8000,  0,   0,   0),
            P ("Akustisk gitar – Ghost-atmosfære",         Ghost,   PingPong, N (n4D),   100,  55,  25, 100,  20,  60, 60, 0.30f, 30,  200,  8000,  0,  20,  20),

            // ---- Vocal ----
            P ("Vokal – Slapback 50-tall",                 Tape,    Single,   110.0f,    100,   8,  22, 100,  35,   0, 50, 0.50f, 15,  200,  5000,  0,   0,   0),
            P ("Vokal – Pop 1/4 med ducking",              HiFi,    Dual,     N (n4),    100,  25,  20, 100,  10,   0, 50, 0.50f, 10,  250,  6000,  0,   0,  60),
            P ("Vokal – Ballade 1/8D tape",                Tape,    Dual,     N (n8D),   100,  30,  18, 100,  25,   0, 50, 0.50f, 20,  200,  6000,  0,   0,  50),
            P ("Vokal – Ping-pong 1/8",                    HiFi,    PingPong, N (n8),    100,  35,  18, 100,  10,   0, 50, 0.50f, 10,  250,  7000,  0,   0,  45),
            P ("Vokal – Lang hale (throw)",                Tape,    PingPong, N (n4D),   100,  55,  25, 100,  30,  25, 50, 0.40f, 20,  300,  4500,  0,   0,  70),
            P ("Vokal – Diffus plate-ekko",                HiFi,    Dual,     140.0f,    120,  30,  25, 100,  10,  80, 45, 0.50f, 15,  200,  8000,  0,   0,  30),
            P ("Vokal – Radio-telefon ekko",               BBD,     Single,   250.0f,    100,  40,  25, 100,  60,   0, 50, 0.70f, 20,  600,  3000,  0,   0,  30),
            P ("Vokal – Reverse-inngang",                  Reverse, Dual,     N (n4),    100,  20,  22, 100,  10,  20, 50, 0.40f, 10,  250,  7000,  0,   0,   0),
            P ("Vokal – Ghost-hvisking",                   Ghost,   PingPong, N (n8D),   100,  50,  20, 100,  20,  50, 50, 0.40f, 25,  300,  6000,  0,  15,  50),
            P ("Vokal – Rap og tale kort ekko",            Digital, Single,   N (n16),   100,  15,  15, 100,   0,   0, 50, 0.50f,  0,  300,  6000,  0,   0,  70),

            // ---- Guitar + vocal on the same track ----
            P ("Gitar + vokal – Myk 1/4 med ducking",      Tape,    Dual,     N (n4),    100,  25,  18, 100,  20,   0, 50, 0.50f, 15,  250,  5500,  0,   0,  55),
            P ("Gitar + vokal – Ping-pong 1/8D",           HiFi,    PingPong, N (n8D),   100,  30,  16, 100,  10,   0, 50, 0.50f, 10,  300,  6000,  0,   0,  50),
            P ("Gitar + vokal – Slapback kafé",            Tape,    Dual,     100.0f,    115,  10,  18, 100,  25,   0, 50, 0.50f, 15,  200,  6000,  0,   0,  20),
            P ("Gitar + vokal – Romklang uten reverb",     HiFi,    Dual,     70.0f,     140,  25,  20, 100,  10,  75, 35, 0.40f, 15,  200,  7000,  0,   0,  30),
            P ("Gitar + vokal – Ballade bred hale",        Tape,    PingPong, N (n4D),   100,  45,  18, 100,  25,  35, 50, 0.40f, 20,  300,  5000,  0,   0,  65),
            P ("Gitar + vokal – Vintage BBD-varme",        BBD,     Dual,     N (n8D),   100,  30,  16, 100,  35,   0, 50, 0.60f, 20,  250,  6000,  0,   0,  45),
            P ("Gitar + vokal – Dobling og bredde",        Pitch,   Dual,     20.0f,     145,   0,  35, 100,   0,   0, 50, 0.50f, 12,  180, 10000,  0,   6,   0),
            P ("Gitar + vokal – Tape-dobling varm",        Tape,    Dual,     26.0f,     130,   0,  35, 100,  30,   0, 50, 0.60f, 25,  150,  8000,  0,   0,   0),
        };
    }
}
