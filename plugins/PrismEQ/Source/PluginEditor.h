#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"
#include "EqGraph.h"

//==============================================================================
using Knob = sbaudio::Knob;

//==============================================================================
/** Controls for the selected band. */
class BandPanel : public juce::Component
{
public:
    explicit BandPanel (PrismEQProcessor&);
    void setBand (int band);
    int getBand() const { return band; }
    std::function<void()> onEdit;
    void refresh();                       // enable/disable controls for the current type
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PrismEQProcessor& proc;
    int band = -1;

    juce::TextButton onBtn { juce::CharPointer_UTF8 ("PÅ") }, soloBtn { "SOLO" }, deleteBtn { "SLETT" };
    juce::ComboBox typeBox, slopeBox, placeBox;
    Knob freq { "FREKVENS" }, gain { "GAIN" }, q { "Q" };
    juce::TextButton dynBtn { "DYNAMISK" }, scBtn { "EKST. SIDECHAIN" };
    Knob range { juce::CharPointer_UTF8 ("OMRÅDE") }, thr { "TERSKEL" }, att { "ATTACK" }, rel { "RELEASE" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtts;
    std::vector<std::unique_ptr<CA>> comboAtts;
    std::vector<std::unique_ptr<BA>> buttonAtts;
};

//==============================================================================
class OutputMeter : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;
private:
    float levL = -100.0f, levR = -100.0f, peakHold = -100.0f;
    int holdFrames = 0;
};

//==============================================================================
class PrismEQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismEQEditor (PrismEQProcessor&);
    ~PrismEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void setStateProperty (const char* id, const juce::var& v);

    PrismEQProcessor& proc;
    sbaudio::LookAndFeel lnf;

    EqGraph graph;
    BandPanel bandPanel;

    // top bar
    sbaudio::PresetBar presetBar;
    juce::TextButton aBtn { "A" }, bBtn { "B" }, copyBtn { juce::CharPointer_UTF8 ("A→B") };
    juce::TextButton undoBtn { "ANGRE" }, redoBtn { juce::CharPointer_UTF8 ("GJØR OM") };
    juce::ComboBox modeBox, characterBox, analyzerBox, rangeBox;

    // output strip
    Knob outKnob { "OUTPUT" }, scaleKnob { "GAIN SCALE" };
    juce::TextButton autoGainBtn { "AUTO GAIN" }, invertBtn { juce::CharPointer_UTF8 ("Ø  FASE") };
    OutputMeter meter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt, invAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, charAtt;

    juce::TooltipWindow tooltips { this, 700 };

    bool userTouched = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismEQEditor)
};
