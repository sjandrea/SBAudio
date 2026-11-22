#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour fossCol { 0xff5fc8ff };   // cold water blue – the applied (total) curve
    const Colour goldCol { 0xffe8c35a };   // learned / static curve
    constexpr float minF = 20.0f, maxF = 20000.0f;
    constexpr float gainRange = 12.0f;          // ± dB shown for curves
    constexpr float specMin = -96.0f, specMax = -6.0f;

    String fmtFreq (float f)
    {
        return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (f)) + " Hz";
    }

    String utf8 (const char* s) { return String (CharPointer_UTF8 (s)); }
}

namespace fossui
{
    //==========================================================================
    Display::Display (FossProcessor& p) : proc (p)
    {
        setOpaque (true);
        inDb.fill (-120.0f);
        outDb.fill (-120.0f);
    }

    Rectangle<float> Display::plot() const
    {
        return getLocalBounds().toFloat().reduced (1.0f).withTrimmedLeft (34.0f).withTrimmedRight (34.0f)
                                         .withTrimmedTop (10.0f).withTrimmedBottom (20.0f);
    }

    float Display::freqToX (float f) const
    {
        const auto p = plot();
        return p.getX() + p.getWidth() * std::log (jlimit (minF, maxF, f) / minF) / std::log (maxF / minF);
    }

    float Display::xToFreq (float x) const
    {
        const auto p = plot();
        return minF * std::pow (maxF / minF, jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
    }

    float Display::gainToY (float db) const
    {
        const auto p = plot();
        return p.getCentreY() - jlimit (-gainRange * 1.1f, gainRange * 1.1f, db) / gainRange * p.getHeight() * 0.5f;
    }

    float Display::specToY (float db) const
    {
        const auto p = plot();
        return p.getBottom() - jlimit (0.0f, 1.0f, (db - specMin) / (specMax - specMin)) * p.getHeight();
    }

    void Display::update()
    {
        const int serial = proc.shared.displaySerial.load (std::memory_order_acquire);
        if (serial != lastSerial)
        {
            lastSerial = serial;
            framesWithoutAudio = 0;
            for (int b = 0; b < foss::numBands; ++b)
            {
                inDb[(size_t) b] = proc.shared.inDb[(size_t) b].load (std::memory_order_relaxed);
                outDb[(size_t) b] = proc.shared.outDb[(size_t) b].load (std::memory_order_relaxed);
                gainDb[(size_t) b] = proc.shared.gainDb[(size_t) b].load (std::memory_order_relaxed);
                staticDb[(size_t) b] = proc.shared.staticDb[(size_t) b].load (std::memory_order_relaxed);
            }
        }
        else if (++framesWithoutAudio > 15)   // host stopped: let the spectrum fall
        {
            for (auto* a : { &inDb, &outDb })
                for (auto& v : *a) v = jmax (-120.0f, v - 2.0f);
        }

        lowHz = proc.apvts.getRawParameterValue (ParamIDs::lowFreq)->load();
        highHz = proc.apvts.getRawParameterValue (ParamIDs::highFreq)->load();
        delta = proc.apvts.getRawParameterValue (ParamIDs::delta)->load() > 0.5f;
        learning = proc.shared.learning.load();
        hasLearned = proc.shared.hasLearned.load();
        learnProgress = proc.shared.learnProgress.load();
        repaint();
    }

    Path Display::curvePath (const std::array<float, foss::numBands>& v, bool gain) const
    {
        // Catmull-Rom through the band values, sampled every 2 px
        const auto p = plot();
        Path path;
        auto valueAt = [&] (float pos)
        {
            const int i = jlimit (0, foss::numBands - 1, (int) std::floor (pos));
            const float t = pos - (float) i;
            auto at = [&] (int k) { return v[(size_t) jlimit (0, foss::numBands - 1, k)]; };
            const float p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t
                           + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t);
        };
        bool first = true;
        for (float x = p.getX(); x <= p.getRight() + 0.5f; x += 2.0f)
        {
            const float pos = foss::freqToBandPos (xToFreq (x));
            const float val = valueAt (jlimit (0.0f, (float) foss::numBands - 1.0f, pos));
            const float y = gain ? gainToY (val) : specToY (val);
            if (first) { path.startNewSubPath (x, y); first = false; }
            else path.lineTo (x, y);
        }
        return path;
    }

