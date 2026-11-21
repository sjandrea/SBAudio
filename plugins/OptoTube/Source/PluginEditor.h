#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace optoui
{
    /** Live values shared by the displays (dB). */
    struct Levels
    {
        float inDb = -100.0f;   // input level (peak-calibrated RMS, like the detector)
        float outDb = -100.0f;
        float grDb = 0.0f;      // gain reduction, positive
    };

    /** Row of buttons that selects the mode (Opto / Manual / Opto + Manual). */
    class ModeSelector : public juce::Component
    {
    public:
        explicit ModeSelector (juce::RangedAudioParameter&);
        void resized() override;

    private:
        void setCurrent (int);
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };

    /** Static compression curve (in -> out) with the current input level as a dot. */
    class TransferCurve : public juce::Component
    {
    public:
        void update (float thresholdDb, float ratio, int mode, float inDb);
        void paint (juce::Graphics&) override;

    private:
        float thr = -20.0f, ratio = 3.0f, inDb = -100.0f;
        int mode = 0;
    };

    /** Scrolling history: input and output level, gain reduction hanging from the top. */
    class History : public juce::Component
    {
    public:
        static constexpr int numPoints = 300; // 10 s at 30 Hz
        void push (const Levels&);
        void paint (juce::Graphics&) override;

    private:
        std::array<Levels, numPoints> data {};
        int write = 0;
    };

    /** IN / GR / UT bar meters. */
    class Meters : public juce::Component
    {
    public:
        void setLevels (const Levels&);
        void paint (juce::Graphics&) override;

    private:
        Levels shown;
        float grHold = 0.0f;
        int holdFrames = 0;
    };
}

class OptoTubeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OptoTubeEditor (OptoTubeProcessor&);
    ~OptoTubeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 900, designH = 540;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (OptoTubeEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        OptoTubeEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void updateTimeSection();

    OptoTubeProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    optoui::ModeSelector modeSelector;
    juce::ComboBox hpfBox;
    optoui::TransferCurve curve;
    optoui::History history;
    optoui::Meters meters;

    sbaudio::Knob threshold { "THRESHOLD" }, ratio { "RATIO" }, attack { "ATTACK" }, release { "RELEASE" },
                  tube { "TUBE" }, mix { "MIX" }, gain { "GAIN" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfAtt;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> compPanel, timePanel, colourPanel;
    optoui::Levels last;
    int framesWithoutAudio = 0;
    bool timeIsAuto = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoTubeEditor)
};
