#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values; parameters not listed use their default
// (every module off). Choices: dc_mode 0 = Auto; hm_base 0 = Auto, 1 = 50 Hz, 2 = 60 Hz;
// dn_src 0 = Adaptive, 1 = Learned; dn_type 0 = General, 1 = Voice, 2 = Guitar; ds_mode 0 = Classic, 1 = Spectral.
namespace renske
{
    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace ParamIDs;
        auto P = [] (const char* name, std::vector<std::pair<juce::String, float>> v)
        {
            return sbaudio::FactoryPreset { juce::String (juce::CharPointer_UTF8 (name)), std::move (v) };
        };

        return {
            P ("Standard", {}),

            // ---- speech ----
            P ("Tale – Podkast-rens", { { dnOn, 1 }, { dnType, 1 }, { dnRed, 12 }, { mcOn, 1 }, { mcSens, 45 },
                                        { plOn, 1 }, { dsOn, 1 }, { dsThr, -32 }, { brOn, 1 }, { brGain, -10 } }),
            P ("Tale – Lett støyvask", { { dnOn, 1 }, { dnType, 1 }, { dnRed, 8 }, { dnSmooth, 50 } }),
            P ("Tale – Bråkete opptak", { { isOn, 1 }, { isAmt, 70 }, { isBg, -24 }, { dnOn, 1 }, { dnType, 1 }, { dnRed, 10 } }),
            P ("Tale – Mindre rom", { { rvOn, 1 }, { rvAmt, 55 }, { rvTail, 0.8f }, { dnOn, 1 }, { dnType, 1 }, { dnRed, 8 } }),
            P ("Tale – Kirke og hall", { { rvOn, 1 }, { rvAmt, 70 }, { rvTail, 2.5f } }),
            P ("Tale – Telefon og Zoom", { { dnOn, 1 }, { dnType, 1 }, { dnRed, 15 }, { ckOn, 1 }, { ckSens, 40 }, { dcOn, 1 } }),

            // ---- vocal ----
            P ("Vokal – Munnlyder og pust", { { mcOn, 1 }, { mcSens, 55 }, { brOn, 1 }, { brGain, -12 } }),
            P ("Vokal – Popp og S", { { plOn, 1 }, { plSens, 55 }, { dsOn, 1 }, { dsThr, -30 }, { dsMax, 10 } }),
            P ("Vokal – Klikkspor-lekkasje", { { blOn, 1 }, { blAmt, 70 }, { blMax, 24 } }),

            // ---- instruments ----
            P ("Gitar – Forsterkersus og brumm", { { dnOn, 1 }, { dnType, 2 }, { dnRed, 10 }, { hmOn, 1 }, { hmHarm, 8 } }),
            P ("Gitar – Akustisk rens", { { dnOn, 1 }, { dnType, 2 }, { dnRed, 8 }, { dnSmooth, 55 } }),

            // ---- fixes ----
            P ("Brumm 50 Hz", { { hmOn, 1 }, { hmBase, 1 }, { hmHarm, 8 } }),
            P ("Brumm 60 Hz", { { hmOn, 1 }, { hmBase, 2 }, { hmHarm, 8 } }),
            P ("Klippet opptak", { { dcOn, 1 }, { dcMode, 0 }, { dcGain, -4 } }),
            P ("Digitale klikk", { { ckOn, 1 }, { ckSens, 60 }, { ckWidth, 1.0f } }),

            // ---- restoration ----
            P ("Vinyl – Lett", { { ckOn, 1 }, { ckSens, 45 }, { crOn, 1 }, { crAmt, 30 } }),
            P ("Vinyl – Gammel og slitt", { { ckOn, 1 }, { ckSens, 65 }, { ckWidth, 3.0f }, { crOn, 1 }, { crAmt, 60 },
                                            { hmOn, 1 }, { dnOn, 1 }, { dnRed, 10 } }),
            P ("Kassett – Sus", { { dnOn, 1 }, { dnRed, 14 }, { dnThr, 2 } }),

            // ---- rebalance ----
            P ("Rebalanse – Vokal ned (karaoke)", { { rbOn, 1 }, { rbVox, -24 }, { rbSep, 70 } }),
            P ("Rebalanse – Bare vokal", { { rbOn, 1 }, { rbBass, -24 }, { rbPerc, -24 }, { rbOther, -24 }, { rbSep, 75 } }),
            P ("Rebalanse – Trommer ned", { { rbOn, 1 }, { rbPerc, -12 }, { rbSep, 60 } }),
            P ("Rebalanse – Vokal litt fram", { { rbOn, 1 }, { rbVox, 3 }, { rbSep, 40 } }),
        };
    }
}
