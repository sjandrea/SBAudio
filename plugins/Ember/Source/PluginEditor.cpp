#include "PluginEditor.h"

using namespace juce;
using namespace ember;
using namespace emberui;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    constexpr float specMin = -96.0f, specMax = -6.0f;
    const Colour emberCol { 0xffff7a3d };

    String hzLabel (float f)
    {
        return f < 1000.0f ? String (roundToInt (f)) + " Hz" : String (f / 1000.0f, f < 10000.0f ? 2 : 1) + " kHz";
    }

    void fillStyleBox (ComboBox& box)
    {
        for (const auto& g : styleGroups)
        {
            box.addSectionHeading (g.name);
            for (int i = g.first; i < g.first + g.count; ++i)
                box.addItem (styleNames[i], i + 1);
        }
    }

    PopupMenu styleMenu (int current, int idOffset)
    {
        PopupMenu m;
        for (const auto& g : styleGroups)
        {
            m.addSectionHeader (g.name);
            for (int i = g.first; i < g.first + g.count; ++i)
                m.addItem (idOffset + i, styleNames[i], true, i == current);
        }
        return m;
    }
}

Colour emberui::bandColour (int band)
{
    static const Colour c[] = { Colour (0xffff7a3d), Colour (0xffffc23d), Colour (0xff6fd06a),
                                Colour (0xff3fd0c9), Colour (0xff6f9dff), Colour (0xffc98bff) };
    return c[(size_t) jlimit (0, maxBands - 1, band)];
}

//==============================================================================
BandDisplay::BandDisplay (EmberProcessor& p) : proc (p)
{
    pointFreq.resize ((size_t) numPoints);
    for (int i = 0; i < numPoints; ++i)
        pointFreq[(size_t) i] = minFreq * std::pow (maxFreq / minFreq, (float) i / (float) (numPoints - 1));
    specPre.assign ((size_t) numPoints, specMin - 10.0f);
    specPost.assign ((size_t) numPoints, specMin - 10.0f);
    setTooltip (CharPointer_UTF8 ("Dra opp/ned i et bånd = Drive (Shift = fin) · scrollhjul = Drive\n"
                                  "Dra en delelinje = delefrekvens · dobbeltklikk = nytt bånd her\n"
                                  "Dobbeltklikk på en delelinje = slå sammen bånd · høyreklikk = meny"));
    update();
}

void BandDisplay::resized() {}

Rectangle<float> BandDisplay::plot() const { return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (18.0f); }

float BandDisplay::freqToX (float f) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (jmax (minFreq, f) / minFreq) / std::log (maxFreq / minFreq);
}

