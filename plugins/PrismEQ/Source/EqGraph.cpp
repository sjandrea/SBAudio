#include "EqGraph.h"
#include <sbaudio_common/sbaudio_common.h>

using namespace juce;
using namespace prism;
namespace col = sbaudio::colours;

namespace
{
    constexpr float minFreq = 10.0f, maxFreq = 30000.0f;
    constexpr float specMin = -96.0f, specMax = 6.0f;
    const Identifier displayRangeId ("displayRange"), analyzerId ("analyzer");

    String fmtFreq (float f)
    {
        if (f >= 1000.0f)
            return String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz";
        return String (roundToInt (f)) + " Hz";
    }
    String fmtDb (float d) { return (d > 0.0f ? "+" : "") + String (d, 1) + " dB"; }
}

Colour prismui::bandColour (int band)
{
    static const Colour palette[] = {
        Colour (0xff3fd0c9), Colour (0xffffa53d), Colour (0xffa98bff), Colour (0xff6fd36f),
        Colour (0xffff6f91), Colour (0xff5aa9ff), Colour (0xffe8d44d), Colour (0xffff8a5c)
    };
    return palette[band % 8];
}

//==============================================================================
EqGraph::EqGraph (PrismEQProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);
    setRepaintsOnMouseActivity (false);
}

float EqGraph::getDisplayRange() const
{
    const float r = (float) proc.apvts.state.getProperty (displayRangeId, 12.0f);
    return r == 3.0f || r == 6.0f || r == 12.0f || r == 30.0f ? r : 12.0f;
}

int EqGraph::getAnalyzerMode() const { return (int) proc.apvts.state.getProperty (analyzerId, 2); }

void EqGraph::setSelectedBand (int b)
{
    if (b == selected)
        return;
    selected = b;
    if (onSelectionChanged)
        onSelectionChanged (b);
    repaint();
}

Rectangle<float> EqGraph::plot() const
{
    return getLocalBounds().toFloat().withTrimmedRight (34.0f).withTrimmedBottom (18.0f).reduced (0.0f, 8.0f);
}

float EqGraph::freqToX (float f) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
}