    void Display::paint (Graphics& g)
    {
        const auto full = getLocalBounds().toFloat();
        g.fillAll (col::bg);
        g.setColour (col::well);
        g.fillRoundedRectangle (full, 6.0f);

        const auto p = plot();
        g.setFont (uiFont (9.5f));

        // frequency grid
        const float lines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900,
                                1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
        for (float f : lines)
        {
            const int fi = roundToInt (f);
            const bool major = fi == 100 || fi == 1000 || fi == 10000;
            g.setColour (col::panelEdge.withAlpha (major ? 0.9f : 0.35f));
            g.drawVerticalLine (roundToInt (freqToX (f)), p.getY(), p.getBottom());
        }
        const float labels[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        g.setColour (col::textDim);
        for (float f : labels)
        {
            const String t = f >= 1000 ? String (roundToInt (f / 1000)) + "k" : String (roundToInt (f));
            g.drawText (t, Rectangle<float> (freqToX (f) - 20.0f, p.getBottom() + 3.0f, 40.0f, 14.0f), Justification::centred);
        }

        // gain grid (right axis) and spectrum scale (left axis)
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = gainToY ((float) db);
            g.setColour (db == 0 ? col::panelEdge.brighter (0.3f) : col::panelEdge.withAlpha (0.6f));
            g.drawHorizontalLine (roundToInt (y), p.getX(), p.getRight());
            g.setColour (col::textDim);
            g.drawText ((db > 0 ? "+" : "") + String (db), Rectangle<float> (p.getRight() + 4.0f, y - 7.0f, 28.0f, 14.0f), Justification::centredLeft);
        }
        g.setColour (col::textDim.withAlpha (0.6f));
        for (int db = -90; db <= -10; db += 20)
            g.drawText (String (db), Rectangle<float> (p.getX() - 32.0f, specToY ((float) db) - 7.0f, 28.0f, 14.0f), Justification::centredRight);

        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (p.toNearestInt());

        // spectra
        {
            auto in = curvePath (inDb, false);
            Path fill (in);
            fill.lineTo (p.getRight(), p.getBottom());
            fill.lineTo (p.getX(), p.getBottom());
            fill.closeSubPath();
            g.setColour (col::textDim.withAlpha (0.16f));
            g.fillPath (fill);
            g.setColour (col::textDim.withAlpha (0.45f));
            g.strokePath (in, PathStrokeType (1.0f));

            auto out = curvePath (outDb, false);
            g.setColour (col::text.withAlpha (0.55f));
            g.strokePath (out, PathStrokeType (1.2f));
        }

        // outside the frequency range
        g.setColour (Colours::black.withAlpha (0.35f));
        const float xl = freqToX (lowHz), xh = freqToX (highHz);
        if (lowHz > minF + 0.5f) g.fillRect (Rectangle<float> (p.getX(), p.getY(), xl - p.getX(), p.getHeight()));
        if (highHz < maxF - 1.0f) g.fillRect (Rectangle<float> (xh, p.getY(), p.getRight() - xh, p.getHeight()));

        // static curve (learned correction + brighten + bass)
        {
            auto st = curvePath (staticDb, true);
            Path dashed;
            const float dash[] = { 5.0f, 4.0f };
            PathStrokeType (1.6f).createDashedStroke (dashed, st, dash, 2);
            g.setColour (goldCol.withAlpha (0.9f));
            g.fillPath (dashed);
        }

        // total applied curve
        {
            auto tot = curvePath (gainDb, true);
            Path fill (tot);
            const float y0 = gainToY (0.0f);
            fill.lineTo (p.getRight(), y0);
            fill.lineTo (p.getX(), y0);
            fill.closeSubPath();
            g.setColour (fossCol.withAlpha (0.18f));
            g.fillPath (fill);
            g.setColour (fossCol);
            g.strokePath (tot, PathStrokeType (2.0f));
        }

        // range handles
        auto handle = [&] (int h, float x, const String& txt)
        {
            const bool active = hover == h || dragging == h;
            g.setColour (active ? col::text : col::textDim.withAlpha (0.8f));
            g.drawVerticalLine (roundToInt (x), p.getY(), p.getBottom());
            auto tab = Rectangle<float> (0.0f, 0.0f, 54.0f, 16.0f).withCentre ({ x, p.getY() + 10.0f });
            tab = tab.withX (jlimit (p.getX(), p.getRight() - tab.getWidth(), tab.getX()));
            g.setColour (active ? col::panelEdge.brighter (0.4f) : col::panel);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (active ? col::text : col::textDim);
            g.drawText (txt, tab, Justification::centred);
        };
        handle (0, xl, fmtFreq (lowHz));
        handle (1, xh, fmtFreq (highHz));

        // legend
        auto legend = p.reduced (10.0f, 8.0f).removeFromBottom (14.0f).removeFromRight (360.0f);
        auto item = [&] (const String& txt, Colour c, bool dashedLine)
        {
            auto r = legend.removeFromRight ((float) txt.length() * 5.4f + 30.0f);
            const float y = r.getCentreY();
            g.setColour (c);
            if (dashedLine) { g.drawLine (r.getX(), y, r.getX() + 7.0f, y, 1.6f); g.drawLine (r.getX() + 11.0f, y, r.getX() + 18.0f, y, 1.6f); }
            else g.drawLine (r.getX(), y, r.getX() + 18.0f, y, 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (10.0f));
            g.drawText (txt, r.withTrimmedLeft (23.0f), Justification::centredLeft);
        };
        item ("Spektrum ut", col::text.withAlpha (0.55f), false);
        item ("Statisk", goldCol, true);
        item (delta ? utf8 ("Total (Δ)") : String ("Total"), fossCol, false);

        // learning overlay
        if (learning)
        {
            auto box = Rectangle<float> (300.0f, 46.0f).withCentre ({ p.getCentreX(), p.getY() + 40.0f });
            g.setColour (col::panel.withAlpha (0.92f));
            g.fillRoundedRectangle (box, 6.0f);
            g.setColour (goldCol);
            g.drawRoundedRectangle (box, 6.0f, 1.0f);
            g.setFont (uiFont (12.0f, true));
            g.drawText (utf8 ("LYTTER …  ") + String (roundToInt (learnProgress * 100.0f)) + " %",
                        box.withTrimmedTop (6.0f).removeFromTop (18.0f), Justification::centred);
            auto bar = box.reduced (16.0f, 0.0f).withTrimmedTop (28.0f).withHeight (5.0f);
            g.setColour (col::well);
            g.fillRoundedRectangle (bar, 2.5f);
            g.setColour (goldCol);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * learnProgress), 2.5f);
        }
    }

    int Display::hitHandle (float x) const
    {
        const float dl = std::abs (x - freqToX (lowHz)), dh = std::abs (x - freqToX (highHz));
        if (jmin (dl, dh) > 8.0f) return -1;
        return dl <= dh ? 0 : 1;
    }

    RangedAudioParameter* Display::handleParam (int h) const
    {
        return proc.apvts.getParameter (h == 0 ? ParamIDs::lowFreq : ParamIDs::highFreq);
    }

    void Display::mouseMove (const MouseEvent& e)
    {
        const int h = plot().contains (e.position) ? hitHandle (e.position.x) : -1;
        if (h != hover)
        {
            hover = h;
            setMouseCursor (h >= 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
            repaint();
        }
    }

    void Display::mouseExit (const MouseEvent&)
    {
        hover = -1;
        setMouseCursor (MouseCursor::NormalCursor);
        repaint();
    }

    void Display::mouseDown (const MouseEvent& e)
    {
        dragging = hitHandle (e.position.x);
        if (dragging >= 0)
            handleParam (dragging)->beginChangeGesture();
    }

    void Display::mouseDrag (const MouseEvent& e)
    {
        if (dragging < 0) return;
        auto* prm = handleParam (dragging);
        float f = xToFreq (e.position.x);
        f = dragging == 0 ? jmin (f, highHz / 1.5f) : jmax (f, lowHz * 1.5f);
        prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (f)));
    }

    void Display::mouseUp (const MouseEvent&)
    {
        if (dragging >= 0)
            handleParam (dragging)->endChangeGesture();
        dragging = -1;
    }

    void Display::mouseDoubleClick (const MouseEvent& e)
    {
        const int h = hitHandle (e.position.x);
        if (h < 0) return;
        auto* prm = handleParam (h);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getDefaultValue());
        prm->endChangeGesture();
    }
}

