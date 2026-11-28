#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace renskeui
{
    /** One row in the module chain: power button, name, live activity. Click selects the module. */
    class ChainRow : public juce::Component
    {
    public:
        ChainRow (RenskeProcessor&, const renske::ModuleInfo&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;

        void setSelected (bool);
        void setActivity (float value, bool isOn);   // events/s or dB

        std::function<void()> onSelect;
        const renske::ModuleInfo& info;

    private:
        juce::TextButton power;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
        bool selected = false, on = false;
        float activity = 0.0f;
    };

    /** Scrolling spectrogram: input, output or what is removed. */
    class Spectrogram : public juce::Component
    {
    public:
        Spectrogram();
        void push (const std::array<float, renske::Shared::numBands>& in,
                   const std::array<float, renske::Shared::numBands>& out);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        static constexpr int numCols = 480;
        juce::Image img { juce::Image::ARGB, numCols, renske::Shared::numBands, true };
        int writeX = 0;
        int mode = 1;   // 0 = in, 1 = out, 2 = removed
        juce::TextButton modeBtns[3];
        juce::Rectangle<int> plotArea() const;
    };

    /** Controls for the selected module, built from its parameter list. */
    class ModulePanel : public juce::Component, private juce::Timer
    {
    public:
        explicit ModulePanel (RenskeProcessor&);
        ~ModulePanel() override;
        void show (int moduleIndex);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void updateLearnButton();

        RenskeProcessor& proc;
        const renske::ModuleInfo* info = nullptr;
        juce::TextButton power { "AV" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;

        juce::OwnedArray<sbaudio::Knob> knobs;
        juce::OwnedArray<juce::ComboBox> combos;
        juce::OwnedArray<juce::TextButton> toggles;
        juce::OwnedArray<juce::Label> captions;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;

        juce::TextButton learnBtn;
        juce::String status;
    };

    /** IN / UT peak meters. */
    class Meters : public juce::Component
    {
    public:
        void setLevels (float inDb, float outDb);
        void paint (juce::Graphics&) override;

    private:
        float in = -100.0f, out = -100.0f, inHold = -100.0f, outHold = -100.0f;
        int inHoldFrames = 0, outHoldFrames = 0;
    };

    /** Assistant: listens for 10 s, then proposes a chain. */
    class Assistant : public juce::Component
    {
    public:
        explicit Assistant (RenskeProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void tick();

    private:
        void showResults();
        RenskeProcessor& proc;
        juce::TextButton analyseBtn { "ANALYSER" }, applyBtn { "BRUK FORSLAG" };
        juce::TextEditor results;
        std::vector<renske::Suggestion> suggestions;
        bool wasAnalysing = false;
    };
}

class RenskeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RenskeEditor (RenskeProcessor&);
    ~RenskeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1060, designH = 680;

private:
    struct Content : public juce::Component
    {
        explicit Content (RenskeEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        RenskeEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void select (int index);

    RenskeProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    juce::OwnedArray<renskeui::ChainRow> rows;
    renskeui::Spectrogram spectrogram;
    renskeui::ModulePanel modulePanel;
    renskeui::Meters meters;
    renskeui::Assistant assistant;

    sbaudio::Knob outKnob { "OUTPUT" }, mixKnob { "MIX" };
    juce::TextButton deltaBtn;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAtt;
    juce::TooltipWindow tooltips { this, 700 };

    std::array<float, renske::Shared::numModules> shownActivity {};
    juce::Rectangle<int> chainPanel, rightPanel;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenskeEditor)
};
