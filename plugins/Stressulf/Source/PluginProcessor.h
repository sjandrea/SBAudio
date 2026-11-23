#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "StressulfEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* input   = "input";
    inline constexpr const char* attack  = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* ratio   = "ratio";    // index into stressulf::ratioNames
    inline constexpr const char* dist    = "dist";     // 0 = Clean, 1 = Dist 2, 2 = Dist 3
    inline constexpr const char* detHp   = "detHp";
    inline constexpr const char* detBand = "detBand";
    inline constexpr const char* audioHp = "audioHp";
    inline constexpr const char* link    = "link";
    inline constexpr const char* smash   = "smash";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* output  = "output";
}

class StressulfProcessor : public juce::AudioProcessor
{
public:
    StressulfProcessor();
    ~StressulfProcessor() override = default;

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

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    // Live values for the editor (written by the audio thread, read+reset by the GUI).
    std::atomic<float> meterGrDb { 0.0f };    // max gain reduction since last read
    std::atomic<float> meterInMs { 0.0f };    // max block mean-square since last read (before Input)
    std::atomic<float> meterOutMs { 0.0f };

    stressulf::Settings currentSettings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void storeMax (std::atomic<float>&, float) noexcept;
    static float blockMeanSquare (const juce::AudioBuffer<float>&) noexcept;

    stressulf::Engine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StressulfProcessor)
};
