#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour gold     { 0xffe6c36a };   // "glans": polished gold
    const Colour goldSoft { 0xfff3dfa6 };

    String u8 (const char* s) { return String (CharPointer_UTF8 (s)); }

    void drawWell (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::well.withAlpha (0.55f));   // the painting shows through
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    String signedDb (float v, int dec = 1) { return (v >= 0.05f ? "+" : "") + String (v, dec); }
    String lufsStr (float v) { return v <= -99.0f ? u8 ("\xe2\x80\x93") : String (v, 1); }
}

namespace dominaui
{
    //==========================================================================
    SegmentSelector::SegmentSelector (RangedAudioParameter& p, const StringArray& names, std::vector<float> v,
                                      const StringArray& tips)
        : values (std::move (v)), attachment (p, [this] (float x) { setCurrent (x); }, nullptr)
    {
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new TextButton (names[i]));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, gold);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->setTooltip (tips[i]);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture (values[(size_t) i]); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void SegmentSelector::setCurrent (float x)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (std::abs (x - values[(size_t) i]) < 0.05f, dontSendNotification);
    }

    void SegmentSelector::resized()
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
    void AnalyseButton::update (int s, float p, bool a, bool w)
    {
        ++pulse;
        if (s == state && std::abs (p - progress) < 0.002f && a == analysed && w == waiting && s == domina::Idle && a)
            return;
        state = s; progress = p; analysed = a; waiting = w;
        repaint();
    }

    void AnalyseButton::mouseUp (const MouseEvent& e)
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }

    void AnalyseButton::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool over = isMouseOver();
        const bool busy = state != domina::Idle;
        const bool callToAction = ! busy && ! analysed;

        g.setColour (callToAction ? gold.withAlpha (over ? 1.0f : 0.9f) : col::panel.brighter (over ? 0.08f : 0.0f));
        g.fillRoundedRectangle (r, 6.0f);

        if (busy)
        {
            auto fill = r.withWidth (r.getWidth() * jlimit (0.0f, 1.0f, progress));
            const float a = waiting ? 0.18f + 0.08f * std::sin ((float) pulse * 0.15f) : 0.32f;
            g.setColour (gold.withAlpha (a));
            g.fillRoundedRectangle (fill, 6.0f);
        }
        g.setColour (callToAction ? gold.darker (0.3f) : gold.withAlpha (busy ? 0.9f : 0.6f));
        g.drawRoundedRectangle (r, 6.0f, 1.2f);

        String text, sub;
        if (state == domina::Listening)     { text = waiting ? u8 ("VENTER P\xc3\x85 LYD") : "LYTTER"; sub = u8 ("klikk for \xc3\xa5 avbryte"); }
        else if (state == domina::Refining) { text = "FINJUSTERER"; sub = u8 ("klikk for \xc3\xa5 avbryte"); }
        else                               { text = analysed ? u8 ("ANALYSER P\xc3\x85 NYTT") : "ANALYSER"; }

        auto tr = r.reduced (12.0f, 6.0f);
        g.setColour (callToAction ? col::bg : (busy ? goldSoft : gold));
        g.setFont (uiFont (14.0f, true));
        if (busy)
        {
            g.drawText (text, tr.removeFromTop (tr.getHeight() * 0.58f), Justification::centredLeft);
            g.drawText (String (roundToInt (progress * 100.0f)) + " %", r.reduced (12.0f, 6.0f).removeFromTop (r.getHeight() * 0.5f),
                        Justification::centredRight);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (sub, tr, Justification::centredLeft);
        }
        else
        {
            // small "listening" icon: three bars
            auto icon = tr.removeFromLeft (18.0f).withSizeKeepingCentre (16.0f, 16.0f);
            const float hs[] = { 0.5f, 1.0f, 0.7f };
            for (int i = 0; i < 3; ++i)
            {
                const float h = icon.getHeight() * hs[i];
                g.fillRoundedRectangle (icon.getX() + (float) i * 6.0f, icon.getCentreY() - h * 0.5f, 3.5f, h, 1.5f);
            }
            tr.removeFromLeft (8.0f);
            g.drawText (text, tr, Justification::centredLeft);
        }
    }

    //==========================================================================
    Spectrum::Spectrum (DominaProcessor& p) : proc (p) {}

    Rectangle<float> Spectrum::plot() const
    {
        auto r = getLocalBounds().toFloat().reduced (12.0f);
        r.removeFromTop (22.0f);
        r.removeFromBottom (14.0f);
        r.removeFromRight (26.0f);
        return r;
    }

    float Spectrum::freqToX (float f) const
    {
        const auto p = plot();
        return p.getX() + p.getWidth() * std::log (jmax (20.0f, f) / 20.0f) / std::log (1000.0f);
    }

    void Spectrum::resized()
    {
        const int n = jmax (64, (int) plot().getWidth() / 2);
        freqs.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            freqs[(size_t) i] = 20.0f * std::pow (1000.0f, (float) i / (float) (n - 1));
        specIn.assign ((size_t) n, -120.0f);
        specOut.assign ((size_t) n, -120.0f);
        eqTotal.assign ((size_t) n, 0.0f);
        eqAuto.assign ((size_t) n, 0.0f);
    }

    void Spectrum::analyse (const sbaudio::AnalyzerRing& ring, std::vector<float>& sm)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        ring.read (fftData.data(), fftSize);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const float binHz = (float) (proc.getSampleRateForDisplay() / fftSize);
        const float norm = 4.0f / (float) fftSize;
        const int n = (int) freqs.size();
        for (int i = 0; i < n; ++i)
        {
            const float f = freqs[(size_t) i];
            const float fLo = i > 0 ? std::sqrt (f * freqs[(size_t) i - 1]) : f;
            const float fHi = i < n - 1 ? std::sqrt (f * freqs[(size_t) i + 1]) : f;
            const int k0 = jlimit (1, fftSize / 2 - 2, (int) std::floor (fLo / binHz));
            const int k1 = jlimit (1, fftSize / 2 - 2, (int) std::ceil (fHi / binHz));
            float mag;
            if (k1 - k0 <= 1)
            {
                const float pos = jlimit (1.0f, (float) (fftSize / 2 - 2), f / binHz);
                const int k = (int) pos;
                const float t = pos - (float) k;
                mag = fftData[(size_t) k] * (1.0f - t) + fftData[(size_t) k + 1] * t;
            }
            else
            {
                mag = 0.0f;
                for (int k = k0; k <= k1; ++k) mag = jmax (mag, fftData[(size_t) k]);
            }
            // +3 dB/octave tilt around 1 kHz so a balanced mix (pink-ish) looks flat
            const float db = Decibels::gainToDecibels (mag * norm, -140.0f) + 3.0f * std::log2 (f / 1000.0f);
            float& s = sm[(size_t) i];
            s += (db - s) * (db > s ? 0.6f : 0.12f);
        }
    }

    void Spectrum::update()
    {
        if (freqs.empty()) return;
        const int serial = proc.shared.displaySerial.load();
        if (serial != lastSerial)
        {
            lastSerial = serial;
            analyse (proc.shared.preRing, specIn);
            analyse (proc.shared.postRing, specOut);
        }
        else   // host stopped: let the curves fall
        {
            for (auto* v : { &specIn, &specOut })
                for (auto& x : *v) x = jmax (-140.0f, x - 1.5f);
        }

        // EQ curve: auto-EQ (from the analysis) + tone
        const double fs = proc.getSampleRateForDisplay();
        std::array<domina::Coeffs, domina::numEqBands> ac;
        std::array<domina::Coeffs, domina::numToneBands> tc;
        for (int i = 0; i < domina::numEqBands; ++i)
            ac[(size_t) i] = domina::designBand (domina::corrBands[i], fs, proc.shared.corrDb[(size_t) i].load());
        const char* toneIds[] = { ParamIDs::low, ParamIDs::mid, ParamIDs::presence, ParamIDs::high };
        for (int i = 0; i < domina::numToneBands; ++i)
            tc[(size_t) i] = domina::designBand (domina::toneBands[i], fs, proc.apvts.getRawParameterValue (toneIds[i])->load());

        for (size_t i = 0; i < freqs.size(); ++i)
        {
            double a = 0.0, t = 0.0;
            for (auto& c : ac) a += c.magDb (freqs[i], fs);
            for (auto& c : tc) t += c.magDb (freqs[i], fs);
            eqAuto[i] = (float) a;
            eqTotal[i] = (float) (a + t);
        }
        repaint();
    }

    void Spectrum::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        const auto p = plot();

        constexpr float specTop = 0.0f, specBottom = -84.0f, eqRange = 15.0f;
        auto specY = [&] (float db) { return p.getY() + p.getHeight() * jlimit (0.0f, 1.0f, (specTop - db) / (specTop - specBottom)); };
        auto eqY   = [&] (float db) { return p.getCentreY() - (p.getHeight() * 0.5f) * jlimit (-1.0f, 1.0f, db / eqRange); };

        // grid
        g.setFont (uiFont (9.5f));
        for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
        {
            const float x = freqToX (f);
            g.setColour (col::panelEdge.withAlpha (0.6f));
            g.drawVerticalLine (roundToInt (x), p.getY(), p.getBottom());
            g.setColour (col::textDim);
            const String label = f >= 1000.0f ? String (roundToInt (f / 1000.0f)) + "k" : String (roundToInt (f));
            g.drawText (label, Rectangle<float> (x - 18.0f, p.getBottom() + 2.0f, 36.0f, 12.0f), Justification::centred);
        }
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            const float y = eqY (db);
            g.setColour (col::panelEdge.withAlpha (db == 0.0f ? 0.9f : 0.45f));
            g.drawHorizontalLine (roundToInt (y), p.getX(), p.getRight());
            g.setColour (gold.withAlpha (0.7f));
            g.drawText (signedDb (db, 0), Rectangle<float> (p.getRight() + 3.0f, y - 7.0f, 24.0f, 14.0f), Justification::centredLeft);
        }

        if (! freqs.empty())
        {
            auto spectrumPath = [&] (const std::vector<float>& v, bool closed)
            {
                Path path;
                for (size_t i = 0; i < freqs.size(); ++i)
                {
                    const float x = freqToX (freqs[i]), y = specY (v[i]);
                    if (i == 0) { if (closed) { path.startNewSubPath (x, p.getBottom()); path.lineTo (x, y); } else path.startNewSubPath (x, y); }
                    else path.lineTo (x, y);
                }
                if (closed) { path.lineTo (p.getRight(), p.getBottom()); path.closeSubPath(); }
                return path;
            };

            g.setColour (col::slate.withAlpha (0.38f));
            g.fillPath (spectrumPath (specIn, true));

            g.setGradientFill (ColourGradient (gold.withAlpha (0.16f), 0.0f, p.getY(), gold.withAlpha (0.02f), 0.0f, p.getBottom(), false));
            g.fillPath (spectrumPath (specOut, true));
            g.setColour (gold.withAlpha (0.85f));
            g.strokePath (spectrumPath (specOut, false), PathStrokeType (1.3f));

            auto eqPath = [&] (const std::vector<float>& v)
            {
                Path path;
                for (size_t i = 0; i < freqs.size(); ++i)
                {
                    const float x = freqToX (freqs[i]), y = eqY (v[i]);
                    if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
                }
                return path;
            };
            Path dashed;
            const float dashes[] = { 4.0f, 3.0f };
            PathStrokeType (1.3f).createDashedStroke (dashed, eqPath (eqAuto), dashes, 2);
            g.setColour (col::violet.withAlpha (0.9f));
            g.fillPath (dashed);
            g.setColour (col::text);
            g.strokePath (eqPath (eqTotal), PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        if (! proc.shared.dataValid.load())
        {
            g.setColour (col::textDim);
            g.setFont (uiFont (11.0f, true));
            g.drawText (u8 ("Auto-EQ vises etter analysen"), p.withTrimmedTop (p.getHeight() * 0.56f).withHeight (18.0f),
                        Justification::centred);
        }

        // title + legend
        auto head = full.reduced (12.0f, 8.0f).removeFromTop (16.0f);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("SPEKTRUM  /  EQ", head, Justification::centredLeft);
        auto item = [&] (const String& name, Colour c, int kind)
        {
            auto r = head.removeFromRight (kind == 2 ? 92.0f : 74.0f);
            g.setColour (c);
            if (kind == 0) g.fillRect (r.getX(), r.getCentreY() - 4.0f, 14.0f, 8.0f);
            else if (kind == 1) g.fillRect (r.getX(), r.getCentreY() - 1.0f, 14.0f, 2.0f);
            else { g.fillRect (r.getX(), r.getCentreY() - 1.0f, 5.0f, 2.0f); g.fillRect (r.getX() + 8.0f, r.getCentreY() - 1.0f, 5.0f, 2.0f); }
            g.setColour (col::textDim);
            g.setFont (uiFont (10.0f, true));
            g.drawText (name, r.withTrimmedLeft (19.0f), Justification::centredLeft);
        };
        item ("EQ totalt", col::text, 1);
        item ("Auto-EQ", col::violet, 2);
        item ("Ut", gold, 1);
        item ("Inn", col::slate, 0);
    }

    //==========================================================================
    void LoudnessPanel::setValues (const MeterValues& nv)
    {
        v = nv;
        if (nv.limiterGr >= limHold) { limHold = nv.limiterGr; limHoldFrames = 45; }
        else if (--limHoldFrames <= 0) limHold = jmax (nv.limiterGr, limHold - 0.3f);
        repaint();
    }

    void LoudnessPanel::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        auto r = full.reduced (14.0f, 10.0f);
        r.removeFromBottom ((float) buttonArea - 6.0f);

        auto head = r.removeFromTop (16.0f);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("LOUDNESS", head, Justification::centredLeft);
        g.setColour (gold);
        g.setFont (uiFont (10.5f, true));
        g.drawText (u8 ("m\xc3\xa5l ") + String (v.target, 1) + " LUFS", head, Justification::centredRight);

        // big short-term number
        r.removeFromTop (4.0f);
        auto big = r.removeFromTop (46.0f);
        const bool onTarget = v.shortTerm > -99.0f && std::abs (v.shortTerm - v.target) <= 1.0f;
        g.setColour (onTarget ? goldSoft : col::text);
        g.setFont (uiFont (38.0f, true));
        g.drawText (lufsStr (v.shortTerm), big.removeFromLeft (big.getWidth() * 0.55f), Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        g.drawText ("LUFS KORT SIKT", big.removeFromTop (big.getHeight() * 0.5f), Justification::bottomRight);
        g.setColour (col::text);
        g.setFont (uiFont (10.5f, true));
        g.drawText (u8 ("momentan ") + lufsStr (v.momentary), big, Justification::topRight);

        // bar with target marker
        r.removeFromTop (4.0f);
        auto bar = r.removeFromTop (10.0f);
        auto scaleR = r.removeFromTop (13.0f);
        constexpr float lo = -24.0f, hi = -4.0f;
        auto bx = [&] (float l) { return bar.getX() + bar.getWidth() * jlimit (0.0f, 1.0f, (l - lo) / (hi - lo)); };
        g.setColour (col::panel);
        g.fillRoundedRectangle (bar, 3.0f);
        if (v.shortTerm > lo)
        {
            g.setGradientFill (ColourGradient (col::slate, bar.getX(), 0.0f, gold, bx (v.target), 0.0f, false));
            g.fillRoundedRectangle (bar.withRight (bx (v.shortTerm)), 3.0f);
        }
        if (v.momentary > lo)
        {
            g.setColour (col::text.withAlpha (0.8f));
            g.fillRect (bx (v.momentary) - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        }
        const float tx = bx (v.target);
        g.setColour (goldSoft);
        g.fillRect (tx - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
        g.setFont (uiFont (9.0f));
        g.setColour (col::textDim);
        for (float l : { -24.0f, -18.0f, -12.0f, -6.0f })
            g.drawText (String (roundToInt (l)), Rectangle<float> (bx (l) - 14.0f, scaleR.getY() + 1.0f, 28.0f, 11.0f), Justification::centred);

        // stats
        r.removeFromTop (4.0f);
        auto stats = r.removeFromTop (32.0f);
        const float cw = stats.getWidth() / 3.0f;
        auto cell = [&] (const String& label, const String& value, Colour c)
        {
            auto cr = stats.removeFromLeft (cw);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.0f, true));
            g.drawText (label, cr.removeFromTop (12.0f), Justification::centredLeft);
            g.setColour (c);
            g.setFont (uiFont (14.5f, true));
            g.drawText (value, cr, Justification::centredLeft);
        };
        cell ("INTEGRERT", lufsStr (v.integrated), col::text);
        cell ("TRUE PEAK", v.truePeak <= -99.0f ? u8 ("\xe2\x80\x93") : String (v.truePeak, 1) + " dB", col::text);
        cell ("GAIN", v.analysed ? signedDb (v.drive) + " dB" : u8 ("\xe2\x80\x93"), v.analysed ? gold : col::textDim);

        // gain reduction bars
        r.removeFromTop (6.0f);
        auto grBar = [&] (const String& label, float db, float maxDb)
        {
            auto row = r.removeFromTop (13.0f);
            r.removeFromTop (2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.0f, true));
            g.drawText (label, row.removeFromLeft (72.0f), Justification::centredLeft);
            auto valR = row.removeFromRight (40.0f);
            auto b = row.reduced (0.0f, 3.0f);
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (gold.withAlpha (0.85f));
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * jlimit (0.0f, 1.0f, db / maxDb)), 2.0f);
            g.setColour (col::text);
            g.drawText (db < 0.05f ? String ("0.0") : "-" + String (db, 1), valR, Justification::centredRight);
        };
        grBar ("LIMITER", limHold, 12.0f);
        grBar ("KOMP LAV", v.compGr[0], 12.0f);
        grBar ("KOMP MID", v.compGr[1], 12.0f);
        grBar (u8 ("KOMP H\xc3\x98Y"), v.compGr[2], 12.0f);
        grBar ("DE-ESS", v.deessGr, 12.0f);
    }
}

