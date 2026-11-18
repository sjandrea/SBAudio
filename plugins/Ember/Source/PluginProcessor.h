#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "SaturationDsp.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* input    = "input";
    inline constexpr const char* output   = "output";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* quality  = "quality";   // 0 = 2x, 1 = 4x, 2 = 8x oversampling
    inline constexpr const char* autoGain = "autoGain";
    inline constexpr const char* bands    = "bands";     // number of bands in use, 1..6
    inline juce::String split (int index) { return "x" + juce::String (index + 1); } // x1..x5 (Hz)

    // Per band: "b<1..6>_<name>"
    inline constexpr const char* bandParamNames[] = { "style", "drive", "dyn", "tone", "fb", "fbfreq",
                                                      "mix", "level", "on", "mute" };
    inline juce::String band (int index, const char* name) { return "b" + juce::String (index + 1) + "_" + name; }
}

namespace ember
{
    inline const juce::StringArray qualityNames { "Normal (2x)", juce::String (juce::CharPointer_UTF8 ("Høy (4x)")), "Maks (8x)" };
    inline constexpr float defaultSplits[maxSplits] = { 120.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f };
}

class EmberProcessor : public juce::AudioProcessor
{
public:
    EmberProcessor();
    ~EmberProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    // Factory presets are also exposed as host programs (Logic's preset menu).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    int getNumBands() const;
    /** Crossover frequencies as the engine uses them (sorted, spaced). */
    std::array<float, ember::maxSplits> getSplits() const;
    double getSampleRateSafe() const { return sampleRateHz > 0 ? sampleRateHz : 48000.0; }

    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }
    juce::RangedAudioParameter* bandParam (int band, const char* name) const { return param (ParamIDs::band (band, name)); }
    float bandValue (int band, const char* name) const;
    /** Sets a parameter from the GUI (plain value) as a single host gesture. */
    void setPlain (const juce::String& id, float plainValue);

    /** Splits the band that contains freq in two (both halves keep its settings).
        Returns the index of the upper half, or -1 if not possible. */
    int insertSplit (float freq);
    /** Removes a band; its neighbour takes over the frequency range. */
    void deleteBand (int band);

    // ---- live data for the editor --------------------------------------------
    sbaudio::AnalyzerRing preRing, postRing;
    std::atomic<float> inPeak { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<int> soloMask { 0 };        // GUI only, not saved

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void storeMax (std::atomic<float>&, float) noexcept;
    void copyBand (int from, int to);
    void readSettings();

    ember::Engine engine;
    ember::Settings settings;
    std::atomic<bool> restructurePending { false };
    bool haveSettings = false;

    struct BandPtrs { std::atomic<float>* p[10] {}; };
    std::array<BandPtrs, ember::maxBands> bandPtrs;
    std::array<std::atomic<float>*, ember::maxSplits> splitPtrs {};
    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pQuality = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pBands = nullptr;

    double sampleRateHz = 0.0;
    int maxBlock = 512;
    int lastLatency = -1;
    std::vector<float> monoScratch;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberProcessor)
};
