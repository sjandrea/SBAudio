#pragma once

#include "PluginProcessor.h"

// Built-in presets. Values are plain parameter values (same units as the GUI shows);
// parameters that are not listed use their default. Choice parameters use the list index.
namespace nordlys
{
    namespace presetbuild
    {
        struct Builder
        {
            explicit Builder (const char* name) : p { juce::String (juce::CharPointer_UTF8 (name)), {} } {}

            Builder& g (const char* id, float v) { p.values.push_back ({ id, v }); return *this; }

            /** Layer parameter: layer 0..3 (A..D). Setting anything on a layer turns it on. */
            Builder& l (int layer, const char* name, float v)
            {
                ensureOn (layer);
                p.values.push_back ({ layerId (layer, name), v });
                return *this;
            }
            Builder& off (int layer)
            {
                p.values.push_back ({ layerId (layer, lp::on), 0.0f });
                return *this;
            }

            /** Next free modulation slot. layer: 0 = all, 1..4 = A..D. amt in %. */
            Builder& mod (int src, int dst, float amt, int layer = 0, int via = srcOff)
            {
                const int m = slot++;
                jassert (m < numSlots);
                p.values.push_back ({ slotId (m, mp::src), (float) src });
                p.values.push_back ({ slotId (m, mp::via), (float) via });
                p.values.push_back ({ slotId (m, mp::dst), (float) dst });
                p.values.push_back ({ slotId (m, mp::layer), (float) layer });
                p.values.push_back ({ slotId (m, mp::amt), amt });
                return *this;
            }

            Builder& amp (int layer, float a, float d, float s, float r)
            { return l (layer, lp::aA, a).l (layer, lp::aD, d).l (layer, lp::aS, s).l (layer, lp::aR, r); }
            Builder& fenv (int layer, float amount, float a, float d, float s, float r)
            { return l (layer, lp::fenv, amount).l (layer, lp::fA, a).l (layer, lp::fD, d).l (layer, lp::fS, s).l (layer, lp::fR, r); }
            Builder& osc (int layer, int type, int wave, float shape)
            { return l (layer, lp::osc, (float) type).l (layer, lp::wave, (float) wave).l (layer, lp::shape, shape); }
            Builder& uni (int layer, int n, float detune, float spread)
            { return l (layer, lp::uni, (float) n).l (layer, lp::detune, detune).l (layer, lp::spread, spread); }
            Builder& filter (int layer, int type, float cutoff, float reso)
            { return l (layer, lp::ftype, (float) type).l (layer, lp::cutoff, cutoff).l (layer, lp::reso, reso); }
            Builder& grain (int layer, float size, float dens, float spray, float pitch, float shim)
            { return l (layer, lp::gsize, size).l (layer, lp::gdens, dens).l (layer, lp::gspray, spray).l (layer, lp::gpitch, pitch).l (layer, lp::gshim, shim); }
            Builder& lfo (int layer, int which, float rate, int shape, float fade = 0.0f)
            {
                return which == 1 ? l (layer, lp::l1rate, rate).l (layer, lp::l1shape, (float) shape).l (layer, lp::l1fade, fade)
                                  : l (layer, lp::l2rate, rate).l (layer, lp::l2shape, (float) shape).l (layer, lp::l2fade, fade);
            }
            Builder& reverb (int type, float mix, float decay, float tone = 0.0f)
            { return g (ParamIDs::revType, (float) type).g (ParamIDs::revMix, mix).g (ParamIDs::revDecay, decay).g (ParamIDs::revTone, tone); }
            Builder& delay (int time, float mix, float fb, float tone = 6000.0f)
            { return g (ParamIDs::dlyTime, (float) time).g (ParamIDs::dlyMix, mix).g (ParamIDs::dlyFb, fb).g (ParamIDs::dlyTone, tone); }
            Builder& chorus (float mix, float rate = 0.4f, float depth = 40.0f)
            { return g (ParamIDs::choMix, mix).g (ParamIDs::choRate, rate).g (ParamIDs::choDepth, depth); }
            Builder& orb (float x, float y, float rate, float depth)
            { return g (ParamIDs::orbX, x).g (ParamIDs::orbY, y).g (ParamIDs::orbRate, rate).g (ParamIDs::orbDepth, depth); }
            Builder& arp (int mode, int rate, int oct, float gate)
            { return g (ParamIDs::arpOn, 1.0f).g (ParamIDs::arpMode, (float) mode).g (ParamIDs::arpRate, (float) rate)
                      .g (ParamIDs::arpOct, (float) oct).g (ParamIDs::arpGate, gate); }

            operator sbaudio::FactoryPreset() const { return p; }

        private:
            void ensureOn (int layer)
            {
                if (onSet[layer]) return;
                onSet[layer] = true;
                p.values.push_back ({ layerId (layer, lp::on), 1.0f });
            }

            sbaudio::FactoryPreset p;
            bool onSet[numLayers] {};
            int slot = 0;
        };

        /** Symmetry value that selects an FM ratio (index into fmRatios). */
        inline float fmSym (int ratioIndex) { return 100.0f * (float) ratioIndex / (float) (numFmRatios - 1); }

        // wave banks
        enum { wAnalog = 0, wGlass, wChoir, wStrings, wOrgan, wDigital, wAir, wMetal };
        // reverb types
        enum { rAmbience = 0, rRoom, rChamber, rHall, rPlate, rVintage };
        // delay times (index into delayTimeNames)
        enum { d32 = 0, d16T, d16, d8T, d16D, d8, d4T, d8D, d4, d4D, d2 };
        // arp rates (index into arpRateNames)
        enum { a1 = 0, a2, a4, a8, a8T, a16, a16T, a32 };
    }

    inline std::vector<sbaudio::FactoryPreset> factoryPresets()
    {
        using namespace presetbuild;
        using B = Builder;
        constexpr int A = 0, Bl = 1, C = 2;   // layers (B is taken by the builder type)

        return {
            sbaudio::FactoryPreset { "Standard", {} },

            // ---------------- Pads ----------------
            B ("Pad – Nordlys")
                .osc (A, oscWavetable, wGlass, 30.0f).uni (A, 5, 18.0f, 80.0f).filter (A, fltLP24, 3000.0f, 15.0f)
                .amp (A, 1200.0f, 2000.0f, 85.0f, 3500.0f).fenv (A, 20.0f, 2000.0f, 4000.0f, 40.0f, 3000.0f)
                .lfo (A, 2, 0.15f, lfoTriangle)
                .osc (Bl, oscGranular, wChoir, 40.0f).grain (Bl, 250.0f, 8.0f, 40.0f, 10.0f, 30.0f).l (Bl, lp::level, 60.0f)
                .filter (Bl, fltLP24, 6000.0f, 10.0f).amp (Bl, 2500.0f, 2000.0f, 90.0f, 5000.0f).l (Bl, lp::spread, 90.0f)
                .mod (srcLfo2, dstShape, 25.0f, 1).mod (srcOrbX, dstCutoff, 35.0f).mod (srcOrbY, dstShape, 30.0f, 2)
                .mod (srcModWheel, dstCutoff, 40.0f)
                .orb (0.0f, 0.0f, 0.08f, 40.0f).chorus (30.0f).reverb (rHall, 40.0f, 6.0f),

            B ("Pad – Varm analog")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 7, 22.0f, 90.0f).filter (A, fltLP24, 1800.0f, 10.0f)
                .fenv (A, 25.0f, 800.0f, 3000.0f, 40.0f, 2500.0f).amp (A, 600.0f, 1500.0f, 90.0f, 2500.0f)
                .lfo (A, 2, 0.25f, lfoTriangle)
                .osc (Bl, oscAnalog, wAnalog, 100.0f).l (Bl, lp::oct, -1.0f).uni (Bl, 3, 8.0f, 60.0f).l (Bl, lp::level, 50.0f)
                .filter (Bl, fltLP24, 1200.0f, 5.0f).amp (Bl, 600.0f, 1500.0f, 90.0f, 2500.0f)
                .mod (srcLfo2, dstCutoff, 15.0f).mod (srcModWheel, dstCutoff, 40.0f).mod (srcOrbX, dstDetune, 20.0f)
                .chorus (40.0f).reverb (rHall, 30.0f, 4.0f),

            B ("Pad – Glassmorgen")
                .osc (A, oscWavetable, wGlass, 60.0f).uni (A, 3, 10.0f, 70.0f).filter (A, fltLP24, 9000.0f, 5.0f)
                .amp (A, 400.0f, 3000.0f, 60.0f, 3000.0f).lfo (A, 2, 0.2f, lfoSine)
                .osc (Bl, oscGranular, wGlass, 50.0f).grain (Bl, 80.0f, 10.0f, 60.0f, 5.0f, 60.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 55.0f).amp (Bl, 1500.0f, 2000.0f, 90.0f, 5000.0f).l (Bl, lp::spread, 100.0f)
                .mod (srcLfo2, dstShape, 30.0f, 1).mod (srcModWheel, dstGrainDensity, 50.0f, 2).mod (srcOrbX, dstCutoff, 30.0f)
                .delay (d4D, 25.0f, 45.0f, 5000.0f).reverb (rPlate, 35.0f, 5.0f, 20.0f),

            B ("Pad – Kor i katedral")
                .osc (A, oscWavetable, wChoir, 0.0f).uni (A, 5, 12.0f, 70.0f).filter (A, fltLP24, 5000.0f, 10.0f)
                .amp (A, 900.0f, 2000.0f, 90.0f, 4000.0f).lfo (A, 2, 0.08f, lfoDrift).lfo (A, 1, 5.0f, lfoSine, 1500.0f)
                .osc (Bl, oscGranular, wChoir, 30.0f).grain (Bl, 300.0f, 8.0f, 30.0f, 12.0f, 40.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 45.0f).amp (Bl, 2000.0f, 2000.0f, 90.0f, 5000.0f)
                .mod (srcLfo2, dstShape, 50.0f, 1).mod (srcLfo1, dstFine, 8.0f).mod (srcModWheel, dstShape, 40.0f)
                .mod (srcOrbY, dstLevel, 60.0f, 2)
                .reverb (rHall, 50.0f, 8.0f, -10.0f),

            B ("Pad – Dyp drone")
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).uni (A, 7, 30.0f, 100.0f).filter (A, fltLP24, 600.0f, 30.0f)
                .amp (A, 3000.0f, 2000.0f, 100.0f, 6000.0f).lfo (A, 2, 0.1f, lfoDrift)
                .osc (Bl, oscGranular, wMetal, 50.0f).grain (Bl, 500.0f, 5.0f, 70.0f, 30.0f, 10.0f).l (Bl, lp::level, 50.0f)
                .amp (Bl, 4000.0f, 2000.0f, 100.0f, 6000.0f)
                .osc (C, oscNoise, wAnalog, 40.0f).filter (C, fltBP, 1200.0f, 60.0f).l (C, lp::level, 30.0f)
                .amp (C, 4000.0f, 2000.0f, 100.0f, 6000.0f)
                .mod (srcLfo2, dstCutoff, 30.0f, 1).mod (srcOrbX, dstCutoff, 50.0f).mod (srcOrbY, dstLevel, 80.0f, 3)
                .mod (srcModWheel, dstGrainDensity, 60.0f, 2)
                .orb (-20.0f, 0.0f, 0.05f, 60.0f).reverb (rVintage, 45.0f, 12.0f),