//==============================================================================
DominaEditor::DominaEditor (DominaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      styleSelector (*p.apvts.getParameter (ParamIDs::style), { "VARM", "BALANSERT", u8 ("\xc3\x85PEN") }, { 0.0f, 1.0f, 2.0f },
                     { u8 ("Fyldig og myk: litt mer bunn, mindre topp, roligere kompresjon og mer metning."),
                       u8 ("N\xc3\xb8ytral balanse som passer de fleste sjangre."),
                       u8 ("Klar og luftig: mer topp og presence, raskere kompresjon og renere metning.") }),
      loudnessSelector (*p.apvts.getParameter (ParamIDs::loudness), { "LAV", "MIDDELS", u8 ("H\xc3\x98Y") },
                        { domina::loudnessPresets[0], domina::loudnessPresets[1], domina::loudnessPresets[2] },
                        { u8 ("-14 LUFS: dynamisk og str\xc3\xb8mmevennlig (Spotify, Apple Music, YouTube)."),
                          u8 ("-11 LUFS: tett og konkurransedyktig, fortsatt med luft."),
                          u8 ("-8 LUFS: h\xc3\xb8yt og kompakt, for klubb og tunge sjangre. Mer limiting.") }),
      spectrum (p)
{
    setLookAndFeel (&lnf);
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    content.addAndMakeVisible (analyseButton);
    analyseButton.setTooltip (u8 ("Spill av den kraftigste delen av l\xc3\xa5ta (f.eks. refrenget). Domina lytter i ca. 15 sekunder, "
                                  "setter EQ, kompresjon og gain, og treffer loudness-m\xc3\xa5let. Pauser telles ikke med."));
    analyseButton.onClick = [this]
    {
        if (proc.shared.analysisState.load() == domina::Idle) proc.startAnalysis();
        else proc.cancelAnalysis();
    };

    content.addAndMakeVisible (styleSelector);
    content.addAndMakeVisible (loudnessSelector);
    content.addAndMakeVisible (spectrum);
    spectrum.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (loudnessPanel);
    loudnessPanel.onReset = [this] { proc.resetMeters(); };
    loudnessPanel.setTooltip (u8 ("Klikk for \xc3\xa5 nullstille integrert loudness og true peak."));

    for (auto* b : { &compareButton, &monoBassButton })
    {
        b->setClickingTogglesState (true);
        b->setColour (TextButton::buttonOnColourId, gold);
        b->setColour (TextButton::textColourOnId, col::bg);
        content.addAndMakeVisible (b);
    }
    compareButton.setTooltip (u8 ("Spiller originalen med samme loudness som masteren, s\xc3\xa5 du h\xc3\xb8rer forskjellen "
                                  "i klang og ikke bare i niv\xc3\xa5."));
    monoBassButton.setTooltip (u8 ("Gj\xc3\xb8r bassen under ca. 120 Hz mono. Strammere bunn og tryggere for vinyl og klubbanlegg."));
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::compare, compareButton));
    buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::monoBass, monoBassButton));

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &low, ParamIDs::low }, { &mid, ParamIDs::mid }, { &presence, ParamIDs::presence }, { &high, ParamIDs::high },
        { &autoEq, ParamIDs::autoEq }, { &compression, ParamIDs::compression }, { &deess, ParamIDs::deess },
        { &saturation, ParamIDs::saturation }, { &width, ParamIDs::width }, { &loudness, ParamIDs::loudness },
        { &ceiling, ParamIDs::ceiling } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, gold);
    }
    for (auto* k : { &low, &mid, &presence, &high })
        k->slider.getProperties().set ("bipolar", true);

    low.slider.setTooltip (u8 ("Bunn (hylle rundt 100 Hz)."));
    mid.slider.setTooltip (u8 ("Mellomtone rundt 900 Hz. Ned gir en mer \xc2\xabmoderne\xc2\xbb, mindre boksete lyd."));
    presence.slider.setTooltip (u8 ("Tilstedev\xc3\xa6relse rundt 4 kHz: vokal og attack frem eller tilbake."));
    high.slider.setTooltip (u8 ("Topp og luft (hylle rundt 9 kHz)."));
    autoEq.slider.setTooltip (u8 ("Hvor mye av korreksjonen fra analysen som brukes. Den lilla kurven i grafen viser den."));
    compression.slider.setTooltip (u8 ("Multiband-kompresjon (3 b\xc3\xa5nd) som limer miksen sammen. Tersklene settes av analysen."));
    deess.slider.setTooltip (u8 ("Demper skarpe S-lyder og cymbaler rundt 7,5 kHz bare n\xc3\xa5r de stikker ut."));
    saturation.slider.setTooltip (u8 ("B\xc3\xa5nd-/r\xc3\xb8rlignende metning f\xc3\xb8r limiteren: varme og tetthet, runder av toppene."));
    width.slider.setTooltip (u8 ("Stereobredde (sideniv\xc3\xa5). 100 % = uendret, 0 % = mono."));
    loudness.slider.setTooltip (u8 ("M\xc3\xa5l for integrert loudness. LAV/MIDDELS/H\xc3\x98Y er snarveier."));
    ceiling.slider.setTooltip (u8 ("H\xc3\xb8yeste tillatte topp (true peak). -1 dBTP er trygt for str\xc3\xb8mmetjenester."));

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    timerCallback();
    startTimerHz (30);
}

