#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour dovreCol  { 0xff8fd16a };   // moss green
    const Colour dovreSoft { 0xffcdeaa0 };   // pale lichen (right channel)

    String utf8 (const char* s) { return String (CharPointer_UTF8 (s)); }

    String fmtMs (float ms)
    {
        return ms < 1000.0f ? String (ms, ms < 100.0f ? 1 : 0) + " ms" : String (ms * 0.001f, 2) + " s";
    }

    void drawPanel (Graphics& g, Rectangle<int> r, const String& title)
    {
        auto f = r.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (f, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (dovreCol);
        g.fillEllipse (f.getX() + 12.0f, f.getY() + 10.0f, 5.0f, 5.0f);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (title, Rectangle<float> (f.getX() + 22.0f, f.getY() + 5.0f, 200.0f, 15.0f), Justification::centredLeft);
    }
}

namespace dovreui
{
    //==========================================================================
    ChoiceButtons::ChoiceButtons (RangedAudioParameter& p, const StringArray& labels, Colour on)
        : attachment (p, [this] (float v) { setCurrent (roundToInt (v)); }, nullptr)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new TextButton (labels[i].toUpperCase()));
            b->setClickingTogglesState (false);
            b->setColour (TextButton::buttonOnColourId, on);
            b->setColour (TextButton::textColourOnId, col::bg);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
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
    void EchoDisplay::setSettings (const dovre::Settings& ns, bool sy, const String& nn, float b)
    {
        auto same = [] (float x, float y) { return std::abs (x - y) < 1.0e-4f; };
        if (valid && ns.mode == s.mode && ns.style == s.style && same (ns.delayMsL, s.delayMsL) && same (ns.delayMsR, s.delayMsR)
            && same (ns.feedback, s.feedback) && same (ns.diffusion, s.diffusion) && same (ns.diffSize, s.diffSize)
            && same (ns.pitchSt, s.pitchSt) && same (ns.detuneCents, s.detuneCents) && same (ns.mix, s.mix)
            && same (ns.highCutHz, s.highCutHz) && same (ns.lowCutHz, s.lowCutHz) && same (ns.age, s.age)
            && sy == synced && nn == note && same (b, bpm))
            return;
        s = ns; synced = sy; note = nn; bpm = b; valid = true;
        repaint();
    }

    void EchoDisplay::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        g.setColour (col::well);
        g.fillRoundedRectangle (full, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (full.reduced (0.5f), 6.0f, 1.0f);

        // ---- taps -----------------------------------------------------------
        struct Tap { float t, amp; int ch; };   // ch: 0 = L, 1 = R, 2 = both
        std::vector<Tap> taps;
        const float dl = s.delayMsL * 0.001f, dr = (s.style == dovre::Single ? s.delayMsL : s.delayMsR) * 0.001f;
        const float fb = jlimit (0.0f, 1.0f, s.feedback);
        // EQ/colour loss per repeat, rough: darker / thinner loops die faster
        const float loss = (s.mode == dovre::Digital ? 1.0f : 0.92f) * (s.highCutHz < 3000.0f ? 0.9f : 1.0f);
        const float g1 = fb * loss;

        float span = jmax (dl, dr) * 6.0f;
        {
            const float rep = g1 < 0.02f ? 1.0f : jmin (12.0f, std::log (0.004f) / std::log (jmax (0.02f, g1)));
            span = s.style == dovre::PingPong ? 0.5f * (dl + dr) * (rep + 1.3f) : jmax (dl, dr) * (rep + 1.3f);
        }
        const float steps[] = { 0.05f, 0.1f, 0.2f, 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 8.0f, 10.0f, 15.0f, 20.0f, 30.0f };
        float T = steps[0];
        for (float st : steps) { T = st; if (st >= span) break; }

        if (s.style == dovre::Single)
        {
            float a = 1.0f;
            for (float t = dl; t <= T && a > 0.004f && taps.size() < 80; t += dl, a *= g1) taps.push_back ({ t, a, 2 });
        }
        else if (s.style == dovre::Dual)
        {
            float a = 1.0f;
            for (float t = dl; t <= T && a > 0.004f && taps.size() < 80; t += dl, a *= g1) taps.push_back ({ t, a, 0 });
            a = 1.0f;
            for (float t = dr; t <= T && a > 0.004f && taps.size() < 160; t += dr, a *= g1) taps.push_back ({ t, a, 1 });
        }
        else
        {
            float a = 1.0f, t = 0.0f;
            for (int k = 0; k < 120; ++k)
            {
                t += (k % 2 == 0) ? dl : dr;
                if (t > T || a < 0.004f) break;
                taps.push_back ({ t, a, k % 2 });
                a *= g1;
            }
        }

        // ---- layout ---------------------------------------------------------
        auto plot = full.reduced (14.0f, 10.0f);
        auto head = plot.removeFromTop (34.0f);
        plot.removeFromBottom (14.0f);
        const float mid = plot.getCentreY();
        const float halfH = plot.getHeight() * 0.5f - 2.0f;

        auto tx = [&] (float t) { return plot.getX() + plot.getWidth() * jlimit (0.0f, 1.0f, t / T); };
        auto hy = [&] (float amp) { const float db = 20.0f * std::log10 (jmax (1.0e-5f, amp)); return halfH * jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f); };

        // grid
        g.setFont (uiFont (9.5f));
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
            g.drawText (i == 0 ? String ("0") : fmtMs (t * 1000.0f), lr,
                        i == 0 ? Justification::centredLeft : (i == nTicks ? Justification::centredRight : Justification::centred));
        }
        g.setColour (col::panelEdge);
        g.drawHorizontalLine (roundToInt (mid), plot.getX(), plot.getRight());
        g.setColour (col::textDim);
        g.drawText ("L", Rectangle<float> (plot.getRight() - 14.0f, plot.getY(), 14.0f, 12.0f), Justification::centredRight);
        g.drawText ("R", Rectangle<float> (plot.getRight() - 14.0f, plot.getBottom() - 12.0f, 14.0f, 12.0f), Justification::centredRight);

        // dry signal
        g.setColour (col::text.withAlpha (0.6f));
        g.fillRect (Rectangle<float> (plot.getX(), mid - halfH, 2.0f, halfH * 2.0f));

        // echoes
        const float wetLevel = jlimit (0.0f, 1.0f, 2.0f * s.mix);
        const float blur = s.diffusion * (0.15f + 1.6f * s.diffSize) * 0.05f;           // seconds of smear
        const float pxBlur = plot.getWidth() * blur / T;
        const bool rev = s.mode == dovre::Reverse;

        for (size_t i = 0; i < taps.size(); ++i)
        {
            const auto& tp = taps[i];
            const float amp = tp.amp * jmax (0.25f, wetLevel) * std::pow (s.diffusion > 0.0f ? 1.0f - 0.35f * s.diffusion : 1.0f, (float) i * 0.5f);
            const float h = hy (amp);
            const float x = tx (tp.t);
            const Colour c = tp.ch == 1 ? dovreSoft : dovreCol;
            for (int side = 0; side < 2; ++side)
            {
                if (tp.ch != 2 && tp.ch != side) continue;
                const float dir = side == 0 ? -1.0f : 1.0f;
                if (rev)
                {
                    const float w = jmax (6.0f, plot.getWidth() * (side == 0 ? dl : dr) * 0.5f / T);
                    Path p;
                    p.startNewSubPath (x - w, mid);
                    p.quadraticTo (x - w * 0.15f, mid, x, mid + dir * h);
                    p.lineTo (x, mid);
                    p.closeSubPath();
                    g.setColour (c.withAlpha (0.35f));
                    g.fillPath (p);
                    g.setColour (c);
                    g.drawLine (x, mid, x, mid + dir * h, 1.6f);
                }
                else if (pxBlur > 2.0f)
                {
                    const float w = jmax (3.0f, pxBlur * (1.0f + 0.4f * (float) i));
                    Path p;
                    p.startNewSubPath (x - w * 0.2f, mid);
                    p.quadraticTo (x, mid + dir * h * 1.6f, x + w, mid);
                    p.closeSubPath();
                    g.setGradientFill (ColourGradient (c.withAlpha (0.7f), x, mid + dir * h, c.withAlpha (0.05f), x + w, mid, false));
                    g.fillPath (p);
                    g.setColour (c);
                    g.drawLine (x, mid, x, mid + dir * h * (1.0f - s.diffusion * 0.6f), 1.4f);
                }
                else
                {
                    g.setColour (c.withAlpha (0.18f));
                    g.fillRect (Rectangle<float> (x - 3.0f, side == 0 ? mid - h : mid, 6.0f, h));
                    g.setColour (c);
                    g.fillRect (Rectangle<float> (x - 1.0f, side == 0 ? mid - h : mid, 2.0f, h));
                }
            }
        }

        // header
        g.setColour (dovreCol);
        g.setFont (uiFont (14.0f, true));
        g.drawText (dovre::modeNames[s.mode].toUpperCase() + String (CharPointer_UTF8 ("  ·  ")) + dovre::styleNames[s.style].toUpperCase(),
                    head.removeFromTop (17.0f), Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (11.0f));
        g.drawText (utf8 (dovre::modeDescription (s.mode)), head.withTrimmedRight (250.0f), Justification::centredLeft, true);

        auto right = full.reduced (14.0f, 10.0f).removeFromTop (34.0f).removeFromRight (250.0f);
        g.setColour (col::text);
        g.setFont (uiFont (12.0f, true));
        String times = s.style == dovre::Single ? fmtMs (s.delayMsL)
                                                : "L " + fmtMs (s.delayMsL) + String (CharPointer_UTF8 ("  ·  R ")) + fmtMs (dr * 1000.0f);
        g.drawText (times, right.removeFromTop (17.0f), Justification::centredRight);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.5f));
        String sub = synced ? note + String (CharPointer_UTF8 (" ved ")) + String (roundToInt (bpm)) + " BPM" : String ("Fri tid");
        if (s.mode == dovre::Pitch || s.mode == dovre::Ghost)
        {
            if (std::abs (s.pitchSt) > 0.5f) sub << String (CharPointer_UTF8 ("  ·  ")) << (s.pitchSt > 0 ? "+" : "") << roundToInt (s.pitchSt) << " st";
            if (std::abs (s.detuneCents) > 0.5f) sub << String (CharPointer_UTF8 ("  ·  ±")) << roundToInt (std::abs (s.detuneCents)) << " ct";
        }
        g.drawText (sub, right, Justification::centredRight);
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
            g.setColour (db > -1.0f ? col::red : dovreCol);
            g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * p), 3.0f);
        };
        bar (r.withWidth (w), levL);
        bar (r.withTrimmedLeft (w + 4.0f), levR);
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        g.drawText ("UT", labels, Justification::centred);
    }
}

