#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace emberui
{
    juce::Colour bandColour (int band);

    /** Large display: spectrum, band regions, draggable crossovers and drive handles. */
    class BandDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit BandDisplay (EmberProcessor&);

        void update();   // ~30 fps from the editor timer
        int getSelected() const { return selected; }
        void setSelected (int);
        std::function<void (int)> onSelectionChanged;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    private:
        juce::Rectangle<float> plot() const;
        float freqToX (float f) const;
        float xToFreq (float x) const;
        float driveToY (float drive01) const;
        float specToY (float db) const;
        int bandAt (float x) const;
        int splitAt (float x) const;
        float bandLo (int b) const;
        float bandHi (int b) const;
        void showBandMenu (int band);
        void computeSpectrum();

        EmberProcessor& proc;
        int numBands = 1, selected = 0, hoverBand = -1, hoverSplit = -1;
        std::array<float, ember::maxSplits> splits {};
        struct View { int style; float drive; bool on, mute; };
        std::array<View, ember::maxBands> view {};

        // interaction
        enum class Drag { none, split, drive } drag = Drag::none;
        int dragIndex = -1;
        float dragStartDrive = 0.0f;
        juce::RangedAudioParameter* dragParam = nullptr;

        // analyzer
        static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
        juce::dsp::FFT fft { fftOrder };
        juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
        std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
        static constexpr int numPoints = 220;
        std::vector<float> pointFreq, specPre, specPost;
    };

    /** Controls for the selected band. */
    class BandPanel : public juce::Component
    {
    public:
        explicit BandPanel (EmberProcessor&);
        void setBand (int band);
        void refresh();
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        EmberProcessor& proc;
        int band = -1;

        juce::TextButton onBtn { juce::CharPointer_UTF8 ("PÅ") }, muteBtn { "MUTE" }, soloBtn { "SOLO" }, deleteBtn { "SLETT" };
        juce::ComboBox styleBox;
        sbaudio::Knob drive { "DRIVE" }, dyn { "DYNAMICS" }, tone { "TONE" }, mix { "MIX" }, level { "LEVEL" },
                      fb { "FEEDBACK" }, fbFreq { "FB FREKVENS" };
        juce::String rangeText;

        using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
        using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
        std::vector<std::unique_ptr<SA>> sliderAtts;
        std::unique_ptr<CA> styleAtt;
        std::vector<std::unique_ptr<BA>> buttonAtts;
    };

    class LevelMeter : public juce::Component
    {
    public:
        void setLevels (float in, float l, float r);
        void paint (juce::Graphics&) override;
    private:
        float levIn = -100.0f, levL = -100.0f, levR = -100.0f, peakHold = -100.0f;
        int holdFrames = 0;
    };
}

class EmberEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EmberEditor (EmberProcessor&);
    ~EmberEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1060, designH = 640;

private:
    /** Everything is laid out at design size inside this component and scaled as a whole. */
    struct Content : public juce::Component
    {
        explicit Content (EmberEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        EmberEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void selectBand (int);

    EmberProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    juce::ComboBox qualityBox;
    juce::TextButton autoGainBtn { "AUTO GAIN" };
    emberui::BandDisplay display;
    emberui::BandPanel bandPanel;
    sbaudio::Knob inKnob { "INPUT" }, outKnob { "OUTPUT" }, mixKnob { "MIX" };
    emberui::LevelMeter meter;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Rectangle<int> globalArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmberEditor)
};