            B ("Pad – Vinterlys")
                .osc (A, oscWavetable, wAir, 40.0f).uni (A, 5, 14.0f, 80.0f).filter (A, fltLP24, 4000.0f, 10.0f)
                .amp (A, 1500.0f, 2000.0f, 90.0f, 4000.0f).lfo (A, 2, 0.12f, lfoDrift)
                .osc (Bl, oscGranular, wGlass, 60.0f).grain (Bl, 150.0f, 8.0f, 50.0f, 6.0f, 50.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 40.0f).amp (Bl, 2500.0f, 2000.0f, 100.0f, 5000.0f).l (Bl, lp::spread, 100.0f)
                .mod (srcLfo2, dstShape, 30.0f, 1).mod (srcOrbX, dstCutoff, 35.0f).mod (srcOrbY, dstGrainDensity, 40.0f, 2)
                .mod (srcModWheel, dstLevel, 50.0f, 2)
                .orb (0.0f, 0.0f, 0.06f, 35.0f).chorus (25.0f).reverb (rHall, 45.0f, 7.0f, 10.0f),

            B ("Pad – Polarnatt")
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).uni (A, 7, 25.0f, 100.0f).filter (A, fltLP24, 900.0f, 20.0f)
                .fenv (A, 30.0f, 4000.0f, 6000.0f, 30.0f, 4000.0f).amp (A, 2000.0f, 3000.0f, 100.0f, 5000.0f)
                .osc (Bl, oscWavetable, wChoir, 60.0f).uni (Bl, 3, 10.0f, 70.0f).l (Bl, lp::level, 50.0f).filter (Bl, fltLP24, 3000.0f, 10.0f)
                .amp (Bl, 3000.0f, 3000.0f, 100.0f, 6000.0f).lfo (Bl, 2, 0.05f, lfoDrift)
                .mod (srcLfo2, dstShape, 40.0f, 2).mod (srcModWheel, dstCutoff, 45.0f).mod (srcOrbX, dstCutoff, 40.0f, 1)
                .orb (-30.0f, 0.0f, 0.04f, 40.0f).reverb (rVintage, 50.0f, 10.0f, -20.0f),

            B ("Pad – Sølvstrenger")
                .osc (A, oscWavetable, wStrings, 70.0f).uni (A, 7, 16.0f, 100.0f).filter (A, fltLP24, 6000.0f, 8.0f)
                .amp (A, 700.0f, 2000.0f, 90.0f, 2500.0f).lfo (A, 1, 4.5f, lfoSine, 1200.0f)
                .osc (Bl, oscGranular, wGlass, 40.0f).grain (Bl, 200.0f, 8.0f, 40.0f, 8.0f, 40.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 35.0f).amp (Bl, 1500.0f, 2000.0f, 90.0f, 4000.0f)
                .mod (srcLfo1, dstFine, 6.0f, 1).mod (srcModWheel, dstShape, 40.0f, 1).mod (srcOrbX, dstCutoff, 30.0f)
                .chorus (35.0f).reverb (rHall, 40.0f, 5.0f, 15.0f),

            B ("Pad – Myk 80-talls")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 4, 12.0f, 80.0f).filter (A, fltLP24, 2200.0f, 15.0f)
                .fenv (A, 20.0f, 600.0f, 2000.0f, 50.0f, 1500.0f).amp (A, 300.0f, 1500.0f, 85.0f, 1500.0f)
                .osc (Bl, oscAnalog, wAnalog, 100.0f).l (Bl, lp::sym, 40.0f).l (Bl, lp::level, 45.0f).filter (Bl, fltLP24, 2000.0f, 10.0f)
                .amp (Bl, 300.0f, 1500.0f, 85.0f, 1500.0f).lfo (Bl, 1, 0.5f, lfoTriangle)
                .mod (srcLfo1, dstSym, 25.0f, 2).mod (srcModWheel, dstCutoff, 40.0f).mod (srcVelocity, dstCutoff, 20.0f)
                .chorus (60.0f, 0.6f, 50.0f).reverb (rPlate, 30.0f, 3.0f),

            B ("Pad – Pulserende")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 5, 15.0f, 90.0f).filter (A, fltLP24, 2500.0f, 20.0f)
                .amp (A, 400.0f, 1000.0f, 100.0f, 1200.0f).lfo (A, 1, 4.0f, lfoSquare)
                .osc (Bl, oscWavetable, wGlass, 50.0f).l (Bl, lp::level, 40.0f).amp (Bl, 800.0f, 1000.0f, 100.0f, 2500.0f)
                .mod (srcLfo1, dstLevel, -70.0f, 1).mod (srcModWheel, dstLfo1Rate, 50.0f, 1).mod (srcOrbX, dstCutoff, 40.0f)
                .delay (d8, 20.0f, 30.0f).reverb (rHall, 35.0f, 4.0f),

            B ("Pad – Fjellvann")
                .osc (A, oscFM, wAnalog, 15.0f).l (A, lp::sym, fmSym (3)).uni (A, 3, 8.0f, 70.0f).filter (A, fltLP24, 5000.0f, 5.0f)
                .amp (A, 1200.0f, 2000.0f, 90.0f, 4000.0f)
                .osc (Bl, oscWavetable, wAir, 50.0f).uni (Bl, 3, 10.0f, 80.0f).l (Bl, lp::level, 55.0f)
                .amp (Bl, 2000.0f, 2000.0f, 100.0f, 5000.0f).lfo (Bl, 2, 0.1f, lfoSine)
                .mod (srcLfo2, dstShape, 35.0f, 2).mod (srcModWheel, dstShape, 30.0f, 1).mod (srcOrbX, dstShape, 25.0f, 1)
                .delay (d4D, 15.0f, 40.0f, 4000.0f).reverb (rHall, 45.0f, 6.0f),

            B ("Pad – Bølgende hav")
                .osc (A, oscWavetable, wDigital, 30.0f).uni (A, 5, 12.0f, 90.0f).filter (A, fltLP24, 3000.0f, 15.0f)
                .amp (A, 1500.0f, 2000.0f, 100.0f, 4000.0f).lfo (A, 1, 0.07f, lfoTriangle).lfo (A, 2, 0.11f, lfoSine)
                .mod (srcLfo2, dstShape, 50.0f, 1).mod (srcLfo1, dstPan, 60.0f, 1).mod (srcLfo2, dstCutoff, 20.0f)
                .mod (srcModWheel, dstCutoff, 40.0f)
                .reverb (rHall, 45.0f, 8.0f),

            B ("Pad – Glødende kor")
                .osc (A, oscWavetable, wChoir, 25.0f).uni (A, 5, 12.0f, 80.0f).filter (A, fltLP24, 4000.0f, 15.0f).l (A, lp::drive, 25.0f)
                .amp (A, 800.0f, 2000.0f, 90.0f, 3500.0f).lfo (A, 2, 0.09f, lfoDrift)
                .osc (Bl, oscGranular, wChoir, 50.0f).grain (Bl, 350.0f, 8.0f, 40.0f, 10.0f, 60.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 40.0f).amp (Bl, 2000.0f, 2000.0f, 100.0f, 5000.0f)
                .mod (srcLfo2, dstShape, 45.0f, 1).mod (srcModWheel, dstGrainDensity, 50.0f, 2).mod (srcOrbY, dstShape, 40.0f)
                .orb (0.0f, 20.0f, 0.05f, 40.0f).chorus (20.0f).reverb (rHall, 50.0f, 7.0f),

            B ("Pad – Lys og luft")
                .osc (A, oscWavetable, wAir, 80.0f).uni (A, 5, 15.0f, 100.0f).filter (A, fltHP, 300.0f, 10.0f)
                .amp (A, 1000.0f, 2000.0f, 90.0f, 3500.0f)
                .osc (Bl, oscNoise, wAnalog, 75.0f).filter (Bl, fltBP, 4000.0f, 30.0f).l (Bl, lp::level, 25.0f).l (Bl, lp::spread, 100.0f)
                .amp (Bl, 1500.0f, 2000.0f, 100.0f, 4000.0f).lfo (Bl, 2, 0.15f, lfoDrift)
                .mod (srcLfo2, dstCutoff, 30.0f, 2).mod (srcOrbX, dstShape, 40.0f, 1).mod (srcModWheel, dstLevel, 60.0f, 2)
                .orb (0.0f, 0.0f, 0.07f, 40.0f).reverb (rPlate, 40.0f, 5.0f, 30.0f),

            B ("Pad – Tidevann")
                .osc (A, oscWavetable, wGlass, 20.0f).uni (A, 5, 14.0f, 90.0f).filter (A, fltLP24, 1500.0f, 25.0f)
                .amp (A, 1500.0f, 2000.0f, 100.0f, 4500.0f)
                .osc (Bl, oscGranular, wAir, 60.0f).grain (Bl, 300.0f, 10.0f, 60.0f, 15.0f, 20.0f).l (Bl, lp::level, 50.0f)
                .amp (Bl, 2000.0f, 2000.0f, 100.0f, 5000.0f).l (Bl, lp::spread, 100.0f)
                .mod (srcOrbX, dstCutoff, 70.0f, 1).mod (srcOrbY, dstShape, 60.0f, 1).mod (srcOrbX, dstGrainSize, 50.0f, 2)
                .mod (srcOrbY, dstLevel, -50.0f, 2)
                .orb (0.0f, 0.0f, 0.04f, 80.0f).reverb (rHall, 50.0f, 8.0f),

            // ---------------- Textures / atmospheres ----------------
            B ("Tekstur – Granulær vind")
                .osc (A, oscGranular, wAir, 50.0f).grain (A, 400.0f, 12.0f, 90.0f, 40.0f, 20.0f).filter (A, fltLP24, 4000.0f, 10.0f)
                .l (A, lp::spread, 100.0f).amp (A, 1500.0f, 2000.0f, 100.0f, 4000.0f)
                .osc (Bl, oscNoise, wAnalog, 55.0f).filter (Bl, fltBP, 900.0f, 50.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 2000.0f, 2000.0f, 100.0f, 4000.0f).lfo (Bl, 2, 0.2f, lfoDrift).l (Bl, lp::spread, 100.0f)
                .mod (srcLfo2, dstCutoff, 40.0f, 2).mod (srcOrbX, dstShape, 50.0f, 1).mod (srcModWheel, dstCutoff, 40.0f)
                .orb (0.0f, 0.0f, 0.12f, 50.0f).delay (d4, 20.0f, 40.0f, 4000.0f).reverb (rHall, 50.0f, 9.0f),

            B ("Tekstur – Skimrende stjerner")
                .osc (A, oscGranular, wGlass, 70.0f).grain (A, 40.0f, 14.0f, 100.0f, 20.0f, 80.0f).l (A, lp::oct, 1.0f)
                .l (A, lp::spread, 100.0f).filter (A, fltHP, 400.0f, 5.0f).amp (A, 600.0f, 2000.0f, 100.0f, 4000.0f)
                .mod (srcModWheel, dstGrainSize, -40.0f).mod (srcOrbX, dstShape, 40.0f)
                .delay (d8D, 30.0f, 50.0f, 7000.0f).reverb (rPlate, 45.0f, 7.0f, 30.0f),

            B ("Tekstur – Digitalt hav")
                .osc (A, oscWavetable, wDigital, 40.0f).uni (A, 4, 14.0f, 80.0f).filter (A, fltLP24, 2500.0f, 25.0f)
                .amp (A, 800.0f, 2000.0f, 90.0f, 3000.0f).lfo (A, 1, 6.0f, lfoSH).lfo (A, 2, 0.15f, lfoDrift)
                .mod (srcLfo1, dstShape, 30.0f, 0, srcModWheel).mod (srcLfo2, dstCutoff, 30.0f).mod (srcOrbX, dstShape, 40.0f)
                .orb (0.0f, 0.0f, 0.1f, 30.0f).chorus (25.0f).reverb (rHall, 40.0f, 6.0f),

            B ("Tekstur – Isflak")
                .osc (A, oscGranular, wMetal, 60.0f).grain (A, 25.0f, 10.0f, 100.0f, 50.0f, 30.0f).l (A, lp::oct, 1.0f)
                .filter (A, fltHP, 1500.0f, 10.0f).l (A, lp::spread, 100.0f).amp (A, 500.0f, 2000.0f, 100.0f, 3000.0f)
                .mod (srcModWheel, dstGrainDensity, 60.0f).mod (srcOrbX, dstShape, 50.0f).mod (srcOrbY, dstCutoff, 40.0f)
                .delay (d8D, 30.0f, 45.0f, 7000.0f).reverb (rPlate, 45.0f, 6.0f, 30.0f),

            B ("Tekstur – Regn på taket")
                .osc (A, oscNoise, wAnalog, 70.0f).filter (A, fltBP, 2500.0f, 40.0f).l (A, lp::spread, 100.0f)
                .amp (A, 800.0f, 2000.0f, 100.0f, 3000.0f).lfo (A, 1, 9.0f, lfoSH)
                .osc (Bl, oscGranular, wGlass, 70.0f).grain (Bl, 15.0f, 6.0f, 100.0f, 80.0f, 20.0f).l (Bl, lp::oct, 2.0f)
                .l (Bl, lp::level, 50.0f).l (Bl, lp::spread, 100.0f).amp (Bl, 800.0f, 2000.0f, 100.0f, 3000.0f)
                .mod (srcLfo1, dstCutoff, 40.0f, 1).mod (srcModWheel, dstGrainDensity, 80.0f, 2).mod (srcOrbX, dstCutoff, 40.0f)
                .reverb (rRoom, 30.0f, 2.0f),

            B ("Tekstur – Maskinrom")
                .osc (A, oscWavetable, wMetal, 50.0f).l (A, lp::oct, -1.0f).uni (A, 3, 20.0f, 60.0f).filter (A, fltLP24, 1200.0f, 40.0f)
                .amp (A, 1000.0f, 2000.0f, 100.0f, 3000.0f).lfo (A, 2, 0.08f, lfoDrift).lfo (A, 1, 6.0f, lfoSH)
                .osc (Bl, oscNoise, wAnalog, 30.0f).filter (Bl, fltLP12, 600.0f, 20.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 1000.0f, 2000.0f, 100.0f, 3000.0f)
                .mod (srcLfo2, dstShape, 50.0f, 1).mod (srcLfo1, dstCutoff, 20.0f, 1, srcModWheel).mod (srcOrbX, dstCutoff, 50.0f)
                .delay (d16D, 20.0f, 50.0f, 3000.0f).reverb (rVintage, 35.0f, 5.0f),

            B ("Tekstur – Hvisken")
                .osc (A, oscNoise, wAnalog, 60.0f).filter (A, fltBP, 1200.0f, 75.0f).l (A, lp::spread, 100.0f)
                .amp (A, 1000.0f, 2000.0f, 100.0f, 3000.0f).lfo (A, 2, 0.3f, lfoDrift)
                .osc (Bl, oscGranular, wChoir, 40.0f).grain (Bl, 120.0f, 6.0f, 70.0f, 20.0f, 0.0f).l (Bl, lp::level, 35.0f)
                .amp (Bl, 1500.0f, 2000.0f, 100.0f, 3500.0f)
                .mod (srcLfo2, dstCutoff, 50.0f, 1).mod (srcOrbX, dstShape, 50.0f, 2).mod (srcModWheel, dstReso, 30.0f, 1)
                .g (ParamIDs::master, 3.0f).reverb (rChamber, 45.0f, 4.0f),

            B ("Tekstur – Krystallgrotte")
                .osc (A, oscGranular, wGlass, 50.0f).grain (A, 60.0f, 12.0f, 80.0f, 10.0f, 100.0f).l (A, lp::spread, 100.0f)
                .amp (A, 800.0f, 2000.0f, 100.0f, 4000.0f)
                .osc (Bl, oscFM, wAnalog, 25.0f).l (Bl, lp::sym, fmSym (6)).l (Bl, lp::level, 30.0f).amp (Bl, 1.0f, 2500.0f, 0.0f, 3000.0f)
                .mod (srcOrbX, dstShape, 50.0f, 1).mod (srcModWheel, dstGrainSize, -50.0f, 1).mod (srcVelocity, dstLevel, 40.0f, 2)
                .delay (d4D, 30.0f, 55.0f, 5000.0f).reverb (rHall, 50.0f, 9.0f, 20.0f),

            B ("Tekstur – Fjern radio")
                .osc (A, oscWavetable, wDigital, 60.0f).filter (A, fltBP, 1500.0f, 50.0f).l (A, lp::drive, 50.0f)
                .amp (A, 300.0f, 2000.0f, 100.0f, 1500.0f).lfo (A, 1, 7.0f, lfoSH).lfo (A, 2, 0.2f, lfoDrift)
                .osc (Bl, oscNoise, wAnalog, 60.0f).filter (Bl, fltBP, 2000.0f, 30.0f).l (Bl, lp::level, 20.0f)
                .amp (Bl, 300.0f, 2000.0f, 100.0f, 1500.0f)
                .mod (srcLfo1, dstFine, 15.0f, 1).mod (srcLfo2, dstCutoff, 40.0f, 1).mod (srcModWheel, dstShape, 50.0f, 1)
                .delay (d8, 20.0f, 40.0f, 2500.0f).reverb (rRoom, 25.0f, 1.5f),

            B ("Tekstur – Stjernestøv")
                .osc (A, oscGranular, wAir, 70.0f).grain (A, 20.0f, 14.0f, 100.0f, 100.0f, 70.0f).l (A, lp::oct, 2.0f)
                .filter (A, fltHP, 800.0f, 5.0f).l (A, lp::spread, 100.0f).amp (A, 800.0f, 2000.0f, 100.0f, 4000.0f)
                .mod (srcOrbX, dstShape, 50.0f).mod (srcModWheel, dstGrainDensity, -50.0f)
                .orb (0.0f, 0.0f, 0.1f, 60.0f).delay (d8D, 35.0f, 50.0f, 8000.0f).reverb (rPlate, 50.0f, 8.0f, 40.0f),

            B ("Tekstur – Puls i mørket")
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::oct, -1.0f).filter (A, fltLP24, 400.0f, 30.0f)
                .amp (A, 500.0f, 1000.0f, 100.0f, 2000.0f).lfo (A, 1, 2.0f, lfoSquare)
                .osc (Bl, oscNoise, wAnalog, 30.0f).filter (Bl, fltLP24, 800.0f, 20.0f).l (Bl, lp::level, 30.0f)
                .amp (Bl, 1500.0f, 1000.0f, 100.0f, 3000.0f).lfo (Bl, 2, 0.1f, lfoDrift)
                .mod (srcLfo1, dstLevel, -60.0f, 1).mod (srcLfo2, dstCutoff, 40.0f, 2).mod (srcOrbX, dstCutoff, 50.0f, 1)
                .mod (srcModWheel, dstLfo1Rate, 50.0f, 1)
                .delay (d4, 25.0f, 50.0f, 2000.0f).reverb (rVintage, 40.0f, 8.0f),

            B ("Atmosfære – Fjern tone")
                .osc (A, oscGranular, wChoir, 50.0f).grain (A, 600.0f, 6.0f, 30.0f, 8.0f, 50.0f).l (A, lp::oct, 1.0f)
                .amp (A, 4000.0f, 2000.0f, 100.0f, 8000.0f).l (A, lp::spread, 100.0f)
                .osc (Bl, oscWavetable, wAir, 60.0f).uni (Bl, 3, 10.0f, 60.0f).l (Bl, lp::level, 60.0f)
                .filter (Bl, fltLP24, 2500.0f, 10.0f).amp (Bl, 3000.0f, 2000.0f, 100.0f, 8000.0f)
                .mod (srcOrbX, dstShape, 40.0f, 1).mod (srcOrbY, dstCutoff, 50.0f, 2)
                .orb (0.0f, 0.0f, 0.03f, 70.0f).delay (d2, 20.0f, 35.0f, 3000.0f).reverb (rVintage, 55.0f, 14.0f),

            B ("Atmosfære – Nordlysdans")
                .osc (A, oscWavetable, wChoir, 30.0f).uni (A, 5, 12.0f, 90.0f).filter (A, fltLP24, 3500.0f, 15.0f)
                .amp (A, 2000.0f, 2000.0f, 100.0f, 6000.0f).lfo (A, 2, 0.06f, lfoDrift)
                .osc (Bl, oscGranular, wGlass, 60.0f).grain (Bl, 180.0f, 10.0f, 60.0f, 12.0f, 60.0f).l (Bl, lp::oct, 1.0f)
                .l (Bl, lp::level, 45.0f).l (Bl, lp::spread, 100.0f).amp (Bl, 3000.0f, 2000.0f, 100.0f, 7000.0f)
                .osc (C, oscWavetable, wAir, 50.0f).l (C, lp::oct, -1.0f).l (C, lp::level, 40.0f).amp (C, 3000.0f, 2000.0f, 100.0f, 7000.0f)
                .mod (srcOrbX, dstShape, 50.0f, 1).mod (srcOrbY, dstShape, 50.0f, 2).mod (srcOrbX, dstCutoff, 30.0f)
                .mod (srcLfo2, dstPan, 40.0f, 1)
                .orb (0.0f, 0.0f, 0.06f, 75.0f).delay (d2, 20.0f, 40.0f, 3000.0f).reverb (rHall, 55.0f, 12.0f),

            B ("Atmosfære – Dyp skog")
                .osc (A, oscNoise, wAnalog, 35.0f).filter (A, fltBP, 700.0f, 40.0f).l (A, lp::spread, 100.0f).l (A, lp::level, 50.0f)
                .amp (A, 3000.0f, 2000.0f, 100.0f, 6000.0f).lfo (A, 2, 0.07f, lfoDrift)
                .osc (Bl, oscWavetable, wAir, 30.0f).l (Bl, lp::oct, -1.0f).uni (Bl, 3, 10.0f, 60.0f).amp (Bl, 3000.0f, 2000.0f, 100.0f, 6000.0f)
                .osc (C, oscGranular, wChoir, 40.0f).grain (C, 500.0f, 5.0f, 50.0f, 15.0f, 10.0f).l (C, lp::level, 40.0f)
                .amp (C, 4000.0f, 2000.0f, 100.0f, 7000.0f)
                .mod (srcLfo2, dstCutoff, 40.0f, 1).mod (srcOrbX, dstShape, 40.0f, 3).mod (srcOrbY, dstLevel, 60.0f, 1)
                .orb (0.0f, -20.0f, 0.03f, 50.0f).reverb (rVintage, 50.0f, 11.0f, -30.0f),

            B ("Atmosfære – Kosmisk drone")
                .osc (A, oscGranular, wMetal, 40.0f).grain (A, 800.0f, 6.0f, 60.0f, 20.0f, 30.0f).l (A, lp::oct, -2.0f)
                .amp (A, 4000.0f, 2000.0f, 100.0f, 8000.0f).l (A, lp::spread, 100.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, -2.0f).uni (Bl, 7, 35.0f, 100.0f).filter (Bl, fltLP24, 500.0f, 35.0f)
                .l (Bl, lp::level, 50.0f).amp (Bl, 4000.0f, 2000.0f, 100.0f, 8000.0f).lfo (Bl, 2, 0.04f, lfoDrift)
                .mod (srcLfo2, dstCutoff, 50.0f, 2).mod (srcOrbX, dstShape, 60.0f, 1).mod (srcOrbY, dstGrainSize, 50.0f, 1)
                .orb (0.0f, 0.0f, 0.02f, 70.0f).delay (d2, 25.0f, 60.0f, 2000.0f).reverb (rVintage, 55.0f, 16.0f),

            B ("Atmosfære – Sakral")
                .osc (A, oscWavetable, wOrgan, 20.0f).filter (A, fltLP24, 3000.0f, 5.0f).amp (A, 1500.0f, 2000.0f, 100.0f, 5000.0f)
                .osc (Bl, oscGranular, wChoir, 20.0f).grain (Bl, 400.0f, 8.0f, 30.0f, 8.0f, 30.0f).l (Bl, lp::level, 50.0f)
                .amp (Bl, 2500.0f, 2000.0f, 100.0f, 6000.0f)
                .mod (srcOrbX, dstShape, 40.0f, 1).mod (srcModWheel, dstShape, 50.0f, 2)
                .g (ParamIDs::velSens, 20.0f).reverb (rHall, 60.0f, 14.0f, -10.0f),

            B ("Atmosfære – Uro")
                .osc (A, oscGranular, wDigital, 50.0f).grain (A, 90.0f, 9.0f, 80.0f, 60.0f, 20.0f).l (A, lp::spread, 100.0f)
                .filter (A, fltLP24, 5000.0f, 20.0f).amp (A, 1500.0f, 2000.0f, 100.0f, 4000.0f).lfo (A, 1, 3.0f, lfoSH).lfo (A, 2, 0.13f, lfoDrift)
                .osc (Bl, oscWavetable, wMetal, 30.0f).l (Bl, lp::oct, -1.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 1500.0f, 30.0f)
                .amp (Bl, 2000.0f, 2000.0f, 100.0f, 4000.0f)
                .mod (srcLfo1, dstShape, 30.0f, 1).mod (srcLfo2, dstCutoff, 40.0f).mod (srcOrbX, dstGrainDensity, 60.0f, 1)
                .mod (srcLfo1, dstPitch, 4.0f, 1, srcModWheel)
                .delay (d16T, 20.0f, 45.0f, 4000.0f).reverb (rHall, 45.0f, 7.0f),

            B ("Atmosfære – Morgendis")
                .osc (A, oscWavetable, wAir, 20.0f).uni (A, 5, 10.0f, 90.0f).filter (A, fltLP24, 600.0f, 10.0f)
                .fenv (A, 60.0f, 5000.0f, 6000.0f, 60.0f, 5000.0f).amp (A, 2500.0f, 2000.0f, 100.0f, 6000.0f)
                .osc (Bl, oscGranular, wStrings, 50.0f).grain (Bl, 400.0f, 7.0f, 40.0f, 8.0f, 20.0f).l (Bl, lp::level, 35.0f)
                .amp (Bl, 4000.0f, 2000.0f, 100.0f, 7000.0f)
                .mod (srcOrbX, dstCutoff, 40.0f, 1).mod (srcModWheel, dstShape, 40.0f, 1)
                .chorus (20.0f).reverb (rHall, 50.0f, 9.0f),

            // ---------------- Keys ----------------
            B ("Keys – Elektrisk piano")
                .osc (A, oscFM, wAnalog, 22.0f).l (A, lp::sym, fmSym (1)).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 2.0f, 2500.0f, 0.0f, 600.0f)
                .osc (Bl, oscFM, wAnalog, 15.0f).l (Bl, lp::sym, fmSym (10)).filter (Bl, fltOff, 8000.0f, 0.0f)
                .l (Bl, lp::level, 35.0f).amp (Bl, 1.0f, 300.0f, 0.0f, 300.0f)
                .mod (srcVelocity, dstShape, 30.0f, 1).mod (srcVelocity, dstLevel, 50.0f, 2)
                .g (ParamIDs::velSens, 80.0f).chorus (35.0f, 0.8f, 30.0f).reverb (rRoom, 15.0f, 1.5f),

            B ("Keys – Glassklokker")
                .osc (A, oscFM, wAnalog, 30.0f).l (A, lp::sym, fmSym (6)).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 1.0f, 3500.0f, 0.0f, 3000.0f)
                .osc (Bl, oscWavetable, wGlass, 70.0f).l (Bl, lp::oct, 1.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 1.0f, 1500.0f, 0.0f, 2000.0f)
                .mod (srcVelocity, dstShape, 25.0f, 1).mod (srcModWheel, dstShape, 40.0f, 1)
                .delay (d8, 20.0f, 35.0f).reverb (rHall, 40.0f, 4.0f, 10.0f),

            B ("Keys – Orgel")
                .osc (A, oscWavetable, wOrgan, 70.0f).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 5.0f, 100.0f, 100.0f, 80.0f)
                .lfo (A, 1, 6.0f, lfoSine)
                .mod (srcLfo1, dstFine, 15.0f, 0, srcModWheel).mod (srcOrbX, dstShape, 30.0f)
                .g (ParamIDs::velSens, 0.0f).chorus (50.0f, 4.0f, 20.0f).reverb (rRoom, 15.0f, 1.5f),

            B ("Keys – Vintage EP")
                .osc (A, oscFM, wAnalog, 18.0f).l (A, lp::sym, fmSym (1)).filter (A, fltLP12, 5000.0f, 0.0f)
                .amp (A, 2.0f, 3000.0f, 0.0f, 500.0f).lfo (A, 1, 4.5f, lfoSine)
                .osc (Bl, oscFM, wAnalog, 25.0f).l (Bl, lp::sym, fmSym (10)).filter (Bl, fltOff, 8000.0f, 0.0f)
                .l (Bl, lp::level, 30.0f).amp (Bl, 1.0f, 200.0f, 0.0f, 200.0f)
                .mod (srcVelocity, dstShape, 40.0f, 1).mod (srcVelocity, dstLevel, 60.0f, 2).mod (srcLfo1, dstPan, 30.0f, 1, srcModWheel)
                .g (ParamIDs::velSens, 85.0f).chorus (25.0f, 0.5f, 25.0f).reverb (rRoom, 15.0f, 1.2f),

            B ("Keys – Myk klokke-EP")
                .osc (A, oscFM, wAnalog, 20.0f).l (A, lp::sym, fmSym (3)).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 2.0f, 2500.0f, 0.0f, 800.0f)
                .osc (Bl, oscWavetable, wGlass, 40.0f).l (Bl, lp::level, 30.0f).amp (Bl, 2.0f, 1500.0f, 0.0f, 1000.0f)
                .mod (srcVelocity, dstShape, 30.0f, 1).mod (srcModWheel, dstShape, 30.0f, 1)
                .g (ParamIDs::velSens, 75.0f).chorus (30.0f).delay (d8D, 12.0f, 30.0f).reverb (rPlate, 25.0f, 2.5f),

            B ("Keys – Celesta")
                .osc (A, oscFM, wAnalog, 22.0f).l (A, lp::sym, fmSym (7)).l (A, lp::oct, 1.0f).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 1.0f, 1500.0f, 0.0f, 1200.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, 1.0f).l (Bl, lp::level, 50.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 2000.0f, 0.0f, 1500.0f)
                .mod (srcVelocity, dstShape, 30.0f, 1)
                .g (ParamIDs::velSens, 70.0f).reverb (rHall, 30.0f, 3.0f),

            B ("Keys – Vibrafon")
                .osc (A, oscFM, wAnalog, 12.0f).l (A, lp::sym, fmSym (7)).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 1.0f, 4000.0f, 0.0f, 1500.0f).lfo (A, 1, 5.0f, lfoSine)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 4000.0f, 0.0f, 1500.0f).lfo (Bl, 1, 5.0f, lfoSine)
                .mod (srcLfo1, dstLevel, 30.0f, 0, srcModWheel).mod (srcVelocity, dstShape, 25.0f, 1)
                .g (ParamIDs::velSens, 70.0f).reverb (rRoom, 25.0f, 2.0f),

            B ("Keys – Kirkeorgel")
                .osc (A, oscWavetable, wOrgan, 100.0f).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 30.0f, 100.0f, 100.0f, 600.0f)
                .osc (Bl, oscWavetable, wOrgan, 40.0f).l (Bl, lp::oct, -1.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 40.0f, 100.0f, 100.0f, 700.0f)
                .mod (srcModWheel, dstShape, -50.0f, 1)
                .g (ParamIDs::velSens, 0.0f).reverb (rHall, 50.0f, 8.0f),

            B ("Keys – Rock-orgel")
                .osc (A, oscWavetable, wOrgan, 60.0f).filter (A, fltLP12, 6000.0f, 0.0f).l (A, lp::drive, 45.0f)
                .amp (A, 3.0f, 100.0f, 100.0f, 60.0f).lfo (A, 1, 6.5f, lfoSine)
                .mod (srcLfo1, dstPan, 40.0f, 1).mod (srcModWheel, dstLfo1Rate, -40.0f, 1)
                .g (ParamIDs::velSens, 0.0f).chorus (55.0f, 5.0f, 25.0f).reverb (rRoom, 15.0f, 1.2f),

            B ("Keys – Pianopad")
                .osc (A, oscFM, wAnalog, 20.0f).l (A, lp::sym, fmSym (1)).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 2.0f, 3000.0f, 0.0f, 1000.0f)
                .osc (Bl, oscWavetable, wAir, 50.0f).uni (Bl, 3, 10.0f, 80.0f).l (Bl, lp::level, 50.0f)
                .amp (Bl, 1500.0f, 2000.0f, 90.0f, 3000.0f)
                .mod (srcVelocity, dstShape, 35.0f, 1).mod (srcModWheel, dstLevel, 60.0f, 2)
                .g (ParamIDs::velSens, 70.0f).chorus (20.0f).reverb (rHall, 35.0f, 4.0f),

            B ("Strings – Ensemble")
                .osc (A, oscWavetable, wStrings, 50.0f).uni (A, 7, 18.0f, 100.0f).filter (A, fltLP24, 5000.0f, 5.0f)
                .amp (A, 400.0f, 1000.0f, 90.0f, 1200.0f).lfo (A, 1, 5.0f, lfoSine, 1000.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, -1.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 1500.0f, 5.0f)
                .amp (Bl, 400.0f, 1000.0f, 90.0f, 1200.0f)
                .mod (srcLfo1, dstFine, 6.0f).mod (srcModWheel, dstCutoff, 35.0f).mod (srcVelocity, dstCutoff, 20.0f)
                .chorus (40.0f).reverb (rHall, 35.0f, 3.5f),

            B ("Strings – Langsomme")
                .osc (A, oscWavetable, wStrings, 40.0f).uni (A, 7, 20.0f, 100.0f).filter (A, fltLP24, 3500.0f, 5.0f)
                .amp (A, 1500.0f, 2000.0f, 95.0f, 2500.0f).lfo (A, 1, 5.0f, lfoSine, 2000.0f)
                .osc (Bl, oscWavetable, wStrings, 70.0f).l (Bl, lp::oct, 1.0f).uni (Bl, 5, 15.0f, 100.0f).l (Bl, lp::level, 40.0f)
                .filter (Bl, fltLP24, 5000.0f, 5.0f).amp (Bl, 2000.0f, 2000.0f, 95.0f, 3000.0f)
                .mod (srcLfo1, dstFine, 5.0f).mod (srcModWheel, dstCutoff, 40.0f).mod (srcOrbX, dstShape, 30.0f)
                .chorus (30.0f).reverb (rHall, 40.0f, 4.5f),

            B ("Strings – Pizzicato")
                .osc (A, oscWavetable, wStrings, 30.0f).uni (A, 3, 8.0f, 60.0f).filter (A, fltLP12, 2500.0f, 10.0f)
                .fenv (A, 40.0f, 1.0f, 250.0f, 0.0f, 250.0f).amp (A, 1.0f, 350.0f, 0.0f, 300.0f)
                .mod (srcVelocity, dstCutoff, 30.0f)
                .reverb (rHall, 30.0f, 2.5f),

            B ("Strings – Høye fioliner")
                .osc (A, oscWavetable, wStrings, 80.0f).l (A, lp::oct, 1.0f).uni (A, 7, 14.0f, 100.0f).filter (A, fltLP24, 7000.0f, 5.0f)
                .amp (A, 500.0f, 1500.0f, 90.0f, 1500.0f).lfo (A, 1, 5.5f, lfoSine, 800.0f)
                .mod (srcLfo1, dstFine, 8.0f).mod (srcModWheel, dstCutoff, 30.0f).mod (srcAftertouch, dstFine, 15.0f)
                .chorus (25.0f).reverb (rHall, 40.0f, 3.5f),

            // ---------------- Brass ----------------
            B ("Brass – Synth-brass")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 4, 10.0f, 60.0f).filter (A, fltLP24, 800.0f, 15.0f)
                .fenv (A, 50.0f, 60.0f, 500.0f, 40.0f, 400.0f).amp (A, 30.0f, 800.0f, 85.0f, 300.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, -1.0f).l (Bl, lp::level, 50.0f).filter (Bl, fltLP24, 800.0f, 10.0f)
                .fenv (Bl, 40.0f, 60.0f, 500.0f, 40.0f, 400.0f).amp (Bl, 30.0f, 800.0f, 85.0f, 300.0f)
                .mod (srcVelocity, dstCutoff, 25.0f).mod (srcModWheel, dstCutoff, 30.0f)
                .chorus (25.0f).reverb (rPlate, 20.0f, 2.0f),

            B ("Brass – Myke horn")
                .osc (A, oscAnalog, wAnalog, 55.0f).uni (A, 3, 8.0f, 50.0f).filter (A, fltLP24, 600.0f, 10.0f)
                .fenv (A, 35.0f, 200.0f, 900.0f, 50.0f, 500.0f).amp (A, 120.0f, 800.0f, 90.0f, 500.0f).lfo (A, 1, 5.0f, lfoSine, 900.0f)
                .mod (srcLfo1, dstFine, 6.0f).mod (srcVelocity, dstCutoff, 30.0f).mod (srcModWheel, dstCutoff, 30.0f)
                .reverb (rHall, 30.0f, 3.0f),

            B ("Brass – Stab")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 6, 18.0f, 90.0f).filter (A, fltLP24, 1200.0f, 20.0f)
                .fenv (A, 55.0f, 1.0f, 250.0f, 10.0f, 200.0f).amp (A, 1.0f, 350.0f, 0.0f, 250.0f).l (A, lp::drive, 20.0f)
                .mod (srcVelocity, dstCutoff, 30.0f).mod (srcOrbX, dstCutoff, 40.0f)
                .delay (d8D, 15.0f, 25.0f).reverb (rPlate, 25.0f, 2.0f),

            // ---------------- Synth ----------------
            B ("Synth – Supersaw")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 7, 30.0f, 100.0f).filter (A, fltLP24, 7000.0f, 5.0f)
                .amp (A, 5.0f, 1000.0f, 90.0f, 600.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, 1.0f).uni (Bl, 7, 25.0f, 100.0f).l (Bl, lp::level, 45.0f)
                .filter (Bl, fltLP24, 7000.0f, 5.0f).amp (Bl, 5.0f, 1000.0f, 90.0f, 600.0f)
                .mod (srcModWheel, dstDetune, 40.0f).mod (srcOrbX, dstCutoff, 40.0f)
                .chorus (20.0f).delay (d8D, 15.0f, 30.0f).reverb (rHall, 25.0f, 3.0f),

            B ("Synth – Akkord-stab")
                .osc (A, oscWavetable, wDigital, 50.0f).uni (A, 3, 12.0f, 80.0f).filter (A, fltLP24, 2500.0f, 25.0f)
                .fenv (A, 45.0f, 1.0f, 300.0f, 0.0f, 300.0f).amp (A, 1.0f, 500.0f, 0.0f, 400.0f)
                .osc (Bl, oscAnalog, wAnalog, 100.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 2000.0f, 10.0f)
                .fenv (Bl, 40.0f, 1.0f, 300.0f, 0.0f, 300.0f).amp (Bl, 1.0f, 500.0f, 0.0f, 400.0f)
                .mod (srcVelocity, dstCutoff, 30.0f).mod (srcOrbX, dstShape, 40.0f, 1)
                .delay (d8D, 25.0f, 40.0f).reverb (rHall, 30.0f, 3.0f),

            B ("Synth – Retro-poly")
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::sym, 35.0f).uni (A, 2, 8.0f, 50.0f).filter (A, fltLP24, 1800.0f, 25.0f)
                .fenv (A, 30.0f, 5.0f, 700.0f, 40.0f, 500.0f).amp (A, 5.0f, 800.0f, 80.0f, 500.0f).lfo (A, 1, 0.6f, lfoTriangle)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::fine, 8.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltLP24, 1800.0f, 25.0f)
                .fenv (Bl, 30.0f, 5.0f, 700.0f, 40.0f, 500.0f).amp (Bl, 5.0f, 800.0f, 80.0f, 500.0f)
                .mod (srcLfo1, dstSym, 20.0f, 1).mod (srcModWheel, dstCutoff, 35.0f).mod (srcVelocity, dstCutoff, 20.0f)
                .chorus (45.0f, 0.5f, 45.0f).reverb (rRoom, 20.0f, 1.8f),

            B ("Pluck – Harpe i tåke")
                .osc (A, oscWavetable, wStrings, 30.0f).filter (A, fltLP12, 3000.0f, 10.0f).fenv (A, 40.0f, 1.0f, 600.0f, 0.0f, 800.0f)
                .amp (A, 1.0f, 1200.0f, 0.0f, 1500.0f)
                .osc (Bl, oscGranular, wStrings, 40.0f).grain (Bl, 200.0f, 8.0f, 40.0f, 10.0f, 25.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 800.0f, 2000.0f, 80.0f, 3000.0f)
                .mod (srcVelocity, dstCutoff, 30.0f, 1).mod (srcModWheel, dstLevel, 60.0f, 2)
                .reverb (rHall, 40.0f, 6.0f),

            B ("Pluck – Glass-pluck")
                .osc (A, oscWavetable, wGlass, 70.0f).uni (A, 2, 6.0f, 60.0f).filter (A, fltLP24, 3000.0f, 20.0f)
                .fenv (A, 45.0f, 1.0f, 350.0f, 0.0f, 300.0f).amp (A, 1.0f, 700.0f, 0.0f, 600.0f)
                .mod (srcVelocity, dstCutoff, 30.0f).mod (srcOrbX, dstShape, 40.0f)
                .delay (d8D, 25.0f, 35.0f).reverb (rHall, 30.0f, 3.5f),

            B ("Pluck – Kalimba")
                .osc (A, oscFM, wAnalog, 18.0f).l (A, lp::sym, fmSym (6)).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 1.0f, 900.0f, 0.0f, 700.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 1200.0f, 0.0f, 900.0f)
                .mod (srcVelocity, dstShape, 35.0f, 1)
                .g (ParamIDs::velSens, 75.0f).reverb (rRoom, 25.0f, 1.5f),

            B ("Pluck – Marimba")
                .osc (A, oscFM, wAnalog, 14.0f).l (A, lp::sym, fmSym (7)).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 1.0f, 500.0f, 0.0f, 400.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::level, 70.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 700.0f, 0.0f, 500.0f)
                .mod (srcVelocity, dstShape, 40.0f, 1)
                .g (ParamIDs::velSens, 80.0f).reverb (rRoom, 20.0f, 1.2f),

            B ("Pluck – Analog pluck")
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 2, 8.0f, 50.0f).filter (A, fltLP24, 500.0f, 30.0f)
                .fenv (A, 60.0f, 1.0f, 200.0f, 0.0f, 200.0f).amp (A, 1.0f, 400.0f, 0.0f, 300.0f)
                .mod (srcVelocity, dstCutoff, 35.0f).mod (srcModWheel, dstReso, 40.0f).mod (srcOrbX, dstCutoff, 40.0f)
                .delay (d8D, 20.0f, 30.0f).reverb (rRoom, 20.0f, 1.5f),

            B ("Pluck – Digital klokke")
                .osc (A, oscWavetable, wDigital, 80.0f).filter (A, fltLP24, 5000.0f, 10.0f)
                .fenv (A, 30.0f, 1.0f, 500.0f, 0.0f, 500.0f).amp (A, 1.0f, 1200.0f, 0.0f, 1200.0f)
                .osc (Bl, oscFM, wAnalog, 20.0f).l (Bl, lp::sym, fmSym (5)).l (Bl, lp::oct, 1.0f).l (Bl, lp::level, 35.0f)
                .amp (Bl, 1.0f, 800.0f, 0.0f, 800.0f)
                .mod (srcVelocity, dstShape, 30.0f).mod (srcOrbX, dstShape, 40.0f, 1)
                .delay (d4T, 20.0f, 35.0f).reverb (rPlate, 30.0f, 3.0f),

            // ---------------- Bass ----------------
            B ("Bass – Sub og knurr")
                .g (ParamIDs::polyMode, (float) modeMono).g (ParamIDs::glide, 40.0f).g (ParamIDs::master, -2.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).uni (A, 2, 6.0f, 20.0f).filter (A, fltLP24, 400.0f, 25.0f)
                .fenv (A, 45.0f, 1.0f, 350.0f, 10.0f, 200.0f).amp (A, 2.0f, 600.0f, 80.0f, 150.0f).l (A, lp::drive, 30.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 70.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 2.0f, 600.0f, 100.0f, 150.0f)
                .mod (srcModWheel, dstCutoff, 40.0f, 1).mod (srcVelocity, dstCutoff, 25.0f, 1)
                .reverb (rRoom, 0.0f, 1.0f),

            B ("Bass – Wobble")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 60.0f).g (ParamIDs::master, -1.0f)
                .osc (A, oscWavetable, wDigital, 40.0f).l (A, lp::oct, -1.0f).uni (A, 3, 10.0f, 40.0f)
                .filter (A, fltLP24, 300.0f, 55.0f).amp (A, 2.0f, 200.0f, 100.0f, 120.0f).l (A, lp::drive, 40.0f)
                .lfo (A, 1, 2.0f, lfoTriangle)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 2.0f, 200.0f, 100.0f, 120.0f)
                .mod (srcLfo1, dstCutoff, 55.0f, 1, srcModWheel).mod (srcOrbX, dstShape, 50.0f, 1)
                .reverb (rRoom, 5.0f, 1.0f),

            B ("Bass – Fet analog")
                .g (ParamIDs::polyMode, (float) modeMono).g (ParamIDs::glide, 30.0f).g (ParamIDs::master, -1.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).filter (A, fltLP24, 350.0f, 30.0f)
                .fenv (A, 40.0f, 1.0f, 400.0f, 20.0f, 200.0f).amp (A, 1.0f, 500.0f, 90.0f, 150.0f).l (A, lp::drive, 35.0f)
                .osc (Bl, oscAnalog, wAnalog, 100.0f).l (Bl, lp::oct, -1.0f).l (Bl, lp::fine, -6.0f).l (Bl, lp::level, 60.0f)
                .filter (Bl, fltLP24, 350.0f, 20.0f).fenv (Bl, 40.0f, 1.0f, 400.0f, 20.0f, 200.0f).amp (Bl, 1.0f, 500.0f, 90.0f, 150.0f)
                .mod (srcVelocity, dstCutoff, 25.0f).mod (srcModWheel, dstCutoff, 40.0f)
                .reverb (rRoom, 0.0f, 1.0f),

            B ("Bass – Sub sinus")
                .g (ParamIDs::polyMode, (float) modeMono).g (ParamIDs::glide, 20.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscAnalog, wAnalog, 0.0f).l (A, lp::oct, -1.0f).filter (A, fltOff, 8000.0f, 0.0f).amp (A, 3.0f, 300.0f, 100.0f, 120.0f)
                .osc (Bl, oscAnalog, wAnalog, 33.3f).l (Bl, lp::level, 25.0f).filter (Bl, fltLP12, 800.0f, 0.0f)
                .amp (Bl, 3.0f, 300.0f, 100.0f, 120.0f)
                .mod (srcModWheel, dstLevel, 80.0f, 2)
                .reverb (rRoom, 0.0f, 1.0f),

            B ("Bass – FM-bass")
                .g (ParamIDs::polyMode, (float) modeMono).g (ParamIDs::glide, 15.0f).g (ParamIDs::master, -1.0f)
                .osc (A, oscFM, wAnalog, 35.0f).l (A, lp::sym, fmSym (1)).l (A, lp::oct, -1.0f).filter (A, fltLP24, 2500.0f, 10.0f)
                .fenv (A, 0.0f, 1.0f, 250.0f, 0.0f, 200.0f).amp (A, 1.0f, 600.0f, 70.0f, 150.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 600.0f, 100.0f, 150.0f)
                .mod (srcFiltEnv, dstShape, 40.0f, 1).mod (srcVelocity, dstShape, 30.0f, 1).mod (srcModWheel, dstShape, 40.0f, 1)
                .reverb (rRoom, 0.0f, 1.0f),

            B ("Bass – Pluck-bass")
                .g (ParamIDs::master, -1.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).uni (A, 2, 6.0f, 30.0f).filter (A, fltLP24, 250.0f, 35.0f)
                .fenv (A, 55.0f, 1.0f, 180.0f, 0.0f, 150.0f).amp (A, 1.0f, 450.0f, 0.0f, 150.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 55.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 450.0f, 0.0f, 150.0f)
                .mod (srcVelocity, dstCutoff, 30.0f, 1).mod (srcOrbX, dstCutoff, 40.0f, 1)
                .reverb (rRoom, 5.0f, 1.0f),

            B ("Bass – Reese")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 50.0f).g (ParamIDs::master, -2.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).uni (A, 5, 22.0f, 60.0f).filter (A, fltLP24, 700.0f, 20.0f)
                .amp (A, 5.0f, 500.0f, 100.0f, 200.0f).l (A, lp::drive, 30.0f).lfo (A, 2, 0.2f, lfoTriangle)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 55.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 5.0f, 500.0f, 100.0f, 200.0f)
                .mod (srcLfo2, dstCutoff, 20.0f, 1).mod (srcModWheel, dstCutoff, 50.0f, 1).mod (srcOrbX, dstDetune, 40.0f, 1)
                .reverb (rRoom, 0.0f, 1.0f),

            B ("Bass – Acid")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 70.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).filter (A, fltLP24, 300.0f, 78.0f)
                .fenv (A, 65.0f, 1.0f, 250.0f, 0.0f, 150.0f).amp (A, 1.0f, 400.0f, 80.0f, 100.0f).l (A, lp::drive, 35.0f)
                .mod (srcVelocity, dstCutoff, 30.0f).mod (srcModWheel, dstCutoff, 50.0f).mod (srcOrbX, dstReso, 30.0f)
                .delay (d8D, 15.0f, 30.0f, 3000.0f).reverb (rRoom, 5.0f, 1.0f),

            B ("Bass – Mørk drone-bass")
                .osc (A, oscWavetable, wMetal, 30.0f).l (A, lp::oct, -2.0f).uni (A, 3, 10.0f, 40.0f).filter (A, fltLP24, 400.0f, 30.0f)
                .amp (A, 600.0f, 2000.0f, 100.0f, 1500.0f).lfo (A, 2, 0.08f, lfoDrift)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 60.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 600.0f, 2000.0f, 100.0f, 1500.0f)
                .mod (srcLfo2, dstShape, 40.0f, 1).mod (srcLfo2, dstCutoff, 25.0f, 1).mod (srcModWheel, dstCutoff, 50.0f, 1)
                .reverb (rVintage, 20.0f, 4.0f),

            // ---------------- Lead ----------------
            B ("Lead – Nordlys-lead")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 80.0f).g (ParamIDs::master, 2.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 3, 10.0f, 50.0f).filter (A, fltLP24, 3500.0f, 20.0f)
                .fenv (A, 20.0f, 5.0f, 800.0f, 50.0f, 600.0f).amp (A, 5.0f, 600.0f, 85.0f, 400.0f)
                .lfo (A, 1, 5.5f, lfoSine, 600.0f)
                .mod (srcLfo1, dstFine, 12.0f).mod (srcModWheel, dstCutoff, 30.0f).mod (srcAftertouch, dstFine, 20.0f)
                .delay (d8D, 25.0f, 35.0f).reverb (rPlate, 20.0f, 2.5f),

            B ("Lead – Synkron kvadrat")
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::sym, 30.0f).filter (A, fltLP24, 4000.0f, 15.0f)
                .amp (A, 3.0f, 500.0f, 80.0f, 300.0f).lfo (A, 1, 0.4f, lfoSine)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, 1.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 5000.0f, 10.0f)
                .amp (Bl, 3.0f, 500.0f, 80.0f, 300.0f)
                .mod (srcLfo1, dstSym, 35.0f, 1).mod (srcModWheel, dstCutoff, 35.0f).mod (srcOrbX, dstSym, 30.0f, 1)
                .delay (d4T, 20.0f, 30.0f).reverb (rPlate, 20.0f, 2.0f),

            B ("Lead – Myk sinus")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 60.0f).g (ParamIDs::master, 2.0f)
                .osc (A, oscAnalog, wAnalog, 10.0f).filter (A, fltLP12, 4000.0f, 0.0f).amp (A, 10.0f, 500.0f, 90.0f, 400.0f)
                .lfo (A, 1, 5.2f, lfoSine, 700.0f)
                .mod (srcLfo1, dstFine, 12.0f).mod (srcAftertouch, dstFine, 20.0f).mod (srcModWheel, dstShape, 40.0f)
                .delay (d8D, 25.0f, 35.0f).reverb (rHall, 25.0f, 3.0f),

            B ("Lead – Skrikende sag")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 40.0f).g (ParamIDs::master, -1.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 3, 12.0f, 50.0f).filter (A, fltLP24, 3000.0f, 35.0f).l (A, lp::drive, 60.0f)
                .fenv (A, 25.0f, 1.0f, 600.0f, 40.0f, 400.0f).amp (A, 2.0f, 400.0f, 90.0f, 300.0f).lfo (A, 1, 6.0f, lfoSine, 500.0f)
                .osc (Bl, oscAnalog, wAnalog, 100.0f).l (Bl, lp::oct, 1.0f).l (Bl, lp::level, 35.0f).filter (Bl, fltLP24, 4000.0f, 15.0f)
                .amp (Bl, 2.0f, 400.0f, 90.0f, 300.0f)
                .mod (srcLfo1, dstFine, 15.0f, 0, srcModWheel).mod (srcAftertouch, dstCutoff, 30.0f).mod (srcVelocity, dstCutoff, 20.0f)
                .delay (d8D, 20.0f, 35.0f).reverb (rPlate, 20.0f, 2.0f),

            B ("Lead – Fløyteaktig")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 50.0f).g (ParamIDs::master, 2.0f)
                .osc (A, oscAnalog, wAnalog, 15.0f).filter (A, fltLP12, 3500.0f, 5.0f).amp (A, 60.0f, 500.0f, 90.0f, 300.0f)
                .lfo (A, 1, 5.0f, lfoSine, 900.0f)
                .osc (Bl, oscNoise, wAnalog, 70.0f).filter (Bl, fltBP, 2500.0f, 40.0f).l (Bl, lp::fkey, 100.0f).l (Bl, lp::level, 25.0f)
                .amp (Bl, 30.0f, 300.0f, 40.0f, 200.0f)
                .mod (srcLfo1, dstFine, 10.0f).mod (srcVelocity, dstLevel, 50.0f, 2).mod (srcAftertouch, dstLevel, 40.0f, 1)
                .reverb (rHall, 30.0f, 3.0f),

            B ("Lead – Glide-lead")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 250.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscWavetable, wDigital, 40.0f).uni (A, 2, 10.0f, 40.0f).filter (A, fltLP24, 3500.0f, 25.0f)
                .amp (A, 5.0f, 500.0f, 90.0f, 400.0f).lfo (A, 1, 5.5f, lfoSine, 600.0f)
                .mod (srcLfo1, dstFine, 10.0f).mod (srcModWheel, dstShape, 50.0f).mod (srcOrbX, dstCutoff, 40.0f)
                .delay (d4D, 25.0f, 40.0f).reverb (rPlate, 25.0f, 2.5f),

            B ("Lead – Chiptune")
                .g (ParamIDs::polyMode, (float) modeMono).g (ParamIDs::master, -2.0f)
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::sym, 25.0f).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 1.0f, 200.0f, 80.0f, 60.0f).lfo (A, 1, 6.0f, lfoSine, 300.0f)
                .mod (srcLfo1, dstFine, 15.0f, 0, srcModWheel).mod (srcOrbX, dstSym, 30.0f)
                .reverb (rRoom, 10.0f, 1.0f),

            B ("Lead – Kor-lead")
                .g (ParamIDs::polyMode, (float) modeLegato).g (ParamIDs::glide, 80.0f).g (ParamIDs::master, 2.0f)
                .osc (A, oscWavetable, wChoir, 10.0f).uni (A, 3, 8.0f, 50.0f).filter (A, fltLP24, 5000.0f, 10.0f)
                .amp (A, 30.0f, 600.0f, 90.0f, 500.0f).lfo (A, 1, 5.0f, lfoSine, 800.0f)
                .mod (srcLfo1, dstFine, 10.0f).mod (srcModWheel, dstShape, 60.0f).mod (srcAftertouch, dstShape, 40.0f)
                .delay (d8D, 20.0f, 35.0f).reverb (rHall, 35.0f, 4.0f),

            // ---------------- Arp ----------------
            B ("Arp – Krystallpuls")
                .arp (arpUpDown, a16, 2, 45.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscWavetable, wGlass, 50.0f).filter (A, fltLP24, 5000.0f, 10.0f).fenv (A, 30.0f, 1.0f, 300.0f, 0.0f, 300.0f)
                .amp (A, 1.0f, 400.0f, 0.0f, 300.0f).uni (A, 2, 8.0f, 60.0f)
                .mod (srcOrbX, dstCutoff, 40.0f).mod (srcOrbY, dstShape, 40.0f).mod (srcModWheel, dstShape, 40.0f)
                .delay (d8D, 30.0f, 40.0f).reverb (rHall, 30.0f, 4.0f),

            B ("Arp – Analog sekvens")
                .arp (arpUp, a16, 3, 30.0f).g (ParamIDs::master, 2.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).filter (A, fltLP24, 900.0f, 40.0f).fenv (A, 60.0f, 1.0f, 220.0f, 0.0f, 200.0f)
                .amp (A, 1.0f, 300.0f, 0.0f, 200.0f).lfo (A, 2, 0.1f, lfoTriangle)
                .mod (srcLfo2, dstCutoff, 25.0f).mod (srcOrbX, dstCutoff, 50.0f).mod (srcModWheel, dstReso, 40.0f)
                .delay (d8D, 25.0f, 35.0f).reverb (rRoom, 20.0f, 1.5f),
            B ("Arp – Glassregn")
                .arp (arpRandom, a16, 3, 40.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscWavetable, wGlass, 60.0f).filter (A, fltLP24, 6000.0f, 10.0f).fenv (A, 25.0f, 1.0f, 300.0f, 0.0f, 300.0f)
                .amp (A, 1.0f, 500.0f, 0.0f, 500.0f)
                .mod (srcRandom, dstPan, 60.0f).mod (srcOrbX, dstShape, 40.0f).mod (srcModWheel, dstCutoff, -40.0f)
                .delay (d8D, 35.0f, 45.0f, 6000.0f).reverb (rHall, 40.0f, 5.0f),

            B ("Arp – Akkordpuls")
                .arp (arpChord, a8, 1, 40.0f).g (ParamIDs::master, -2.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).uni (A, 4, 14.0f, 80.0f).filter (A, fltLP24, 1500.0f, 25.0f)
                .fenv (A, 40.0f, 1.0f, 250.0f, 0.0f, 200.0f).amp (A, 1.0f, 400.0f, 0.0f, 250.0f)
                .mod (srcOrbX, dstCutoff, 50.0f).mod (srcModWheel, dstReso, 40.0f)
                .delay (d8D, 20.0f, 30.0f).reverb (rHall, 25.0f, 3.0f),

            B ("Arp – Trioler i tåke")
                .arp (arpUpDown, a8T, 2, 70.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscWavetable, wAir, 50.0f).uni (A, 3, 10.0f, 70.0f).filter (A, fltLP24, 3000.0f, 15.0f)
                .amp (A, 20.0f, 800.0f, 40.0f, 900.0f)
                .osc (Bl, oscGranular, wChoir, 50.0f).grain (Bl, 200.0f, 6.0f, 40.0f, 10.0f, 40.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 200.0f, 1000.0f, 60.0f, 1500.0f)
                .mod (srcOrbX, dstShape, 40.0f, 1).mod (srcModWheel, dstLevel, 60.0f, 2)
                .delay (d4T, 30.0f, 45.0f, 4000.0f).reverb (rHall, 45.0f, 6.0f),

            B ("Arp – Rask sekvens")
                .arp (arpUp, a32, 2, 50.0f).g (ParamIDs::master, 1.0f)
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::sym, 30.0f).filter (A, fltLP24, 1500.0f, 30.0f)
                .fenv (A, 45.0f, 1.0f, 120.0f, 0.0f, 100.0f).amp (A, 1.0f, 150.0f, 0.0f, 100.0f)
                .mod (srcOrbX, dstCutoff, 50.0f).mod (srcModWheel, dstSym, 30.0f)
                .delay (d16D, 20.0f, 30.0f).reverb (rRoom, 20.0f, 1.5f),

            B ("Arp – Klokkespill")
                .arp (arpPlayed, a8, 2, 60.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscFM, wAnalog, 25.0f).l (A, lp::sym, fmSym (6)).filter (A, fltOff, 8000.0f, 0.0f)
                .amp (A, 1.0f, 1500.0f, 0.0f, 1500.0f)
                .mod (srcVelocity, dstShape, 25.0f).mod (srcModWheel, dstShape, 40.0f)
                .delay (d8D, 25.0f, 40.0f).reverb (rHall, 40.0f, 4.0f),

            B ("Arp – Mørk bass-sekvens")
                .arp (arpDown, a16, 1, 50.0f).g (ParamIDs::master, 0.0f)
                .osc (A, oscAnalog, wAnalog, 66.7f).l (A, lp::oct, -1.0f).filter (A, fltLP24, 350.0f, 45.0f)
                .fenv (A, 50.0f, 1.0f, 180.0f, 0.0f, 120.0f).amp (A, 1.0f, 250.0f, 0.0f, 120.0f).l (A, lp::drive, 30.0f)
                .osc (Bl, oscAnalog, wAnalog, 0.0f).l (Bl, lp::oct, -2.0f).l (Bl, lp::level, 50.0f).filter (Bl, fltOff, 8000.0f, 0.0f)
                .amp (Bl, 1.0f, 250.0f, 0.0f, 120.0f)
                .mod (srcOrbX, dstCutoff, 50.0f, 1).mod (srcModWheel, dstReso, 40.0f, 1)
                .delay (d8D, 10.0f, 20.0f, 2000.0f).reverb (rRoom, 10.0f, 1.0f),

            B ("Arp – Drømmende")
                .arp (arpUpDown, a4, 2, 95.0f).g (ParamIDs::master, -2.0f)
                .osc (A, oscWavetable, wGlass, 40.0f).uni (A, 5, 12.0f, 90.0f).filter (A, fltLP24, 3500.0f, 10.0f)
                .amp (A, 200.0f, 1500.0f, 70.0f, 2500.0f)
                .osc (Bl, oscGranular, wAir, 50.0f).grain (Bl, 300.0f, 8.0f, 50.0f, 10.0f, 50.0f).l (Bl, lp::level, 40.0f)
                .l (Bl, lp::oct, 1.0f).amp (Bl, 500.0f, 1500.0f, 80.0f, 3000.0f)
                .mod (srcOrbX, dstShape, 40.0f).mod (srcModWheel, dstCutoff, 30.0f)
                .orb (0.0f, 0.0f, 0.05f, 40.0f).delay (d4D, 25.0f, 45.0f).reverb (rHall, 50.0f, 7.0f),

            // ---------------- FX ----------------
            B ("FX – Oppsveip")
                .osc (A, oscNoise, wAnalog, 80.0f).filter (A, fltBP, 200.0f, 60.0f).l (A, lp::spread, 100.0f)
                .fenv (A, 100.0f, 8000.0f, 1.0f, 100.0f, 2000.0f).amp (A, 4000.0f, 1000.0f, 100.0f, 2000.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).uni (Bl, 7, 40.0f, 100.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 200.0f, 30.0f)
                .fenv (Bl, 100.0f, 8000.0f, 1.0f, 100.0f, 2000.0f).amp (Bl, 6000.0f, 1000.0f, 100.0f, 2000.0f)
                .mod (srcFiltEnv, dstPitch, 25.0f, 2)
                .g (ParamIDs::master, 2.0f).delay (d8D, 25.0f, 50.0f).reverb (rHall, 50.0f, 8.0f),

            B ("FX – Romvind")
                .osc (A, oscNoise, wAnalog, 50.0f).filter (A, fltBP, 800.0f, 80.0f).l (A, lp::spread, 100.0f)
                .amp (A, 1500.0f, 2000.0f, 100.0f, 4000.0f).lfo (A, 1, 0.15f, lfoDrift).lfo (A, 2, 0.4f, lfoSine)
                .mod (srcLfo1, dstCutoff, 60.0f, 1).mod (srcLfo2, dstPan, 70.0f, 1).mod (srcOrbX, dstReso, 30.0f)
                .g (ParamIDs::master, 4.0f).reverb (rHall, 50.0f, 10.0f),

            B ("FX – Laser")
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::oct, 1.0f).filter (A, fltLP24, 6000.0f, 30.0f)
                .fenv (A, 0.0f, 1.0f, 250.0f, 0.0f, 200.0f).amp (A, 1.0f, 400.0f, 0.0f, 300.0f)
                .mod (srcFiltEnv, dstPitch, 60.0f).mod (srcModWheel, dstCutoff, -30.0f)
                .delay (d16, 30.0f, 55.0f, 5000.0f).reverb (rPlate, 25.0f, 2.0f),

            B ("FX – Sirene")
                .osc (A, oscAnalog, wAnalog, 100.0f).l (A, lp::sym, 40.0f).filter (A, fltLP24, 4000.0f, 20.0f)
                .amp (A, 5.0f, 500.0f, 100.0f, 300.0f).lfo (A, 1, 2.0f, lfoSquare)
                .mod (srcLfo1, dstPitch, 10.0f).mod (srcModWheel, dstLfo1Rate, 50.0f)
                .g (ParamIDs::master, -6.0f).reverb (rRoom, 20.0f, 1.5f),
        };
    }
}
