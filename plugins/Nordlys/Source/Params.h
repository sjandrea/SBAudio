#pragma once

#include <juce_core/juce_core.h>

/*  Nordlys parameter definitions.

    NB: parameter IDs and the order of every choice list below are stored in Logic projects,
    automation and presets. Never rename an ID or reorder a list – only append.

    Layer parameters are named "l<1..4>_<name>", modulation matrix slots "m<1..8>_<name>".
*/
namespace nordlys
{
    inline constexpr int numLayers = 4;
    inline constexpr int numSlots  = 8;
    inline constexpr int maxVoices = 16;
    inline constexpr int maxUnison = 7;
    inline constexpr int maxGrains = 24;

    enum OscType   { oscAnalog = 0, oscWavetable, oscFM, oscGranular, oscNoise };
    enum FilterType{ fltLP24 = 0, fltLP12, fltBP, fltHP, fltNotch, fltOff };
    enum LfoShape  { lfoSine = 0, lfoTriangle, lfoSaw, lfoSquare, lfoSH, lfoDrift };
    enum ModSrc    { srcOff = 0, srcLfo1, srcLfo2, srcFiltEnv, srcAmpEnv, srcModWheel, srcVelocity,
                     srcAftertouch, srcKey, srcRandom, srcOrbX, srcOrbY, numSources };
    enum ModDst    { dstOff = 0, dstPitch, dstFine, dstCutoff, dstReso, dstLevel, dstPan, dstShape, dstSym,
                     dstGrainSize, dstGrainDensity, dstDetune, dstLfo1Rate, dstLfo2Rate, numDests };
    enum PolyMode  { modePoly = 0, modeMono, modeLegato };
    enum ArpMode   { arpUp = 0, arpDown, arpUpDown, arpRandom, arpPlayed, arpChord };

