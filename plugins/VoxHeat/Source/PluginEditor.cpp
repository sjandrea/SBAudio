#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour heatCol  { 0xffff6f45 };   // glowing-filament orange-red
    const Colour heatSoft { 0xffffb596 };

    const char* charTooltips[] = {
        "TG: transistor/konsoll – framoverlent og tydelig, mest odde overtoner.",
        "TUBE: rør – rundere, varmere og fyldigere, mest like overtoner."
    };

    // ---- display copy of the processor's curves (PluginProcessor.cpp) ----
    constexpr float refLevel = 0.2f, tgBias = 0.06f, tubeBias = 0.30f;
    float tgCurve (float v)   { const float a = std::abs (v); return v / std::cbrt (1.0f + a * a * a); }
    float tubeCurve (float v) { return std::tanh (v); }
    float driveToK (float d)  { return 0.5f * std::pow (10.0f, (d * 33.0f) / 20.0f); }

    /** Output for an input in [-1, 1] (= +/- 2x nominal vocal level), normalised like Auto Gain. */
    float shape (float x, float k, float c)
    {
        const float u = x * 2.0f * refLevel;
        const float kr = k * refLevel;
        const float agT = 2.0f * refLevel / (std::abs (tgCurve (kr + tgBias) - tgCurve (tgBias))
                                           + std::abs (tgCurve (-kr + tgBias) - tgCurve (tgBias)));
        const float agV = 2.0f * refLevel / (std::abs (tubeCurve (kr + tubeBias) - tubeCurve (tubeBias))
                                           + std::abs (tubeCurve (-kr + tubeBias) - tubeCurve (tubeBias)));
        const float yt = (tgCurve (k * u + tgBias) - tgCurve (tgBias)) * agT;
        const float yv = (tubeCurve (k * u + tubeBias) - tubeCurve (tubeBias)) * agV;
        return ((1.0f - c) * yt + c * yv) / (2.0f * refLevel);
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

namespace voxheatui
{
    //==========================================================================
    CharacterSelector::CharacterSelector (RangedAudioParameter& p)
        : attachment (p, [this] (float v) { setCurrent (roundToInt (v)); }, nullptr)
    {
        const char* names[] = { "TG  ·  KONSOLL", "TUBE  ·  RØR" };
        for (int i = 0; i < 2; ++i)
        {
            auto* b = buttons.add (new TextButton (String (CharPointer_UTF8 (names[i]))));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, heatCol);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->setTooltip (String (CharPointer_UTF8 (charTooltips[i])));
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void CharacterSelector::setCurrent (int m)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == m, dontSendNotification);
    }

    void CharacterSelector::resized()
    {
        const int gap = 6, w = (getWidth() - gap) / 2;
        buttons[0]->setBounds (0, 0, w, getHeight());
        buttons[1]->setBounds (w + gap, 0, getWidth() - w - gap, getHeight());
    }

    //==========================================================================
    void SaturationCurve::update (float d, float c, float h)
    {
        if (std::abs (d - drive) < 1.0e-4f && std::abs (c - character) < 1.0e-3f && std::abs (h - heat) < 0.005f)
            return;
        drive = d; character = c; heat = h;
        repaint();
    }

    void SaturationCurve::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);

        auto plot = full.reduced (12.0f);
        plot.removeFromTop (22.0f);
        plot.removeFromLeft (8.0f);
        plot.removeFromBottom (14.0f);
        plot = plot.withSizeKeepingCentre (jmin (plot.getWidth(), plot.getHeight()), jmin (plot.getWidth(), plot.getHeight()));

        constexpr float range = 1.25f;
        auto px = [&] (float v) { return plot.getCentreX() + plot.getWidth()  * 0.5f * jlimit (-1.0f, 1.0f, v / range); };
        auto py = [&] (float v) { return plot.getCentreY() - plot.getHeight() * 0.5f * jlimit (-1.0f, 1.0f, v / range); };

        // grid
        g.setColour (col::panelEdge.withAlpha (0.7f));
        for (float v : { -1.0f, 0.0f, 1.0f })
        {
            g.drawHorizontalLine (roundToInt (py (v)), plot.getX(), plot.getRight());
            g.drawVerticalLine (roundToInt (px (v)), plot.getY(), plot.getBottom());
        }
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f));
        g.drawText ("INN", Rectangle<float> (plot.getRight() - 30.0f, plot.getBottom() + 1.0f, 30.0f, 12.0f), Justification::centredRight);

        // linear reference
        g.setColour (col::textDim.withAlpha (0.35f));
        g.drawLine (px (-range), py (-range), px (range), py (range), 1.0f);

        const float k = driveToK (drive);
        Path curvePath;
        for (int i = 0; i <= 160; ++i)
        {
            const float x = -range + 2.0f * range * (float) i / 160.0f;
            const float y = shape (x, k, character);
            if (i == 0) curvePath.startNewSubPath (px (x), py (y));
            else        curvePath.lineTo (px (x), py (y));
        }

        // glow grows with the measured heat
        if (heat > 0.01f)
        {
            g.setColour (heatCol.withAlpha (0.25f * heat));
            g.strokePath (curvePath, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (heatCol);
        g.strokePath (curvePath, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // title
        auto head = full.reduced (12.0f, 8.0f).removeFromTop (16.0f);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("KURVE", head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (character < 0.5f ? String ("TG") : String ("TUBE"), head, Justification::centredRight);
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
            auto r = legend.removeFromRight (80.0f);
            g.setColour (c);
            if (area) g.fillRect (r.getX(), r.getCentreY() - 4.0f, 14.0f, 8.0f);
            else      g.fillRect (r.getX(), r.getCentreY() - 1.0f, 14.0f, 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (10.0f, true));
            g.drawText (name, r.withTrimmedLeft (19.0f), Justification::centredLeft);
        };
        item ("Ut", col::text, false);
        item ("Inn", col::slate, true);
        item ("Heat", heatCol, true);

        auto plot = full.reduced (12.0f);
        plot.removeFromTop (22.0f);
        plot.removeFromLeft (24.0f);
        plot.removeFromRight (30.0f);

        constexpr float lo = -60.0f;
        auto heatLane = plot.removeFromTop (plot.getHeight() * 0.30f);
        plot.removeFromTop (10.0f);
        auto lvLane = plot;

        auto ly = [&] (float db) { return lvLane.getBottom() - lvLane.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / -lo); };
        auto hy = [&] (float h)  { return heatLane.getY() + heatLane.getHeight() * jlimit (0.0f, 1.0f, h); };

        g.setFont (uiFont (9.5f));
        for (float db = 0.0f; db >= lo; db -= 20.0f)
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (ly (db)), lvLane.getX(), lvLane.getRight());
            g.setColour (col::textDim);
            g.drawText (String (roundToInt (db)), Rectangle<float> (lvLane.getX() - 26.0f, ly (db) - 7.0f, 22.0f, 14.0f), Justification::centredRight);
        }
        for (float h : { 0.0f, 0.5f, 1.0f })
        {
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (hy (h)), heatLane.getX(), heatLane.getRight());
            g.setColour (heatCol.withAlpha (0.8f));
            g.drawText (String (roundToInt (h * 100.0f)) + "%",
                        Rectangle<float> (heatLane.getRight() + 4.0f, hy (h) - 7.0f, 30.0f, 14.0f), Justification::centredLeft);
        }

        const float dx = lvLane.getWidth() / (float) (numPoints - 1);
        auto at = [&] (int i) -> const Levels& { return data[(size_t) ((write + i) % numPoints)]; };

        Path inArea, outLine, heatArea, heatLine;
        inArea.startNewSubPath (lvLane.getX(), lvLane.getBottom());
        heatArea.startNewSubPath (heatLane.getX(), heatLane.getY());
        for (int i = 0; i < numPoints; ++i)
        {
            const float x = lvLane.getX() + dx * (float) i;
            const auto& l = at (i);
            inArea.lineTo (x, ly (l.inDb));
            heatArea.lineTo (x, hy (l.heat));
            if (i == 0) { outLine.startNewSubPath (x, ly (l.outDb)); heatLine.startNewSubPath (x, hy (l.heat)); }
            else        { outLine.lineTo (x, ly (l.outDb));          heatLine.lineTo (x, hy (l.heat)); }
        }
        inArea.lineTo (lvLane.getRight(), lvLane.getBottom());
        inArea.closeSubPath();
        heatArea.lineTo (heatLane.getRight(), heatLane.getY());
        heatArea.closeSubPath();

        g.setColour (col::slate.withAlpha (0.45f));
        g.fillPath (inArea);
        g.setColour (col::text.withAlpha (0.85f));
        g.strokePath (outLine, PathStrokeType (1.4f));
        g.setGradientFill (ColourGradient (heatCol.withAlpha (0.45f), 0.0f, heatLane.getY(), heatCol.withAlpha (0.1f), 0.0f, heatLane.getBottom(), false));
        g.fillPath (heatArea);
        g.setColour (heatCol);
        g.strokePath (heatLine, PathStrokeType (1.6f));
    }

    //==========================================================================
    void Meters::setLevels (const Levels& l)
    {
        shown.inDb  = jmax (l.inDb,  shown.inDb - 1.5f);
        shown.outDb = jmax (l.outDb, shown.outDb - 1.5f);
        shown.heat  = jmax (l.heat,  shown.heat - 0.02f);
        repaint();
    }

    void Meters::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        const float w = (r.getWidth() - 12.0f) / 3.0f;

        auto bar = [&] (Rectangle<float> b, const String& label, const String& value, float t, Colour c)
        {
            auto labelR = b.removeFromBottom (14.0f);
            auto valueR = b.removeFromTop (14.0f);
            b.removeFromTop (2.0f);
            g.setColour (col::well);
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
            t = jlimit (0.0f, 1.0f, t);
            g.setColour (c);
            g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * t).reduced (1.0f, 0.0f), 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (label, labelR, Justification::centred);
            g.setColour (col::text);
            g.drawFittedText (value, valueR.toNearestInt(), Justification::centred, 1, 0.8f);
        };

        auto lvl = [] (float db) { return (db + 60.0f) / 60.0f; };
        bar (r.removeFromLeft (w), "INN", dbString (shown.inDb), lvl (shown.inDb), col::slate);
        r.removeFromLeft (6.0f);
        bar (r.removeFromLeft (w), "HEAT", String (roundToInt (shown.heat * 100.0f)) + "%", shown.heat, heatCol);
        r.removeFromLeft (6.0f);
        bar (r, "UT", dbString (shown.outDb), lvl (shown.outDb), col::text.withAlpha (0.75f));
    }
}

