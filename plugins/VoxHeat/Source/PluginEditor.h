#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

// Flat SLOWHAND layout, same structure as OptoTube: character row, displays (curve, history, meters)
// and three knob panels.

namespace voxheatui
{
    /** Live values shared by the displays. */
    struct Levels
    {
        float inDb = -100.0f;   // RMS after Input gain
        float outDb = -100.0f;  // RMS out
        float heat = 0.0f;      // 0..1, amount of saturation
    };

    /** Row of two buttons for the Character parameter (TG / TUBE). */
    class CharacterSelector : public juce::Component
    {
    public:
        explicit CharacterSelector (juce::RangedAudioParameter&);
        void resized() override;

    private:
        void setCurrent (int);
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };

    /** Saturation transfer curve for the current Drive / Character. */
    class SaturationCurve : public juce::Component
    {
    public:
        void update (float drive01, float character, float heat);
        void paint (juce::Graphics&) override;

    private:
        float drive = -1.0f, character = -1.0f, heat = -1.0f;
    };

    /** Scrolling history: input level area, output line, heat hanging from the top. */
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

    /** INN / HEAT / UT bar meters. */
    class Meters : public juce::Component
    {
    public:
        void setLevels (const Levels&);
        void paint (juce::Graphics&) override;

    private:
        Levels shown;
    };
}

class VoxHeatEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxHeatEditor (VoxHeatProcessor&);
    ~VoxHeatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 900, designH = 540;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (VoxHeatEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        VoxHeatEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();

    VoxHeatProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    voxheatui::CharacterSelector character;
    juce::TextButton autoGain { "AUTO GAIN" };
    voxheatui::SaturationCurve curve;
    voxheatui::History history;
    voxheatui::Meters meters;

    sbaudio::Knob input { "INPUT" }, drive { "DRIVE" }, density { "DENSITY" }, tone { "TONE" },
                  mix { "MIX" }, output { "OUTPUT" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> drivePanel, colourPanel, outPanel;
    voxheatui::Levels last;
    int framesWithoutAudio = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxHeatEditor)
};