float EqGraph::xToFreq (float x) const
{
    const auto p = plot();
    return minFreq * std::pow (maxFreq / minFreq, jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

float EqGraph::dbToY (float db) const
{
    const auto p = plot();
    const float r = getDisplayRange();
    return p.getCentreY() - db / r * p.getHeight() * 0.5f;
}

float EqGraph::yToDb (float y) const
{
    const auto p = plot();
    return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * getDisplayRange();
}

float EqGraph::specDbToY (float db) const
{
    const auto p = plot();
    return p.getBottom() - (db - specMin) / (specMax - specMin) * p.getHeight();
}

void EqGraph::resized()
{
    numPoints = jmax (2, (int) plot().getWidth() / 2);
    pointFreq.resize ((size_t) numPoints);
    for (int i = 0; i < numPoints; ++i)
        pointFreq[(size_t) i] = minFreq * std::pow (maxFreq / minFreq, (float) i / (float) (numPoints - 1));
    bandDb.assign ((size_t) numBands, std::vector<float> ((size_t) numPoints, 0.0f));
    totalDb.assign ((size_t) numPoints, 0.0f);
    dynRangeDb.assign ((size_t) numPoints, 0.0f);
    specPre.assign ((size_t) numPoints, specMin);
    specPost.assign ((size_t) numPoints, specMin);
}

//==============================================================================
void EqGraph::update()
{
    for (int b = 0; b < numBands; ++b)
        bands[(size_t) b] = proc.getBandState (b);
    gainScale = proc.getGainScale();

    if (selected >= 0 && ! bands[(size_t) selected].used)
        setSelectedBand (-1);

    computeCurves();
    if (getAnalyzerMode() > 0)
        computeSpectrum();
    if (mouseInside && drag == Drag::none && hover < 0)
        findPeak (mousePos);
    repaint();
}

void EqGraph::computeCurves()
{
    if (numPoints < 2)
        return;
    const double fs = proc.getSampleRateSafe();
    std::fill (totalDb.begin(), totalDb.end(), 0.0f);
    selectedHasDyn = false;

    BandDesign d;
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        auto& curve = bandDb[(size_t) b];
        if (! s.used)
            continue;
        const float g = typeHasGain (s.type) ? s.gain * gainScale : 0.0f;
        designBand (d, s.type, s.freq, g, s.q, s.slope, fs);
        for (int i = 0; i < numPoints; ++i)
            curve[(size_t) i] = (float) d.magnitudeDb (pointFreq[(size_t) i], fs);

        if (! s.on)
            continue;

        const bool dyn = s.dyn && typeCanDynamic (s.type);
        const float live = dyn ? proc.dynGainDb[(size_t) b].load() : 0.0f;
        if (std::abs (live) > 0.01f)
        {
            BandDesign dl;
            designBand (dl, s.type, s.freq, g + live, s.q, s.slope, fs);
            for (int i = 0; i < numPoints; ++i)
                totalDb[(size_t) i] += (float) dl.magnitudeDb (pointFreq[(size_t) i], fs);
        }
        else
        {
            for (int i = 0; i < numPoints; ++i)
                totalDb[(size_t) i] += curve[(size_t) i];
        }

        if (b == selected && dyn)
        {
            selectedHasDyn = true;
            BandDesign dr;
            designBand (dr, s.type, s.freq, g + s.range, s.q, s.slope, fs);
            for (int i = 0; i < numPoints; ++i)
                dynRangeDb[(size_t) i] = (float) dr.magnitudeDb (pointFreq[(size_t) i], fs);
        }
    }
}

void EqGraph::computeSpectrum()
{
    const double fs = proc.getSampleRateSafe();
    const float binHz = (float) (fs / fftSize);
    const float norm = 4.0f / (float) fftSize; // hann coherent gain 0.5, one-sided x2

    // light smoothing across neighbouring display points (display only)
    // (the smoothed copies are display-only, so the smoothing does not accumulate)
    auto smooth = [&] (const std::vector<float>& src, std::vector<float>& dst)
    {
        dst = src;
        for (int i = 1; i < numPoints - 1; ++i)
            dst[(size_t) i] = 0.25f * src[(size_t) i - 1] + 0.5f * src[(size_t) i] + 0.25f * src[(size_t) i + 1];
    };

    auto analyse = [&] (const AnalyzerRing& ring, std::vector<float>& out)
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
            {
                for (int k = b0; k <= b1; ++k)
                    mag = jmax (mag, fftData[(size_t) k]);
            }
            float db = Decibels::gainToDecibels (mag * norm, -140.0f) + 4.5f * std::log2 (f / 1000.0f);
            db = jlimit (specMin - 10.0f, specMax + 10.0f, db);
            auto& o = out[(size_t) i];
            o = db > o ? o + (db - o) * 0.6f : o + (db - o) * 0.12f; // fast rise, slow fall
        }
    };

    analyse (proc.postRing, specPost);
    smooth (specPost, specPostDisp);
    if (getAnalyzerMode() == 2)
    {
        analyse (proc.preRing, specPre);
        smooth (specPre, specPreDisp);
    }
}

void EqGraph::findPeak (Point<float> m)
{
    peakValid = false;
    if (getAnalyzerMode() == 0 || numPoints < 3 || ! plot().contains (m))
        return;
    const auto p = plot();
    const int centre = jlimit (0, numPoints - 1, roundToInt ((m.x - p.getX()) / p.getWidth() * (float) (numPoints - 1)));
    const int span = 10;
    int best = -1; float bestDb = -1000.0f;
    for (int i = jmax (1, centre - span); i <= jmin (numPoints - 2, centre + span); ++i)
    {
        const float v = specPost[(size_t) i];
        if (v > bestDb && v >= specPost[(size_t) i - 1] && v >= specPost[(size_t) i + 1])
        {
            best = i; bestDb = v;
        }
    }
    if (best < 0 || bestDb < -70.0f)
        return;
    const float y = specDbToY (bestDb);
    if (std::abs (y - m.y) > 40.0f)
        return;
    peakValid = true;
    peakFreq = pointFreq[(size_t) best];
    peakDb = bestDb;
}

