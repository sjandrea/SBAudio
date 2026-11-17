#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include "EqDsp.h"

namespace ParamIDs
{
    // Global parameters
    inline constexpr const char* output    = "output";
    inline constexpr const char* autoGain  = "autoGain";
    inline constexpr const char* gainScale = "gainScale";
    inline constexpr const char* mode      = "mode";      // 0 = Zero Latency, 1 = Linear Phase
    inline constexpr const char* character = "character"; // 0 = Clean, 1 = Subtle, 2 = Warm
    inline constexpr const char* invert    = "invert";

    // Per band: "b<1..24>_<name>"
    inline constexpr const char* bandParamNames[] = { "used", "on", "type", "freq", "gain", "q", "slope", "place",
                                                      "dyn", "range", "thr", "att", "rel", "sc" };
    inline juce::String band (int index, const char* name) { return "b" + juce::String (index + 1) + "_" + name; }
}

/** Simple lock-free single-writer ring for the spectrum analyzer (display only). */
struct AnalyzerRing
{
    static constexpr int size = 1 << 15;
    std::vector<float> data = std::vector<float> ((size_t) size, 0.0f);
    std::atomic<int> writePos { 0 };

    void push (const float* x, int n) noexcept
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            data[(size_t) w] = x[i];
            w = (w + 1) & (size - 1);
        }
        writePos.store (w, std::memory_order_release);
    }

    /** Copies the latest n samples (oldest first). */
    void read (float* dest, int n) const noexcept
    {
        int r = (writePos.load (std::memory_order_acquire) - n) & (size - 1);
        for (int i = 0; i < n; ++i)
        {
            dest[i] = data[(size_t) r];
            r = (r + 1) & (size - 1);
        }
    }
};

class PrismEQProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PrismEQProcessor();
    ~PrismEQProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.1; }

    // Factory presets are exposed as host programs (Logic shows them in its preset menu).
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    prism::BandState getBandState (int band) const;
    /** Static gain scale (0..2) as a factor. */
    float getGainScale() const { return pGainScale->load() / 100.0f; }
    double getSampleRateSafe() const { return sampleRateHz > 0 ? sampleRateHz : 48000.0; }

    /** Sets a band parameter from the GUI (plain value) as a single host gesture. */
    void setBandParam (int band, const char* name, float plainValue);
    juce::RangedAudioParameter* bandParam (int band, const char* name) const;

    /** Creates a band at the first free slot. Returns its index or -1 when all are in use. */
    int createBand (int type, float freq, float gain, float q);
    void deleteBand (int band);
    void resetAllParameters();

    // ---- undo / redo and A/B (parameter snapshots; host automation is not recorded) ----
    std::vector<float> captureParams() const;
    void applyParams (const std::vector<float>&);
    /** Called regularly by the editor. userEdit = the change since last call came from the GUI. */
    void syncUndo (bool userEdit);
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void selectAB (int slot);
    void copyCurrentToOtherAB();
    int getABSlot() const { return abIndex; }

    // ---- live data for the editor --------------------------------------------
    AnalyzerRing preRing, postRing;
    std::array<std::atomic<float>, prism::numBands> dynGainDb {};  // current dynamic offset per band
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<float> autoGainDb { 0.0f };
    std::atomic<int> soloBand { -1 };
    std::atomic<bool> sidechainActive { false };

    static constexpr int subBlock = 32;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void rebuildLinearPhase();
    int linearFirLength() const;
    double computeAutoGain() const;

    struct BandPtrs
    {
        std::atomic<float>* p[14] {};
        float get (int i) const { return p[i]->load(); }
    };
    std::array<BandPtrs, prism::numBands> bandPtrs;

    struct BandRuntime
    {
        prism::BandDesign design;
        std::array<std::array<prism::SectionState, prism::BandDesign::maxSections>, 2> st {};
        prism::Section detect;
        prism::SectionState detectSt;
        float env = 0.0f;           // detector level (linear)
        float dynDb = 0.0f;         // smoothed dynamic gain offset
        float sFreq = 1000.0f, sGain = 0.0f, sQ = 1.0f; // smoothed
        float mix = 0.0f;           // on/off fade
        bool init = false;
        int lastType = -1, lastSlope = -1;
        double lastF = 0, lastG = 0, lastQ = 0;
    };
    std::array<BandRuntime, prism::numBands> rt;

    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pGainScale = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pCharacter = nullptr;
    std::atomic<float>* pInvert = nullptr;

    double sampleRateHz = 0.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> dryBuffer, scBuffer, delayBuf;
    int delayWrite = 0;
    std::vector<float> monoScratch;

    // solo
    prism::Section soloFilter;
    std::array<prism::SectionState, 2> soloSt {};
    float soloMix = 0.0f;
    int lastSolo = -1;

    // output stage
    juce::SmoothedValue<float> outGainSm;
    std::array<float, 2> dcX {}, dcY {};

    // linear phase
    juce::dsp::Convolution convolution;
    std::atomic<bool> linearActive { false }, linearReady { false };
    int linearLatency = 0;
    juce::int64 lastLinearHash = 0;
    int lastReportedLatency = -1;

    int currentProgram = 0;

    std::vector<std::vector<float>> undoStack, redoStack;
    std::vector<float> committed;
    std::array<std::vector<float>, 2> abSlots;
    int abIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismEQProcessor)
};