//==============================================================================
VoxHeatEditor::VoxHeatEditor (VoxHeatProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      character (*p.apvts.getParameter (ParamIDs::character))
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    content.addAndMakeVisible (character);

    autoGain.setClickingTogglesState (true);
    autoGain.setColour (TextButton::buttonOnColourId, col::readCol);
    autoGain.setColour (TextButton::textColourOnId, col::bg);
    autoGain.setTooltip (CharPointer_UTF8 ("Holder lydnivået omtrent likt når du endrer Drive, så du hører fargen – ikke bare mer volum."));
    content.addAndMakeVisible (autoGain);
    autoAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::autogain, autoGain);

    content.addAndMakeVisible (curve);
    content.addAndMakeVisible (history);
    content.addAndMakeVisible (meters);
    for (auto* c : std::initializer_list<Component*> { &curve, &history, &meters })
        c->setInterceptsMouseClicks (false, false);

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &input, ParamIDs::input }, { &drive, ParamIDs::drive }, { &density, ParamIDs::density },
        { &tone, ParamIDs::tone }, { &mix, ParamIDs::mix }, { &output, ParamIDs::output } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, heatCol);
    }
    for (auto* k : { &input, &tone, &output })
        k->slider.getProperties().set ("bipolar", true);

    input.slider.setTooltip (CharPointer_UTF8 ("Nivå inn i metningen. Mer input = vokalen presses hardere inn i kurven."));
    drive.slider.setTooltip (CharPointer_UTF8 ("Mengden metning."));
    density.slider.setTooltip (CharPointer_UTF8 ("Kompander: svake og sterke stavelser får like mye farge, mens dynamikken beholdes."));
    tone.slider.setTooltip (CharPointer_UTF8 ("Tilt-EQ etter metningen: mot venstre mørkere og mykere, mot høyre lysere med mer luft."));
    mix.slider.setTooltip (CharPointer_UTF8 ("Blanding av tørt og mettet signal (parallell metning)."));
    output.slider.setTooltip (CharPointer_UTF8 ("Utgangsnivå."));

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);
    startTimerHz (30);
}