//==============================================================================
Point<float> EqGraph::nodePos (int b) const
{
    const auto& s = bands[(size_t) b];
    const float g = typeHasGain (s.type) ? s.gain * gainScale : 0.0f;
    return { freqToX (jlimit (minFreq, maxFreq, s.freq)), jlimit (plot().getY(), plot().getBottom(), dbToY (g)) };
}

Point<float> EqGraph::dynHandlePos (int b) const
{
    const auto& s = bands[(size_t) b];
    const float g = s.gain * gainScale + s.range;
    return { freqToX (jlimit (minFreq, maxFreq, s.freq)), jlimit (plot().getY(), plot().getBottom(), dbToY (g)) };
}

int EqGraph::hitNode (Point<float> m) const
{
    int best = -1; float bestD = 11.0f;
    // the selected band wins ties
    for (int b = 0; b < numBands; ++b)
    {
        if (! bands[(size_t) b].used)
            continue;
        const float d = nodePos (b).getDistanceFrom (m) - (b == selected ? 1.0f : 0.0f);
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

int EqGraph::hitDynHandle (Point<float> m) const
{
    if (selected < 0)
        return -1;
    const auto& s = bands[(size_t) selected];
    if (! s.used || ! s.dyn || ! typeCanDynamic (s.type))
        return -1;
    return dynHandlePos (selected).getDistanceFrom (m) < 9.0f ? selected : -1;
}

//==============================================================================
void EqGraph::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto p = plot();

    g.setGradientFill (ColourGradient (Colour (0xff12151b), 0.0f, 0.0f, Colour (0xff0b0d11), 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // ---- grid ----
    g.setFont (sbaudio::uiFont (10.0f));
    for (float f : { 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f,
                     1000.f, 2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f, 20000.f })
    {
        const float x = freqToX (f);
        const bool major = f == 100.f || f == 1000.f || f == 10000.f;
        g.setColour (Colours::white.withAlpha (major ? 0.10f : 0.04f));
        g.drawVerticalLine (roundToInt (x), p.getY(), p.getBottom());
    }
    g.setColour (col::textDim);
    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = freqToX (f);
        const String t = f >= 1000.f ? String (roundToInt (f / 1000.f)) + "k" : String (roundToInt (f));
        g.drawText (t, Rectangle<float> (x - 20.0f, p.getBottom() + 4.0f, 40.0f, 14.0f), Justification::centred);
    }

    const float range = getDisplayRange();
    const float step = range <= 3.0f ? 1.0f : range <= 6.0f ? 2.0f : range <= 12.0f ? 3.0f : 6.0f;
    for (float d = -range; d <= range + 0.01f; d += step)
    {
        const float y = dbToY (d);
        g.setColour (Colours::white.withAlpha (std::abs (d) < 0.01f ? 0.16f : 0.05f));
        g.drawHorizontalLine (roundToInt (y), p.getX(), p.getRight());
        g.setColour (std::abs (d) < 0.01f ? col::text : col::textDim);
        g.drawText ((d > 0 ? "+" : "") + String (roundToInt (d)), Rectangle<float> (p.getRight() + 2.0f, y - 7.0f, 28.0f, 14.0f),
                    Justification::centredLeft);
    }

    if (numPoints < 2)
        return;

    auto xAt = [&] (int i) { return p.getX() + p.getWidth() * (float) i / (float) (numPoints - 1); };

    // ---- analyzer ----
    const int am = getAnalyzerMode();
    auto drawSpectrum = [&] (const std::vector<float>& s, Colour c, float fillAlpha)
    {
        Path path;
        path.startNewSubPath (p.getX(), p.getBottom());
        for (int i = 0; i < numPoints; ++i)
            path.lineTo (xAt (i), jlimit (p.getY(), p.getBottom(), specDbToY (s[(size_t) i])));
        path.lineTo (p.getRight(), p.getBottom());
        path.closeSubPath();
        g.setGradientFill (ColourGradient (c.withAlpha (fillAlpha), 0.0f, p.getY(), c.withAlpha (fillAlpha * 0.3f), 0.0f, p.getBottom(), false));
        g.fillPath (path);
        g.setColour (c.withAlpha (fillAlpha * 2.2f));
        g.strokePath (path, PathStrokeType (1.0f));
    };
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p.toNearestInt());
        if (am == 2 && specPreDisp.size() == (size_t) numPoints)  drawSpectrum (specPreDisp, Colour (0xff7d8796), 0.10f);
        if (am >= 1 && specPostDisp.size() == (size_t) numPoints) drawSpectrum (specPostDisp, Colour (0xff3a8fd0), 0.18f);
    }

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (p.toNearestInt().expanded (0, 8));

    const float zeroY = dbToY (0.0f);
    auto curvePath = [&] (const std::vector<float>& db)
    {
        Path path;
        for (int i = 0; i < numPoints; ++i)
        {
            const float y = jlimit (p.getY() - 20.0f, p.getBottom() + 20.0f, dbToY (db[(size_t) i]));
            if (i == 0) path.startNewSubPath (xAt (i), y);
            else        path.lineTo (xAt (i), y);
        }
        return path;
    };

    // ---- selected / hovered band ----
    for (int b : { hover, selected })
    {
        if (b < 0 || ! bands[(size_t) b].used)
            continue;
        const auto c = prismui::bandColour (b);
        auto path = curvePath (bandDb[(size_t) b]);
        Path fill (path);
        fill.lineTo (p.getRight(), zeroY);
        fill.lineTo (p.getX(), zeroY);
        fill.closeSubPath();
        g.setColour (c.withAlpha (b == selected ? 0.16f : 0.08f));
        g.fillPath (fill);
        g.setColour (c.withAlpha (bands[(size_t) b].on ? 0.8f : 0.35f));
        g.strokePath (path, PathStrokeType (1.2f));
    }

    // dynamic range of the selected band
    if (selected >= 0 && selectedHasDyn)
    {
        const auto c = prismui::bandColour (selected);
        auto a = curvePath (bandDb[(size_t) selected]);
        auto bpath = curvePath (dynRangeDb);
        Path area;
        area.startNewSubPath (xAt (0), dbToY (bandDb[(size_t) selected][0]));
        for (int i = 1; i < numPoints; ++i) area.lineTo (xAt (i), dbToY (bandDb[(size_t) selected][(size_t) i]));
        for (int i = numPoints - 1; i >= 0; --i) area.lineTo (xAt (i), dbToY (dynRangeDb[(size_t) i]));
        area.closeSubPath();
        g.setColour (c.withAlpha (0.12f));
        g.fillPath (area);
        float dashes[] = { 4.0f, 3.0f };
        PathStrokeType (1.0f).createDashedStroke (bpath, bpath, dashes, 2);
        g.setColour (c.withAlpha (0.7f));
        g.fillPath (bpath);
    }

    // ---- total curve ----
    {
        auto path = curvePath (totalDb);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.strokePath (path, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xffeef2f6));
        g.strokePath (path, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ---- nodes ----
    g.setFont (sbaudio::uiFont (10.0f, true));
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.used)
            continue;
        const auto pos = nodePos (b);
        const auto c = prismui::bandColour (b);
        const float r = b == selected ? 8.5f : 7.5f;
        auto circle = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos);

        if (b == selected || b == hover)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (circle.expanded (4.0f));
        }
        if (s.on)
        {
            g.setColour (c);
            g.fillEllipse (circle);
            g.setColour (Colours::black.withAlpha (0.8f));
        }
        else
        {
            g.setColour (col::well);
            g.fillEllipse (circle);
            g.setColour (c.withAlpha (0.6f));
            g.drawEllipse (circle.reduced (0.75f), 1.5f);
        }
        g.drawText (String (b + 1), circle.translated (0.0f, 0.5f), Justification::centred);

        if (b == selected)
        {
            g.setColour (Colours::white);
            g.drawEllipse (circle.expanded (1.5f), 1.4f);
        }

        // live dynamic indicator: small bar from node towards current dynamic gain
        if (s.on && s.dyn && typeCanDynamic (s.type))
        {
            const float live = proc.dynGainDb[(size_t) b].load();
            const float y2 = dbToY (s.gain * gainScale + live);
            g.setColour (c.withAlpha (0.9f));
            g.fillRect (Rectangle<float> (pos.x + r + 3.0f, jmin (pos.y, y2), 3.0f, jmax (1.0f, std::abs (y2 - pos.y))));
        }
    }

    // dynamic range handle of the selected band
    if (selected >= 0 && selectedHasDyn)
    {
        const auto hp = dynHandlePos (selected);
        Path diamond;
        diamond.addPolygon (hp, 4, 6.0f, MathConstants<float>::pi * 0.25f);
        g.setColour (prismui::bandColour (selected).withAlpha (hoverDyn >= 0 ? 1.0f : 0.8f));
        g.strokePath (diamond, PathStrokeType (1.6f));
    }

    // ---- spectrum grab marker ----
    if (peakValid && hover < 0 && drag == Drag::none)
    {
        const Point<float> pk (freqToX (peakFreq), specDbToY (peakDb));
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (pk), 1.3f);
        g.drawLine (pk.x - 3.0f, pk.y, pk.x + 3.0f, pk.y, 1.2f);
        g.drawLine (pk.x, pk.y - 3.0f, pk.x, pk.y + 3.0f, 1.2f);
        g.setFont (sbaudio::uiFont (10.5f, true));
        g.drawText (fmtFreq (peakFreq), Rectangle<float> (pk.x - 40.0f, pk.y - 24.0f, 80.0f, 12.0f), Justification::centred);
    }

    // ---- value read-out for the hovered / dragged band ----
    const int info = drag != Drag::none ? dragBand : hover;
    if (info >= 0 && bands[(size_t) info].used)
    {
        const auto& s = bands[(size_t) info];
        String t = typeNames[s.type] + "   " + fmtFreq (s.freq);
        if (typeHasGain (s.type)) t << "   " << fmtDb (s.gain);
        t << "   Q " << String (s.q, 2);
        if (typeHasSlope (s.type)) t << "   " << slopeNames[s.slope];
        if (s.dyn && typeCanDynamic (s.type)) t << "   dyn " << fmtDb (s.range);
        if (s.place != Stereo) t << "   " << placementNames[s.place];

        g.setFont (sbaudio::uiFont (11.5f, true));
        const float w = GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 16.0f;
        auto pos = nodePos (info);
        auto box = Rectangle<float> (w, 20.0f).withCentre ({ pos.x, pos.y - 26.0f });
        if (box.getY() < p.getY()) box.setY (pos.y + 16.0f);
        box = box.constrainedWithin (p);
        g.setColour (col::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (prismui::bandColour (info).withAlpha (0.7f));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (col::text);
        g.drawText (t, box, Justification::centred);
    }

    // ---- hint ----
    bool any = false;
    for (auto& s : bands) any = any || s.used;
    if (! any)
    {
        g.setColour (col::textDim);
        g.setFont (sbaudio::uiFont (14.0f));
        g.drawText (CharPointer_UTF8 ("Dobbeltklikk i grafen for å lage et bånd  ·  klikk på en topp i spekteret for å gripe den"),
                    p.withHeight (40.0f).withY (p.getCentreY() + 30.0f), Justification::centred);
    }
}