    inline const juce::StringArray oscTypeNames    { "Analog", "Wavetable", "FM", "Granular", "Noise" };
    inline const juce::StringArray waveNames       { "Analog", "Glass", "Choir", "Strings", "Organ", "Digital", "Air", "Metal" };
    inline const juce::StringArray filterTypeNames { "Low Pass 24", "Low Pass 12", "Band Pass", "High Pass", "Notch", "Off" };
    inline const juce::StringArray lfoShapeNames   { "Sine", "Triangle", "Saw", "Square", "S&H", "Drift" };
    inline const juce::StringArray modSrcNames     { "Off", "LFO 1", "LFO 2", "Filter Env", "Amp Env", "Mod Wheel", "Velocity",
                                                     "Aftertouch", "Key", "Random", "Orb X", "Orb Y" };
    inline const juce::StringArray modDstNames     { "Off", "Pitch", "Fine", "Cutoff", "Resonance", "Level", "Pan", "Shape",
                                                     "Symmetry", "Grain Size", "Grain Density", "Detune", "LFO 1 Rate", "LFO 2 Rate" };
    inline const juce::StringArray layerTargetNames{ "All", "A", "B", "C", "D" };
    inline const juce::StringArray polyModeNames   { "Poly", "Mono", "Legato" };
    inline const juce::StringArray arpModeNames    { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" };
    inline const juce::StringArray arpRateNames    { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline constexpr double arpRateBeats[]         { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    inline const juce::StringArray delayTimeNames  { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" };
    inline constexpr double delayTimeBeats[]       { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
    inline const juce::StringArray reverbTypeNames { "Ambience", "Room", "Chamber", "Hall", "Plate", "Vintage" }; // = aura::Algo

    /** FM operator ratios selected by the Symmetry knob in FM mode. */
    inline constexpr float fmRatios[] { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    inline constexpr int numFmRatios = 12;
    inline int fmRatioIndex (float sym01) { return juce::jlimit (0, numFmRatios - 1, juce::roundToInt (sym01 * (numFmRatios - 1))); }

    inline juce::String layerId (int layer, const char* name) { return "l" + juce::String (layer + 1) + "_" + name; }
    inline juce::String slotId  (int slot,  const char* name) { return "m" + juce::String (slot + 1) + "_" + name; }
    inline juce::String layerLetter (int layer) { return juce::String::charToString ((juce::juce_wchar) ('A' + layer)); }

    // Per-layer parameter names (prefixed with l1_ … l4_)
    namespace lp
    {
        inline constexpr const char* on      = "on";
        inline constexpr const char* level   = "level";
        inline constexpr const char* pan     = "pan";
        inline constexpr const char* oct     = "oct";
        inline constexpr const char* coarse  = "coarse";
        inline constexpr const char* fine    = "fine";
        inline constexpr const char* osc     = "osc";
        inline constexpr const char* wave    = "wave";
        inline constexpr const char* shape   = "shape";
        inline constexpr const char* sym     = "sym";
        inline constexpr const char* uni     = "uni";
        inline constexpr const char* detune  = "detune";
        inline constexpr const char* spread  = "spread";
        inline constexpr const char* gsize   = "gsize";
        inline constexpr const char* gdens   = "gdens";
        inline constexpr const char* gspray  = "gspray";
        inline constexpr const char* gpitch  = "gpitch";
        inline constexpr const char* gshim   = "gshim";
        inline constexpr const char* ftype   = "ftype";
        inline constexpr const char* cutoff  = "cutoff";
        inline constexpr const char* reso    = "reso";
        inline constexpr const char* fenv    = "fenv";
        inline constexpr const char* fkey    = "fkey";
        inline constexpr const char* drive   = "drive";
        inline constexpr const char* aA      = "aA";
        inline constexpr const char* aD      = "aD";
        inline constexpr const char* aS      = "aS";
        inline constexpr const char* aR      = "aR";
        inline constexpr const char* fA      = "fA";
        inline constexpr const char* fD      = "fD";
        inline constexpr const char* fS      = "fS";
        inline constexpr const char* fR      = "fR";
        inline constexpr const char* l1rate  = "lfo1rate";
        inline constexpr const char* l1shape = "lfo1shape";
        inline constexpr const char* l1fade  = "lfo1fade";
        inline constexpr const char* l2rate  = "lfo2rate";
        inline constexpr const char* l2shape = "lfo2shape";
        inline constexpr const char* l2fade  = "lfo2fade";
    }

    // Modulation matrix slot parameter names (prefixed with m1_ … m8_)
    namespace mp
    {
        inline constexpr const char* src   = "src";
        inline constexpr const char* via   = "via";
        inline constexpr const char* dst   = "dst";
        inline constexpr const char* layer = "layer";
        inline constexpr const char* amt   = "amt";
    }
}

// Global parameters
namespace ParamIDs
{
    inline constexpr const char* master   = "master";
    inline constexpr const char* polyMode = "polymode";
    inline constexpr const char* glide    = "glide";
    inline constexpr const char* bend     = "bend";
    inline constexpr const char* velSens  = "velsens";
    inline constexpr const char* orbX     = "orbx";
    inline constexpr const char* orbY     = "orby";
    inline constexpr const char* orbRate  = "orbrate";
    inline constexpr const char* orbDepth = "orbdepth";
    inline constexpr const char* arpOn    = "arpon";
    inline constexpr const char* arpMode  = "arpmode";
    inline constexpr const char* arpRate  = "arprate";
    inline constexpr const char* arpOct   = "arpoct";
    inline constexpr const char* arpGate  = "arpgate";
    inline constexpr const char* choMix   = "chomix";
    inline constexpr const char* choRate  = "chorate";
    inline constexpr const char* choDepth = "chodepth";
    inline constexpr const char* dlyMix   = "dlymix";
    inline constexpr const char* dlyTime  = "dlytime";
    inline constexpr const char* dlyFb    = "dlyfb";
    inline constexpr const char* dlyTone  = "dlytone";
    inline constexpr const char* revMix   = "revmix";
    inline constexpr const char* revType  = "revtype";
    inline constexpr const char* revDecay = "revdecay";
    inline constexpr const char* revTone  = "revtone";
}
