#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace nordlysui
{
    /** X/Y macro pad drawn as a glowing sphere. Shows the set point and the live (orbiting) position. */
    class OrbPad : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        OrbPad (juce::RangedAudioParameter& x, juce::RangedAudioParameter& y);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void setLive (float lx, float ly);

    private:
        juce::Rectangle<float> area() const;
        void setFromMouse (juce::Point<float>);
        float px = 0.0f, py = 0.0f, liveX = 0.0f, liveY = 0.0f;
        std::array<juce::Point<float>, 24> trail {};
        int trailPos = 0;
        juce::ParameterAttachment ax, ay;
    };

    /** Draws one cycle of the current layer oscillator. */
    class WaveDisplay : public juce::Component
    {
    public:
        struct State { int osc = 0, wave = 0; float shape = 0.0f, sym = 0.5f, spray = 0.0f; bool on = true; int layer = 0;
                       bool operator!= (const State& o) const
                       {
                           using juce::exactlyEqual;
                           return osc != o.osc || wave != o.wave || ! exactlyEqual (shape, o.shape) || ! exactlyEqual (sym, o.sym)
                                  || ! exactlyEqual (spray, o.spray) || on != o.on || layer != o.layer;
                       } };
        void setState (const State&);
        void paint (juce::Graphics&) override;

    private:
        State st;
        bool valid = false;
    };

    /** Plain component whose painting is supplied by the editor. */
    struct Page : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    struct ModRow : public juce::Component
    {
        ModRow (NordlysProcessor&, int slot);
        void resized() override;
        void paint (juce::Graphics&) override;

        int slot;
        juce::ComboBox src, via, dst, layer;
        juce::Slider amt { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcA, viaA, dstA, layerA;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtA;
    };
}

class NordlysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NordlysEditor (NordlysProcessor&);
    ~NordlysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1200, designH = 760;

private:
    struct Content : public juce::Component
    {
        explicit Content (NordlysEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        NordlysEditor& owner;
    };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void showPage (int);
    void attachLayer (int);
    void updateLayerLabels();

    void setupKnob (sbaudio::Knob&, juce::Component& parent, const juce::String& tip = {}, bool bipolar = false);
    void setupCombo (juce::ComboBox&, juce::Component& parent, const juce::StringArray& items);
    void attachKnob (sbaudio::Knob&, const juce::String& id, std::vector<std::unique_ptr<SliderAtt>>&);

    NordlysProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };
    sbaudio::PresetBar presetBar;

    // left column
    nordlysui::OrbPad orb;
    sbaudio::Knob orbRate { "FART" }, orbDepth { "RADIUS" }, master { "MASTER" };
    std::array<juce::TextButton, nordlys::numLayers> layerOn;
    std::array<juce::Slider, nordlys::numLayers> layerLevel;
    std::array<juce::Label, nordlys::numLayers> layerInfo;

    // page tabs
    juce::OwnedArray<juce::TextButton> tabs;
    nordlysui::Page layerPage, modPage, fxPage;
    int page = 0, currentLayer = 0;

    // layer page
    nordlysui::WaveDisplay wave;
    juce::ComboBox oscType, waveBank, filterType, lfo1Shape, lfo2Shape;
    sbaudio::Knob shape { "FORM" }, sym { "SYMMETRI" }, uni { "UNISON" }, detune { "DETUNE" }, spread { "BREDDE" },
                  gsize { juce::String (juce::CharPointer_UTF8 ("STØRRELSE")) }, gdens { "TETTHET" }, gspray { "SPREDNING" }, gpitch { "PITCH" }, gshim { "SHIMMER" },
                  oct { "OKTAV" }, coarse { "HALVTONE" }, fine { "FIN" }, level { juce::String (juce::CharPointer_UTF8 ("NIVÅ")) }, pan { "PANORERING" },
                  cutoff { "CUTOFF" }, reso { "RESONANS" }, fenvAmt { "ENV" }, fkey { "KEY" }, drive { "DRIVE" },
                  fA { "A" }, fD { "D" }, fS { "S" }, fR { "R" }, aA { "A" }, aD { "D" }, aS { "S" }, aR { "R" },
                  l1rate { "RATE" }, l1fade { "FADE" }, l2rate { "RATE" }, l2fade { "FADE" };
    std::vector<std::unique_ptr<SliderAtt>> layerSliderAtts;
    std::vector<std::unique_ptr<ComboAtt>> layerComboAtts;
    int shownOsc = -1;

    // mod page
    juce::OwnedArray<nordlysui::ModRow> modRows;

    // arp / fx page
    juce::TextButton arpOn { juce::String (juce::CharPointer_UTF8 ("ARP PÅ")) };
    juce::ComboBox arpMode, arpRate, dlyTime, revType, polyMode;
    sbaudio::Knob arpOct { "OKTAVER" }, arpGate { "GATE" },
                  choMix { "MIX" }, choRate { "RATE" }, choDepth { "DYBDE" },
                  dlyMix { "MIX" }, dlyFb { "FEEDBACK" }, dlyTone { "TONE" },
                  revMix { "MIX" }, revDecay { "DECAY" }, revTone { "TONE" },
                  glide { "GLIDE" }, bend { "BEND" }, velSens { "VELOCITY" };

    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;
    juce::TooltipWindow tooltips { this, 700 };

    float peak = 0.0f;
    int voicesShown = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NordlysEditor)
};