//==============================================================================
void EqGraph::mouseMove (const MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;
    hover = hitNode (e.position);
    hoverDyn = hitDynHandle (e.position);
    if (hoverDyn >= 0) hover = -1;
    peakValid = false;
    if (hover < 0 && hoverDyn < 0)
        findPeak (e.position);
    setMouseCursor (hover >= 0 || hoverDyn >= 0 ? MouseCursor::DraggingHandCursor
                                                : (peakValid ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor));
    repaint();
}

void EqGraph::mouseExit (const MouseEvent&)
{
    mouseInside = false;
    hover = hoverDyn = -1;
    peakValid = false;
    repaint();
}

void EqGraph::beginGesture (int band, std::initializer_list<const char*> names)
{
    endGesture();
    for (auto* n : names)
        if (auto* prm = proc.bandParam (band, n))
        {
            prm->beginChangeGesture();
            gestureParams.push_back (prm);
        }
}

void EqGraph::endGesture()
{
    for (auto* prm : gestureParams)
        prm->endChangeGesture();
    gestureParams.clear();
}

void EqGraph::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    dragStart = e.position;
    drag = Drag::none;

    const int dynHit = hitDynHandle (e.position);
    const int hit = dynHit >= 0 ? -1 : hitNode (e.position);

    if (e.mods.isPopupMenu())
    {
        if (hit >= 0)
        {
            setSelectedBand (hit);
            showBandMenu (hit);
        }
        return;
    }

    if (dynHit >= 0)
    {
        drag = Drag::dyn;
        dragBand = dynHit;
        startRange = bands[(size_t) dynHit].range;
        beginGesture (dynHit, { "range" });
        return;
    }

    if (hit >= 0)
    {
        if (e.mods.isAltDown())
        {
            proc.deleteBand (hit);
            setSelectedBand (-1);
            hover = -1;
            return;
        }
        setSelectedBand (hit);
        drag = Drag::node;
        dragBand = hit;
        startFreq = bands[(size_t) hit].freq;
        startGain = bands[(size_t) hit].gain;
        beginGesture (hit, { "freq", "gain" });
        return;
    }

    if (peakValid && e.getNumberOfClicks() == 1)
    {
        createBandAt (e.position, true);
        return;
    }

    setSelectedBand (-1);
}

