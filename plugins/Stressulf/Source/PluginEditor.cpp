#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour stressCol  { 0xffff4f6b };   // hot red-pink
    const Colour stressSoft { 0xffffa3b2 };

    void drawWell (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    String dbString (float db) { return db <= -99.0f ? String ("-inf") : String (db, 1); }
    String utf8 (const char* s) { return String (CharPointer_UTF8 (s)); }
}

namespace stressulfui
{
    //==========================================================================
    ChoiceButtons::ChoiceButtons (RangedAudioParameter& p, const StringArray& labels, Colour on)
        : attachment (p, [this] (float v) { setCurrent (roundToInt (v)); }, nullptr)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new TextButton (labels[i]));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, on);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void ChoiceButtons::setTooltips (const StringArray& t)
    {
        for (int i = 0; i < jmin (t.size(), buttons.size()); ++i)
            buttons[i]->setTooltip (t[i]);
    }

    void ChoiceButtons::setCurrent (int m)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == m, dontSendNotification);
    }

    void ChoiceButtons::resized()
    {
        const int gap = 5, n = buttons.size();
        const float w = (float) (getWidth() - gap * (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const int x0 = roundToInt ((w + gap) * (float) i), x1 = i == n - 1 ? getWidth() : roundToInt ((w + gap) * (float) i + w);
            buttons[i]->setBounds (x0, 0, x1 - x0, getHeight());
        }
    }

    //==========================================================================
    void TransferCurve::update (int r, bool s, float lvl)
    {
        if (r == ratio && s == smash && std::abs (lvl - level) < 0.05f)
            return;
        ratio = r; smash = s; level = lvl;
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

        constexpr float lo = -50.0f, hi = 0.0f;
        auto px = [&] (float db) { return plot.getX() + plot.getWidth() * jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)); };
        auto py = [&] (float db) { return plot.getBottom() - plot.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)); };

        g.setFont (uiFont (9.5f));
        for (float db = lo; db <= hi; db += 10.0f)
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (py (db)), plot.getX(), plot.getRight());
            g.drawVerticalLine (roundToInt (px (db)), plot.getY(), plot.getBottom());
            if (roundToInt (db) % 20 == 0 || roundToInt (db) == roundToInt (lo))
            {
                g.setColour (col::textDim);
                g.drawText (String (roundToInt (db)), Rectangle<float> (plot.getX() - 26.0f, py (db) - 7.0f, 22.0f, 14.0f), Justification::centredRight);
                g.drawText (String (roundToInt (db)), Rectangle<float> (px (db) - 15.0f, plot.getBottom() + 2.0f, 30.0f, 12.0f), Justification::centred);
            }
        }

        g.setColour (col::textDim.withAlpha (0.35f));
        g.drawLine (px (lo), py (lo), px (hi), py (hi), 1.0f);

        const float tx = px (stressulf::thresholdDb);
        g.setColour (stressCol.withAlpha (0.35f));
        g.drawVerticalLine (roundToInt (tx), plot.getY(), plot.getBottom());

        Path curvePath;
        for (int i = 0; i <= 150; ++i)
        {
            const float in = lo + (hi - lo) * (float) i / 150.0f;
            const float out = in - stressulf::staticGr (in, ratio, smash);
            if (i == 0) curvePath.startNewSubPath (px (in), py (out));
            else        curvePath.lineTo (px (in), py (out));
        }
        Path fill (curvePath);
        fill.lineTo (px (hi), plot.getBottom());
        fill.lineTo (px (lo), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (stressCol.withAlpha (0.22f), 0.0f, plot.getY(), stressCol.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (stressCol);
        g.strokePath (curvePath, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        if (level > lo)
        {
            const float l = jmin (level, hi);
            const Point<float> pt (px (l), py (l - stressulf::staticGr (l, ratio, smash)));
            g.setColour (stressSoft.withAlpha (0.3f));
            g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (pt));
            g.setColour (stressSoft);
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (pt));
        }

        auto head = full.reduced (12.0f, 8.0f).removeFromTop (16.0f);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("KURVE", head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (smash ? String ("SMASH") : stressulf::ratioNames[jmax (0, ratio)].toUpperCase(), head, Justification::centredRight);
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
        item ("Demping", stressCol, true);

        auto plot = full.reduced (12.0f);
        plot.removeFromTop (22.0f);
        plot.removeFromLeft (24.0f);
        plot.removeFromRight (24.0f);

        constexpr float lo = -60.0f, grRange = 30.0f;
        auto grLane = plot.removeFromTop (plot.getHeight() * 0.36f);
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
        for (float gr : { 0.0f, 15.0f, 30.0f })
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (gy (gr)), grLane.getX(), grLane.getRight());
            g.setColour (stressCol.withAlpha (0.8f));
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
        g.setGradientFill (ColourGradient (stressCol.withAlpha (0.45f), 0.0f, grLane.getY(), stressCol.withAlpha (0.1f), 0.0f, grLane.getBottom(), false));
        g.fillPath (grArea);
        g.setColour (stressCol);
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
            g.drawFittedText (value, valueR.toNearestInt(), Justification::centred, 1, 0.8f);
        };

        auto lvl = [] (float db) { return (db + 60.0f) / 60.0f; };
        bar (r.removeFromLeft (w), "INN", dbString (shown.inDb), lvl (shown.inDb), false, col::slate);
        r.removeFromLeft (6.0f);
        bar (r.removeFromLeft (w), "GR", grHold < 0.05f ? String ("0.0") : "-" + String (grHold, 1), shown.grDb / 30.0f, true, stressCol);
        r.removeFromLeft (6.0f);
        bar (r, "UT", dbString (shown.outDb), lvl (shown.outDb), false, col::text.withAlpha (0.75f));
    }
}