DominaEditor::~DominaEditor()
{
    setLookAndFeel (nullptr);
}

void DominaEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void DominaEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void DominaEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    analyseButton.setBounds (12, 56, 196, 48);
    statusRect = { 222, 56, 244, 48 };
    styleSelector.setBounds (478, 72, 250, 32);
    loudnessSelector.setBounds (740, 72, 248, 32);

    const int dispY = 116, dispH = 262;
    spectrum.setBounds (12, dispY, 664, dispH);
    loudnessPanel.setBounds (688, dispY, 300, dispH);
    compareButton.setBounds (loudnessPanel.getX() + 14, loudnessPanel.getBottom() - 36, loudnessPanel.getWidth() - 28, 26);

    const int panelY = dispY + dispH + 12, panelH = designH - panelY - 12, gap = 12;
    toneRect = { 12, panelY, 430, panelH };
    dynRect  = { toneRect.getRight() + gap, panelY, 260, panelH };
    outRect  = { dynRect.getRight() + gap, panelY, designW - 12 - (dynRect.getRight() + gap), panelH };
    monoBassButton.setBounds (outRect.getRight() - 14 - 92, outRect.getY() + 9, 92, 22);

    auto knobRow = [] (Rectangle<int> panel, std::initializer_list<sbaudio::Knob*> ks)
    {
        auto r = panel.reduced (12, 12);
        r.removeFromTop (26);
        r = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 132));
        const int w = r.getWidth() / (int) ks.size();
        for (auto* k : ks)
            k->setBounds (r.removeFromLeft (w).reduced (3, 0));
    };
    knobRow (toneRect, { &low, &mid, &presence, &high, &autoEq });
    knobRow (dynRect, { &compression, &deess, &saturation });
    knobRow (outRect, { &width, &loudness, &ceiling });
}