float BandDisplay::xToFreq (float x) const
{
    const auto p = plot();
    return minFreq * std::pow (maxFreq / minFreq, jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

float BandDisplay::driveToY (float d) const
{
    const auto p = plot();
    return p.getBottom() - 16.0f - jlimit (0.0f, 1.0f, d) * (p.getHeight() - 72.0f);
}

float BandDisplay::specToY (float db) const
{
    const auto p = plot();
    return p.getBottom() - p.getHeight() * jlimit (0.0f, 1.0f, (db - specMin) / (specMax - specMin));
}

float BandDisplay::bandLo (int b) const { return b <= 0 ? minFreq : splits[(size_t) b - 1]; }
float BandDisplay::bandHi (int b) const { return b >= numBands - 1 ? maxFreq : splits[(size_t) b]; }

int BandDisplay::bandAt (float x) const
{
    const float f = xToFreq (x);
    for (int b = 0; b < numBands - 1; ++b)
        if (f < splits[(size_t) b])
            return b;
    return numBands - 1;
}

int BandDisplay::splitAt (float x) const
{
    int best = -1;
    float bestD = 6.0f;
    for (int i = 0; i < numBands - 1; ++i)
    {
        const float d = std::abs (freqToX (splits[(size_t) i]) - x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void BandDisplay::setSelected (int b)
{
    b = jlimit (0, numBands - 1, b);
    if (b == selected)
        return;
    selected = b;
    if (onSelectionChanged)
        onSelectionChanged (b);
    repaint();
}

void BandDisplay::update()
{
    numBands = proc.getNumBands();
    splits = proc.getSplits();
    for (int b = 0; b < maxBands; ++b)
    {
        auto& v = view[(size_t) b];
        v.style = (int) proc.bandValue (b, "style");
        v.drive = proc.bandValue (b, "drive") * 0.01f;
        v.on    = proc.bandValue (b, "on") > 0.5f;
        v.mute  = proc.bandValue (b, "mute") > 0.5f;
    }
    if (selected >= numBands)
        setSelected (numBands - 1);
    computeSpectrum();
    repaint();
}

void BandDisplay::computeSpectrum()
{
    const float binHz = (float) (proc.getSampleRateSafe() / fftSize);
    const float norm = 4.0f / (float) fftSize;

    auto analyse = [&] (const sbaudio::AnalyzerRing& ring, std::vector<float>& out)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        ring.read (fftData.data(), fftSize);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int i = 0; i < numPoints; ++i)
        {
            const float f = pointFreq[(size_t) i];
            const float fLo = i > 0 ? std::sqrt (f * pointFreq[(size_t) i - 1]) : f;
            const float fHi = i < numPoints - 1 ? std::sqrt (f * pointFreq[(size_t) i + 1]) : f;
            const int b0 = jlimit (1, fftSize / 2 - 1, (int) std::floor (fLo / binHz));
            const int b1 = jlimit (1, fftSize / 2 - 1, (int) std::ceil (fHi / binHz));
            float mag = 0.0f;
            if (b1 - b0 <= 1)
            {
                const float pos = jlimit (1.0f, (float) (fftSize / 2 - 2), f / binHz);
                const int k = (int) pos;
                const float t = pos - (float) k;
                mag = fftData[(size_t) k] * (1.0f - t) + fftData[(size_t) k + 1] * t;
            }
            else
                for (int k = b0; k <= b1; ++k)
                    mag = jmax (mag, fftData[(size_t) k]);

            float db = Decibels::gainToDecibels (mag * norm, -140.0f) + 4.5f * std::log2 (f / 1000.0f);
            db = jlimit (specMin - 10.0f, specMax + 10.0f, db);
            auto& o = out[(size_t) i];
            o = db > o ? o + (db - o) * 0.6f : o + (db - o) * 0.12f;
        }
    };
    analyse (proc.preRing, specPre);
    analyse (proc.postRing, specPost);
}

void BandDisplay::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const auto p = plot();

    g.setColour (col::well);
    g.fillRoundedRectangle (full, 6.0f);

    // grid
    g.setFont (uiFont (10.0f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = freqToX (f);
        const bool major = f == 100.0f || f == 1000.0f || f == 10000.0f;
        g.setColour (Colours::white.withAlpha (major ? 0.08f : 0.04f));
        g.drawVerticalLine (roundToInt (x), p.getY(), p.getBottom());
        if (f < 20000.0f)
        {
            g.setColour (col::textDim);
            const String t = f < 1000.0f ? String ((int) f) : String ((int) f / 1000) + "k";
            g.drawText (t, Rectangle<float> (x - 20.0f, p.getBottom() + 2.0f, 40.0f, 14.0f), Justification::centred);
        }
    }

    // band regions
    for (int b = 0; b < numBands; ++b)
    {
        const auto& v = view[(size_t) b];
        const float x0 = freqToX (bandLo (b)), x1 = freqToX (bandHi (b));
        const auto r = Rectangle<float> (x0, p.getY(), x1 - x0, p.getHeight());
        const auto c = bandColour (b);
        const bool sel = b == selected;
        const bool soloed = (proc.soloMask.load() >> b) & 1;
        const bool active = v.on && ! v.mute;

        g.setGradientFill (ColourGradient (c.withAlpha (active ? (sel ? 0.22f : 0.10f) : 0.03f), 0.0f, p.getBottom(),
                                           c.withAlpha (sel ? 0.05f : 0.0f), 0.0f, p.getY(), false));
        g.fillRect (r);
        if (b == hoverBand && drag == Drag::none)
        {
            g.setColour (Colours::white.withAlpha (0.025f));
            g.fillRect (r);
        }
        if (v.mute)
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (r.toNearestInt());
            g.setColour (Colours::white.withAlpha (0.04f));
            for (float sx = r.getX() - r.getHeight(); sx < r.getRight(); sx += 12.0f)
                g.drawLine (sx, r.getBottom(), sx + r.getHeight(), r.getY(), 1.0f);
        }

        // label
        auto lab = r.reduced (6.0f, 6.0f).removeFromTop (34.0f);
        g.setColour (active ? c : col::textDim);
        g.setFont (uiFont (r.getWidth() > 90.0f ? 12.5f : 10.5f, true));
        g.drawText (String (b + 1) + "  " + (v.on ? styleNames[v.style] : String ("Bypass")), lab.removeFromTop (16.0f),
                    Justification::centredLeft, true);
        g.setFont (uiFont (10.5f));
        g.setColour (col::textDim);
        String sub = v.mute ? String ("Mute") : v.on ? String (roundToInt (v.drive * 100.0f)) + " %" : String();
        if (soloed) sub += "  SOLO";
        g.drawText (sub, lab, Justification::centredLeft, true);

        // drive line and handle
        if (v.on)
        {
            const float y = driveToY (v.drive);
            g.setColour (c.withAlpha (active ? 0.55f : 0.25f));
            g.drawHorizontalLine (roundToInt (y), x0 + 4.0f, x1 - 4.0f);
            const auto h = Rectangle<float> (12.0f, 12.0f).withCentre ({ (x0 + x1) * 0.5f, y });
            g.setColour (active ? c : c.withAlpha (0.4f));
            g.fillEllipse (h);
            if (sel)
            {
                g.setColour (Colours::white.withAlpha (0.8f));
                g.drawEllipse (h.expanded (2.0f), 1.5f);
            }
        }
    }

    // spectrum
    auto spectrumPath = [&] (const std::vector<float>& s, bool close)
    {
        Path path;
        for (int i = 0; i < numPoints; ++i)
        {
            const float x = freqToX (pointFreq[(size_t) i]), y = specToY (s[(size_t) i]);
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        if (close)
        {
            path.lineTo (p.getRight(), p.getBottom());
            path.lineTo (p.getX(), p.getBottom());
            path.closeSubPath();
        }
        return path;
    };
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p.toNearestInt());
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillPath (spectrumPath (specPre, true));
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (spectrumPath (specPre, false), PathStrokeType (1.0f));
        g.setColour (emberCol.withAlpha (0.12f));
        g.fillPath (spectrumPath (specPost, true));
        g.setColour (emberCol.withAlpha (0.75f));
        g.strokePath (spectrumPath (specPost, false), PathStrokeType (1.4f));
    }

    // crossovers
    for (int i = 0; i < numBands - 1; ++i)
    {
        const float x = freqToX (splits[(size_t) i]);
        const bool hot = i == hoverSplit || (drag == Drag::split && dragIndex == i);
        g.setColour (Colours::white.withAlpha (hot ? 0.85f : 0.35f));
        g.drawLine (x, p.getY(), x, p.getBottom(), hot ? 2.0f : 1.2f);
        auto grip = Rectangle<float> (8.0f, 26.0f).withCentre ({ x, p.getCentreY() });
        g.setColour (hot ? Colours::white : col::panelEdge.brighter (0.6f));
        g.fillRoundedRectangle (grip, 3.0f);
        if (hot)
        {
            auto t = Rectangle<float> (70.0f, 18.0f).withCentre ({ x, p.getBottom() - 14.0f });
            g.setColour (col::panel);
            g.fillRoundedRectangle (t, 4.0f);
            g.setColour (col::text);
            g.setFont (uiFont (11.0f, true));
            g.drawText (hzLabel (splits[(size_t) i]), t, Justification::centred);
        }
    }

    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (full.reduced (0.5f), 6.0f, 1.0f);
}

void BandDisplay::mouseMove (const MouseEvent& e)
{
    const int s = splitAt (e.position.x);
    const int b = s < 0 ? bandAt (e.position.x) : -1;
    if (s != hoverSplit || b != hoverBand)
    {
        hoverSplit = s;
        hoverBand = b;
        repaint();
    }
    setMouseCursor (s >= 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::UpDownResizeCursor);
}

void BandDisplay::mouseExit (const MouseEvent&)
{
    hoverSplit = hoverBand = -1;
    repaint();
}

void BandDisplay::mouseDown (const MouseEvent& e)
{
    drag = Drag::none;
    const int s = splitAt (e.position.x);

    if (s >= 0 && ! e.mods.isPopupMenu())
    {
        drag = Drag::split;
        dragIndex = s;
        dragParam = proc.param (ParamIDs::split (s));
        dragParam->beginChangeGesture();
        return;
    }

    const int b = bandAt (e.position.x);
    setSelected (b);
    if (e.mods.isPopupMenu())
    {
        showBandMenu (b);
        return;
    }
    drag = Drag::drive;
    dragIndex = b;
    dragParam = proc.bandParam (b, "drive");
    dragStartDrive = proc.bandValue (b, "drive");
    dragParam->beginChangeGesture();
}

void BandDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragParam == nullptr)
        return;

    if (drag == Drag::split)
    {
        // keep at least ~1/4 octave to the neighbours
        const float lo = (dragIndex == 0 ? minFreq : splits[(size_t) dragIndex - 1]) * 1.2f;
        const float hi = (dragIndex == numBands - 2 ? maxFreq : splits[(size_t) dragIndex + 1]) / 1.2f;
        const float f = jlimit (lo, jmax (lo, hi), xToFreq (e.position.x));
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (f));
        splits[(size_t) dragIndex] = f;
    }
    else if (drag == Drag::drive)
    {
        const float dy = (float) e.getDistanceFromDragStartY();
        const float scale = e.mods.isShiftDown() ? 0.05f : 0.4f;
        const float d = jlimit (0.0f, 100.0f, dragStartDrive - dy * scale);
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (d));
        view[(size_t) dragIndex].drive = d * 0.01f;
    }
    repaint();
}

