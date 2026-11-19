#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour auraCol { 0xff8fa8ff };   // soft blue-violet
    const Colour auraWarm { 0xffd9b8ff };

    String fmtTime (float s)
    {
        return s < 1.0f ? String (roundToInt (s * 1000.0f)) + " ms" : String (s, s < 10.0f ? 1 : 0) + " s";
    }
}

namespace auraui
{
    //==========================================================================
    AlgoSelector::AlgoSelector (RangedAudioParameter& p)
        : attachment (p, [this] (float v) { setCurrent (roundToInt (v)); }, nullptr)
    {
        for (int i = 0; i < aura::numAlgos; ++i)
        {
            auto* b = buttons.add (new TextButton (aura::algoNames[i].toUpperCase()));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, auraCol);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->setTooltip (String (CharPointer_UTF8 (aura::algoDescription (i))));
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    void AlgoSelector::setCurrent (int a)
    {
        current = a;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == a, dontSendNotification);
    }

    void AlgoSelector::resized()
    {
        const int gap = 6, n = buttons.size();
        const int w = (getWidth() - gap * (n - 1)) / n;
        int x = 0;
        for (int i = 0; i < n; ++i)
        {
            const int bw = i == n - 1 ? getWidth() - x : w;
            buttons[i]->setBounds (x, 0, bw, getHeight());
            x += w + gap;
        }
    }

    //==========================================================================
    void DecayDisplay::setSettings (const aura::Settings& ns)
    {
        auto same = [] (float a, float b) { return std::abs (a - b) < 1.0e-6f; };
        if (valid && ns.algo == s.algo && same (ns.preDelayMs, s.preDelayMs) && same (ns.decayS, s.decayS) && same (ns.size, s.size)
            && same (ns.tone, s.tone) && same (ns.lowCutHz, s.lowCutHz) && same (ns.mod, s.mod))
            return;
        s = ns;
        valid = true;
        repaint();
    }