//==============================================================================
StressulfEditor::StressulfEditor (StressulfProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      ratioButtons (*p.apvts.getParameter (ParamIDs::ratio),
                    { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE", "OPTO" }, stressCol),
      distButtons (*p.apvts.getParameter (ParamIDs::dist), { "CLEAN", "DIST 2", "DIST 3" }, stressCol)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    ratioButtons.setTooltips ({
        utf8 ("1:1 – ingen kompresjon, bare farge (bruk med DIST 2 / DIST 3)."),
        utf8 ("2:1 – svært myk knekk, nesten usynlig utjevning."),
        utf8 ("3:1 – myk og musikalsk."),
        utf8 ("4:1 – allsidig."),
        utf8 ("6:1 – tydelig og tett."),
        utf8 ("10:1 – nær begrensning."),
        utf8 ("20:1 – begrenser."),
        utf8 ("NUKE – nesten brickwall, svært hard knekk."),
        utf8 ("OPTO – programavhengig: rask attack, release som blir langsommere jo lenger den demper.") });
    distButtons.setTooltips ({
        utf8 ("Ren kompresjon."),
        utf8 ("DIST 2 – like overtoner (rør-aktig): varm og tykk. Øker med kompresjonen."),
        utf8 ("DIST 3 – odde overtoner (tape-aktig): kant og bitt. Øker med kompresjonen.") });
    content.addAndMakeVisible (ratioButtons);
    content.addAndMakeVisible (distButtons);

    setupToggle (smashBtn,   ParamIDs::smash,   utf8 ("SMASH: alle knapper inne. Overkomprimerer med rask attack og ekstra forvrengning – for rom og parallell knusing."));
    setupToggle (detHpBtn,   ParamIDs::detHp,   utf8 ("Detektor-HP: detektoren lytter mindre på bassen, så den ikke pumper av basstromme og bass."));
    setupToggle (bandBtn,    ParamIDs::detBand, utf8 ("Band-emfase: detektoren lytter ekstra på 6 kHz. Demper S-lyder og skarpe topper – fungerer som en de-esser."));
    setupToggle (audioHpBtn, ParamIDs::audioHp, utf8 ("Audio-HP: fjerner rumling under 80 Hz i selve lyden."));
    setupToggle (linkBtn,    ParamIDs::link,    utf8 ("Stereo-link: begge kanaler dempes likt, så stereobildet står stille."));
    smashBtn.setColour (TextButton::buttonOnColourId, col::red);

    content.addAndMakeVisible (curve);
    content.addAndMakeVisible (history);
    content.addAndMakeVisible (meters);
    for (auto* c : std::initializer_list<Component*> { &curve, &history, &meters })
        c->setInterceptsMouseClicks (false, false);

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &input, ParamIDs::input }, { &attack, ParamIDs::attack }, { &release, ParamIDs::release },
        { &mix, ParamIDs::mix }, { &output, ParamIDs::output } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, stressCol);
    }
    output.slider.getProperties().set ("bipolar", true);
    input.slider.getProperties().set ("bipolar", true);

    input.slider.setTooltip (utf8 ("Driver signalet inn mot den faste terskelen. Mer input = mer kompresjon (og mer farge)."));
    attack.slider.setTooltip (utf8 ("Hvor raskt den griper inn: fra 50 µs (fanger alt) til 30 ms (slipper anslaget gjennom)."));
    release.slider.setTooltip (utf8 ("Hvor raskt den slipper. Programavhengig: blir langsommere når den demper lenge."));
    mix.slider.setTooltip (utf8 ("Parallellkompresjon: blander inn ukomprimert signal. 100 % = bare komprimert."));
    output.slider.setTooltip (utf8 ("Utgangsnivå. Skru ned når du skrur opp Input."));

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);
    startTimerHz (30);
}

StressulfEditor::~StressulfEditor()
{
    setLookAndFeel (nullptr);
}

