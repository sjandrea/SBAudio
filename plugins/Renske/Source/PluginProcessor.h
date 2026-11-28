#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "Modules.h"
#include <map>
#include <set>

namespace renske
{
    /** One suggestion from the assistant: a sentence and the parameter values it would set. */
    struct Suggestion
    {
        juce::String text;
        std::vector<std::pair<juce::String, float>> values;
    };
}

class RenskeProcessor : public juce::AudioProcessor
{
public:
    RenskeProcessor();
    ~RenskeProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.2; }

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
    renske::Shared shared;

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };   // peak since last GUI read
    std::atomic<bool> sidechainActive { false };

    // noise profile
    void startLearn()  { shared.learnCommand = 1; }
    void finishLearn() { shared.learnCommand = 2; }
    void cancelLearn() { shared.learnCommand = 3; }
    bool hasNoiseProfile() const { return profileAvailable.load(); }

    // assistant
    static constexpr float analysisSeconds = 10.0f;
    void startAnalysis();
    void stopAnalysis() { shared.analyse = 0; }
    bool isAnalysing() const { return shared.analyse.load() != 0; }
    float analysisProgress() const { return juce::jlimit (0.0f, 1.0f, shared.anSeconds.load() / analysisSeconds); }
    std::vector<renske::Suggestion> makeSuggestions() const;
    void applySuggestions (const std::vector<renske::Suggestion>&);

    int getTotalLatency() const { return latency; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    renske::Settings readSettings() const;
    float p (const char* id) const { return params.at (id)->load(); }
    void setParam (const juce::String& id, float plainValue);

    renske::ClickStage clickStage;
    renske::HumStage humStage;
    renske::PlosiveStage plosiveStage;
    renske::SpectralStage spectral;
    renske::DelayLine dryDelay;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> outGain, mixGain, deltaGain;
    int latency = 0;
    double sr = 44100.0;

    std::map<juce::String, std::atomic<float>*> params;

    // noise profile handover (message thread <-> audio thread)
    juce::SpinLock profileLock;
    std::vector<float> savedProfile;      // last learned profile (dB not used: linear power per bin)
    std::vector<float> pendingProfile;    // from setStateInformation, applied on the audio thread
    std::atomic<bool> pendingProfileFlag { false };
    std::atomic<bool> profileAvailable { false };
    int seenProfileVersion = 0;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenskeProcessor)
};