//==============================================================================
DovregubbenEditor::DovregubbenEditor (DovregubbenProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      modeButtons (*p.apvts.getParameter (ParamIDs::mode), dovre::modeNames, dovreCol),
      styleButtons (*p.apvts.getParameter (ParamIDs::style), dovre::styleNames, dovreCol)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    content.addAndMakeVisible (modeButtons);
    content.addAndMakeVisible (styleButtons);
    content.addAndMakeVisible (display);
    content.addAndMakeVisible (meter);

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &mix, ParamIDs::mix }, { &delay, ParamIDs::delay }, { &note, ParamIDs::note }, { &feedback, ParamIDs::feedback },
        { &ratio, ParamIDs::ratio }, { &width, ParamIDs::width }, { &age, ParamIDs::age }, { &duck, ParamIDs::duck },
        { &diffusion, ParamIDs::diffusion }, { &diffSize, ParamIDs::diffSize }, { &rate, ParamIDs::modRate }, { &depth, ParamIDs::modDepth },
        { &lowCut, ParamIDs::lowCut }, { &highCut, ParamIDs::highCut }, { &pitch, ParamIDs::pitch }, { &detune, ParamIDs::detune } };
    for (auto& [k, id] : knobs)
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, dovreCol);
    }
    pitch.slider.getProperties().set ("bipolar", true);
    detune.slider.getProperties().set ("bipolar", true);

    syncBtn.setClickingTogglesState (true);
    syncBtn.setColour (TextButton::buttonOnColourId, dovreCol);
    syncBtn.setColour (TextButton::textColourOnId, col::bg);
    content.addAndMakeVisible (syncBtn);
    syncAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::sync, syncBtn);

    // texts for the info strip at the bottom (shown when the mouse is over a control)
    setInfo (mix,       "MIX – forholdet mellom tørt signal og ekko. 50 % = begge fullt. Bruk 100 % når Dovregubben står på en aux (send).");
    setInfo (delay,     "DELAY – tiden til første ekko (venstre kanal). 15–35 ms = dobling, 80–140 ms = slapback, 250 ms+ = tydelige ekko.");
    setInfo (note,      "DELAY – notelengde i takt med prosjektets tempo. T = triol, D = punktert.");
    setInfo (syncBtn,   "SYNC – lås delaytiden til tempoet i Logic (notelengder) i stedet for millisekunder.");
    setInfo (feedback,  "FEEDBACK – hvor mye av ekkoet som sendes tilbake. 0 % = ett ekko (dobling), 30–50 % = flere repetisjoner, 90 %+ = nesten uendelig.");
    setInfo (ratio,     "DELAY R – høyre kanals tid i prosent av venstre. 100 % = like, andre verdier gir bredde og rytmiske mønstre. Brukes i Dual og Ping-Pong.");
    setInfo (width,     "WIDTH – stereobredden på ekkoene. 0 % = mono i midten, 100 % = full bredde.");
    setInfo (age,       "AGE – slitasje og karakter: metning, mørkere klang, støy, wow og flutter. Hva den gjør, avhenger av MODE.");
    setInfo (duck,      "DUCKING – demper ekkoene mens du synger eller spiller, og slipper dem fram i pausene. Gir tydelig vokal med mye ekko.");
    setInfo (diffusion, "DIFFUSION AMOUNT – smører ut ekkoene, fra tydelige repetisjoner til en myk, reverb-aktig vask.");
    setInfo (diffSize,  "DIFFUSION SIZE – hvor lang tid utsmøringen tar. Liten = tett og kort, stor = lang og romslig.");
    setInfo (rate,      "MOD RATE – hastigheten på modulasjonen av delaytiden (Hz). Lav = svak svevning, høy = vibrato.");
    setInfo (depth,     "MOD DEPTH – hvor mye delaytiden moduleres. Litt modulasjon gjør doblinger og ekko levende og brede.");
    setInfo (lowCut,    "LOW CUT – fjerner bunn fra ekkoene (inne i sløyfen, så hver repetisjon blir tynnere). 150–300 Hz holder miksen ren.");
    setInfo (highCut,   "HIGH CUT – fjerner diskant fra ekkoene (inne i sløyfen, så hver repetisjon blir mørkere). 4–7 kHz legger ekkoet bak vokalen.");
    setInfo (pitch,     "PITCH – i Pitch-mode: transponering i halvtoner for hver repetisjon (±12). I Ghost: størrelsen på frekvensskiftet.");
    setInfo (detune,    "DETUNE – i Pitch-mode: venstre kanal stemmes opp og høyre ned (cent). 5–12 cent = klassisk dobling.");
    for (int i = 0; i < modeButtons.buttons.size(); ++i)
        infos.push_back ({ modeButtons.buttons[i], "MODE " + dovre::modeNames[i].toUpperCase() + String (CharPointer_UTF8 (" – ")) + utf8 (dovre::modeDescription (i)) });
    infos.push_back ({ styleButtons.buttons[0], utf8 ("STYLE SINGLE – ett ekko i midten (mono).") });
    infos.push_back ({ styleButtons.buttons[1], utf8 ("STYLE DUAL – to uavhengige ekko, venstre og høyre, hver med sin tid (DELAY og DELAY R).") });
    infos.push_back ({ styleButtons.buttons[2], utf8 ("STYLE PING-PONG – ekkoene hopper mellom venstre og høyre.") });

    content.addMouseListener (this, true);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    timerCallback();
    startTimerHz (30);
}

