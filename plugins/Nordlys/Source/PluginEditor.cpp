#include "PluginEditor.h"

using namespace juce;
using namespace nordlys;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour auroraGreen  { 0xff52e5a4 };
    const Colour auroraTeal   { 0xff3fd0c9 };
    const Colour auroraViolet { 0xffa98bff };
    const Colour auroraPink   { 0xffe88bd8 };

    const Identifier uiPageId ("uiPage");

    void drawSection (Graphics& g, Rectangle<float> r, const String& title, Colour titleCol = col::textDim)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (titleCol);
            g.setFont (uiFont (10.0f, true));
            g.drawText (title, r.reduced (12.0f, 6.0f).removeFromTop (14.0f), Justification::centredLeft, false);
        }
    }

    void drawSubTitle (Graphics& g, float x, float y, const String& t)
    {
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (t, Rectangle<float> (x, y, 260.0f, 14.0f), Justification::centredLeft, false);
    }

    void divider (Graphics& g, float x, float y1, float y2)
    {
        g.setColour (col::panelEdge);
        g.drawVerticalLine (roundToInt (x), y1, y2);
    }

    // ---- layer page geometry (page-local) ----
    const Rectangle<int> secOsc    { 0,   0, 864, 140 };
    const Rectangle<int> secTune   { 0, 148, 864, 122 };
    const Rectangle<int> secFilter { 0, 278, 864, 136 };
    const Rectangle<int> secAmp    { 0, 422, 864, 136 };

    // ---- fx page geometry ----
    const Rectangle<int> secArp    { 0,   0, 864, 132 };
    const Rectangle<int> secCho    { 0, 140, 280, 196 };
    const Rectangle<int> secDly    { 292, 140, 280, 196 };
    const Rectangle<int> secRev    { 584, 140, 280, 196 };
    const Rectangle<int> secPlay   { 0, 344, 864, 140 };
}

namespace nordlysui
{
    //==========================================================================
    OrbPad::OrbPad (RangedAudioParameter& x, RangedAudioParameter& y)
        : ax (x, [this] (float v) { px = v * 0.01f; repaint(); }, nullptr),
          ay (y, [this] (float v) { py = v * 0.01f; repaint(); }, nullptr)
    {
        ax.sendInitialUpdate();
        ay.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
        setTooltip (String (CharPointer_UTF8 ("Orb: dra punktet for å forme lyden. X og Y er modulasjonskilder (se MODULASJON). "
                                              "Med FART og RADIUS sirkler Orb automatisk rundt punktet. Dobbeltklikk = midten.")));
    }

    Rectangle<float> OrbPad::area() const
    {
        const auto b = getLocalBounds().toFloat().reduced (6.0f);
        const float d = jmin (b.getWidth(), b.getHeight());
        return Rectangle<float> (d, d).withCentre (b.getCentre());
    }

    void OrbPad::setLive (float lx, float ly)
    {
        if (std::abs (lx - liveX) < 1.0e-4f && std::abs (ly - liveY) < 1.0e-4f)
            return;
        liveX = lx; liveY = ly;
        trail[(size_t) trailPos] = { lx, ly };
        trailPos = (trailPos + 1) % (int) trail.size();
        repaint();
    }

