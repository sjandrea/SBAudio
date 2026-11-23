#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "MasteringEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* style       = "style";        // 0 = Varm, 1 = Balansert, 2 = Åpen
    inline constexpr const char* loudness    = "loudness";     // target, LUFS
    inline constexpr const char* low         = "low";          // dB
    inline constexpr const char* mid         = "mid";
    inline constexpr const char* presence    = "presence";
    inline constexpr const char* high        = "high";
    inline constexpr const char* autoEq      = "autoEq";       // %
    inline constexpr const char* compression = "compression";  // %
    inline constexpr const char* deess       = "deess";        // %
    inline constexpr const char* saturation  = "saturation";   // %
    inline constexpr const char* width       = "width";        // %
    inline constexpr const char* monoBass    = "monoBass";
    inline constexpr const char* ceiling     = "ceiling";      // dBTP
    inline constexpr const char* compare     = "compare";
}

class DominaProcessor : public juce::AudioProcessor
{
public:
    DominaProcessor();
    ~DominaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    void startAnalysis()  { shared.command.store (1); }
    void cancelAnalysis() { shared.command.store (2); }
    void resetMeters()    { shared.command.store (3); }

    domina::Settings readSettings() const;
    double getSampleRateForDisplay() const { const double r = getSampleRate(); return r > 0.0 ? r : 48000.0; }

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;
    domina::SharedState shared;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    domina::Engine engine { shared };

    std::atomic<float>* pStyle = nullptr;
    std::atomic<float>* pLoudness = nullptr;
    std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pMid = nullptr;
    std::atomic<float>* pPresence = nullptr;
    std::atomic<float>* pHigh = nullptr;
    std::atomic<float>* pAutoEq = nullptr;
    std::atomic<float>* pCompression = nullptr;
    std::atomic<float>* pDeess = nullptr;
    std::atomic<float>* pSaturation = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMonoBass = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pCompare = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DominaProcessor)
};
