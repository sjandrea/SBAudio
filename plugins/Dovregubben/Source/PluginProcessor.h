#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sbaudio_common/sbaudio_common.h>
#include "DelayEngine.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* mode      = "mode";      // choice, dovre::modeNames
    inline constexpr const char* style     = "style";     // choice, dovre::styleNames
    inline constexpr const char* mix       = "mix";       // %
    inline constexpr const char* sync      = "sync";      // bool
    inline constexpr const char* delay     = "delay";     // ms (when not synced)
    inline constexpr const char* note      = "note";      // choice, dovre::noteNames (when synced)
    inline constexpr const char* ratio     = "ratio";     // R delay, % of L
    inline constexpr const char* feedback  = "feedback";  // %
    inline constexpr const char* width     = "width";     // %
    inline constexpr const char* age       = "age";       // %
    inline constexpr const char* duck      = "duck";      // %
    inline constexpr const char* diffusion = "diffusion"; // %
    inline constexpr const char* diffSize  = "diffsize";  // %
    inline constexpr const char* modRate   = "modrate";   // Hz
    inline constexpr const char* modDepth  = "moddepth";  // %
    inline constexpr const char* lowCut    = "lowcut";    // Hz
    inline constexpr const char* highCut   = "highcut";   // Hz
    inline constexpr const char* pitch     = "pitch";     // semitones
    inline constexpr const char* detune    = "detune";    // cents
}

class DovregubbenProcessor : public juce::AudioProcessor
{
public:
    DovregubbenProcessor();
    ~DovregubbenProcessor() override = default;

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

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Current settings, with delay times resolved from tempo when synced. */
    dovre::Settings readSettings() const;

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    std::atomic<float> inPeak { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<float> bpm { 120.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void storeMax (std::atomic<float>&, float) noexcept;

    dovre::Engine engine;
    std::atomic<float>* p[19] {};
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DovregubbenProcessor)
};