    void OrbPad::paint (Graphics& g)
    {
        const auto a = area();
        const auto c = a.getCentre();
        const float r = a.getWidth() * 0.5f;
        auto toPt = [&] (float x, float y) { return Point<float> (c.x + x * r * 0.86f, c.y - y * r * 0.86f); };

        // outer glow
        g.setGradientFill (ColourGradient (auroraTeal.withAlpha (0.18f), c.x, c.y, auroraTeal.withAlpha (0.0f), c.x + r * 1.05f, c.y, true));
        g.fillEllipse (a.expanded (6.0f));

        // sphere body
        ColourGradient body (Colour (0xff1d2a35), c.x - r * 0.35f, c.y - r * 0.45f, Colour (0xff07090c), c.x + r * 0.6f, c.y + r * 0.8f, true);
        g.setGradientFill (body);
        g.fillEllipse (a);

        // aurora bands that follow the live position
        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addEllipse (a);
            g.reduceClipRegion (clip);
            const auto lp = toPt (liveX, liveY);
            g.setGradientFill (ColourGradient (auroraGreen.withAlpha (0.35f), lp.x, lp.y, auroraGreen.withAlpha (0.0f), lp.x + r * 0.9f, lp.y, true));
            g.fillEllipse (a);
            g.setGradientFill (ColourGradient (auroraViolet.withAlpha (0.25f), c.x + (c.x - lp.x) * 0.6f, c.y + (c.y - lp.y) * 0.6f,
                                               auroraViolet.withAlpha (0.0f), c.x + (c.x - lp.x) * 0.6f + r * 0.8f, c.y, true));
            g.fillEllipse (a);

            // latitude / longitude lines
            g.setColour (Colours::white.withAlpha (0.06f));
            for (int i = 1; i < 4; ++i)
            {
                const float f = (float) i / 4.0f;
                g.drawEllipse (a.reduced (r * f * 0.0f, r * f), 1.0f);
                g.drawEllipse (a.reduced (r * f, r * f * 0.0f), 1.0f);
            }
            g.drawHorizontalLine (roundToInt (c.y), a.getX(), a.getRight());
            g.drawVerticalLine (roundToInt (c.x), a.getY(), a.getBottom());
        }
        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawEllipse (a.reduced (0.5f), 1.2f);
        // specular highlight
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), c.x - r * 0.3f, a.getY(), Colours::white.withAlpha (0.0f), c.x - r * 0.3f, c.y, false));
        g.fillEllipse (a.reduced (r * 0.25f, r * 0.05f).withHeight (r * 0.8f));

        // trail
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const int age = ((int) trail.size() + trailPos - 1 - (int) i) % (int) trail.size();
            const float alpha = 0.35f * (1.0f - (float) age / (float) trail.size());
            const auto p = toPt (trail[i].x, trail[i].y);
            g.setColour (auroraGreen.withAlpha (alpha));
            g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (p));
        }

        // set point
        const auto sp = toPt (px, py);
        g.setColour (Colours::white.withAlpha (isMouseOverOrDragging() ? 0.9f : 0.6f));
        g.drawEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (sp), 1.6f);

        // live point
        const auto lp = toPt (liveX, liveY);
        g.setGradientFill (ColourGradient (auroraGreen, lp.x, lp.y, auroraGreen.withAlpha (0.0f), lp.x + 14.0f, lp.y, true));
        g.fillEllipse (Rectangle<float> (28.0f, 28.0f).withCentre (lp));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (lp));

        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        g.drawText ("X " + String (roundToInt (px * 100.0f)) + "   Y " + String (roundToInt (py * 100.0f)),
                    getLocalBounds().toFloat().removeFromBottom (14.0f), Justification::centredRight, false);
    }

    void OrbPad::setFromMouse (Point<float> p)
    {
        const auto a = area();
        const float r = a.getWidth() * 0.5f * 0.86f;
        float x = (p.x - a.getCentreX()) / r, y = (a.getCentreY() - p.y) / r;
        x = jlimit (-1.0f, 1.0f, x);
        y = jlimit (-1.0f, 1.0f, y);
        ax.setValueAsPartOfGesture (x * 100.0f);
        ay.setValueAsPartOfGesture (y * 100.0f);
    }

    void OrbPad::mouseDown (const MouseEvent& e) { ax.beginGesture(); ay.beginGesture(); setFromMouse (e.position); }
    void OrbPad::mouseDrag (const MouseEvent& e) { setFromMouse (e.position); }
    void OrbPad::mouseUp (const MouseEvent&)     { ax.endGesture(); ay.endGesture(); }
    void OrbPad::mouseDoubleClick (const MouseEvent&) { ax.setValueAsCompleteGesture (0.0f); ay.setValueAsCompleteGesture (0.0f); }

    //==========================================================================
    void WaveDisplay::setState (const State& s)
    {
        if (valid && ! (s != st)) return;
        st = s;
        valid = true;
        repaint();
    }

    void WaveDisplay::paint (Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::well);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        auto plot = b.reduced (10.0f, 18.0f);
        g.setColour (col::panelEdge.withAlpha (0.6f));
        g.drawHorizontalLine (roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

        const auto& bank = WaveBank::get();
        const int N = 180;
        Path p;
        Random rnd (7);
        float lp = 0.0f;
        const float coef = std::min (1.0f, std::exp2 (st.shape * 7.2f) * 150.0f / 6000.0f);
        for (int i = 0; i <= N; ++i)
        {
            const float ph = (float) i / (float) N;
            float v = 0.0f;
            switch (st.osc)
            {
                case oscFM:
                {
                    const float ratio = fmRatios[fmRatioIndex (st.sym)];
                    const float idx = st.shape * 10.0f / MathConstants<float>::twoPi;
                    const float cyc = ratio < 1.0f ? 2.0f : 1.0f;
                    v = bank.sine (ph * cyc + idx * bank.sine (ph * cyc * ratio)) * 0.9f;
                    break;
                }
                case oscNoise:
                    lp += coef * (rnd.nextFloat() * 2.0f - 1.0f - lp);
                    v = jlimit (-1.0f, 1.0f, lp * (1.2f + (1.0f - coef) * 2.0f));
                    break;
                default:
                {
                    const int b2 = st.osc == oscAnalog ? 0 : st.wave;
                    float q = ph;
                    if (st.osc != oscGranular)
                    {
                        const float w = jmap (st.sym, 0.06f, 0.94f);
                        q = q < w ? 0.5f * q / w : 0.5f + 0.5f * (q - w) / (1.0f - w);
                    }
                    v = bank.readMorph (b2, st.shape, 0, jlimit (0.0f, 0.99999f, q));
                    break;
                }
            }
            const float x = plot.getX() + ph * plot.getWidth();
            const float y = plot.getCentreY() - v * plot.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        const auto c = st.on ? auroraGreen : col::textDim;
        g.setColour (c.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

        // granular: show the region the grains are taken from
        if (st.osc == oscGranular)
        {
            auto bar = b.reduced (10.0f, 0.0f).removeFromBottom (12.0f).withTrimmedBottom (4.0f);
            g.setColour (col::panelEdge);
            g.fillRoundedRectangle (bar, 2.0f);
            const float lo = jlimit (0.0f, 1.0f, st.shape - st.spray * 0.5f), hi = jlimit (0.0f, 1.0f, st.shape + st.spray * 0.5f);
            g.setColour (auroraViolet.withAlpha (0.7f));
            g.fillRoundedRectangle (bar.withX (bar.getX() + bar.getWidth() * lo).withWidth (jmax (3.0f, bar.getWidth() * (hi - lo))), 2.0f);
            g.setColour (Colours::white);
            g.fillRect (Rectangle<float> (2.0f, bar.getHeight() + 2.0f).withCentre ({ bar.getX() + bar.getWidth() * st.shape, bar.getCentreY() }));
        }

        const String dot (CharPointer_UTF8 ("  ·  "));
        String title = "LAG " + layerLetter (st.layer) + dot + oscTypeNames[st.osc].toUpperCase();
        if (st.osc == oscWavetable || st.osc == oscGranular) title << dot << waveNames[st.wave].toUpperCase();
        if (st.osc == oscFM) title << dot << "RATIO " << String (fmRatios[fmRatioIndex (st.sym)], 1);
        if (! st.on) title << "  (AV)";
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        g.drawText (title, b.reduced (10.0f, 3.0f).removeFromTop (14.0f), Justification::centredLeft, false);
    }

    //==========================================================================
    ModRow::ModRow (NordlysProcessor& p, int s) : slot (s)
    {
        auto setup = [this] (ComboBox& c, const StringArray& items)
        {
            c.addItemList (items, 1);
            addAndMakeVisible (c);
        };
        setup (src, modSrcNames);
        setup (via, modSrcNames);
        setup (dst, modDstNames);
        setup (layer, layerTargetNames);
        via.setTooltip (String (CharPointer_UTF8 ("Via: mengden skaleres med denne kilden (f.eks. Mod Wheel = vibrato bare når hjulet er oppe).")));
        layer.setTooltip (String (CharPointer_UTF8 ("Hvilket lag modulasjonen gjelder: alle eller ett bestemt.")));
        amt.setTextBoxStyle (Slider::TextBoxRight, false, 58, 22);
        amt.setColour (Slider::trackColourId, auroraTeal);
        amt.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (amt);

        srcA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, slotId (s, mp::src), src);
        viaA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, slotId (s, mp::via), via);
        dstA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, slotId (s, mp::dst), dst);
        layerA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, slotId (s, mp::layer), layer);
        amtA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, slotId (s, mp::amt), amt);
    }

    void ModRow::resized()
    {
        auto r = getLocalBounds().reduced (0, 5);
        r.removeFromLeft (34);
        src.setBounds (r.removeFromLeft (160)); r.removeFromLeft (10);
        via.setBounds (r.removeFromLeft (160)); r.removeFromLeft (10);
        dst.setBounds (r.removeFromLeft (170)); r.removeFromLeft (10);
        layer.setBounds (r.removeFromLeft (80)); r.removeFromLeft (14);
        amt.setBounds (r.removeFromLeft (r.getWidth() - 10));
    }

    void ModRow::paint (Graphics& g)
    {
        const bool used = src.getSelectedItemIndex() > 0 && dst.getSelectedItemIndex() > 0;
        g.setColour (used ? auroraTeal : col::textDim);
        g.setFont (uiFont (13.0f, true));
        g.drawText (String (slot + 1), getLocalBounds().removeFromLeft (28), Justification::centred, false);
    }
}