void EqGraph::createBandAt (Point<float> pos, bool fromPeak)
{
    int type = Bell;
    float freq = fromPeak ? peakFreq : xToFreq (pos.x);
    float gain = fromPeak ? 0.0f : jlimit (-30.0f, 30.0f, yToDb (pos.y) / jmax (0.01f, gainScale));
    float q = fromPeak ? 4.0f : 1.0f;

    if (! fromPeak)
    {
        if (freq < 30.0f)        { type = LowCut;  freq = 30.0f;    gain = 0.0f; q = 0.71f; }
        else if (freq > 18000.0f){ type = HighCut; freq = 18000.0f; gain = 0.0f; q = 0.71f; }
    }

    const int b = proc.createBand (type, freq, gain, q);
    if (b < 0)
        return;
    bands[(size_t) b] = proc.getBandState (b);
    setSelectedBand (b);
    drag = Drag::node;
    dragBand = b;
    startFreq = freq;
    startGain = gain;
    dragStart = pos;
    beginGesture (b, { "freq", "gain" });
    peakValid = false;
}

void EqGraph::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::none || dragBand < 0)
        return;

    const bool fine = e.mods.isShiftDown();
    const float k = fine ? 0.15f : 1.0f;
    const auto delta = (e.position - dragStart) * k;
    const auto& s = bands[(size_t) dragBand];

    if (drag == Drag::dyn)
    {
        const float dbPerPx = getDisplayRange() * 2.0f / plot().getHeight();
        proc.bandParam (dragBand, "range")->setValueNotifyingHost (
            proc.bandParam (dragBand, "range")->convertTo0to1 (jlimit (-30.0f, 30.0f, startRange - delta.y * dbPerPx / jmax (0.1f, gainScale))));
        return;
    }

    // frequency: constant pixels per octave
    const float octPerPx = std::log2 (maxFreq / minFreq) / plot().getWidth();
    float f = startFreq * std::pow (2.0f, delta.x * octPerPx);
    // Cmd/Ctrl constrains to vertical movement
    if (e.mods.isCommandDown()) f = startFreq;
    auto* pf = proc.bandParam (dragBand, "freq");
    pf->setValueNotifyingHost (pf->convertTo0to1 (jlimit (minFreq, maxFreq, f)));

    if (typeHasGain (s.type))
    {
        const float dbPerPx = getDisplayRange() * 2.0f / plot().getHeight();
        float gdb = startGain - delta.y * dbPerPx / jmax (0.1f, gainScale);
        auto* pg = proc.bandParam (dragBand, "gain");
        pg->setValueNotifyingHost (pg->convertTo0to1 (jlimit (-30.0f, 30.0f, gdb)));
    }
}