void BandDisplay::mouseUp (const MouseEvent&)
{
    if (dragParam != nullptr)
        dragParam->endChangeGesture();
    dragParam = nullptr;
    drag = Drag::none;
    repaint();
}

void BandDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int s = splitAt (e.position.x);
    if (s >= 0)
    {
        proc.deleteBand (s + 1); // merge the band above into the one below
        update();
        setSelected (s);
        if (onSelectionChanged) onSelectionChanged (selected);
        return;
    }
    const int nb = proc.insertSplit (xToFreq (e.position.x));
    if (nb >= 0)
    {
        update();
        setSelected (nb);
        if (onSelectionChanged) onSelectionChanged (selected);
    }
}

void BandDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int b = bandAt (e.position.x);
    auto* prm = proc.bandParam (b, "drive");
    const float d = jlimit (0.0f, 100.0f, proc.bandValue (b, "drive") + w.deltaY * (e.mods.isShiftDown() ? 5.0f : 25.0f));
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (d));
    prm->endChangeGesture();
    update();
}

void BandDisplay::showBandMenu (int band)
{
    const auto& v = view[(size_t) band];
    PopupMenu m;
    m.addSectionHeader (String (CharPointer_UTF8 ("BÅND ")) + String (band + 1));
    m.addSubMenu ("Type", styleMenu (v.style, 100));
    m.addSeparator();
    m.addItem (1, CharPointer_UTF8 ("På"), true, v.on);
    m.addItem (2, "Mute", true, v.mute);
    m.addItem (3, "Solo", true, (proc.soloMask.load() >> band) & 1);
    m.addSeparator();
    m.addItem (4, CharPointer_UTF8 ("Del båndet i to"), numBands < maxBands);
    m.addItem (5, CharPointer_UTF8 ("Slett båndet"), numBands > 1);

    SafePointer<BandDisplay> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& p = safe->proc;
        if (r >= 100)
            p.setPlain (ParamIDs::band (band, "style"), (float) (r - 100));
        else if (r == 1)
            p.setPlain (ParamIDs::band (band, "on"), p.bandValue (band, "on") > 0.5f ? 0.0f : 1.0f);
        else if (r == 2)
            p.setPlain (ParamIDs::band (band, "mute"), p.bandValue (band, "mute") > 0.5f ? 0.0f : 1.0f);
        else if (r == 3)
            p.soloMask = p.soloMask.load() ^ (1 << band);
        else if (r == 4)
        {
            const float f = std::sqrt (safe->bandLo (band) * safe->bandHi (band));
            const int nb = p.insertSplit (f);
            safe->update();
            if (nb >= 0) safe->setSelected (nb);
        }
        else if (r == 5)
        {
            p.deleteBand (band);
            safe->update();
            safe->setSelected (jmax (0, band - 1));
            if (safe->onSelectionChanged) safe->onSelectionChanged (safe->selected);
        }
        safe->update();
    });
}