//==============================================================================
NordlysEditor::NordlysEditor (NordlysProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      orb (*p.apvts.getParameter (ParamIDs::orbX), *p.apvts.getParameter (ParamIDs::orbY)),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    // ---- left column ----
    content.addAndMakeVisible (orb);
    setupKnob (orbRate, content, String (CharPointer_UTF8 ("Hvor fort Orb sirkler rundt punktet (0 = stille).")));
    setupKnob (orbDepth, content, String (CharPointer_UTF8 ("Hvor stor sirkel Orb beveger seg i.")));
    setupKnob (master, content, String (CharPointer_UTF8 ("Utgangsnivå.")));
    attachKnob (orbRate, ParamIDs::orbRate, sliderAtts);
    attachKnob (orbDepth, ParamIDs::orbDepth, sliderAtts);
    attachKnob (master, ParamIDs::master, sliderAtts);

    for (int l = 0; l < numLayers; ++l)
    {
        auto& b = layerOn[(size_t) l];
        b.setButtonText (layerLetter (l));
        b.setClickingTogglesState (true);
        b.setColour (TextButton::buttonOnColourId, auroraGreen);
        b.setColour (TextButton::textColourOnId, col::bg);
        b.setTooltip (String (CharPointer_UTF8 ("Slå lag ")) + layerLetter (l) + String (CharPointer_UTF8 (" av/på")));
        content.addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, layerId (l, lp::on), b));

        auto& s = layerLevel[(size_t) l];
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setTextBoxStyle (Slider::TextBoxRight, false, 50, 20);
        s.setColour (Slider::trackColourId, auroraTeal);
        content.addAndMakeVisible (s);
        sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, layerId (l, lp::level), s));
        s.setDoubleClickReturnValue (true, 80.0);

        auto& info = layerInfo[(size_t) l];
        info.setFont (uiFont (10.0f, true));
        info.setColour (Label::textColourId, col::textDim);
        info.setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (info);
    }

    // ---- tabs ----
    const char* tabNames[] = { "LAG A", "LAG B", "LAG C", "LAG D", "MODULASJON", "ARP · FX" };
    for (int i = 0; i < 6; ++i)
    {
        auto* t = tabs.add (new TextButton (String (CharPointer_UTF8 (tabNames[i]))));
        t->setClickingTogglesState (false);
        t->setColour (TextButton::buttonOnColourId, auroraTeal);
        t->setColour (TextButton::textColourOnId, col::bg);
        t->onClick = [this, i] { showPage (i); };
        content.addAndMakeVisible (t);
    }
    content.addChildComponent (layerPage);
    content.addChildComponent (modPage);
    content.addChildComponent (fxPage);

    // ---- layer page ----
    layerPage.addAndMakeVisible (wave);
    setupCombo (oscType, layerPage, oscTypeNames);
    setupCombo (waveBank, layerPage, waveNames);
    setupCombo (filterType, layerPage, filterTypeNames);
    setupCombo (lfo1Shape, layerPage, lfoShapeNames);
    setupCombo (lfo2Shape, layerPage, lfoShapeNames);
    oscType.setTooltip (String (CharPointer_UTF8 ("Analog: klassiske bølgeformer.  Wavetable: morfer gjennom en bølgebank.  FM: to-operator FM.  "
                                                  "Granular: mange små korn fra bølgebanken – tekstur og skimmer.  Noise: farget støy.")));
    waveBank.setTooltip (String (CharPointer_UTF8 ("Bølgebank for Wavetable og Granular.")));

    setupKnob (shape, layerPage, String (CharPointer_UTF8 ("Analog: sinus → trekant → sag → firkant.  Wavetable/Granular: posisjon i banken.  "
                                                           "FM: modulasjonsmengde (lysere).  Noise: farge (mørk → lys).")));
    setupKnob (sym, layerPage, String (CharPointer_UTF8 ("Analog/Wavetable: bøyer bølgeformen (50 % = uendret).  FM: frekvensforhold mellom operatorene.")));
    setupKnob (uni, layerPage, String (CharPointer_UTF8 ("Antall stemmer per tone (1–7). Gir bredde og fylde.")));
    setupKnob (detune, layerPage, String (CharPointer_UTF8 ("Hvor mye unison-stemmene er stemt fra hverandre (cent).")));
    setupKnob (spread, layerPage, String (CharPointer_UTF8 ("Stereobredde for unison-stemmer, korn og støy.")));
    setupKnob (gsize, layerPage, String (CharPointer_UTF8 ("Lengden på hvert korn. Korte korn = summing/skimmer, lange = myk flate.")));
    setupKnob (gdens, layerPage, String (CharPointer_UTF8 ("Hvor mange korn som overlapper.")));
    setupKnob (gspray, layerPage, String (CharPointer_UTF8 ("Tilfeldig variasjon i hvor i banken kornene hentes fra.")));
    setupKnob (gpitch, layerPage, String (CharPointer_UTF8 ("Tilfeldig tonehøydevariasjon per korn (cent).")));
    setupKnob (gshim, layerPage, String (CharPointer_UTF8 ("Andel korn som spilles en oktav, kvint eller duodesim over – gir skimmer.")));
    setupKnob (oct, layerPage, {}, true);
    setupKnob (coarse, layerPage, {}, true);
    setupKnob (fine, layerPage, {}, true);
    setupKnob (level, layerPage);
    setupKnob (pan, layerPage, {}, true);
    setupKnob (cutoff, layerPage);
    setupKnob (reso, layerPage);
    setupKnob (fenvAmt, layerPage, String (CharPointer_UTF8 ("Hvor mye filter-envelopen åpner (+) eller lukker (−) filteret.")), true);
    setupKnob (fkey, layerPage, String (CharPointer_UTF8 ("Key tracking: lysere filter på høye toner.")));
    setupKnob (drive, layerPage, String (CharPointer_UTF8 ("Metning før filteret.")));
    for (auto* k : { &fA, &fD, &fS, &fR, &aA, &aD, &aS, &aR, &l1rate, &l1fade, &l2rate, &l2fade })
        setupKnob (*k, layerPage);
    l1fade.slider.setTooltip (String (CharPointer_UTF8 ("Tid LFO-en bruker på å fade inn etter hver tone (fin for forsinket vibrato).")));
    l2fade.slider.setTooltip (l1fade.slider.getTooltip());

    layerPage.painter = [] (Graphics& g)
    {
        drawSection (g, secOsc.toFloat(), "");
        drawSubTitle (g, 244.0f, 14.0f, "OSCILLATOR");
        drawSubTitle (g, 244.0f, 70.0f, String (CharPointer_UTF8 ("BØLGEBANK")));
        divider (g, 384.0f, (float) secOsc.getY() + 16.0f, (float) secOsc.getBottom() - 16.0f);

        drawSection (g, secTune.toFloat(), "GRANULAR");
        divider (g, 432.0f, (float) secTune.getY() + 12.0f, (float) secTune.getBottom() - 12.0f);
        drawSubTitle (g, 444.0f, (float) secTune.getY() + 6.0f, "STEMMING / MIKS");

        drawSection (g, secFilter.toFloat(), "FILTER");
        divider (g, 538.0f, (float) secFilter.getY() + 12.0f, (float) secFilter.getBottom() - 12.0f);
        drawSubTitle (g, 550.0f, (float) secFilter.getY() + 6.0f, "FILTER-ENVELOPE");

        drawSection (g, secAmp.toFloat(), "AMP-ENVELOPE");
        divider (g, 328.0f, (float) secAmp.getY() + 12.0f, (float) secAmp.getBottom() - 12.0f);
        drawSubTitle (g, 340.0f, (float) secAmp.getY() + 6.0f, "LFO 1");
        divider (g, 594.0f, (float) secAmp.getY() + 12.0f, (float) secAmp.getBottom() - 12.0f);
        drawSubTitle (g, 606.0f, (float) secAmp.getY() + 6.0f, "LFO 2");
    };

    // ---- mod page ----
    for (int m = 0; m < numSlots; ++m)
        modPage.addAndMakeVisible (modRows.add (new nordlysui::ModRow (proc, m)));
    modPage.painter = [this] (Graphics& g)
    {
        const auto b = modPage.getLocalBounds().toFloat();
        drawSection (g, b.withHeight (420.0f), "MODULASJONSMATRISE");
        g.setColour (col::textDim);
        g.setFont (uiFont (9.5f, true));
        const float y = 28.0f;
        g.drawText ("KILDE", Rectangle<float> (46.0f, y, 160.0f, 14.0f), Justification::centredLeft);
        g.drawText ("VIA", Rectangle<float> (216.0f, y, 160.0f, 14.0f), Justification::centredLeft);
        g.drawText (String (CharPointer_UTF8 ("MÅL")), Rectangle<float> (386.0f, y, 160.0f, 14.0f), Justification::centredLeft);
        g.drawText ("LAG", Rectangle<float> (566.0f, y, 80.0f, 14.0f), Justification::centredLeft);
        g.drawText ("MENGDE", Rectangle<float> (660.0f, y, 160.0f, 14.0f), Justification::centredLeft);

        auto help = b.withTrimmedTop (430.0f);
        drawSection (g, help, "KILDER");
        g.setColour (col::text.withAlpha (0.8f));
        g.setFont (uiFont (11.0f));
        g.drawFittedText (String (CharPointer_UTF8 (
            "LFO 1 / LFO 2 og Filter Env / Amp Env er lagets egne (stilles inn på lagsiden).  Mod Wheel, Velocity og Aftertouch kommer fra keyboardet.  "
            "Key = tonehøyde (midt-C = 0).  Random = ny verdi for hver tone.  Orb X / Orb Y = posisjonen i Orb (også når den sirkler automatisk).\n"
            "Pitch: ±100 % = ±2 oktaver.  Fine: ±100 % = ±1 halvtone.  Cutoff: ±100 % = ±6 oktaver.  Dobbeltklikk på mengden for å nullstille.")),
            help.reduced (14.0f, 10.0f).withTrimmedTop (16.0f).toNearestInt(), Justification::topLeft, 6);
    };

    // ---- arp / fx page ----
    arpOn.setClickingTogglesState (true);
    arpOn.setColour (TextButton::buttonOnColourId, auroraGreen);
    arpOn.setColour (TextButton::textColourOnId, col::bg);
    fxPage.addAndMakeVisible (arpOn);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, ParamIDs::arpOn, arpOn));
    setupCombo (arpMode, fxPage, arpModeNames);
    setupCombo (arpRate, fxPage, arpRateNames);
    setupCombo (dlyTime, fxPage, delayTimeNames);
    setupCombo (revType, fxPage, reverbTypeNames);
    setupCombo (polyMode, fxPage, polyModeNames);
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, ParamIDs::arpMode, arpMode));
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, ParamIDs::arpRate, arpRate));
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, ParamIDs::dlyTime, dlyTime));
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, ParamIDs::revType, revType));
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, ParamIDs::polyMode, polyMode));
    polyMode.setTooltip (String (CharPointer_UTF8 ("Poly: akkorder (16 stemmer).  Mono: én tone, envelopene starter på nytt.  "
                                                   "Legato: én tone, glir uten ny anslag når du holder tonen over.")));

    const std::pair<sbaudio::Knob*, const char*> fxKnobs[] = {
        { &arpOct, ParamIDs::arpOct }, { &arpGate, ParamIDs::arpGate },
        { &choMix, ParamIDs::choMix }, { &choRate, ParamIDs::choRate }, { &choDepth, ParamIDs::choDepth },
        { &dlyMix, ParamIDs::dlyMix }, { &dlyFb, ParamIDs::dlyFb }, { &dlyTone, ParamIDs::dlyTone },
        { &revMix, ParamIDs::revMix }, { &revDecay, ParamIDs::revDecay }, { &revTone, ParamIDs::revTone },
        { &glide, ParamIDs::glide }, { &bend, ParamIDs::bend }, { &velSens, ParamIDs::velSens } };
    for (auto& [k, id] : fxKnobs)
    {
        setupKnob (*k, fxPage, {}, k == &revTone);
        attachKnob (*k, id, sliderAtts);
    }
    glide.slider.setTooltip (String (CharPointer_UTF8 ("Portamento: tiden det tar å gli fra forrige tone til neste.")));
    bend.slider.setTooltip (String (CharPointer_UTF8 ("Pitch bend-område i halvtoner.")));
    velSens.slider.setTooltip (String (CharPointer_UTF8 ("Hvor mye anslagsstyrken påvirker volumet.")));
    arpGate.slider.setTooltip (String (CharPointer_UTF8 ("Hvor lenge hver arp-tone klinger, i prosent av steget.")));

    fxPage.painter = [] (Graphics& g)
    {
        drawSection (g, secArp.toFloat(), "ARPEGGIATOR", auroraGreen);
        drawSubTitle (g, 130.0f, 30.0f, "MODUS");
        drawSubTitle (g, 300.0f, 30.0f, "RATE");
        g.setColour (col::textDim);
        g.setFont (uiFont (10.5f));
        g.drawFittedText (String (CharPointer_UTF8 ("Hold en akkord – arpeggiatoren spiller tonene i takt med Logic. "
                                                    "Sustain-pedalen holder akkorden. Chord spiller hele akkorden på hvert steg.")),
                          Rectangle<int> (620, 34, 230, 80), Justification::topLeft, 5);
        drawSection (g, secCho.toFloat(), "CHORUS");
        drawSection (g, secDly.toFloat(), "DELAY  (PING-PONG)");
        drawSubTitle (g, (float) secDly.getX() + 14.0f, (float) secDly.getY() + 28.0f, "TID");
        drawSection (g, secRev.toFloat(), "REVERB");
        drawSubTitle (g, (float) secRev.getX() + 14.0f, (float) secRev.getY() + 28.0f, "TYPE");
        drawSection (g, secPlay.toFloat(), String (CharPointer_UTF8 ("SPILLEMÅTE")));
        drawSubTitle (g, 14.0f, (float) secPlay.getY() + 30.0f, "STEMMER");
    };

    // ---- keyboard ----
    keyboard.setAvailableRange (24, 108);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffd9dde3));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff15181d));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff6d7480));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, auroraGreen.withAlpha (0.75f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, auroraTeal.withAlpha (0.3f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.3f));
    content.addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (designW * 2 / 3, designH * 2 / 3, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    showPage (jlimit (0, 5, (int) proc.apvts.state.getProperty (uiPageId, 0)));
    timerCallback();   // fill displays right away instead of on the first tick
    startTimerHz (30);
}