DovregubbenEditor::~DovregubbenEditor()
{
    content.removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void DovregubbenEditor::setInfo (Component& c, const char* text)
{
    infos.push_back ({ &c, utf8 (text) });
}

String DovregubbenEditor::infoFor (Component* c) const
{
    for (; c != nullptr && c != &content; c = c->getParentComponent())
        for (auto& [comp, txt] : infos)
            if (comp == c)
                return txt;
    return {};
}

void DovregubbenEditor::mouseEnter (const MouseEvent& e)
{
    auto t = infoFor (e.eventComponent);
    if (t.isNotEmpty() && t != info) { info = t; content.repaint (infoStrip); }
}

void DovregubbenEditor::mouseExit (const MouseEvent&)
{
    if (info.isNotEmpty()) { info = {}; content.repaint (infoStrip); }
}

void DovregubbenEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void DovregubbenEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void DovregubbenEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    modeButtons.setBounds (62, 54, 580, 30);
    styleButtons.setBounds (712, 54, designW - 12 - 712, 30);

    meter.setBounds (designW - 12 - 30, 94, 30, 170);
    display.setBounds (12, 94, meter.getX() - 8 - 12, 170);

    const int top = 276, panelH = 280;
    delayPanel = { 12, top, 340, panelH };
    const int rx = delayPanel.getRight() + 12, rw = (designW - 12 - rx - 12) / 2, rh = (panelH - 12) / 2;
    diffPanel  = { rx, top, rw, rh };
    modPanel   = { rx + rw + 12, top, designW - 12 - (rx + rw + 12), rh };
    eqPanel    = { rx, top + rh + 12, rw, rh };
    pitchPanel = { rx + rw + 12, top + rh + 12, designW - 12 - (rx + rw + 12), rh };
    infoStrip  = { 12, designH - 12 - 26, designW - 24, 26 };

    // delay panel: big MIX / DELAY / FEEDBACK, then four smaller ones
    syncBtn.setBounds (delayPanel.getRight() - 12 - 56, delayPanel.getY() + 6, 56, 20);
    auto r = delayPanel.reduced (10, 8);
    r.removeFromTop (24);
    auto row1 = r.removeFromTop (130);
    const int bw = row1.getWidth() / 3;
    mix.setBounds (row1.removeFromLeft (bw));
    delay.setBounds (row1.removeFromLeft (bw));
    note.setBounds (delay.getBounds());
    feedback.setBounds (row1);
    r.removeFromTop (4);
    const int sw = r.getWidth() / 4;
    for (auto* k : { &ratio, &width, &age })
        k->setBounds (r.removeFromLeft (sw).withSizeKeepingCentre (sw, jmin (r.getHeight(), 104)));
    duck.setBounds (r.withSizeKeepingCentre (r.getWidth(), jmin (r.getHeight(), 104)));

    auto pair = [] (Rectangle<int> panel, sbaudio::Knob& a, sbaudio::Knob& b)
    {
        auto q = panel.reduced (14, 6);
        q.removeFromTop (20);
        a.setBounds (q.removeFromLeft (q.getWidth() / 2));
        b.setBounds (q);
    };
    pair (diffPanel, diffusion, diffSize);
    pair (modPanel, rate, depth);
    pair (eqPanel, lowCut, highCut);
    pair (pitchPanel, pitch, detune);
}

void DovregubbenEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 360.0f, 44.0f }, "Dovregubben", "DELAY", dovreCol);

    g.setColour (col::textDim);
    g.setFont (uiFont (10.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("MODE", Rectangle<int> (12, 54, 46, 30), Justification::centredLeft);
    g.drawText ("STYLE", Rectangle<int> (656, 54, 52, 30), Justification::centredLeft);

    drawPanel (g, delayPanel, "DELAY");
    drawPanel (g, diffPanel, "DIFFUSION");
    drawPanel (g, modPanel, "MODULATION");
    drawPanel (g, eqPanel, "EQ");
    drawPanel (g, pitchPanel, lastMode == dovre::Ghost ? "GHOST  ·  FREKVENSSKIFT" : "PITCH");

    // info strip
    auto is = infoStrip.toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (is, 5.0f);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (is.reduced (0.5f), 5.0f, 1.0f);
    g.setFont (uiFont (11.0f));
    if (info.isNotEmpty())
    {
        g.setColour (col::text);
        g.drawText (info, is.reduced (12.0f, 0.0f), Justification::centredLeft, true);
    }
    else
    {
        g.setColour (col::textDim);
        g.drawText (utf8 ("Hold musen over en kontroll for å se hva den gjør.  ·  Dobbeltklikk på en knapp for standardverdi."),
                    is.reduced (12.0f, 0.0f), Justification::centredLeft, true);
    }
}

void DovregubbenEditor::timerCallback()
{
    const auto s = proc.readSettings();
    const bool synced = proc.apvts.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
    const int ni = jlimit (0, dovre::noteNames.size() - 1, (int) proc.apvts.getRawParameterValue (ParamIDs::note)->load());
    display.setSettings (s, synced, dovre::noteNames[ni], proc.bpm.load());

    if (synced != lastSync || s.mode != lastMode || s.style != lastStyle)
    {
        const bool panelTitleChanged = (s.mode == dovre::Ghost) != (lastMode == dovre::Ghost);
        lastSync = synced; lastMode = s.mode; lastStyle = s.style;
        delay.setVisible (! synced);
        note.setVisible (synced);
        const bool pitchOn = s.mode == dovre::Pitch || s.mode == dovre::Ghost;
        pitch.setAlpha (pitchOn ? 1.0f : 0.35f);
        detune.setAlpha (pitchOn ? 1.0f : 0.35f);
        ratio.setAlpha (s.style == dovre::Single ? 0.35f : 1.0f);
        if (panelTitleChanged) content.repaint (pitchPanel);
    }

    meter.setLevels (proc.outPeakL.exchange (0.0f), proc.outPeakR.exchange (0.0f));
    proc.inPeak.store (0.0f);
}