    void DecayDisplay::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        g.setColour (col::well);
        g.fillRoundedRectangle (full, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (full.reduced (0.5f), 6.0f, 1.0f);

        const auto& c = aura::config (s.algo);
        const float sizeMul = aura::sizeMultiplier (s.size);
        const float pre = s.preDelayMs * 0.001f;

        // time axis
        const float span = pre + s.decayS * 1.1f + 0.05f;
        const float steps[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 10.0f, 15.0f, 25.0f };
        float T = steps[0];
        for (float st : steps) { T = st; if (st >= span) break; }

        auto plot = full.reduced (14.0f, 12.0f);
        plot.removeFromTop (36.0f);
        plot.removeFromBottom (14.0f);
        plot.removeFromLeft (26.0f);

        auto tx = [&] (float t) { return plot.getX() + plot.getWidth() * jlimit (0.0f, 1.0f, t / T); };
        auto dy = [&] (float db) { return plot.getY() + plot.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };

        // grid
        g.setFont (uiFont (9.5f));
        for (int db = 0; db >= -60; db -= 20)
        {
            const float y = dy ((float) db);
            g.setColour (col::panelEdge.withAlpha (0.7f));
            g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (col::textDim);
            g.drawText (String (db), Rectangle<float> (plot.getX() - 30.0f, y - 7.0f, 25.0f, 14.0f), Justification::centredRight);
        }
        const int nTicks = 5;
        for (int i = 0; i <= nTicks; ++i)
        {
            const float t = T * (float) i / (float) nTicks;
            const float x = tx (t);
            g.setColour (col::panelEdge.withAlpha (0.5f));
            g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
            g.setColour (col::textDim);
            const auto lr = i == 0 ? Rectangle<float> (x, plot.getBottom() + 2.0f, 60.0f, 12.0f)
                          : i == nTicks ? Rectangle<float> (x - 60.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f)
                                        : Rectangle<float> (x - 30.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f);
            g.drawText (i == 0 ? String ("0") : fmtTime (t), lr,
                        i == 0 ? Justification::centredLeft : (i == nTicks ? Justification::centredRight : Justification::centred));
        }

        // pre-delay region
        if (pre > 0.0005f)
        {
            auto pr = Rectangle<float> (plot.getX(), plot.getY(), tx (pre) - plot.getX(), plot.getHeight());
            g.setColour (auraCol.withAlpha (0.08f));
            g.fillRect (pr);
            g.setColour (auraCol.withAlpha (0.5f));
            g.drawVerticalLine (roundToInt (pr.getRight()), plot.getY(), plot.getBottom());
        }

        // late tail: build-up then linear (in dB) decay
        const float onset = pre + aura::lineMs[0] * c.lineScale * sizeMul * 0.0005f;
        const float build = 0.012f + 0.06f * c.lineScale * sizeMul;
        const float peakDb = -6.0f + 20.0f * std::log10 (jmax (0.05f, c.lateLevel));
        auto lateDb = [&] (float t, float rt)
        {
            if (t < onset) return -80.0f;
            const float u = (t - onset) / build;
            const float rise = u < 1.0f ? 20.0f * std::log10 (jmax (1.0e-4f, std::sin (u * MathConstants<float>::halfPi))) : 0.0f;
            const float fall = u < 1.0f ? 0.0f : -60.0f * (t - onset - build) / jmax (0.05f, rt);
            return peakDb + rise + fall;
        };

        Path tail;
        tail.startNewSubPath (tx (onset), plot.getBottom());
        const int N = 240;
        for (int i = 0; i <= N; ++i)
        {
            const float t = onset + (T - onset) * (float) i / (float) N;
            tail.lineTo (tx (t), dy (lateDb (t, s.decayS)));
        }
        tail.lineTo (plot.getRight(), plot.getBottom());
        tail.closeSubPath();
        g.setGradientFill (ColourGradient (auraCol.withAlpha (0.45f), plot.getX(), plot.getY(),
                                           auraCol.withAlpha (0.05f), plot.getRight(), plot.getBottom(), false));
        g.fillPath (tail);
        g.setColour (auraCol);
        g.strokePath (tail, PathStrokeType (1.6f));

        // high-frequency decay (Tone)
        const float hfRt = s.decayS * aura::hfRatio (c, s.tone);
        Path hf;
        bool started = false;
        for (int i = 0; i <= N; ++i)
        {
            const float t = onset + build + (T - onset - build) * (float) i / (float) N;
            const float db = lateDb (t, hfRt) - 3.0f;
            if (db < -60.0f) break;
            if (! started) { hf.startNewSubPath (tx (t), dy (db)); started = true; }
            else hf.lineTo (tx (t), dy (db));
        }
        if (started)
        {
            Path dashed;
            const float dash[] = { 4.0f, 4.0f };
            PathStrokeType (1.3f).createDashedStroke (dashed, hf, dash, 2);
            g.setColour (auraWarm.withAlpha (0.8f));
            g.fillPath (dashed);
        }

        // early reflections
        if (c.erLevel > 0.0f)
        {
            const float erMul = c.erScale * sizeMul * 0.001f;
            for (int k = 0; k < aura::numTaps; ++k)
            {
                for (int side = 0; side < 2; ++side)
                {
                    const float t = pre + (side == 0 ? aura::tapMsL[k] : aura::tapMsR[k]) * erMul;
                    const float db = -3.0f + 20.0f * std::log10 (jmax (1.0e-3f, aura::tapGain (k) / aura::tapGain (0) * c.erLevel));
                    const float x = tx (t);
                    g.setColour (auraWarm.withAlpha (side == 0 ? 0.9f : 0.55f));
                    g.drawLine (x, plot.getBottom(), x, dy (db), 1.4f);
                }
            }
        }

        // header: algorithm name + description, legend
        auto head = full.reduced (14.0f, 10.0f).removeFromTop (34.0f);
        g.setColour (auraCol);
        g.setFont (uiFont (14.0f, true));
        g.drawText (aura::algoNames[s.algo].toUpperCase(), head.removeFromTop (17.0f), Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (11.0f));
        g.drawText (String (CharPointer_UTF8 (aura::algoDescription (s.algo))), head, Justification::centredLeft, true);

        auto legend = full.reduced (14.0f, 10.0f).removeFromTop (17.0f).removeFromRight (300.0f);
        g.setFont (uiFont (10.0f));
        auto item = [&] (const String& txt, Colour cl, bool dashedLine)
        {
            auto r = legend.removeFromRight ((float) txt.length() * 5.6f + 26.0f);
            g.setColour (cl);
            const float y = r.getCentreY();
            if (dashedLine) { g.drawLine (r.getX(), y, r.getX() + 6.0f, y, 1.3f); g.drawLine (r.getX() + 10.0f, y, r.getX() + 16.0f, y, 1.3f); }
            else g.drawLine (r.getX(), y, r.getX() + 16.0f, y, 1.8f);
            g.setColour (col::textDim);
            g.drawText (txt, r.withTrimmedLeft (21.0f), Justification::centredLeft);
        };
        item ("Hale", auraCol, false);
        item ("Diskant", auraWarm, true);
        if (c.erLevel > 0.0f) item ("Refleksjoner", auraWarm, false);
    }

    //==========================================================================
    void LevelMeter::setLevels (float l, float r)
    {
        auto toDb = [] (float v) { return v > 1.0e-5f ? 20.0f * std::log10 (v) : -100.0f; };
        const float nl = toDb (l), nr = toDb (r);
        levL = nl > levL ? nl : jmax (nl, levL - 1.5f);
        levR = nr > levR ? nr : jmax (nr, levR - 1.5f);
        repaint();
    }