NordlysEditor::~NordlysEditor()
{
    stopTimer();
    layerSliderAtts.clear();
    layerComboAtts.clear();
    setLookAndFeel (nullptr);
}

//==============================================================================
void NordlysEditor::setupKnob (sbaudio::Knob& k, Component& parent, const String& tip, bool bipolar)
{
    parent.addAndMakeVisible (k);
    k.slider.setColour (Slider::rotarySliderFillColourId, auroraTeal);
    if (bipolar) k.slider.getProperties().set ("bipolar", true);
    if (tip.isNotEmpty()) k.slider.setTooltip (tip);
}

void NordlysEditor::setupCombo (ComboBox& c, Component& parent, const StringArray& items)
{
    c.addItemList (items, 1);
    parent.addAndMakeVisible (c);
}

void NordlysEditor::attachKnob (sbaudio::Knob& k, const String& id, std::vector<std::unique_ptr<SliderAtt>>& atts)
{
    atts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, k.slider));
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void NordlysEditor::attachLayer (int l)
{
    currentLayer = l;
    layerSliderAtts.clear();
    layerComboAtts.clear();

    const std::pair<sbaudio::Knob*, const char*> knobs[] = {
        { &shape, lp::shape }, { &sym, lp::sym }, { &uni, lp::uni }, { &detune, lp::detune }, { &spread, lp::spread },
        { &gsize, lp::gsize }, { &gdens, lp::gdens }, { &gspray, lp::gspray }, { &gpitch, lp::gpitch }, { &gshim, lp::gshim },
        { &oct, lp::oct }, { &coarse, lp::coarse }, { &fine, lp::fine }, { &level, lp::level }, { &pan, lp::pan },
        { &cutoff, lp::cutoff }, { &reso, lp::reso }, { &fenvAmt, lp::fenv }, { &fkey, lp::fkey }, { &drive, lp::drive },
        { &fA, lp::fA }, { &fD, lp::fD }, { &fS, lp::fS }, { &fR, lp::fR },
        { &aA, lp::aA }, { &aD, lp::aD }, { &aS, lp::aS }, { &aR, lp::aR },
        { &l1rate, lp::l1rate }, { &l1fade, lp::l1fade }, { &l2rate, lp::l2rate }, { &l2fade, lp::l2fade } };
    for (auto& [k, name] : knobs)
        attachKnob (*k, layerId (l, name), layerSliderAtts);

    const std::pair<ComboBox*, const char*> combos[] = {
        { &oscType, lp::osc }, { &waveBank, lp::wave }, { &filterType, lp::ftype }, { &lfo1Shape, lp::l1shape }, { &lfo2Shape, lp::l2shape } };
    for (auto& [c, name] : combos)
        layerComboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, layerId (l, name), *c));

    shownOsc = -1;
    updateLayerLabels();
}

