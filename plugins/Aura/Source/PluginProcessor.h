#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "ReverbEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* algo     = "algo";      // choice, see aura::algoNames
    inline constexpr const char* preDelay = "predelay";  // ms
    inline constexpr const char* decay    = "decay";     // s
    inline constexpr const char* size     = "size";      // %
    inline constexpr const char* tone     = "tone";      // -100..+100
    inline constexpr const char* lowCut   = "lowcut";    // Hz
    inline constexpr const char* mod      = "mod";       // %
}

class AuraProcessor : public juce::AudioProcessor
{
public:
    AuraProcessor();
    ~AuraProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    void reset() override { engine.reset(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    // Factory presets are also exposed as host programs (Logic's preset menu).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    aura::Settings readSettings() const;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    // Peak levels for the editor's meter (written by the audio thread, read+reset by the GUI).
    std::atomic<float> inPeak { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void storeMax (std::atomic<float>&, float) noexcept;

    aura::Engine engine;

    std::atomic<float>* pAlgo = nullptr;
    std::atomic<float>* pPreDelay = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pMod = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraProcessor)
};