void EqGraph::mouseUp (const MouseEvent&)
{
    endGesture();
    drag = Drag::none;
    dragBand = -1;
}

void EqGraph::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = hitNode (e.position);
    if (hit >= 0)
    {
        endGesture();
        drag = Drag::none;
        proc.setBandParam (hit, "on", bands[(size_t) hit].on ? 0.0f : 1.0f);
        return;
    }
    if (plot().contains (e.position))
    {
        createBandAt (e.position, false);
        endGesture();
        drag = Drag::none;
    }
}

void EqGraph::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    int b = hitNode (e.position);
    if (b < 0) b = selected;
    if (b < 0)
        return;
    auto* pq = proc.bandParam (b, "q");
    const float q = bands[(size_t) b].q * std::pow (2.0f, (w.deltaY + w.deltaX) * (e.mods.isShiftDown() ? 0.5f : 2.0f));
    pq->beginChangeGesture();
    pq->setValueNotifyingHost (pq->convertTo0to1 (jlimit (0.025f, 40.0f, q)));
    pq->endChangeGesture();
}

bool EqGraph::keyPressed (const KeyPress& k)
{
    if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && selected >= 0)
    {
        proc.deleteBand (selected);
        setSelectedBand (-1);
        return true;
    }
    return false;
}

void EqGraph::showBandMenu (int b)
{
    const auto s = proc.getBandState (b);
    PopupMenu m, types, slopes, places;
    for (int t = 0; t < numTypes; ++t)
        types.addItem (100 + t, typeNames[t], true, s.type == t);
    for (int i = 0; i < slopeNames.size(); ++i)
        slopes.addItem (200 + i, slopeNames[i], true, s.slope == i);
    for (int i = 0; i < placementNames.size(); ++i)
        places.addItem (300 + i, placementNames[i], true, s.place == i);

    m.addSectionHeader (String (CharPointer_UTF8 ("Bånd ")) + String (b + 1));
    m.addSubMenu ("Filtertype", types);
    m.addSubMenu ("Helning", slopes, typeHasSlope (s.type));
    m.addSubMenu ("Stereo", places);
    m.addItem (1, "Dynamisk", typeCanDynamic (s.type), s.dyn);
    m.addItem (2, CharPointer_UTF8 ("Aktiv (på/av)"), true, s.on);
    m.addSeparator();
    m.addItem (3, "Slett");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this)
                         .withTargetScreenArea (Rectangle<int> (1, 1).withPosition (localPointToGlobal (nodePos (b).toInt()))),
                     [this, b, s] (int r)
                     {
                         if (r != 0 && onEdit) onEdit();
                         if (r >= 100 && r < 100 + numTypes) proc.setBandParam (b, "type", (float) (r - 100));
                         else if (r >= 200 && r < 209)       proc.setBandParam (b, "slope", (float) (r - 200));
                         else if (r >= 300 && r < 305)       proc.setBandParam (b, "place", (float) (r - 300));
                         else if (r == 1)                    proc.setBandParam (b, "dyn", s.dyn ? 0.0f : 1.0f);
                         else if (r == 2)                    proc.setBandParam (b, "on", s.on ? 0.0f : 1.0f);
                         else if (r == 3)                    { proc.deleteBand (b); setSelectedBand (-1); }
                     });
}
