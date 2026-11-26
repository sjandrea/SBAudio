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

    /** Hardware-style knob: black cap, white pointer, tick scale that lights up to the value. */
    class FaceplateLnf : public sbaudio::LookAndFeel
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
    };

    /** Vertical list of LED + label rows bound to a choice parameter (like the ratio LEDs on the hardware). */
    class LedChoice : public juce::Component, public juce::TooltipClient
    {
    public:
        LedChoice (juce::RangedAudioParameter&, const juce::StringArray& labels, const juce::String& title);
        void setTooltips (const juce::StringArray& t) { tips = t; }
        void setAllLit (bool);   // SMASH lights every LED, like the hardware's "all buttons in"
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::String getTooltip() override;

    private:
        int rowAt (juce::Point<int>) const;
        juce::Rectangle<float> rowBounds (int) const;
        juce::StringArray labels, tips;
        juce::String title;
        int current = 0;
        bool allLit = false;
        juce::ParameterAttachment attachment;
    };

    /** Single LED + label row bound to a bool parameter. */
    class LedToggle : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        LedToggle (juce::RangedAudioParameter&, const juce::String& label, juce::Colour led);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::String label;
        juce::Colour ledCol;
        bool on = false;
        juce::ParameterAttachment attachment;
    };

    /** Small dark screen with the last 10 s of input, output and gain reduction. */
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

    /** Vertical gain-reduction LED ladder (1 … 30 dB). */
    class GrLadder : public juce::Component
    {
    public:
        void setGr (float);
        void paint (juce::Graphics&) override;

    private:
        float gr = 0.0f, hold = 0.0f;
        int holdFrames = 0;
    };

    /** Two horizontal LED rows for input and output level. */
    class LevelRows : public juce::Component
    {
    public:
        void setLevels (const Levels&);
        void paint (juce::Graphics&) override;

    private:
        Levels shown;
    };
}

class StressulfEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StressulfEditor (StressulfProcessor&);
    ~StressulfEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1000, designH = 300;

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
    void setupToggle (stressulfui::LedToggle&, const juce::String& tip);

    StressulfProcessor& proc;
    stressulfui::FaceplateLnf lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    stressulfui::LedChoice ratioLeds, distLeds;
    stressulfui::LedToggle smashLed, detHpLed, bandLed, audioHpLed, linkLed;
    stressulfui::History history;
    stressulfui::GrLadder grLadder;
    stressulfui::LevelRows levelRows;

    sbaudio::Knob input { "INPUT" }, attack { "ATTACK" }, release { "RELEASE" }, mix { "MIX" }, output { "OUTPUT" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Image background;   // painting, very faint behind the faceplate (Assets/background.jpg)
    juce::Rectangle<int> faceplate, modeArea;
    stressulfui::Levels last;
    int framesWithoutAudio = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StressulfEditor)
};
