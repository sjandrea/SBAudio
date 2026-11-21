#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

// Photo-based hardware faceplate in the SB Audio steel-blue palette, under the shared SB Audio top bar.
// Artwork lives in Assets/ (made by artwork/make_assets.py from artwork/source.png).
// Faceplate image coordinates are 2x the design size.

namespace voxheat::ui
{
    class FaceplateLookAndFeel : public sbaudio::LookAndFeel
    {
    public:
        FaceplateLookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawLabel (juce::Graphics&, juce::Label&) override;

        juce::Image autoOn;
    };

    /** VU needle, backlight warmth and footer print drawn over the photo's meter face. */
    class VuMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit VuMeter (VoxHeatProcessor&);
        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        VoxHeatProcessor& proc;
        float heatShown = 0.0f;
        float vu = -40.0f;
    };
}

class VoxHeatEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxHeatEditor (VoxHeatProcessor&);
    ~VoxHeatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int barH = 44;
    static constexpr int designW = 796, designH = barH + 494;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (VoxHeatEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        VoxHeatEditor& owner;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::Point<float> centre;   // faceplate image coordinates
        float radius = 0;
        float valueY = 0;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void setupKnob (Knob&, const char* paramID, juce::Point<float> centre, float radius,
                    float valueY, const juce::String& tip);

    VoxHeatProcessor& proc;
    voxheat::ui::FaceplateLookAndFeel lnf;
    Content content { *this };
    sbaudio::PresetBar presetBar;
    voxheat::ui::VuMeter meter;
    juce::Image faceplate, heatOn;

    Knob input, drive, density, tone, mix, output;
    juce::ToggleButton charSwitch, autoGain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> charAtt, autoAtt;
    juce::TooltipWindow tooltips { this, 700 };
    float lampShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxHeatEditor)
};
