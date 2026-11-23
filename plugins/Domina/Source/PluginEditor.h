#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <sbaudio_common/sbaudio_common.h>
#include "PluginProcessor.h"

namespace dominaui
{
    /** Row of buttons bound to a parameter; each button sets one value (style, loudness target …). */
    class SegmentSelector : public juce::Component
    {
    public:
        SegmentSelector (juce::RangedAudioParameter&, const juce::StringArray& names, std::vector<float> values,
                         const juce::StringArray& tooltips);
        void resized() override;

    private:
        void setCurrent (float);
        juce::OwnedArray<juce::TextButton> buttons;
        std::vector<float> values;
        juce::ParameterAttachment attachment;
    };

    /** Big ANALYSER button with progress fill. */
    class AnalyseButton : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void update (int state, float progress, bool analysed, bool waiting);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        std::function<void()> onClick;

    private:
        int state = domina::Idle;
        float progress = 0.0f;
        bool analysed = false, waiting = false;
        int pulse = 0;
    };

    /** Input/output spectrum with the EQ curve on top. */
    class Spectrum : public juce::Component
    {
    public:
        explicit Spectrum (DominaProcessor&);
        void update();
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Rectangle<float> plot() const;
        float freqToX (float f) const;
        void analyse (const sbaudio::AnalyzerRing&, std::vector<float>& smoothed);

        DominaProcessor& proc;
        static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
        juce::dsp::FFT fft { fftOrder };
        juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
        std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
        std::vector<float> freqs, specIn, specOut, eqTotal, eqAuto;
        int lastSerial = -1;
    };

    struct MeterValues
    {
        float shortTerm = -100.0f, momentary = -100.0f, integrated = -100.0f, truePeak = -100.0f;
        float target = -11.0f, drive = 0.0f, limiterGr = 0.0f, deessGr = 0.0f;
        std::array<float, domina::numCompBands> compGr {};
        bool analysed = false;
    };

    /** Loudness readout: short-term LUFS, target bar, integrated, true peak, gain and gain-reduction bars. */
    class LoudnessPanel : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void setValues (const MeterValues&);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override { if (onReset) onReset(); }
        std::function<void()> onReset;
        static constexpr int buttonArea = 40;   // bottom strip left for the compare button

    private:
        MeterValues v;
        float limHold = 0.0f;
        int limHoldFrames = 0;
    };
}

class DominaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DominaEditor (DominaProcessor&);
    ~DominaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1000, designH = 620;

private:
    struct Content : public juce::Component
    {
        explicit Content (DominaEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        DominaEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layoutContent();
    juce::String statusLine (int line) const;

    DominaProcessor& proc;
    sbaudio::LookAndFeel lnf;
    Content content { *this };

    sbaudio::PresetBar presetBar;
    dominaui::AnalyseButton analyseButton;
    dominaui::SegmentSelector styleSelector, loudnessSelector;
    dominaui::Spectrum spectrum;
    dominaui::LoudnessPanel loudnessPanel;
    juce::TextButton compareButton { "SAMMENLIGN  (A/B)" }, monoBassButton { "MONO BASS" };

    sbaudio::Knob low { "LAV" }, mid { "MID" }, presence { "PRESENCE" }, high { juce::String (juce::CharPointer_UTF8 ("H\xc3\x98Y")) },
                  autoEq { "AUTO-EQ" }, compression { "KOMPRESJON" }, deess { "DE-ESS" }, saturation { "METNING" },
                  width { "BREDDE" }, loudness { "LOUDNESS" }, ceiling { "TAK" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Image background;   // painting behind the whole UI (Assets/background.jpg)

    juce::Rectangle<int> toneRect, dynRect, outRect, statusRect;
    int shownState = -1;
    bool shownAnalysed = false, shownWaiting = false;
    float shownInLufs = 0.0f;
    float limHeld = 0.0f;
    std::array<float, domina::numCompBands> compHeld {};
    float deessHeld = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DominaEditor)
};