    void LevelMeter::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        auto labels = r.removeFromBottom (14.0f);
        const float w = (r.getWidth() - 4.0f) / 2.0f;
        auto bar = [&] (Rectangle<float> b, float db)
        {
            g.setColour (col::well);
            g.fillRoundedRectangle (b, 3.0f);
            const float p = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto f = b.withTop (b.getBottom() - b.getHeight() * p);
            g.setColour (db > -1.0f ? col::red : auraCol);
            g.fillRoundedRectangle (f, 3.0f);
        };
        bar (r.withWidth (w), levL);
        bar (r.withTrimmedLeft (w + 4.0f), levR);
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        g.drawText ("UT", labels, Justification::centred);
    }
}

//==============================================================================
AuraEditor::AuraEditor (AuraProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      algoSelector (*p.apvts.getParameter (ParamIDs::algo))
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    content.addAndMakeVisible (algoSelector);
    content.addAndMakeVisible (display);
    content.addAndMakeVisible (meter);

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &preDelay, ParamIDs::preDelay }, { &decay, ParamIDs::decay }, { &size, ParamIDs::size },
        { &tone, ParamIDs::tone }, { &lowCut, ParamIDs::lowCut }, { &mod, ParamIDs::mod } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, auraCol);
    }
    tone.slider.getProperties().set ("bipolar", true);

    preDelay.slider.setTooltip (CharPointer_UTF8 ("Tid før reverben starter. 20–80 ms holder vokalen tydelig foran reverben."));
    decay.slider.setTooltip (CharPointer_UTF8 ("Hvor lang tid det tar før reverben har dødd ut (RT60)."));
    size.slider.setTooltip (CharPointer_UTF8 ("Romstørrelse: avstand mellom refleksjonene og hvor tett halen er. Liten = tett, stor = åpen."));
    tone.slider.setTooltip (CharPointer_UTF8 ("Mørkere (−) eller lysere (+) reverb. Styrer både diskanten inn og hvor fort diskanten dør ut."));
    lowCut.slider.setTooltip (CharPointer_UTF8 ("Fjerner bunn fra reverben, så miksen ikke blir grøtete. 100–250 Hz er vanlig på vokal."));
    mod.slider.setTooltip (CharPointer_UTF8 ("Svak chorus i halen. Gjør reverben bredere og mykere, uten metallisk ring."));

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    display.setSettings (proc.readSettings());
    startTimerHz (30);
}

AuraEditor::~AuraEditor()
{
    setLookAndFeel (nullptr);
}

void AuraEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void AuraEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void AuraEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    algoSelector.setBounds (12, 56, designW - 24, 32);

    meter.setBounds (designW - 12 - 30, 100, 30, 190);
    display.setBounds (12, 100, meter.getX() - 8 - 12, 190);

    knobPanel = { 12, 302, designW - 24, designH - 302 - 12 };
    auto r = knobPanel.reduced (16, 14);
    r.removeFromTop (6);
    // two large knobs (pre-delay, decay), four smaller ones
    const int big = 150, small = (r.getWidth() - 2 * big - 40) / 4;
    auto bigRow = r.removeFromLeft (2 * big);
    preDelay.setBounds (bigRow.removeFromLeft (big));
    decay.setBounds (bigRow);
    r.removeFromLeft (40);
    auto smallRow = r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 118));
    for (auto* k : { &size, &tone, &lowCut, &mod })
        k->setBounds (smallRow.removeFromLeft (small));
}

void AuraEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    g.setFont (uiFont (19.0f, true));
    g.setColour (auraCol);
    g.drawText ("Aura", Rectangle<float> (14.0f, 0.0f, 60.0f, 44.0f), Justification::centredLeft);
    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText (String (CharPointer_UTF8 ("SB AUDIO  ·  REVERB")), Rectangle<float> (66.0f, 0.0f, 160.0f, 44.0f), Justification::centredLeft);

    auto kp = knobPanel.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (kp, 6.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (kp.reduced (0.5f), 6.0f, 1.0f);

    // separator between the main and the shaping controls
    const float sx = (float) decay.getRight() + 20.0f;
    g.drawVerticalLine (roundToInt (sx), kp.getY() + 18.0f, kp.getBottom() - 18.0f);

    g.setColour (col::textDim);
    g.setFont (uiFont (9.5f, true));
    g.drawText (String (CharPointer_UTF8 ("100 % WET  ·  BRUK SOM SEND")), Rectangle<float> (sx + 14.0f, kp.getY() + 8.0f, 300.0f, 14.0f), Justification::centredLeft);
}

void AuraEditor::timerCallback()
{
    display.setSettings (proc.readSettings());
    meter.setLevels (proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f));
    proc.inPeak.store (0.0f);
}