void NordlysEditor::updateLayerLabels()
{
    const int osc = (int) proc.apvts.getRawParameterValue (layerId (currentLayer, lp::osc))->load();
    if (osc == shownOsc) return;
    shownOsc = osc;

    const char* shapeName = "FORM";
    switch (osc)
    {
        case oscWavetable: case oscGranular: shapeName = "POSISJON"; break;
        case oscFM:        shapeName = "FM-MENGDE"; break;
        case oscNoise:     shapeName = "FARGE"; break;
        default: break;
    }
    shape.label.setText (shapeName, dontSendNotification);
    sym.label.setText (osc == oscFM ? "RATIO" : "SYMMETRI", dontSendNotification);

    // FM: show the operator ratio instead of a percentage
    if (auto* prm = proc.apvts.getParameter (layerId (currentLayer, lp::sym)))
    {
        if (osc == oscFM)
            sym.slider.textFromValueFunction = [] (double v) { return String (fmRatios[fmRatioIndex ((float) v * 0.01f)], 1) + ":1"; };
        else
            sym.slider.textFromValueFunction = [prm] (double v) { return prm->getText (prm->convertTo0to1 ((float) v), 0); };
        sym.slider.updateText();
    }

    auto enable = [] (Component& c, bool on) { c.setEnabled (on); c.setAlpha (on ? 1.0f : 0.35f); };
    const bool granular = osc == oscGranular;
    for (auto* k : { &gsize, &gdens, &gspray, &gpitch, &gshim }) enable (*k, granular);
    const bool hasUnison = osc == oscAnalog || osc == oscWavetable || osc == oscFM;
    enable (uni, hasUnison);
    enable (detune, hasUnison);
    enable (sym, osc != oscNoise && osc != oscGranular);
    enable (waveBank, osc == oscWavetable || osc == oscGranular);
}

