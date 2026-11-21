#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace fossui
{
    /** Spectrum (in/out), the applied gain curve, the static (learned + tone) curve and
        draggable low/high limits of the frequency range. */
    class Display : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (FossProcessor&);
        void update();

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

    private:
        juce::Rectangle<float> plot() const;
        float freqToX (float f) const;
        float xToFreq (float x) const;
        float gainToY (float db) const;
        float specToY (float db) const;
        int hitHandle (float x) const;
        juce::RangedAudioParameter* handleParam (int h) const;
        juce::Path curvePath (const std::array<float, foss::numBands>&, bool gain) const;

        FossProcessor& proc;
        std::array<float, foss::numBands> inDb {}, outDb {}, gainDb {}, staticDb {};
        float lowHz = 20.0f, highHz = 20000.0f;
        bool learning = false, hasLearned = false, delta = false;
        float learnProgress = 0.0f;
        int hover = -1, dragging = -1, lastSerial = -1, framesWithoutAudio = 0;
    };
}

class FossEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FossEditor (FossProcessor&);
    ~FossEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 980, designH = 540;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (FossEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        FossEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void updateLearnUi();

    FossProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    fossui::Display display;

    juce::ComboBox profileBox;
    juce::TextButton learnBtn { "LEARN" }, clearBtn { "NULLSTILL" };
    juce::Label learnStatus;
    sbaudio::Knob correction { "CORRECTION" };

    sbaudio::Knob recover { "RECOVER" }, tame { "TAME" }, bias { "BIAS" };
    sbaudio::Knob brighten { "BRIGHTEN" }, bass { "BASS" }, mix { "MIX" }, output { "OUTPUT" };
    juce::TextButton autoGainBtn { "AUTO GAIN" }, deltaBtn { juce::String (juce::CharPointer_UTF8 ("DELTA \xce\x94")) };
    juce::Label autoGainReadout;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAtt;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> learnPanel, adaptPanel, tonePanel;
    bool lastLearning = false, lastHasLearned = false;
    int lastProgressPct = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FossEditor)
};
