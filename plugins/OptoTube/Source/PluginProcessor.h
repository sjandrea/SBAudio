#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "CompressorEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* mode      = "mode";     // 0 = Opto, 1 = Manual, 2 = Opto + Manual
    inline constexpr const char* hpf       = "hpf";      // detector high-pass: 0 = off, 1 = 60, 2 = 100, 3 = 150 Hz
    inline constexpr const char* tube      = "tube";
    inline constexpr const char* gain      = "gain";
    inline constexpr const char* mix       = "mix";
}

namespace optotube
{
    inline const juce::StringArray modeNames { "Opto", "Manual", "Opto + Manual" };
    inline const juce::StringArray hpfNames  { "Off", "60 Hz", "100 Hz", "150 Hz" };
    inline constexpr float hpfFreqs[] = { 0.0f, 60.0f, 100.0f, 150.0f };
}

class OptoTubeProcessor : public juce::AudioProcessor
{
public:
    OptoTubeProcessor();
    ~OptoTubeProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0; }

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

    // Live values for the editor (written by the audio thread, read+reset by the GUI).
    std::atomic<float> meterGrDb { 0.0f };    // max gain reduction since last read
    std::atomic<float> meterInMs { 0.0f };    // max block mean-square since last read
    std::atomic<float> meterOutMs { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void storeMax (std::atomic<float>&, float) noexcept;
    static float blockMeanSquare (const juce::AudioBuffer<float>&) noexcept;

    optotube::CompressorEngine engine;

    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pHpf = nullptr;
    std::atomic<float>* pTube = nullptr;
    std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pMix = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoTubeProcessor)
};
