#include "PluginEditor.h"

using namespace juce;
using namespace prism;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

//==============================================================================
Knob::Knob (const String& caption)
{
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
    slider.setTextBoxIsEditable (true);
    slider.setDoubleClickReturnValue (false, 0.0);
    addAndMakeVisible (slider);
    label.setText (caption, dontSendNotification);
    label.setFont (uiFont (10.0f, true));
    label.setColour (Label::textColourId, col::textDim);
    label.setJustificationType (Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (76, r.getWidth()), 18);
    slider.setBounds (r);
}

//==============================================================================
BandPanel::BandPanel (PrismEQProcessor& p) : proc (p)
{
    typeBox.addItemList (typeNames, 1);
    slopeBox.addItemList (slopeNames, 1);
    placeBox.addItemList (placementNames, 1);
    for (auto* c : { &typeBox, &slopeBox, &placeBox })
    {
        c->onChange = [this] { if (onEdit) onEdit(); };
        addChildComponent (c);
    }
    typeBox.setTooltip ("Filtertype");
    slopeBox.setTooltip (CharPointer_UTF8 ("Helning for Low Cut / High Cut"));
    placeBox.setTooltip (CharPointer_UTF8 ("Hvor i stereobildet båndet virker: begge kanaler, venstre, høyre, midt (mono-innhold) eller side"));

    for (auto* b : { &onBtn, &dynBtn, &scBtn })
    {
        b->setClickingTogglesState (true);
        addChildComponent (b);
    }
    onBtn.setColour (TextButton::buttonOnColourId, col::accent);
    dynBtn.setColour (TextButton::buttonOnColourId, col::amber);
    scBtn.setColour (TextButton::buttonOnColourId, col::violet);
    onBtn.setTooltip (CharPointer_UTF8 ("Slå båndet av/på (dobbeltklikk på noden gjør det samme)"));
    dynBtn.setTooltip (CharPointer_UTF8 ("Dynamisk EQ: båndet endrer gain bare når nivået i båndet går over terskelen"));
    scBtn.setTooltip (CharPointer_UTF8 ("Bruk ekstern sidechain (velg kilde i Side Chain-menyen øverst i Logic-vinduet)"));

    soloBtn.setColour (TextButton::buttonOnColourId, col::amber);
    soloBtn.setTooltip (CharPointer_UTF8 ("Hold inne for å lytte bare på området båndet jobber med"));
    soloBtn.onStateChange = [this]
    {
        const bool down = soloBtn.isDown();
        soloBtn.setToggleState (down, dontSendNotification);
        proc.soloBand = down ? band : -1;
    };
    addChildComponent (soloBtn);

    deleteBtn.setColour (TextButton::buttonOnColourId, col::red);
    deleteBtn.setTooltip (CharPointer_UTF8 ("Slett båndet (eller Alt-klikk på noden / Delete-tasten)"));
    deleteBtn.onClick = [this] { if (band >= 0) proc.deleteBand (band); };
    addChildComponent (deleteBtn);

    for (auto* k : { &freq, &gain, &q, &range, &thr, &att, &rel })
        addChildComponent (k);
    gain.slider.getProperties().set ("bipolar", true);
    range.slider.getProperties().set ("bipolar", true);
    range.slider.setColour (Slider::rotarySliderFillColourId, col::amber);
    for (auto* k : { &thr, &att, &rel })
        k->slider.setColour (Slider::rotarySliderFillColourId, col::amber);
    range.slider.setTooltip (CharPointer_UTF8 ("Hvor mye gain kan endres dynamisk (negativ = demp ved høyt nivå, positiv = løft)"));
    thr.slider.setTooltip (CharPointer_UTF8 ("Nivået i båndet der dynamikken begynner å virke"));
}