VoxHeatEditor::~VoxHeatEditor()
{
    setLookAndFeel (nullptr);
}

void VoxHeatEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void VoxHeatEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void VoxHeatEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    character.setBounds (12, 56, 420, 32);
    autoGain.setBounds (designW - 12 - 150, 56, 150, 32);

    const int dispY = 100, dispH = 220;
    curve.setBounds (12, dispY, dispH, dispH);
    meters.setBounds (designW - 12 - 104, dispY, 104, dispH);
    history.setBounds (curve.getRight() + 10, dispY, meters.getX() - 10 - curve.getRight() - 10, dispH);

    const int panelY = dispY + dispH + 12, panelH = designH - panelY - 12, gap = 12;
    const int panelW = (designW - 24 - 2 * gap) / 3;
    drivePanel  = { 12, panelY, panelW, panelH };
    colourPanel = { drivePanel.getRight() + gap, panelY, panelW, panelH };
    outPanel    = { colourPanel.getRight() + gap, panelY, designW - 12 - (colourPanel.getRight() + gap), panelH };

    auto knobRow = [] (Rectangle<int> panel, std::initializer_list<sbaudio::Knob*> ks)
    {
        auto r = panel.reduced (14, 12);
        r.removeFromTop (26);
        r = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 132));
        const int w = r.getWidth() / (int) ks.size();
        for (auto* k : ks)
            k->setBounds (r.removeFromLeft (w).reduced (4, 0));
    };
    knobRow (drivePanel,  { &input, &drive });
    knobRow (colourPanel, { &density, &tone });
    knobRow (outPanel,    { &mix, &output });
}

void VoxHeatEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    // ---- top bar ----
    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "VoxHeat", "SATURATION", heatCol);

    // ---- knob panels ----
    auto panel = [&] (Rectangle<int> rr, const String& title, const String& note)
    {
        auto r = rr.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto head = r.reduced (16.0f, 12.0f).removeFromTop (16.0f);
        g.setColour (heatCol);
        g.setFont (uiFont (11.0f, true));
        g.drawText (title, head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f));
        g.drawText (note, head, Justification::centredRight);
    };
    panel (drivePanel,  "METNING", "inn og drive");
    panel (colourPanel, "FARGE", "tetthet og tone");
    panel (outPanel,    "UT", String (CharPointer_UTF8 ("blanding og nivå")));
}

void VoxHeatEditor::timerCallback()
{
    // The processor overwrites inRmsDb every ~30 ms; the marker tells us whether audio is still running.
    constexpr float noNewData = 1000.0f;
    const float in = proc.inRmsDb.exchange (noNewData);

    if (in < noNewData)
    {
        framesWithoutAudio = 0;
        last.inDb = in;
        last.outDb = proc.outRmsDb.load();
        last.heat = proc.heat.load();
    }
    else if (++framesWithoutAudio > 10) // host stopped processing: let everything fall back
    {
        last = {};
    }

    history.push (last);
    meters.setLevels (last);
    curve.update (proc.apvts.getRawParameterValue (ParamIDs::drive)->load() * 0.01f,
                  proc.apvts.getRawParameterValue (ParamIDs::character)->load(),
                  last.heat);
}
