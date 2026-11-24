#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace dovreui
{
    /** Row of buttons bound to a choice parameter. */
    class ChoiceButtons : public juce::Component
    {
    public:
        ChoiceButtons (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour on);
        void resized() override;
        juce::OwnedArray<juce::TextButton> buttons;

    private:
        void setCurrent (int);
        juce::ParameterAttachment attachment;
    };

    /** Echo pattern (L up, R down) drawn from the current settings. */
    class EchoDisplay : public juce::Component
    {
    public:
        void setSettings (const dovre::Settings&, bool synced, const juce::String& noteName, float bpm);
        void paint (juce::Graphics&) override;

    private:
        dovre::Settings s;
        bool synced = false, valid = false;
        juce::String note;
        float bpm = 120.0f;
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

class DovregubbenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DovregubbenEditor (DovregubbenProcessor&);
    ~DovregubbenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr int designW = 960, designH = 600;

private:
    struct Content : public juce::Component
    {
        explicit Content (DovregubbenEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        DovregubbenEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void setInfo (juce::Component&, const char* text);
    juce::String infoFor (juce::Component*) const;

    DovregubbenProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    dovreui::ChoiceButtons modeButtons, styleButtons;
    dovreui::EchoDisplay display;
    dovreui::LevelMeter meter;
    juce::TextButton syncBtn { "SYNC" };

    sbaudio::Knob mix { "MIX" }, delay { "DELAY" }, note { "DELAY" }, feedback { "FEEDBACK" },
                  ratio { "DELAY R" }, width { "WIDTH" }, age { "AGE" }, duck { "DUCKING" },
                  diffusion { "AMOUNT" }, diffSize { "SIZE" }, rate { "RATE" }, depth { "DEPTH" },
                  lowCut { "LOW CUT" }, highCut { "HIGH CUT" }, pitch { "PITCH" }, detune { "DETUNE" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;

    std::vector<std::pair<juce::Component*, juce::String>> infos;
    juce::String info;
    juce::Rectangle<int> delayPanel, diffPanel, modPanel, eqPanel, pitchPanel, infoStrip;
    int lastMode = -1, lastStyle = -1;
    bool lastSync = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DovregubbenEditor)
};