void BandPanel::setBand (int b)
{
    if (b == band)
        return;
    if (proc.soloBand.load() == band)
        proc.soloBand = -1;

    sliderAtts.clear();
    comboAtts.clear();
    buttonAtts.clear();
    band = b;

    const bool vis = band >= 0;
    for (auto* c : getChildren())
        c->setVisible (vis);

    if (vis)
    {
        auto& s = proc.apvts;
        auto id = [this] (const char* n) { return ParamIDs::band (band, n); };
        sliderAtts.push_back (std::make_unique<SA> (s, id ("freq"), freq.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("gain"), gain.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("q"), q.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("range"), range.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("thr"), thr.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("att"), att.slider));
        sliderAtts.push_back (std::make_unique<SA> (s, id ("rel"), rel.slider));
        comboAtts.push_back (std::make_unique<CA> (s, id ("type"), typeBox));
        comboAtts.push_back (std::make_unique<CA> (s, id ("slope"), slopeBox));
        comboAtts.push_back (std::make_unique<CA> (s, id ("place"), placeBox));
        buttonAtts.push_back (std::make_unique<BA> (s, id ("on"), onBtn));
        buttonAtts.push_back (std::make_unique<BA> (s, id ("dyn"), dynBtn));
        buttonAtts.push_back (std::make_unique<BA> (s, id ("sc"), scBtn));

        const auto c = prismui::bandColour (band);
        for (auto* k : { &freq, &gain, &q })
            k->slider.setColour (Slider::rotarySliderFillColourId, c);
        refresh();
    }
    repaint();
}

void BandPanel::refresh()
{
    if (band < 0)
        return;
    const auto s = proc.getBandState (band);
    gain.setEnabled (typeHasGain (s.type));
    slopeBox.setEnabled (typeHasSlope (s.type));
    const bool canDyn = typeCanDynamic (s.type);
    dynBtn.setEnabled (canDyn);
    const bool dynOn = canDyn && s.dyn;
    for (auto* c : std::initializer_list<Component*> { &scBtn, &range, &thr, &att, &rel })
        c->setEnabled (dynOn);
    placeBox.setEnabled (proc.getTotalNumInputChannels() > 1 || proc.getBus (true, 0) == nullptr
                         || proc.getBus (true, 0)->getNumberOfChannels() > 1);
}

void BandPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    if (band < 0)
    {
        g.setColour (col::textDim);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (CharPointer_UTF8 ("Klikk på en node for å redigere båndet.  Dobbeltklikk i grafen lager et nytt bånd.\n"
                                            "Dra = frekvens/gain · Shift = finjuster · scrollhjul = Q · Alt-klikk = slett · høyreklikk = meny"),
                          getLocalBounds().reduced (20), Justification::centred, 3);
        return;
    }

    // band badge
    const auto c = prismui::bandColour (band);
    auto badge = Rectangle<float> (12.0f, 12.0f, 26.0f, 26.0f);
    g.setColour (c);
    g.fillEllipse (badge);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.setFont (uiFont (12.0f, true));
    g.drawText (String (band + 1), badge, Justification::centred);
    g.setColour (col::text);
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (CharPointer_UTF8 ("BÅND ")) + String (band + 1), Rectangle<float> (44.0f, 12.0f, 90.0f, 26.0f), Justification::centredLeft);

    // dynamic section separator
    const float sx = (float) dynBtn.getX() - 9.0f;
    g.setColour (col::panelEdge);
    g.fillRect (sx, 12.0f, 1.0f, r.getHeight() - 24.0f);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 10);

    auto left = r.removeFromLeft (124);
    left.removeFromTop (34);
    onBtn.setBounds (left.removeFromTop (26).reduced (2, 1));
    auto row = left.removeFromTop (28);
    soloBtn.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
    deleteBtn.setBounds (row.reduced (2, 2));

    r.removeFromLeft (8);
    auto combos = r.removeFromLeft (128);
    const int ch = jmin (28, combos.getHeight() / 3);
    combos = combos.withSizeKeepingCentre (combos.getWidth(), ch * 3);
    typeBox.setBounds (combos.removeFromTop (ch).reduced (0, 2));
    slopeBox.setBounds (combos.removeFromTop (ch).reduced (0, 2));
    placeBox.setBounds (combos.removeFromTop (ch).reduced (0, 2));

    r.removeFromLeft (8);
    const int dynW = 128;
    const int kw = jmax (56, (r.getWidth() - dynW - 26) / 7);
    for (auto* k : { &freq, &gain, &q })
        k->setBounds (r.removeFromLeft (kw));

    r.removeFromLeft (18);
    auto dyn = r.removeFromLeft (dynW);
    dyn = dyn.withSizeKeepingCentre (dyn.getWidth(), 60);
    dynBtn.setBounds (dyn.removeFromTop (30).reduced (0, 2));
    scBtn.setBounds (dyn.removeFromTop (30).reduced (0, 2));

    r.removeFromLeft (8);
    for (auto* k : { &range, &thr, &att, &rel })
        k->setBounds (r.removeFromLeft (kw));
}

