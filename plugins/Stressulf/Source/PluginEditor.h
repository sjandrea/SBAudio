#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace stressulfui
{
    struct Levels
    {
        float inDb = -100.0f;   // before Input
        float outDb = -100.0f;
        float grDb = 0.0f;      // positive
    };

    /** Row of buttons bound to a choice parameter. */
    class ChoiceButtons : public juce::Component
    {
    public:
        ChoiceButtons (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour on);
        void resized() override;
        void setTooltips (const juce::StringArray&);

    private:
        void setCurrent (int);
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
    };

    /** Static curve (level into the compressor -> out) with the live level as a dot. */
    class TransferCurve : public juce::Component
    {
    public:
        void update (int ratio, bool smash, float levelDb);
        void paint (juce::Graphics&) override;

    private:
        int ratio = -1;
        bool smash = false;
        float level = -100.0f;
    };

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

class StressulfEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StressulfEditor (StressulfProcessor&);
    ~StressulfEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 900, designH = 560;

private:
    struct Content : public juce::Component
    {
        explicit Content (StressulfEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        StressulfEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void setupToggle (juce::TextButton&, const char* paramId, const juce::String& tip);

    StressulfProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    stressulfui::ChoiceButtons ratioButtons, distButtons;
    juce::TextButton smashBtn { "SMASH" }, detHpBtn { "DET HP" }, bandBtn { "BAND" }, audioHpBtn { "AUDIO HP" }, linkBtn { "LINK" };
    stressulfui::TransferCurve curve;
    stressulfui::History history;
    stressulfui::Meters meters;

    sbaudio::Knob input { "INPUT" }, attack { "ATTACK" }, release { "RELEASE" }, mix { "MIX" }, output { "OUTPUT" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Image background;   // painting behind the whole UI (Assets/background.jpg)
    juce::Rectangle<int> dynPanel, colourPanel, outPanel;
    stressulfui::Levels last;
    int framesWithoutAudio = 0;
    bool timesFixed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StressulfEditor)
};
