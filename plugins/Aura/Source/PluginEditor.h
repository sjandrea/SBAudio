#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace auraui
{
    /** Row of buttons that selects the reverb algorithm. */
    class AlgoSelector : public juce::Component
    {
    public:
        explicit AlgoSelector (juce::RangedAudioParameter&);
        void resized() override;

    private:
        void setCurrent (int);
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attachment;
        int current = -1;
    };

    /** Draws the reverb envelope (pre-delay, early reflections, decay) from the current settings. */
    class DecayDisplay : public juce::Component
    {
    public:
        void setSettings (const aura::Settings&);
        void paint (juce::Graphics&) override;

    private:
        aura::Settings s;
        bool valid = false;
    };

    class LevelMeter : public juce::Component
    {
    public:
        void setLevels (float l, float r);
        void paint (juce::Graphics&) override;

    private:
        float levL = -100.0f, levR = -100.0f;
    };
}

class AuraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuraEditor (AuraProcessor&);
    ~AuraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 860, designH = 480;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (AuraEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        AuraEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();

    AuraProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    auraui::AlgoSelector algoSelector;
    auraui::DecayDisplay display;
    auraui::LevelMeter meter;
    sbaudio::Knob preDelay { "PRE-DELAY" }, decay { "DECAY" }, size { "SIZE" }, tone { "TONE" },
                  lowCut { "LOW CUT" }, mod { "MODULATION" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> knobPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraEditor)
};