//==============================================================================
void OutputMeter::setLevels (float l, float r)
{
    const float dl = Decibels::gainToDecibels (l, -100.0f), dr = Decibels::gainToDecibels (r, -100.0f);
    levL = jmax (dl, levL - 1.2f);
    levR = jmax (dr, levR - 1.2f);
    const float pk = jmax (dl, dr);
    if (pk >= peakHold) { peakHold = pk; holdFrames = 60; }
    else if (--holdFrames <= 0) peakHold = jmax (pk, peakHold - 0.5f);
    repaint();
}

void OutputMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromTop (16.0f);
    g.setFont (uiFont (10.5f, true));
    g.setColour (peakHold > 0.0f ? col::red : col::text);
    g.drawText (peakHold <= -99.0f ? String ("-inf") : String (peakHold, 1), text, Justification::centred);

    auto bar = [&] (Rectangle<float> b, float db)
    {
        g.setColour (col::well);
        g.fillRoundedRectangle (b, 2.0f);
        const float t = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = b.withTop (b.getBottom() - b.getHeight() * t);
        g.setGradientFill (ColourGradient (col::red, 0.0f, b.getY(), col::accent, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    };
    r.removeFromTop (2.0f);
    const float w = (r.getWidth() - 3.0f) * 0.5f;
    bar (r.removeFromLeft (w), levL);
    r.removeFromLeft (3.0f);
    bar (r, levR);
}

//==============================================================================
PrismEQEditor::PrismEQEditor (PrismEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p), bandPanel (p), presetBar (*p.presets)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (graph);
    addAndMakeVisible (bandPanel);
    graph.onSelectionChanged = [this] (int b) { bandPanel.setBand (b); };
    graph.onEdit = [this] { userTouched = true; };
    bandPanel.onEdit = [this] { userTouched = true; };

    // ---- presets ----
    addAndMakeVisible (presetBar);
    presetBar.onPresetLoaded = [this] { userTouched = true; };

    // ---- A/B, undo ----
    for (auto* b : { &aBtn, &bBtn, &copyBtn, &undoBtn, &redoBtn })
        addAndMakeVisible (b);
    aBtn.setColour (TextButton::buttonOnColourId, col::accent);
    bBtn.setColour (TextButton::buttonOnColourId, col::accent);
    aBtn.onClick = [this] { proc.selectAB (0); };
    bBtn.onClick = [this] { proc.selectAB (1); };
    copyBtn.onClick = [this] { proc.copyCurrentToOtherAB(); };
    aBtn.setTooltip (CharPointer_UTF8 ("Sammenlign to innstillinger: A og B"));
    bBtn.setTooltip (CharPointer_UTF8 ("Sammenlign to innstillinger: A og B"));
    copyBtn.setTooltip (CharPointer_UTF8 ("Kopier gjeldende innstilling til den andre (A↔B)"));
    undoBtn.onClick = [this] { proc.syncUndo (true); proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    undoBtn.setTooltip ("Angre (Cmd+Z)");
    redoBtn.setTooltip (CharPointer_UTF8 ("Gjør om (Cmd+Shift+Z)"));

    // ---- mode, character, display ----
    modeBox.addItemList ({ "Zero Latency", "Linear Phase" }, 1);
    characterBox.addItemList ({ "Clean", "Subtle", "Warm" }, 1);
    analyzerBox.addItemList ({ "Analyse: av", "Analyse: post", "Analyse: pre+post" }, 1);
    analyzerBox.setTooltip ("Spektrumanalysator: av, bare etter EQ, eller før og etter EQ");
    for (auto* t : { "±3 dB", "±6 dB", "±12 dB", "±30 dB" })
        rangeBox.addItem (String (CharPointer_UTF8 (t)), rangeBox.getNumItems() + 1);
    for (auto* c : { &modeBox, &characterBox, &analyzerBox, &rangeBox })
        addAndMakeVisible (c);
    modeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::mode, modeBox);
    charAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::character, characterBox);
    modeBox.onChange = [this] { userTouched = true; };
    characterBox.onChange = [this] { userTouched = true; };
    modeBox.setTooltip (CharPointer_UTF8 ("Zero Latency: ingen forsinkelse (bruk ved innspilling).\n"
                                          "Linear Phase: ingen faseforskyvning, men ca. 85 ms forsinkelse som Logic kompenserer."));
    characterBox.setTooltip (CharPointer_UTF8 ("Clean: helt ren.  Subtle / Warm: litt analog metning og varme."));

    const int am = graph.getAnalyzerMode();
    analyzerBox.setSelectedId (am + 1, dontSendNotification);
    analyzerBox.onChange = [this] { setStateProperty ("analyzer", analyzerBox.getSelectedId() - 1); };
    const float dr = graph.getDisplayRange();
    rangeBox.setSelectedId (dr == 3.0f ? 1 : dr == 6.0f ? 2 : dr == 30.0f ? 4 : 3, dontSendNotification);
    rangeBox.onChange = [this]
    {
        const float v[] = { 3.0f, 6.0f, 12.0f, 30.0f };
        setStateProperty ("displayRange", v[jlimit (0, 3, rangeBox.getSelectedId() - 1)]);
    };
    rangeBox.setTooltip ("Skala i grafen");

    // ---- output strip ----
    for (auto* c : std::initializer_list<Component*> { &outKnob, &scaleKnob, &autoGainBtn, &invertBtn, &meter })
        addAndMakeVisible (c);
    outKnob.slider.getProperties().set ("bipolar", true);
    outAtt   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::output, outKnob.slider);
    scaleAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::gainScale, scaleKnob.slider);
    autoGainBtn.setClickingTogglesState (true);
    invertBtn.setClickingTogglesState (true);
    autoGainBtn.setColour (TextButton::buttonOnColourId, col::accent);
    invertBtn.setColour (TextButton::buttonOnColourId, col::amber);
    autoAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::autoGain, autoGainBtn);
    invAtt  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::invert, invertBtn);
    autoGainBtn.setTooltip (CharPointer_UTF8 ("Holder opplevd nivå likt når du EQ-er, så du hører klangen og ikke bare volumet"));
    scaleKnob.slider.setTooltip (CharPointer_UTF8 ("Skalerer gain på alle bånd: 50 % = halv effekt, 200 % = dobbel"));
    invertBtn.setTooltip ("Snu polaritet");

    presetBar.attachOverlayTo (*this);

    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (900, 560, 2000, 1300);
    setSize (1120, 680);

    proc.syncUndo (false);
    startTimerHz (30);
}