//==============================================================================
BandPanel::BandPanel (EmberProcessor& p) : proc (p)
{
    fillStyleBox (styleBox);
    styleBox.setTooltip (CharPointer_UTF8 ("Metningstype for båndet. Tube og Tape er varme og myke, Transformer gir tetthet i bunnen,\n"
                                           "Clipping er rett frem, Amp og Effekt er for tydelig forvrengning."));
    addAndMakeVisible (styleBox);

    for (auto* b : { &onBtn, &muteBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    onBtn.setColour (TextButton::buttonOnColourId, emberCol);
    muteBtn.setColour (TextButton::buttonOnColourId, col::red);
    onBtn.setTooltip (CharPointer_UTF8 ("Av = båndet går rent gjennom (bypass)"));
    muteBtn.setTooltip (CharPointer_UTF8 ("Demp båndet helt"));

    soloBtn.setClickingTogglesState (true);
    soloBtn.setColour (TextButton::buttonOnColourId, col::amber);
    soloBtn.setTooltip (CharPointer_UTF8 ("Lytt bare på dette båndet (flere kan være solo samtidig). Lagres ikke."));
    soloBtn.onClick = [this]
    {
        if (band < 0) return;
        const int bit = 1 << band;
        proc.soloMask = soloBtn.getToggleState() ? (proc.soloMask.load() | bit) : (proc.soloMask.load() & ~bit);
    };
    addAndMakeVisible (soloBtn);

    deleteBtn.setColour (TextButton::buttonOnColourId, col::red);
    deleteBtn.setTooltip (CharPointer_UTF8 ("Slett båndet – nabobåndet tar over frekvensområdet"));
    addAndMakeVisible (deleteBtn);

    for (auto* k : { &drive, &dyn, &tone, &mix, &level, &fb, &fbFreq })
        addAndMakeVisible (k);
    dyn.slider.getProperties().set ("bipolar", true);
    tone.slider.getProperties().set ("bipolar", true);
    level.slider.getProperties().set ("bipolar", true);
    for (auto* k : { &fb, &fbFreq })
        k->slider.setColour (Slider::rotarySliderFillColourId, col::violet);

    drive.slider.setTooltip (CharPointer_UTF8 ("Hvor hardt båndet drives inn i metningen. Med Auto Gain på holdes nivået omtrent likt."));
    dyn.slider.setTooltip (CharPointer_UTF8 ("Positiv: gir tilbake dynamikken metningen tar (mer liv og transienter).\n"
                                             "Negativ: komprimerer ekstra (tettere, jevnere)."));
    tone.slider.setTooltip (CharPointer_UTF8 ("Mørkere (−) eller lysere (+) metning, vippet rundt midten av båndet."));
    mix.slider.setTooltip (CharPointer_UTF8 ("Blanding av ren og mettet lyd i dette båndet."));
    level.slider.setTooltip (CharPointer_UTF8 ("Utgangsnivå for båndet – også en enkel EQ."));
    fb.slider.setTooltip (CharPointer_UTF8 ("Tilbakekobling rundt metningen ved FB-frekvensen: resonans og «ring». Høye verdier kan hyle."));
    fbFreq.slider.setTooltip (CharPointer_UTF8 ("Frekvensen tilbakekoblingen resonerer på."));
}

void BandPanel::setBand (int b)
{
    if (b == band)
        return;
    sliderAtts.clear();
    styleAtt.reset();
    buttonAtts.clear();
    band = b;

    auto& s = proc.apvts;
    auto id = [this] (const char* n) { return ParamIDs::band (band, n); };
    const std::pair<sbaudio::Knob*, const char*> knobs[] = { { &drive, "drive" }, { &dyn, "dyn" }, { &tone, "tone" },
                                                             { &mix, "mix" }, { &level, "level" }, { &fb, "fb" },
                                                             { &fbFreq, "fbfreq" } };
    for (auto& [k, name] : knobs)
    {
        sliderAtts.push_back (std::make_unique<SA> (s, id (name), k->slider));
        if (auto* prm = proc.bandParam (band, name))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    styleAtt = std::make_unique<CA> (s, id ("style"), styleBox);
    buttonAtts.push_back (std::make_unique<BA> (s, id ("on"), onBtn));
    buttonAtts.push_back (std::make_unique<BA> (s, id ("mute"), muteBtn));
    deleteBtn.onClick = [this] { proc.deleteBand (band); };

    const auto c = bandColour (band);
    for (auto* k : { &drive, &dyn, &tone, &mix, &level })
        k->slider.setColour (Slider::rotarySliderFillColourId, c);
    refresh();
    repaint();
}

void BandPanel::refresh()
{
    if (band < 0)
        return;
    const int n = proc.getNumBands();
    const auto sp = proc.getSplits();
    const float lo = band == 0 ? 20.0f : sp[(size_t) band - 1];
    const float hi = band >= n - 1 ? 20000.0f : sp[(size_t) band];
    const auto t = hzLabel (lo) + String (CharPointer_UTF8 (" – ")) + hzLabel (hi);
    if (t != rangeText)
    {
        rangeText = t;
        repaint();
    }
    soloBtn.setToggleState (((proc.soloMask.load() >> band) & 1) != 0, dontSendNotification);
    deleteBtn.setEnabled (n > 1);

    const bool on = proc.bandValue (band, "on") > 0.5f;
    for (auto* c : std::initializer_list<Component*> { &styleBox, &drive, &dyn, &tone, &mix, &level, &fb, &fbFreq })
        c->setEnabled (on);
    fbFreq.setAlpha (proc.bandValue (band, "fb") > 0.05f ? 1.0f : 0.55f);
}

void BandPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (band < 0)
        return;

    const auto c = bandColour (band);
    auto badge = Rectangle<float> (14.0f, 14.0f, 26.0f, 26.0f);
    g.setColour (c);
    g.fillEllipse (badge);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.setFont (uiFont (12.0f, true));
    g.drawText (String (band + 1), badge, Justification::centred);
    g.setColour (col::text);
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (CharPointer_UTF8 ("BÅND ")) + String (band + 1), Rectangle<float> (48.0f, 12.0f, 80.0f, 16.0f), Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (uiFont (10.5f));
    g.drawText (rangeText, Rectangle<float> (48.0f, 27.0f, 150.0f, 14.0f), Justification::centredLeft);

    // separator before the feedback section
    const float sx = (float) fb.getX() - 8.0f;
    g.setColour (col::panelEdge);
    g.fillRect (sx, 14.0f, 1.0f, r.getHeight() - 28.0f);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 12);
    auto left = r.removeFromLeft (180);
    left.removeFromTop (38);
    styleBox.setBounds (left.removeFromTop (28));
    left.removeFromTop (10);
    auto row = left.removeFromTop (26);
    onBtn.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
    muteBtn.setBounds (row.reduced (2, 0));
    left.removeFromTop (6);
    row = left.removeFromTop (26);
    soloBtn.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
    deleteBtn.setBounds (row.reduced (2, 0));

    r.removeFromLeft (14);
    const int kw = (r.getWidth() - 16) / 7;
    auto knobRow = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 130));
    for (auto* k : { &drive, &dyn, &tone, &mix, &level })
        k->setBounds (knobRow.removeFromLeft (kw));
    knobRow.removeFromLeft (16);
    fb.setBounds (knobRow.removeFromLeft (kw));
    fbFreq.setBounds (knobRow.removeFromLeft (kw));
}

