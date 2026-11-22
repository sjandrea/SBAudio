#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour optoCol  { 0xffffb35c };   // warm "tube glow" amber
    const Colour optoSoft { 0xffffd9a8 };

    const char* modeTooltips[] = {
        "Programavhengig optisk kompresjon (LA-2A-stil). Attack og release styres automatisk av signalet.",
        "Attack og release som du stiller dem (CL 1B-stil).",
        "Opto og manuell i parallell – den som demper mest bestemmer. Fanger raske topper og rir nivået."
    };

    float gainReductionFor (float inDb, float thr, float ratio, int mode)
    {
        const float slope = 1.0f - 1.0f / jmax (1.0f, ratio);
        const float over = inDb - thr;
        const float opto = optotube::CompressorEngine::staticCurve (over, slope, 10.0f);
        const float man  = optotube::CompressorEngine::staticCurve (over, slope, 6.0f);
        return mode == optotube::Opto ? opto : mode == optotube::Manual ? man : jmax (opto, man);
    }

    void drawWell (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    String dbString (float db) { return db <= -99.0f ? String ("-inf") : String (db, 1); }
}

namespace optoui
{
    //==========================================================================
    ModeSelector::ModeSelector (RangedAudioParameter& p)
        : attachment (p, [this] (float v) { setCurrent (roundToInt (v)); }, nullptr)
    {
        const char* names[] = { "OPTO", "MANUAL", "OPTO + MANUAL" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = buttons.add (new TextButton (names[i]));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, optoCol);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->setTooltip (String (CharPointer_UTF8 (modeTooltips[i])));
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void ModeSelector::setCurrent (int m)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == m, dontSendNotification);
    }

    void ModeSelector::resized()
    {
        const int gap = 6, n = buttons.size();
        const int w = (getWidth() - gap * (n - 1)) / n;
        int x = 0;
        for (int i = 0; i < n; ++i)
        {
            buttons[i]->setBounds (x, 0, i == n - 1 ? getWidth() - x : w, getHeight());
            x += w + gap;
        }
    }

    //==========================================================================
    void TransferCurve::update (float t, float r, int m, float in)
    {
        if (std::abs (t - thr) < 1.0e-4f && std::abs (r - ratio) < 1.0e-4f && m == mode && std::abs (in - inDb) < 0.05f)
            return;
        thr = t; ratio = r; mode = m; inDb = in;
        repaint();
    }

    void TransferCurve::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);

        auto plot = full.reduced (12.0f);
        plot.removeFromTop (22.0f);
        plot.removeFromLeft (22.0f);
        plot.removeFromBottom (14.0f);
        plot = plot.withSizeKeepingCentre (jmin (plot.getWidth(), plot.getHeight()), jmin (plot.getWidth(), plot.getHeight()));

        constexpr float lo = -60.0f;
        auto px = [&] (float db) { return plot.getX() + plot.getWidth() * jlimit (0.0f, 1.0f, (db - lo) / -lo); };
        auto py = [&] (float db) { return plot.getBottom() - plot.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / -lo); };

        g.setFont (uiFont (9.5f));
        for (float db = lo; db <= 0.0f; db += 20.0f)
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (py (db)), plot.getX(), plot.getRight());
            g.drawVerticalLine (roundToInt (px (db)), plot.getY(), plot.getBottom());
            g.setColour (col::textDim);
            g.drawText (String (roundToInt (db)), Rectangle<float> (plot.getX() - 26.0f, py (db) - 7.0f, 22.0f, 14.0f), Justification::centredRight);
            g.drawText (String (roundToInt (db)), Rectangle<float> (px (db) - 15.0f, plot.getBottom() + 2.0f, 30.0f, 12.0f), Justification::centred);
        }

        // unity line
        g.setColour (col::textDim.withAlpha (0.35f));
        g.drawLine (px (lo), py (lo), px (0.0f), py (0.0f), 1.0f);

        // threshold marker
        const float tx = px (thr);
        g.setColour (optoCol.withAlpha (0.35f));
        g.drawVerticalLine (roundToInt (tx), plot.getY(), plot.getBottom());

        // curve
        Path curvePath;
        for (int i = 0; i <= 120; ++i)
        {
            const float in = lo + -lo * (float) i / 120.0f;
            const float out = in - gainReductionFor (in, thr, ratio, mode);
            if (i == 0) curvePath.startNewSubPath (px (in), py (out));
            else        curvePath.lineTo (px (in), py (out));
        }
        Path fill (curvePath);
        fill.lineTo (px (0.0f), plot.getBottom());
        fill.lineTo (px (lo), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (optoCol.withAlpha (0.22f), 0.0f, plot.getY(), optoCol.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (optoCol);
        g.strokePath (curvePath, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // current level
        if (inDb > lo)
        {
            const float out = inDb - gainReductionFor (inDb, thr, ratio, mode);
            const Point<float> pt (px (inDb), py (out));
            g.setColour (optoSoft.withAlpha (0.3f));
            g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (pt));
            g.setColour (optoSoft);
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (pt));
        }

        // title
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("KURVE", full.reduced (12.0f, 8.0f).removeFromTop (16.0f), Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (String (ratio, ratio < 10.0f ? 1 : 0) + String (CharPointer_UTF8 (":1  ·  ")) + String (thr, 1) + " dB",
                    full.reduced (12.0f, 8.0f).removeFromTop (16.0f), Justification::centredRight);
    }

    //==========================================================================
    void History::push (const Levels& l)
    {
        data[(size_t) write] = l;
        write = (write + 1) % numPoints;
        repaint();
    }

    void History::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);

        auto head = full.reduced (12.0f, 8.0f).removeFromTop (16.0f);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("HISTORIKK", head, Justification::centredLeft);

        // legend
        auto legend = head;
        auto item = [&] (const String& name, Colour c, bool area)
        {
            auto r = legend.removeFromRight (90.0f);
            g.setColour (c);
            if (area) g.fillRect (r.getX(), r.getCentreY() - 4.0f, 14.0f, 8.0f);
            else      g.fillRect (r.getX(), r.getCentreY() - 1.0f, 14.0f, 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (10.0f, true));
            g.drawText (name, r.withTrimmedLeft (19.0f), Justification::centredLeft);
        };
        item ("Ut", col::text, false);
        item ("Inn", col::slate, true);
        item ("Demping", optoCol, true);

        auto plot = full.reduced (12.0f);
        plot.removeFromTop (22.0f);
        plot.removeFromLeft (24.0f);
        plot.removeFromRight (24.0f);

        constexpr float lo = -60.0f, grRange = 24.0f;
        auto grLane = plot.removeFromTop (plot.getHeight() * 0.34f);
        plot.removeFromTop (10.0f);
        auto lvLane = plot;

        auto ly = [&] (float db) { return lvLane.getBottom() - lvLane.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / -lo); };
        auto gy = [&] (float gr) { return grLane.getY() + grLane.getHeight() * jlimit (0.0f, 1.0f, gr / grRange); };

        g.setFont (uiFont (9.5f));
        for (float db = 0.0f; db >= lo; db -= 20.0f)
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (ly (db)), lvLane.getX(), lvLane.getRight());
            g.setColour (col::textDim);
            g.drawText (String (roundToInt (db)), Rectangle<float> (lvLane.getX() - 26.0f, ly (db) - 7.0f, 22.0f, 14.0f), Justification::centredRight);
        }
        for (float gr : { 0.0f, 12.0f, 24.0f })
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (gy (gr)), grLane.getX(), grLane.getRight());
            g.setColour (optoCol.withAlpha (0.8f));
            g.drawText (gr == 0.0f ? String ("0") : "-" + String (roundToInt (gr)),
                        Rectangle<float> (grLane.getRight() + 4.0f, gy (gr) - 7.0f, 22.0f, 14.0f), Justification::centredLeft);
        }

        const float dx = lvLane.getWidth() / (float) (numPoints - 1);
        auto at = [&] (int i) -> const Levels& { return data[(size_t) ((write + i) % numPoints)]; };

        Path inArea, outLine, grArea, grLine;
        inArea.startNewSubPath (lvLane.getX(), lvLane.getBottom());
        grArea.startNewSubPath (grLane.getX(), grLane.getY());
        for (int i = 0; i < numPoints; ++i)
        {
            const float x = lvLane.getX() + dx * (float) i;
            const auto& l = at (i);
            inArea.lineTo (x, ly (l.inDb));
            grArea.lineTo (x, gy (l.grDb));
            if (i == 0) { outLine.startNewSubPath (x, ly (l.outDb)); grLine.startNewSubPath (x, gy (l.grDb)); }
            else        { outLine.lineTo (x, ly (l.outDb));          grLine.lineTo (x, gy (l.grDb)); }
        }
        inArea.lineTo (lvLane.getRight(), lvLane.getBottom());
        inArea.closeSubPath();
        grArea.lineTo (grLane.getRight(), grLane.getY());
        grArea.closeSubPath();

        g.setColour (col::slate.withAlpha (0.45f));
        g.fillPath (inArea);
        g.setColour (col::text.withAlpha (0.85f));
        g.strokePath (outLine, PathStrokeType (1.4f));
        g.setGradientFill (ColourGradient (optoCol.withAlpha (0.45f), 0.0f, grLane.getY(), optoCol.withAlpha (0.1f), 0.0f, grLane.getBottom(), false));
        g.fillPath (grArea);
        g.setColour (optoCol);
        g.strokePath (grLine, PathStrokeType (1.6f));
    }

    //==========================================================================
    void Meters::setLevels (const Levels& l)
    {
        shown.inDb  = jmax (l.inDb,  shown.inDb - 1.5f);
        shown.outDb = jmax (l.outDb, shown.outDb - 1.5f);
        shown.grDb  = l.grDb;
        if (l.grDb >= grHold) { grHold = l.grDb; holdFrames = 45; }
        else if (--holdFrames <= 0) grHold = jmax (l.grDb, grHold - 0.3f);
        repaint();
    }

    void Meters::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        const float w = (r.getWidth() - 12.0f) / 3.0f;

        auto bar = [&] (Rectangle<float> b, const String& label, const String& value, float t, bool fromTop, Colour c)
        {
            auto labelR = b.removeFromBottom (14.0f);
            auto valueR = b.removeFromTop (14.0f);
            b.removeFromTop (2.0f);
            g.setColour (col::well);
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
            t = jlimit (0.0f, 1.0f, t);
            auto fill = fromTop ? b.withHeight (b.getHeight() * t) : b.withTop (b.getBottom() - b.getHeight() * t);
            g.setColour (c);
            g.fillRoundedRectangle (fill.reduced (1.0f, 0.0f), 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (label, labelR, Justification::centred);
            g.setColour (col::text);
            g.setFont (uiFont (9.5f, true));
            g.drawFittedText (value, valueR.toNearestInt(), Justification::centred, 1, 0.8f);
        };

        auto lvl = [] (float db) { return (db + 60.0f) / 60.0f; };
        bar (r.removeFromLeft (w), "INN", dbString (shown.inDb), lvl (shown.inDb), false, col::slate);
        r.removeFromLeft (6.0f);
        bar (r.removeFromLeft (w), "GR", grHold < 0.05f ? String ("0.0") : "-" + String (grHold, 1), shown.grDb / 24.0f, true, optoCol);
        r.removeFromLeft (6.0f);
        bar (r, "UT", dbString (shown.outDb), lvl (shown.outDb), false, col::text.withAlpha (0.75f));
    }
}

