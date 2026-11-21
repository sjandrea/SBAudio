#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// Small allocation-free biquad (RBJ cookbook, transposed direct form II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setShelf (bool high, double sr, double freq, double gainDb, double q = 0.7071)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        double B0, B1, B2, A0, A1, A2;
        if (high)
        {
            B0 = A * ((A + 1) + (A - 1) * cw + sA);
            B1 = -2 * A * ((A - 1) + (A + 1) * cw);
            B2 = A * ((A + 1) + (A - 1) * cw - sA);
            A0 = (A + 1) - (A - 1) * cw + sA;
            A1 = 2 * ((A - 1) - (A + 1) * cw);
            A2 = (A + 1) - (A - 1) * cw - sA;
        }
        else
        {
            B0 = A * ((A + 1) - (A - 1) * cw + sA);
            B1 = 2 * A * ((A - 1) - (A + 1) * cw);
            B2 = A * ((A + 1) - (A - 1) * cw - sA);
            A0 = (A + 1) + (A - 1) * cw + sA;
            A1 = -2 * ((A - 1) + (A + 1) * cw);
            A2 = (A + 1) + (A - 1) * cw - sA;
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    inline constexpr const char* input     = "input";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* character = "character";  // 0 = TG, 1 = Tube
    inline constexpr const char* density   = "density";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
    inline constexpr const char* autogain  = "autogain";
}

//==============================================================================
class VoxHeatProcessor : public juce::AudioProcessor
{
public:
    VoxHeatProcessor();
    ~VoxHeatProcessor() override = default;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<sbaudio::PresetManager> presets;

    // Metering (read by the editor)
    std::atomic<float> outRmsDb { -100.0f };
    std::atomic<float> heat { 0.0f };

private:
    static constexpr int osOrder = 2;   // 4x oversampling
    static constexpr int osFactor = 1 << osOrder;
    static constexpr float refLevel = 0.2f; // ~ -14 dBFS, nominal vocal level for the curve

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int latency = 0;
    double sr = 44100.0;
    int currentProgram = 0;

    // Per-channel state
    struct Channel
    {
        float lowState = 0.0f;              // 1-pole split (lows bypass the saturator)
        Biquad preEmph, deEmph;             // sibilance protection
        Biquad toneLow, toneHigh;           // tilt
        float dcX = 0.0f, dcY = 0.0f;       // DC blocker
        float env = 0.0f;                   // compander envelope (OS rate)
        std::vector<float> lowDelay, dryDelay;
        int writePos = 0;
    };
    std::array<Channel, 2> ch;

    float lowCoeff = 0.0f, dcCoeff = 0.0f, envAtt = 0.0f, envRel = 0.0f;
    float lastTone = 1000.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGainS, outGainS;
    juce::SmoothedValue<float> driveS, densityS, mixS, charS, autoS;

    juce::AudioBuffer<float> dryBuf, lowBuf;
    std::vector<float> kArr, densArr, charArr, autoArr, mixArr, inArr, outArr;

    std::atomic<float>* pInput;  std::atomic<float>* pDrive;  std::atomic<float>* pChar;
    std::atomic<float>* pDens;   std::atomic<float>* pTone;   std::atomic<float>* pMix;
    std::atomic<float>* pOutput; std::atomic<float>* pAuto;

    float meterAcc = 0.0f; int meterCount = 0;
    double distAcc = 0.0, linAcc = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxHeatProcessor)
};
