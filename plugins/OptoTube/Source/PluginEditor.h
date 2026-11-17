#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace optotube::ui
{
    /** Hardware look: black knobs with aluminium skirt on a steel-blue faceplate. */
    class HardwareLookAndFeel : public sbaudio::LookAndFeel
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
    };

    /** Rotary control with engraved caption, scale labels around it and a value readout. */
    class HardwareKnob : public juce::Component
    {
    public:
        struct Mark { double value; juce::String text; };

        HardwareKnob (const juce::String& caption, std::vector<Mark> marks, bool stepped = false);
        void paint (juce::Graphics&) override;
        void resized() override;
        void enablementChanged() override { repaint(); }

        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

    private:
        juce::Rectangle<float> knobArea() const;
        juce::String caption;
        std::vector<Mark> marks;
        bool stepped;
    };

    /** Analogue VU meter showing gain reduction, input or output level. */
    class VuMeter : public juce::Component
    {
    public:
        enum Source { GainReduction = 0, Input, Output };
        void setSource (int s) { source = s; repaint(); }
        /** dB value to display: VU (0 = -18 dBFS) or minus gain reduction. */
        void setValue (float vuDb);
        void setReadout (const juce::String& s) { if (s != readout) { readout = s; repaint(); } }
        void paint (juce::Graphics&) override;

    private:
        static float vuToProportion (float vu);
        int source = GainReduction;
        float needle = 0.0f;   // smoothed VU dB
        juce::String readout;
    };
}

class OptoTubeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OptoTubeEditor (OptoTubeProcessor&);
    ~OptoTubeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 960, designH = 410;

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
    void setMeterSource (int);
    void updateEnablement();

    OptoTubeProcessor& proc;
    optotube::ui::HardwareLookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;

    using K = optotube::ui::HardwareKnob;
    K threshold, ratio, attack, release, gain, mode, hpf, tube, mix;
    optotube::ui::VuMeter meter;
    juce::TextButton meterGr { "GR" }, meterIn { "INN" }, meterOut { "UT" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> faceplate, leftGroup, rightGroup, meterArea;
    float lastGr = 0.0f, lastIn = 0.0f, lastOut = 0.0f;
    int framesWithoutAudio = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoTubeEditor)
};