String DominaEditor::statusLine (int line) const
{
    const auto& sh = proc.shared;
    const int st = sh.analysisState.load();
    if (st == domina::Listening)
        return line == 0 ? (sh.waitingForAudio.load() ? u8 ("Venter p\xc3\xa5 lyd \xe2\x80\x93 start avspilling") : u8 ("Lytter p\xc3\xa5 l\xc3\xa5ta \xe2\x80\xa6"))
                         : u8 ("Spill av den kraftigste delen, f.eks. refrenget.");
    if (st == domina::Refining)
        return line == 0 ? u8 ("Finjusterer loudness mot m\xc3\xa5let \xe2\x80\xa6") : u8 ("Snart ferdig \xe2\x80\x93 la l\xc3\xa5ta spille videre.");
    if (sh.dataValid.load())
        return line == 0 ? u8 ("Analysert  \xc2\xb7  inn ") + String (sh.dataInLufs.load(), 1) + " LUFS"
                         : u8 ("Stil, loudness og EQ kan endres uten ny analyse.");
    return line == 0 ? u8 ("Ikke analysert enn\xc3\xa5")
                     : (sh.autoStartDone.load() ? u8 ("Spill av refrenget og trykk ANALYSER.")
                                                : u8 ("Spill av refrenget \xe2\x80\x93 analysen starter av seg selv."));
}

void DominaEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);
    if (background.isValid())
    {
        g.drawImage (background, full, RectanglePlacement::centred | RectanglePlacement::fillDestination);
        g.setColour (col::bg.withAlpha (0.15f));   // keep text and controls readable
        g.fillRect (full);
    }

    // ---- top bar ----
    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel.withAlpha (0.80f));
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "Domina", "MASTERING", gold);

    // ---- status ----
    auto st = statusRect.toFloat();
    g.setColour (col::bg.withAlpha (0.55f));
    g.fillRoundedRectangle (st.expanded (6.0f, 2.0f).withRight (st.getRight() + 4.0f), 6.0f);
    g.setColour (col::text);
    g.setFont (uiFont (12.0f, true));
    g.drawFittedText (statusLine (0), st.removeFromTop (st.getHeight() * 0.5f).toNearestInt(), Justification::bottomLeft, 1, 0.85f);
    g.setColour (col::textDim);
    g.setFont (uiFont (10.5f));
    g.drawFittedText (statusLine (1), st.toNearestInt().withTrimmedTop (2), Justification::topLeft, 1, 0.85f);

    // ---- selector captions ----
    g.setFont (uiFont (10.0f, true));
    g.setColour (col::textDim);
    g.drawText ("STIL", Rectangle<float> ((float) styleSelector.getX(), 54.0f, 100.0f, 14.0f), Justification::centredLeft);
    g.drawText (u8 ("LOUDNESS-M\xc3\x85L"), Rectangle<float> ((float) loudnessSelector.getX(), 54.0f, 140.0f, 14.0f), Justification::centredLeft);

    // ---- knob panels ----
    auto panel = [&] (Rectangle<int> rr, const String& title, const String& note)
    {
        auto r = rr.toFloat();
        g.setColour (col::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto head = r.reduced (16.0f, 12.0f).removeFromTop (16.0f);
        g.setColour (gold);
        g.setFont (uiFont (11.0f, true));
        g.drawText (title, head, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f));
        g.drawText (note, head, Justification::centredRight);
    };
    panel (toneRect, "TONE", "EQ etter smak + auto-EQ fra analysen");
    panel (dynRect, "DYNAMIKK", "");
    panel (outRect, "STEREO  /  UT", "");
}

