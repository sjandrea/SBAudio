#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "Params.h"
#include "SynthEngine.h"

class NordlysProcessor : public juce::AudioProcessor
{
public:
    NordlysProcessor();
    ~NordlysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
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

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard
    nordlys::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readSettings (nordlys::Settings&) const;

    struct LayerPtrs
    {
        std::atomic<float> *on, *level, *pan, *oct, *coarse, *fine, *osc, *wave, *shape, *sym, *uni, *detune, *spread,
                           *gsize, *gdens, *gspray, *gpitch, *gshim, *ftype, *cutoff, *reso, *fenv, *fkey, *drive,
                           *aA, *aD, *aS, *aR, *fA, *fD, *fS, *fR, *l1rate, *l1shape, *l1fade, *l2rate, *l2shape, *l2fade;
    };
    struct SlotPtrs { std::atomic<float> *src, *via, *dst, *layer, *amt; };

    std::array<LayerPtrs, nordlys::numLayers> layerPtrs {};
    std::array<SlotPtrs, nordlys::numSlots> slotPtrs {};
    std::atomic<float> *pMaster, *pPolyMode, *pGlide, *pBend, *pVelSens, *pOrbX, *pOrbY, *pOrbRate, *pOrbDepth,
                       *pArpOn, *pArpMode, *pArpRate, *pArpOct, *pArpGate, *pChoMix, *pChoRate, *pChoDepth,
                       *pDlyMix, *pDlyTime, *pDlyFb, *pDlyTone, *pRevMix, *pRevType, *pRevDecay, *pRevTone;

    nordlys::Settings settings;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NordlysProcessor)
};