//==============================================================================
OptoTubeEditor::OptoTubeEditor (OptoTubeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      modeSelector (*p.apvts.getParameter (ParamIDs::mode))
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    content.addAndMakeVisible (modeSelector);
    hpfBox.addItemList (optotube::hpfNames, 1);
    hpfBox.setTooltip (CharPointer_UTF8 ("Filtrerer bassen bort fra det kompressoren lytter på (ikke fra lyden), så den ikke "
                                         "pumper av proximity-effekt, plopp eller gitarkroppens boom."));
    content.addAndMakeVisible (hpfBox);
    hpfAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::hpf, hpfBox);

    content.addAndMakeVisible (curve);
    content.addAndMakeVisible (history);
    content.addAndMakeVisible (meters);
    for (auto* c : std::initializer_list<Component*> { &curve, &history, &meters })
        c->setInterceptsMouseClicks (false, false);

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &threshold, ParamIDs::threshold }, { &ratio, ParamIDs::ratio }, { &attack, ParamIDs::attack },
        { &release, ParamIDs::release }, { &tube, ParamIDs::tube }, { &mix, ParamIDs::mix }, { &gain, ParamIDs::gain } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, optoCol);
    }
    gain.slider.getProperties().set ("bipolar", true);

    threshold.slider.setTooltip (CharPointer_UTF8 ("Nivået der kompresjonen starter (Peak Reduction på LA-2A). Lavere = mer kompresjon."));
    ratio.slider.setTooltip (CharPointer_UTF8 ("Hvor kraftig signalet over terskelen dempes. 2–4 er typisk for vokal og gitar, 10 nærmer seg limiting."));
    attack.slider.setTooltip (CharPointer_UTF8 ("Hvor raskt den manuelle kompressoren griper inn. Brukes i MANUAL og OPTO + MANUAL."));
    release.slider.setTooltip (CharPointer_UTF8 ("Hvor raskt den manuelle kompressoren slipper. Brukes i MANUAL og OPTO + MANUAL."));
    tube.slider.setTooltip (CharPointer_UTF8 ("Rørkarakter: myk metning med mest 2. harmoniske. 0 = helt rent."));
    mix.slider.setTooltip (CharPointer_UTF8 ("Parallellkompresjon: blander inn ukomprimert signal. 100 % = bare komprimert."));
    gain.slider.setTooltip (CharPointer_UTF8 ("Make-up gain på det komprimerte signalet."));

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    updateTimeSection();
    startTimerHz (30);
}