//==============================================================================
FossEditor::FossEditor (FossProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets), display (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);
    content.addAndMakeVisible (display);
    display.setTooltip (utf8 ("Dra de loddrette strekene for å begrense hvilket frekvensområde Foss jobber i. Dobbeltklikk for å nullstille."));

    // --- Learn / profile ---
    profileBox.addItemList (foss::profileNames, 1);
    profileBox.setTooltip (utf8 ("Målkurven LEARN sammenligner sporet med. Velg den som passer kilden."));
    content.addAndMakeVisible (profileBox);
    profileAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::profile, profileBox);

    learnBtn.setColour (TextButton::buttonOnColourId, goldCol);
    learnBtn.setTooltip (utf8 ("Spill av et typisk parti av sporet og trykk LEARN. Foss lytter i 10 sekunder og lager en "
                               "statisk korreksjonskurve mot profilen. Trykk igjen for å stoppe tidligere (minst 2 s)."));
    learnBtn.onClick = [this]
    {
        if (proc.shared.learning.load()) proc.finishLearning();
        else proc.startLearning();
    };
    content.addAndMakeVisible (learnBtn);

    clearBtn.setTooltip (utf8 ("Fjerner den lærte kurven."));
    clearBtn.onClick = [this] { proc.clearLearned(); };
    content.addAndMakeVisible (clearBtn);

    learnStatus.setFont (uiFont (10.5f));
    learnStatus.setColour (Label::textColourId, col::textDim);
    learnStatus.setJustificationType (Justification::topLeft);
    learnStatus.setMinimumHorizontalScale (1.0f);
    content.addAndMakeVisible (learnStatus);

    // --- knobs ---
    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &correction, ParamIDs::correction }, { &recover, ParamIDs::recover }, { &tame, ParamIDs::tame },
        { &bias, ParamIDs::bias }, { &brighten, ParamIDs::brighten }, { &bass, ParamIDs::bass },
        { &mix, ParamIDs::mix }, { &output, ParamIDs::output } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, fossCol);
    }
    correction.slider.setColour (Slider::rotarySliderFillColourId, goldCol);
    for (auto* k : { &bias, &brighten, &bass, &output })
        k->slider.getProperties().set ("bipolar", true);

    correction.slider.setTooltip (utf8 ("Hvor mye av den lærte korreksjonskurven som brukes. 100 % = helt mot profilen."));
    recover.slider.setTooltip (utf8 ("Løfter frekvenser som er maskert eller underrepresentert, så detaljer kommer fram. Løfter aldri støy eller stillhet."));
    tame.slider.setTooltip (utf8 ("Demper frekvenser som stikker ut akkurat nå: resonanser, harde toner og dominerende områder."));
    bias.slider.setTooltip (utf8 ("Balanse mellom Recover og Tame. − = mer temming, + = mer løft."));
    brighten.slider.setTooltip (utf8 ("Vipper hele balansen lysere (+) eller mørkere (−), rundt 1 kHz."));
    bass.slider.setTooltip (utf8 ("Mer (+) eller mindre (−) bunn under ca. 120 Hz."));
    mix.slider.setTooltip (utf8 ("Blanding av behandlet og ubehandlet signal (fasekorrekt)."));
    output.slider.setTooltip (utf8 ("Utgangsnivå."));

    // --- toggles ---
    for (auto [b, id] : { std::pair<TextButton*, const char*> { &autoGainBtn, ParamIDs::autoGain }, { &deltaBtn, ParamIDs::delta } })
    {
        b->setClickingTogglesState (true);
        b->setColour (TextButton::buttonOnColourId, fossCol);
        content.addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
    }
    autoGainBtn.setTooltip (utf8 ("Holder opplevd lydstyrke lik, så du hører forskjellen i balanse – ikke i nivå."));
    deltaBtn.setTooltip (utf8 ("Lytt bare på det Foss endrer (forskjellen mellom ut og inn)."));

    autoGainReadout.setFont (uiFont (10.0f));
    autoGainReadout.setColour (Label::textColourId, col::textDim);
    autoGainReadout.setJustificationType (Justification::centred);
    content.addAndMakeVisible (autoGainReadout);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    updateLearnUi();
    startTimerHz (30);
}