void NordlysEditor::showPage (int p)
{
    page = p;
    proc.apvts.state.setProperty (uiPageId, p, nullptr);
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == p, dontSendNotification);
    layerPage.setVisible (p < numLayers);
    modPage.setVisible (p == numLayers);
    fxPage.setVisible (p == numLayers + 1);
    if (p < numLayers)
        attachLayer (p);
    repaint();
}

//==============================================================================
void NordlysEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);
}

void NordlysEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void NordlysEditor::layoutContent()
{
    presetBar.setBounds (380, 10, 480, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    // left column
    orb.setBounds (32, 78, 260, 236);
    orbRate.setBounds (52, 318, 100, 72);
    orbDepth.setBounds (172, 318, 100, 72);
    for (int l = 0; l < numLayers; ++l)
    {
        const int y = 432 + l * 36;
        layerOn[(size_t) l].setBounds (26, y, 34, 28);
        layerInfo[(size_t) l].setBounds (66, y, 92, 28);
        layerLevel[(size_t) l].setBounds (158, y, 146, 28);
    }
    master.setBounds (112, 576, 100, 74);

    // tabs + pages
    const int tx = 324, tw = (864 - 5 * 6) / 6;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setBounds (tx + i * (tw + 6), 56, tw, 28);
    const Rectangle<int> pageArea (324, 92, 864, 558);
    layerPage.setBounds (pageArea);
    modPage.setBounds (pageArea);
    fxPage.setBounds (pageArea);

    // ---- layer page ----
    wave.setBounds (10, 10, 222, 120);
    oscType.setBounds (244, 32, 128, 26);
    waveBank.setBounds (244, 88, 128, 26);
    {
        int x = 394;
        for (auto* k : { &shape, &sym, &uni, &detune, &spread })
        {
            k->setBounds (x, 18, 92, 110);
            x += 92;
        }
    }
    {
        int x = 10;
        const int y = secTune.getY() + 20, h = secTune.getHeight() - 26;
        for (auto* k : { &gsize, &gdens, &gspray, &gpitch, &gshim }) { k->setBounds (x, y, 82, h); x += 84; }
        x = 444;
        for (auto* k : { &oct, &coarse, &fine, &level, &pan }) { k->setBounds (x, y, 82, h); x += 84; }
    }
    {
        const int y = secFilter.getY() + 22, h = secFilter.getHeight() - 30;
        filterType.setBounds (12, secFilter.getY() + 56, 120, 26);
        int x = 140;
        for (auto* k : { &cutoff, &reso, &fenvAmt, &fkey, &drive }) { k->setBounds (x, y, 78, h); x += 79; }
        x = 550;
        for (auto* k : { &fA, &fD, &fS, &fR }) { k->setBounds (x, y, 76, h); x += 77; }
    }
    {
        const int y = secAmp.getY() + 22, h = secAmp.getHeight() - 30;
        int x = 10;
        for (auto* k : { &aA, &aD, &aS, &aR }) { k->setBounds (x, y, 78, h); x += 79; }
        lfo1Shape.setBounds (340, secAmp.getY() + 56, 92, 26);
        l1rate.setBounds (436, y, 76, h);
        l1fade.setBounds (514, y, 76, h);
        lfo2Shape.setBounds (606, secAmp.getY() + 56, 92, 26);
        l2rate.setBounds (702, y, 76, h);
        l2fade.setBounds (780, y, 76, h);
    }

    // ---- mod page ----
    for (int m = 0; m < modRows.size(); ++m)
        modRows[m]->setBounds (12, 46 + m * 46, 840, 46);

    // ---- fx page ----
    arpOn.setBounds (16, 50, 96, 32);
    arpMode.setBounds (130, 50, 150, 28);
    arpRate.setBounds (300, 50, 100, 28);
    arpOct.setBounds (420, 22, 90, 104);
    arpGate.setBounds (512, 22, 90, 104);
    {
        auto row = [] (Rectangle<int> sec, std::initializer_list<sbaudio::Knob*> ks, int top)
        {
            const int n = (int) ks.size();
            const int w = (sec.getWidth() - 20) / n;
            int x = sec.getX() + 10;
            for (auto* k : ks) { k->setBounds (x, sec.getY() + top, w, sec.getHeight() - top - 10); x += w; }
        };
        row (secCho, { &choMix, &choRate, &choDepth }, 40);
        dlyTime.setBounds (secDly.getX() + 50, secDly.getY() + 24, 100, 24);
        row (secDly, { &dlyMix, &dlyFb, &dlyTone }, 62);
        revType.setBounds (secRev.getX() + 50, secRev.getY() + 24, 120, 24);
        row (secRev, { &revMix, &revDecay, &revTone }, 62);
    }
    polyMode.setBounds (14, secPlay.getY() + 50, 130, 28);
    {
        int x = 170;
        for (auto* k : { &glide, &bend, &velSens }) { k->setBounds (x, secPlay.getY() + 20, 96, secPlay.getHeight() - 28); x += 104; }
    }

    keyboard.setBounds (12, 662, designW - 24, 86);
    keyboard.setKeyWidth ((float) (designW - 24) / 43.0f);   // 6 octaves visible (43 white keys)
    keyboard.setLowestVisibleKey (36);                         // C1 in Logic's naming (middle C = C3)
}

void NordlysEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    // faint aurora in the background
    g.setGradientFill (ColourGradient (auroraGreen.withAlpha (0.05f), 300.0f, 40.0f, auroraGreen.withAlpha (0.0f), 700.0f, 400.0f, true));
    g.fillRect (full);
    g.setGradientFill (ColourGradient (auroraViolet.withAlpha (0.05f), 1000.0f, 60.0f, auroraViolet.withAlpha (0.0f), 1200.0f, 500.0f, true));
    g.fillRect (full);

    // top bar
    auto bar = full.withHeight (48.0f);
    g.setColour (col::panel.withAlpha (0.9f));
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    {
        const Rectangle<float> logo (16.0f, 0.0f, 350.0f, 48.0f);
        sbaudio::drawBrandTitle (g, logo, "Nordlys", "SYNTH",
                                 FillType (ColourGradient (auroraGreen, logo.getX(), 0.0f, auroraViolet, logo.getX() + 95.0f, 0.0f, false)));
    }

    // voices + output meter
    {
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText ("STEMMER  " + String (jmax (0, voicesShown)) + " / " + String (maxVoices),
                    Rectangle<float> (1000.0f, 0.0f, 120.0f, 48.0f), Justification::centredRight);
        auto m = Rectangle<float> (1130.0f, 18.0f, 56.0f, 10.0f);
        g.setColour (col::well);
        g.fillRoundedRectangle (m, 3.0f);
        const float db = Decibels::gainToDecibels (peak, -60.0f);
        const float p = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -0.5f ? col::red : auroraGreen);
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * p), 3.0f);
        g.setColour (col::textDim);
        g.setFont (uiFont (8.5f, true));
        g.drawText ("UT", m.translated (0.0f, 11.0f), Justification::centred);
    }

    // left column panels
    drawSection (g, { 12.0f, 56.0f, 300.0f, 342.0f }, "ORB", auroraGreen);
    drawSection (g, { 12.0f, 406.0f, 300.0f, 246.0f }, "LAG");

    // keyboard frame
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (Rectangle<float> (11.0f, 661.0f, designW - 22.0f, 88.0f), 3.0f, 1.0f);
}