OptoTubeEditor::~OptoTubeEditor()
{
    setLookAndFeel (nullptr);
}

void OptoTubeEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void OptoTubeEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void OptoTubeEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    modeSelector.setBounds (12, 56, 600, 32);
    hpfBox.setBounds (designW - 12 - 150, 58, 150, 28);

    const int dispY = 100, dispH = 220;
    curve.setBounds (12, dispY, dispH, dispH);
    meters.setBounds (designW - 12 - 104, dispY, 104, dispH);
    history.setBounds (curve.getRight() + 10, dispY, meters.getX() - 10 - curve.getRight() - 10, dispH);

    const int panelY = dispY + dispH + 12, panelH = designH - panelY - 12, gap = 12;
    compPanel   = { 12, panelY, 300, panelH };
    timePanel   = { compPanel.getRight() + gap, panelY, 240, panelH };
    colourPanel = { timePanel.getRight() + gap, panelY, designW - 12 - (timePanel.getRight() + gap), panelH };

    auto knobRow = [] (Rectangle<int> panel, std::initializer_list<sbaudio::Knob*> ks)
    {
        auto r = panel.reduced (14, 12);
        r.removeFromTop (26);
        r = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 132));
        const int w = r.getWidth() / (int) ks.size();
        for (auto* k : ks)
            k->setBounds (r.removeFromLeft (w).reduced (4, 0));
    };
    knobRow (compPanel, { &threshold, &ratio });
    knobRow (timePanel, { &attack, &release });
    knobRow (colourPanel, { &tube, &mix, &gain });
}

void OptoTubeEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    // ---- top bar ----
    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "OptoTube", "COMPRESSOR", optoCol);

    // detector HPF label
    g.setFont (uiFont (10.0f, true));
    g.drawText ("DETEKTOR-HPF", Rectangle<float> ((float) hpfBox.getX() - 110.0f, (float) hpfBox.getY(), 102.0f, (float) hpfBox.getHeight()),
                Justification::centredRight);

    // ---- knob panels ----
    auto panel = [&] (Rectangle<int> rr, const String& title, const String& note, bool dimNote)
    {
        auto r = rr.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto head = r.reduced (16.0f, 12.0f).removeFromTop (16.0f);
        g.setColour (optoCol);
        g.setFont (uiFont (11.0f, true));
        g.drawText (title, head, Justification::centredLeft);
        g.setColour (dimNote ? col::textDim : optoSoft);
        g.setFont (uiFont (10.0f, ! dimNote));
        g.drawText (note, head, Justification::centredRight);
    };
    panel (compPanel, "KOMPRESSOR", "terskel og styrke", true);
    panel (timePanel, "TID", timeIsAuto ? String ("automatisk i OPTO") : String ("manuell"), ! timeIsAuto);
    panel (colourPanel, String (CharPointer_UTF8 ("KARAKTER  /  UT")), "", true);
}

void OptoTubeEditor::updateTimeSection()
{
    const bool isAuto = (int) proc.apvts.getRawParameterValue (ParamIDs::mode)->load() == optotube::Opto;
    attack.setAlpha (isAuto ? 0.4f : 1.0f);
    release.setAlpha (isAuto ? 0.4f : 1.0f);
    if (isAuto != timeIsAuto)
    {
        timeIsAuto = isAuto;
        content.repaint (timePanel);
    }
}

void OptoTubeEditor::timerCallback()
{
    const float gr = proc.meterGrDb.exchange (-1.0f);
    const float in = proc.meterInMs.exchange (-1.0f);
    const float out = proc.meterOutMs.exchange (-1.0f);

    // mean square -> dB, calibrated like the detector (sine peak)
    auto toDb = [] (float ms) { return ms <= 1.0e-10f ? -100.0f : 10.0f * std::log10 (ms) + 3.01f; };

    if (gr >= 0.0f || in >= 0.0f || out >= 0.0f)
    {
        framesWithoutAudio = 0;
        if (gr >= 0.0f)  last.grDb = gr;
        if (in >= 0.0f)  last.inDb = toDb (in);
        if (out >= 0.0f) last.outDb = toDb (out);
    }
    else if (++framesWithoutAudio > 10) // host stopped processing: let everything fall back
    {
        last = {};
    }

    history.push (last);
    meters.setLevels (last);
    curve.update (proc.apvts.getRawParameterValue (ParamIDs::threshold)->load(),
                  proc.apvts.getRawParameterValue (ParamIDs::ratio)->load(),
                  (int) proc.apvts.getRawParameterValue (ParamIDs::mode)->load(),
                  last.inDb);
    updateTimeSection();
}