FossEditor::~FossEditor()
{
    setLookAndFeel (nullptr);
}

void FossEditor::paint (Graphics& g) { g.fillAll (col::bg); }

void FossEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void FossEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    display.setBounds (12, 56, designW - 24, 262);

    const int top = 330, h = designH - top - 12;
    learnPanel = { 12, top, 300, h };
    adaptPanel = { learnPanel.getRight() + 10, top, 300, h };
    tonePanel  = { adaptPanel.getRight() + 10, top, designW - 12 - adaptPanel.getRight() - 10, h };

    // learn panel: left column controls, correction knob right
    {
        auto r = learnPanel.reduced (14, 12);
        r.removeFromTop (20);
        auto knobCol = r.removeFromRight (104);
        correction.setBounds (knobCol.withSizeKeepingCentre (104, 120));
        r.removeFromRight (10);
        profileBox.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        auto btns = r.removeFromTop (34);
        clearBtn.setBounds (btns.removeFromRight (74));
        btns.removeFromRight (6);
        learnBtn.setBounds (btns);
        r.removeFromTop (8);
        learnStatus.setBounds (r);
    }
    // adaptive panel: three knobs
    {
        auto r = adaptPanel.reduced (12, 12);
        r.removeFromTop (20);
        const int w = r.getWidth() / 3;
        for (auto* k : { &recover, &tame, &bias })
            k->setBounds (r.removeFromLeft (w).withSizeKeepingCentre (w, 130));
    }
    // tone & output
    {
        auto r = tonePanel.reduced (12, 12);
        r.removeFromTop (20);
        auto toggles = r.removeFromBottom (26);
        r.removeFromBottom (6);
        const int w = r.getWidth() / 4;
        for (auto* k : { &brighten, &bass, &mix, &output })
            k->setBounds (r.removeFromLeft (w).withSizeKeepingCentre (w, jmin (r.getHeight(), 112)));
        const int tw = (toggles.getWidth() - 12) / 3;
        autoGainBtn.setBounds (toggles.removeFromLeft (tw));
        toggles.removeFromLeft (6);
        autoGainReadout.setBounds (toggles.removeFromLeft (tw));
        toggles.removeFromLeft (6);
        deltaBtn.setBounds (toggles);
    }
}

void FossEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "Foss", "SMART EQ", fossCol);

    auto panel = [&] (Rectangle<int> r, const String& title, const String& sub, Colour accent)
    {
        auto f = r.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (f, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.0f);
        auto head = f.reduced (14.0f, 10.0f).removeFromTop (16.0f);
        g.setColour (accent);
        g.setFont (uiFont (10.5f, true));
        g.drawText (title, head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f));
        g.drawText (sub, head, Justification::centredRight);
    };
    panel (learnPanel, "LEARN  /  PROFIL", "statisk", goldCol);
    panel (adaptPanel, "ADAPTIV", "sanntid", fossCol);
    panel (tonePanel, "TONE  /  UT", "", col::text);
}

void FossEditor::updateLearnUi()
{
    const bool learning = proc.shared.learning.load();
    const bool has = proc.shared.hasLearned.load();
    const int pct = roundToInt (proc.shared.learnProgress.load() * 100.0f);

    if (learning == lastLearning && has == lastHasLearned && (! learning || pct == lastProgressPct) && lastProgressPct >= 0)
        return;
    lastLearning = learning;
    lastHasLearned = has;
    lastProgressPct = pct;

    learnBtn.setToggleState (learning, dontSendNotification);
    learnBtn.setButtonText (learning ? ((float) pct * foss::learnSeconds / 100.0f < foss::minLearnSeconds ? utf8 ("AVBRYT") : utf8 ("STOPP"))
                                     : (has ? utf8 ("LEARN PÅ NYTT") : String ("LEARN")));
    clearBtn.setEnabled (has && ! learning);
    correction.setEnabled (has);
    correction.setAlpha (has ? 1.0f : 0.45f);

    if (learning)
        learnStatus.setText (utf8 ("Lytter … ") + String (pct) + utf8 (" %. Spill av et typisk parti – bare lyd teller, pauser hoppes over."),
                             dontSendNotification);
    else if (has)
        learnStatus.setText (utf8 ("Kurve lært. Den gule kurven korrigerer sporet mot profilen – bytt profil når som helst."),
                             dontSendNotification);
    else
        learnStatus.setText (utf8 ("Ingen kurve lært ennå. Start avspilling og trykk LEARN for en statisk korreksjon. "
                                   "Det adaptive panelet virker uten learn."),
                             dontSendNotification);
}

void FossEditor::timerCallback()
{
    proc.syncLearnedToState();
    display.update();
    updateLearnUi();

    const bool ag = proc.apvts.getRawParameterValue (ParamIDs::autoGain)->load() > 0.5f;
    const float agDb = proc.shared.autoGainDb.load();
    autoGainReadout.setText (ag ? (agDb > 0.05f ? "+" : "") + String (agDb, 1) + " dB" : String ("–"), dontSendNotification);
}