//==============================================================================
void LevelMeter::setLevels (float in, float l, float r)
{
    auto fall = [] (float& lev, float v) { lev = jmax (Decibels::gainToDecibels (v, -100.0f), lev - 1.5f); };
    fall (levIn, in);
    fall (levL, l);
    fall (levR, r);
    const float pk = jmax (Decibels::gainToDecibels (l, -100.0f), Decibels::gainToDecibels (r, -100.0f));
    if (pk >= peakHold) { peakHold = pk; holdFrames = 45; }
    else if (--holdFrames <= 0) peakHold = jmax (pk, peakHold - 0.5f);
    repaint();
}

void LevelMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromTop (16.0f);
    g.setFont (uiFont (10.5f, true));
    g.setColour (peakHold > 0.0f ? col::red : col::text);
    g.drawText (peakHold <= -99.0f ? String ("-inf") : String (peakHold, 1), text, Justification::centred);
    auto labels = r.removeFromBottom (14.0f);

    auto bar = [&] (Rectangle<float> b, float db, Colour c)
    {
        g.setColour (col::well);
        g.fillRoundedRectangle (b, 2.0f);
        const float t = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = b.withTop (b.getBottom() - b.getHeight() * t);
        g.setGradientFill (ColourGradient (col::red, 0.0f, b.getY(), c, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    };
    r.removeFromTop (2.0f);
    const float w = (r.getWidth() - 10.0f) / 3.0f;
    auto inR = r.removeFromLeft (w);
    bar (inR, levIn, col::textDim);
    r.removeFromLeft (6.0f);
    auto lR = r.removeFromLeft (w);
    bar (lR, levL, emberCol);
    r.removeFromLeft (4.0f);
    bar (r, levR, emberCol);

    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText ("INN", labels.withX (inR.getX() - 6.0f).withWidth (inR.getWidth() + 12.0f), Justification::centred);
    g.drawText ("UT", labels.withX (lR.getX()).withRight (r.getRight()), Justification::centred);
}

//==============================================================================
EmberEditor::EmberEditor (EmberProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets), display (p), bandPanel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);
    presetBar.onPresetLoaded = [this] { proc.soloMask = 0; display.update(); };

    qualityBox.addItemList (qualityNames, 1);
    qualityBox.setTooltip (CharPointer_UTF8 ("Oversampling: høyere gir mindre aliasing (renere diskant ved mye drive), men mer CPU og litt mer latens."));
    content.addAndMakeVisible (qualityBox);
    qualityAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::quality, qualityBox);

    autoGainBtn.setClickingTogglesState (true);
    autoGainBtn.setColour (TextButton::buttonOnColourId, emberCol);
    autoGainBtn.setTooltip (CharPointer_UTF8 ("Holder nivået omtrent likt når du skrur Drive, så du hører karakteren og ikke bare volumet."));
    content.addAndMakeVisible (autoGainBtn);
    autoAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::autoGain, autoGainBtn);

    content.addAndMakeVisible (display);
    content.addAndMakeVisible (bandPanel);
    display.onSelectionChanged = [this] (int b) { selectBand (b); };

    const std::pair<sbaudio::Knob*, const char*> knobs[] = { { &inKnob, ParamIDs::input }, { &outKnob, ParamIDs::output },
                                                             { &mixKnob, ParamIDs::mix } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.param (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, emberCol);
    }
    inKnob.slider.getProperties().set ("bipolar", true);
    outKnob.slider.getProperties().set ("bipolar", true);
    inKnob.slider.setTooltip (CharPointer_UTF8 ("Nivå inn i metningen – mer input gir mer metning i alle bånd."));
    outKnob.slider.setTooltip (CharPointer_UTF8 ("Utgangsnivå."));
    mixKnob.slider.setTooltip (CharPointer_UTF8 ("Global blanding av ren og mettet lyd (parallell metning)."));
    content.addAndMakeVisible (meter);

    const int sel = jlimit (0, proc.getNumBands() - 1, (int) proc.apvts.state.getProperty ("selectedBand", 0));
    display.setSelected (sel);
    bandPanel.setBand (display.getSelected());

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 9 / 5, designH * 9 / 5);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    startTimerHz (30);
}