void DominaEditor::timerCallback()
{
    auto& sh = proc.shared;

    dominaui::MeterValues mv;
    mv.shortTerm  = sh.outShort.load();
    mv.momentary  = sh.outMomentary.load();
    mv.integrated = sh.outIntegrated.load();
    mv.truePeak   = sh.truePeakDb.load();
    mv.target     = proc.apvts.getRawParameterValue (ParamIDs::loudness)->load();
    mv.drive      = sh.driveDb.load();
    mv.analysed   = sh.dataValid.load();

    auto held = [] (float fresh, float& h) { if (fresh >= 0.0f) h = jmax (fresh, h - 0.4f); else h = jmax (0.0f, h - 0.4f); return h; };
    mv.limiterGr = held (sh.limiterGrDb.exchange (-1.0f), limHeld);
    for (int b = 0; b < domina::numCompBands; ++b)
        mv.compGr[(size_t) b] = held (sh.compGrDb[(size_t) b].exchange (-1.0f), compHeld[(size_t) b]);
    mv.deessGr = held (sh.deessGrDb.exchange (-1.0f), deessHeld);
    loudnessPanel.setValues (mv);

    const int state = sh.analysisState.load();
    const bool waiting = sh.waitingForAudio.load();
    analyseButton.update (state, sh.progress.load(), mv.analysed, waiting);

    const float inL = sh.dataInLufs.load();
    if (state != shownState || mv.analysed != shownAnalysed || waiting != shownWaiting || std::abs (inL - shownInLufs) > 0.05f)
    {
        shownState = state; shownAnalysed = mv.analysed; shownWaiting = waiting; shownInLufs = inL;
        content.repaint (statusRect);
    }

    spectrum.update();
}
