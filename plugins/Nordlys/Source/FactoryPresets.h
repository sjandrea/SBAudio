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

            B ("Atmosfære – Fjern tone")
                .osc (A, oscGranular, wChoir, 50.0f).grain (A, 600.0f, 6.0f, 30.0f, 8.0f, 50.0f).l (A, lp::oct, 1.0f)
                .amp (A, 4000.0f, 2000.0f, 100.0f, 8000.0f).l (A, lp::spread, 100.0f)
                .osc (Bl, oscWavetable, wAir, 60.0f).uni (Bl, 3, 10.0f, 60.0f).l (Bl, lp::level, 60.0f)
                .filter (Bl, fltLP24, 2500.0f, 10.0f).amp (Bl, 3000.0f, 2000.0f, 100.0f, 8000.0f)
                .mod (srcOrbX, dstShape, 40.0f, 1).mod (srcOrbY, dstCutoff, 50.0f, 2)
                .orb (0.0f, 0.0f, 0.03f, 70.0f).delay (d2, 20.0f, 35.0f, 3000.0f).reverb (rVintage, 55.0f, 14.0f),

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

            B ("Strings – Ensemble")
                .osc (A, oscWavetable, wStrings, 50.0f).uni (A, 7, 18.0f, 100.0f).filter (A, fltLP24, 5000.0f, 5.0f)
                .amp (A, 400.0f, 1000.0f, 90.0f, 1200.0f).lfo (A, 1, 5.0f, lfoSine, 1000.0f)
                .osc (Bl, oscAnalog, wAnalog, 66.7f).l (Bl, lp::oct, -1.0f).l (Bl, lp::level, 40.0f).filter (Bl, fltLP24, 1500.0f, 5.0f)
                .amp (Bl, 400.0f, 1000.0f, 90.0f, 1200.0f)
                .mod (srcLfo1, dstFine, 6.0f).mod (srcModWheel, dstCutoff, 35.0f).mod (srcVelocity, dstCutoff, 20.0f)
                .chorus (40.0f).reverb (rHall, 35.0f, 3.5f),

            B ("Pluck – Harpe i tåke")
                .osc (A, oscWavetable, wStrings, 30.0f).filter (A, fltLP12, 3000.0f, 10.0f).fenv (A, 40.0f, 1.0f, 600.0f, 0.0f, 800.0f)
                .amp (A, 1.0f, 1200.0f, 0.0f, 1500.0f)
                .osc (Bl, oscGranular, wStrings, 40.0f).grain (Bl, 200.0f, 8.0f, 40.0f, 10.0f, 25.0f).l (Bl, lp::level, 40.0f)
                .amp (Bl, 800.0f, 2000.0f, 80.0f, 3000.0f)
                .mod (srcVelocity, dstCutoff, 30.0f, 1).mod (srcModWheel, dstLevel, 60.0f, 2)
                .reverb (rHall, 40.0f, 6.0f),

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
        };
    }
}