void NordlysEditor::timerCallback()
{
    orb.setLive (proc.engine.orbXOut.load(), proc.engine.orbYOut.load());

    const float pk = proc.engine.outPeak.exchange (0.0f);
    const float newPeak = jmax (pk, peak * 0.85f);
    const int v = proc.engine.activeVoices.load();
    if (std::abs (newPeak - peak) > 1.0e-4f || v != voicesShown)
    {
        peak = newPeak;
        voicesShown = v;
        content.repaint (990, 0, 210, 48);
    }

    for (int l = 0; l < numLayers; ++l)
    {
        const int osc = (int) proc.apvts.getRawParameterValue (layerId (l, lp::osc))->load();
        const int wv = (int) proc.apvts.getRawParameterValue (layerId (l, lp::wave))->load();
        String txt = oscTypeNames[osc].toUpperCase();
        if (osc == oscWavetable || osc == oscGranular) txt = waveNames[wv].toUpperCase() + (osc == oscGranular ? " ~" : "");
        layerInfo[(size_t) l].setText (txt, dontSendNotification);
        const bool on = proc.apvts.getRawParameterValue (layerId (l, lp::on))->load() > 0.5f;
        layerLevel[(size_t) l].setAlpha (on ? 1.0f : 0.4f);
        layerInfo[(size_t) l].setAlpha (on ? 1.0f : 0.5f);
    }

    if (page < numLayers)
    {
        updateLayerLabels();
        nordlysui::WaveDisplay::State st;
        auto raw = [this] (const char* n) { return proc.apvts.getRawParameterValue (layerId (currentLayer, n))->load(); };
        st.layer = currentLayer;
        st.osc = (int) raw (lp::osc);
        st.wave = (int) raw (lp::wave);
        st.shape = raw (lp::shape) * 0.01f;
        st.sym = raw (lp::sym) * 0.01f;
        st.spray = raw (lp::gspray) * 0.01f;
        st.on = raw (lp::on) > 0.5f;
        wave.setState (st);
    }
    else if (page == numLayers)
    {
        for (auto* r : modRows)
        {
            const bool used = r->src.getSelectedItemIndex() > 0 && r->dst.getSelectedItemIndex() > 0;
            const float a = used ? 1.0f : 0.5f;
            if (std::abs (r->amt.getAlpha() - a) > 0.01f) { r->amt.setAlpha (a); r->via.setAlpha (a); r->layer.setAlpha (a); r->repaint(); }
        }
    }
}
