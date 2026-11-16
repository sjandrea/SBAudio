#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

namespace prismui
{
    juce::Colour bandColour (int band);
}

/** The large interactive EQ display: grid, spectrum analyzer, curves and band nodes. */
class EqGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EqGraph (PrismEQProcessor&);

    /** Called by the editor timer (~30 fps). */
    void update();

    int getSelectedBand() const { return selected; }
    void setSelectedBand (int);
    std::function<void (int)> onSelectionChanged;
    std::function<void()> onEdit; // a GUI edit that does not start with a mouse click in the editor

    float getDisplayRange() const;
    int getAnalyzerMode() const; // 0 off, 1 post, 2 pre + post

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Rectangle<float> plot() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float specDbToY (float db) const;

    juce::Point<float> nodePos (int band) const;
    juce::Point<float> dynHandlePos (int band) const;
    int hitNode (juce::Point<float>) const;
    int hitDynHandle (juce::Point<float>) const;

    void computeCurves();
    void computeSpectrum();
    void findPeak (juce::Point<float> mouse);
    void showBandMenu (int band);
    void createBandAt (juce::Point<float>, bool fromPeak);
    void beginGesture (int band, std::initializer_list<const char*> names);
    void endGesture();

    PrismEQProcessor& proc;
    std::array<prism::BandState, prism::numBands> bands {};
    float gainScale = 1.0f;

    // curves, one value per display point
    int numPoints = 0;
    std::vector<float> pointFreq;
    std::vector<std::vector<float>> bandDb;     // static per band
    std::vector<float> totalDb, dynRangeDb;     // live total; selected band at gain+range
    bool selectedHasDyn = false;

    // analyzer
    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> specPre, specPost, specPreDisp, specPostDisp;

    // interaction
    int selected = -1, hover = -1, hoverDyn = -1;
    enum class Drag { none, node, dyn } drag = Drag::none;
    int dragBand = -1;
    juce::Point<float> dragStart;
    float startFreq = 0, startGain = 0, startRange = 0;
    std::vector<juce::RangedAudioParameter*> gestureParams;

    bool peakValid = false;
    float peakFreq = 0, peakDb = 0;
    juce::Point<float> mousePos;
    bool mouseInside = false;
};