void StressulfEditor::setupToggle (TextButton& b, const char* id, const String& tip)
{
    b.setClickingTogglesState (true);
    b.setColour (TextButton::buttonOnColourId, stressCol);
    b.setColour (TextButton::textColourOnId, col::bg);
    b.setTooltip (tip);
    content.addAndMakeVisible (b);
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
}

void StressulfEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void StressulfEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void StressulfEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    ratioButtons.setBounds (12, 56, 700, 32);
    smashBtn.setBounds (designW - 12 - 160, 56, 160, 32);

    const int dispY = 100, dispH = 212;
    curve.setBounds (12, dispY, dispH, dispH);
    meters.setBounds (designW - 12 - 104, dispY, 104, dispH);
    history.setBounds (curve.getRight() + 10, dispY, meters.getX() - 10 - curve.getRight() - 10, dispH);

    const int panelY = dispY + dispH + 12, panelH = designH - panelY - 12, gap = 12;
    dynPanel    = { 12, panelY, 360, panelH };
    colourPanel = { dynPanel.getRight() + gap, panelY, 250, panelH };
    outPanel    = { colourPanel.getRight() + gap, panelY, designW - 12 - (colourPanel.getRight() + gap), panelH };

    auto knobRow = [] (Rectangle<int> panel, std::initializer_list<sbaudio::Knob*> ks)
    {
        auto r = panel.reduced (14, 12);
        r.removeFromTop (26);
        r = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 140));
        const int w = r.getWidth() / (int) ks.size();
        for (auto* k : ks)
            k->setBounds (r.removeFromLeft (w).reduced (4, 0));
    };
    knobRow (dynPanel, { &input, &attack, &release });
    knobRow (outPanel, { &mix, &output });

    // colour panel: distortion row, then a 2 x 2 grid of detector/audio toggles
    auto r = colourPanel.reduced (16, 12);
    r.removeFromTop (28);
    distButtons.setBounds (r.removeFromTop (34));
    r.removeFromTop (30);
    const int bw = (r.getWidth() - 6) / 2, bh = 32;
    detHpBtn.setBounds   (r.getX(),           r.getY(),          bw, bh);
    bandBtn.setBounds    (r.getX() + bw + 6,  r.getY(),          bw, bh);
    audioHpBtn.setBounds (r.getX(),           r.getY() + bh + 8, bw, bh);
    linkBtn.setBounds    (r.getX() + bw + 6,  r.getY() + bh + 8, bw, bh);
}

void StressulfEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "Stressulf", "COMPRESSOR", stressCol);

    auto panel = [&] (Rectangle<int> rr, const String& title, const String& note)
    {
        auto r = rr.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto head = r.reduced (16.0f, 12.0f).removeFromTop (16.0f);
        g.setColour (stressCol);
        g.setFont (uiFont (11.0f, true));
        g.drawText (title, head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f));
        g.drawText (note, head, Justification::centredRight);
    };
    panel (dynPanel, "DYNAMIKK", timesFixed ? utf8 ("attack/release styres av OPTO") : utf8 ("inn, attack og release"));
    panel (colourPanel, "KARAKTER", utf8 ("farge og detektor"));
    panel (outPanel, "UT", utf8 ("blanding og nivå"));

    // small caption above the toggle grid
    g.setColour (col::textDim);
    g.setFont (uiFont (9.5f, true));
    g.drawText (utf8 ("DETEKTOR  ·  LYD"), Rectangle<int> (detHpBtn.getX(), detHpBtn.getY() - 18, 200, 14), Justification::centredLeft);
}

void StressulfEditor::timerCallback()
{
    const float gr = proc.meterGrDb.exchange (-1.0f);
    const float in = proc.meterInMs.exchange (-1.0f);
    const float out = proc.meterOutMs.exchange (-1.0f);
    auto toDb = [] (float ms) { return ms <= 1.0e-10f ? -100.0f : 10.0f * std::log10 (ms) + 3.01f; };

    if (gr >= 0.0f || in >= 0.0f || out >= 0.0f)
    {
        framesWithoutAudio = 0;
        if (gr >= 0.0f)  last.grDb = gr;
        if (in >= 0.0f)  last.inDb = toDb (in);
        if (out >= 0.0f) last.outDb = toDb (out);
    }
    else if (++framesWithoutAudio > 10)
    {
        last = {};
    }

    history.push (last);
    meters.setLevels (last);

    const auto s = proc.currentSettings();
    curve.update (s.ratio, s.smash, last.inDb > -99.0f ? last.inDb + s.inputDb : -100.0f);

    const bool fixed = s.ratio == stressulf::opto && ! s.smash;
    attack.setAlpha (fixed ? 0.4f : 1.0f);
    release.setAlpha (fixed ? 0.4f : 1.0f);
    if (fixed != timesFixed)
    {
        timesFixed = fixed;
        content.repaint (dynPanel);
    }
}