EmberEditor::~EmberEditor()
{
    proc.soloMask = 0;
    setLookAndFeel (nullptr);
}

void EmberEditor::selectBand (int b)
{
    proc.apvts.state.setProperty ("selectedBand", b, nullptr);
    bandPanel.setBand (b);
}

void EmberEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void EmberEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void EmberEditor::layoutContent()
{
    presetBar.setBounds (150, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());
    autoGainBtn.setBounds (designW - 8 - 100, 8, 100, 28);
    qualityBox.setBounds (autoGainBtn.getX() - 8 - 130, 8, 130, 28);

    display.setBounds (8, 52, designW - 16, 360);
    const int py = display.getBottom() + 8;
    globalArea = { designW - 8 - 280, py, 280, designH - py - 8 };
    bandPanel.setBounds (8, py, globalArea.getX() - 16, designH - py - 8);

    auto g = globalArea.reduced (12, 12);
    meter.setBounds (g.removeFromRight (54).withTrimmedTop (4));
    g.removeFromRight (8);
    g = g.withSizeKeepingCentre (g.getWidth(), jmin (g.getHeight(), 130));
    const int kw = g.getWidth() / 3;
    inKnob.setBounds (g.removeFromLeft (kw));
    mixKnob.setBounds (g.removeFromLeft (kw));
    outKnob.setBounds (g);
}

void EmberEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    g.setFont (uiFont (19.0f, true));
    g.setColour (emberCol);
    g.drawText ("Ember", Rectangle<float> (14.0f, 0.0f, 70.0f, 44.0f), Justification::centredLeft);
    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText ("SB AUDIO", Rectangle<float> (78.0f, 0.0f, 70.0f, 44.0f), Justification::centredLeft);

    auto ga = globalArea.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (ga, 6.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (ga.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (col::textDim);
    g.setFont (uiFont (10.5f, true));
    g.drawText ("GLOBALT", ga.reduced (14.0f, 10.0f).removeFromTop (14.0f), Justification::centredLeft);
}

void EmberEditor::timerCallback()
{
    display.update();
    if (display.getSelected() != (int) proc.apvts.state.getProperty ("selectedBand", -1))
        selectBand (display.getSelected());
    bandPanel.refresh();
    meter.setLevels (proc.inPeak.exchange (0.0f), proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f));
}