PrismEQEditor::~PrismEQEditor()
{
    proc.soloBand = -1;
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void PrismEQEditor::setStateProperty (const char* id, const var& v)
{
    proc.apvts.state.setProperty (Identifier (id), v, nullptr);
}

void PrismEQEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);

    auto top = getLocalBounds().removeFromTop (44).toFloat();
    g.setColour (col::panel);
    g.fillRect (top);
    g.setColour (col::panelEdge);
    g.fillRect (top.removeFromBottom (1.0f));

    g.setFont (uiFont (18.0f, true));
    g.setColour (col::text);
    g.drawText ("Prism", Rectangle<float> (14.0f, 0.0f, 60.0f, 44.0f), Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("EQ", Rectangle<float> (64.0f, 0.0f, 40.0f, 44.0f), Justification::centredLeft);
    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText ("SB AUDIO", Rectangle<float> (96.0f, 0.0f, 60.0f, 44.0f), Justification::centredLeft);

    // output strip background
    auto strip = getLocalBounds().removeFromBottom (150).removeFromRight (240).reduced (8, 8).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (strip, 6.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (strip.reduced (0.5f), 6.0f, 1.0f);
}

void PrismEQEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44).reduced (8, 8);
    top.removeFromLeft (150);

    presetBar.setBounds (top.removeFromLeft (26 + 3 + jlimit (180, 300, getWidth() / 4) + 3 + 26 + 6 + 62));
    top.removeFromLeft (16);
    aBtn.setBounds (top.removeFromLeft (28));
    bBtn.setBounds (top.removeFromLeft (28));
    top.removeFromLeft (3);
    copyBtn.setBounds (top.removeFromLeft (44));
    top.removeFromLeft (16);
    undoBtn.setBounds (top.removeFromLeft (58));
    top.removeFromLeft (3);
    redoBtn.setBounds (top.removeFromLeft (68));

    modeBox.setBounds (top.removeFromRight (122));
    top.removeFromRight (6);
    characterBox.setBounds (top.removeFromRight (84));

    auto bottom = r.removeFromBottom (150);
    auto strip = bottom.removeFromRight (240).reduced (8, 8).reduced (8, 6);
    meter.setBounds (strip.removeFromRight (26));
    strip.removeFromRight (8);
    auto knobs = strip.removeFromTop (strip.getHeight() - 30);
    outKnob.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
    scaleKnob.setBounds (knobs);
    strip.removeFromTop (4);
    autoGainBtn.setBounds (strip.removeFromLeft (strip.getWidth() * 3 / 5).reduced (1, 0));
    invertBtn.setBounds (strip.reduced (1, 0));

    bandPanel.setBounds (bottom.reduced (8, 8).withTrimmedRight (0));
    graph.setBounds (r.reduced (8, 6).withTrimmedTop (2));
    // display options float in the top-left corner of the graph
    auto opts = graph.getBounds().reduced (8, 8).removeFromTop (22);
    analyzerBox.setBounds (opts.removeFromLeft (150));
    opts.removeFromLeft (6);
    rangeBox.setBounds (opts.removeFromLeft (80));
    presetBar.getOverlay().setBounds (getLocalBounds());
}

void PrismEQEditor::mouseDown (const MouseEvent&)
{
    userTouched = true;
}

bool PrismEQEditor::keyPressed (const KeyPress& k)
{
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (k.getModifiers().isShiftDown()) proc.redo();
        else { proc.syncUndo (true); proc.undo(); }
        return true;
    }
    return false;
}

void PrismEQEditor::timerCallback()
{
    graph.update();
    bandPanel.refresh();
    meter.setLevels (proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f));

    if (! ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        proc.syncUndo (userTouched);
        userTouched = false;
    }
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    aBtn.setToggleState (proc.getABSlot() == 0, dontSendNotification);
    bBtn.setToggleState (proc.getABSlot() == 1, dontSendNotification);
    copyBtn.setButtonText (proc.getABSlot() == 0 ? CharPointer_UTF8 ("A→B") : CharPointer_UTF8 ("B→A"));

    const bool linear = proc.apvts.getRawParameterValue (ParamIDs::mode)->load() > 0.5f;
    modeBox.setColour (ComboBox::textColourId, linear ? col::amber : col::text);
}
