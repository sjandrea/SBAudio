#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "SmartEqEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* recover    = "recover";
    inline constexpr const char* tame       = "tame";
    inline constexpr const char* bias       = "bias";
    inline constexpr const char* brighten   = "brighten";
    inline constexpr const char* bass       = "bass";
    inline constexpr const char* correction = "correction";
    inline constexpr const char* profile    = "profile";
    inline constexpr const char* lowFreq    = "lowFreq";
    inline constexpr const char* highFreq   = "highFreq";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline constexpr const char* autoGain   = "autoGain";
    inline constexpr const char* delta      = "delta";
}

class FossProcessor : public juce::AudioProcessor
{
public:
    FossProcessor();
    ~FossProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
    // Learn (called from the GUI)
    void startLearning()  { shared.learnCommand.store (1); }
    void finishLearning() { shared.learnCommand.store (2); }
    void cancelLearning() { shared.learnCommand.store (3); }
    void clearLearned();
    /** Copies a freshly learned curve into the saved state. Call from the message thread. */
    void syncLearnedToState();

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;
    foss::SharedState shared;

    static const juce::Identifier learnedId;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    foss::Settings readSettings() const;
    void loadLearnedFromState();

    foss::Engine engine { shared };
    int stateVersionSynced = 0;
    juce::String learnedString;

    std::atomic<float>* pRecover = nullptr;
    std::atomic<float>* pTame = nullptr;
    std::atomic<float>* pBias = nullptr;
    std::atomic<float>* pBrighten = nullptr;
    std::atomic<float>* pBass = nullptr;
    std::atomic<float>* pCorrection = nullptr;
    std::atomic<float>* pProfile = nullptr;
    std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pHigh = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pDelta = nullptr;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FossProcessor)
};
