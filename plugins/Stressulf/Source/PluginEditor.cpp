#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour stressCol  { 0xffff4f6b };   // hot red-pink
    const Colour ledRed     { 0xffff3b3b };
    const Colour ledAmber   { 0xffffb03b };
    const Colour ledGreen   { 0xff52e07a };
    const Colour plateTop   { 0xff1d1f24 };
    const Colour plateBot   { 0xff111216 };
    const Colour silk       { 0xffd8dbe0 };   // white "screen print" on the faceplate
    const Colour silkDim    { 0xff8a9099 };

    String utf8 (const char* s) { return String (CharPointer_UTF8 (s)); }

    void drawLed (Graphics& g, Point<float> c, float d, Colour colour, bool on)
    {
        auto r = Rectangle<float> (d, d).withCentre (c);
        if (on)
        {
            g.setColour (colour.withAlpha (0.28f));
            g.fillEllipse (r.expanded (d * 0.6f));
            g.setGradientFill (ColourGradient (colour.brighter (0.6f), r.getCentreX(), r.getY(),
                                               colour, r.getCentreX(), r.getBottom(), false));
        }
        else
        {
            g.setColour (colour.withSaturation (0.5f).darker (2.6f));
        }
        g.fillEllipse (r);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (r.expanded (0.5f), 1.0f);
    }

    void drawScreen (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.expanded (1.5f), 4.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0a0b0d), 0.0f, r.getY(), Colour (0xff060708), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colour (0xff2e3138));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
}

namespace stressulfui
{
    //==========================================================================
    void FaceplateLnf::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
    {
        const auto area   = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float size  = jmin (area.getWidth(), area.getHeight());
        const auto centre = area.getCentre();
        const float r     = size * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool enabled = s.isEnabled() && s.getAlpha() > 0.6f;

        // tick scale (11 ticks) – ticks up to the value glow red
        constexpr int numTicks = 11;
        for (int i = 0; i < numTicks; ++i)
        {
            const float t = (float) i / (float) (numTicks - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const bool major = i % 5 == 0;
            const auto p1 = centre.getPointOnCircumference (r * (major ? 0.82f : 0.86f), a);
            const auto p2 = centre.getPointOnCircumference (r * 0.98f, a);
            const bool lit = enabled && t <= pos + 0.001f;
            g.setColour (lit ? stressCol : silkDim.withAlpha (0.7f));
            g.drawLine ({ p1, p2 }, major ? 2.0f : 1.4f);
        }

        // knob: skirt + cap
        const float skirtR = r * 0.76f;
        auto skirt = Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (centre);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (skirt.translated (0.0f, 3.0f).expanded (1.5f));
        g.setGradientFill (ColourGradient (Colour (0xff2a2c31), skirt.getX(), skirt.getY(),
                                           Colour (0xff050506), skirt.getRight(), skirt.getBottom(), false));
        g.fillEllipse (skirt);

        // knurled edge
        g.setColour (Colours::white.withAlpha (0.05f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 48.0f;
            g.drawLine ({ centre.getPointOnCircumference (skirtR * 0.86f, a), centre.getPointOnCircumference (skirtR * 0.99f, a) }, 1.0f);
        }

        const float capR = skirtR * 0.80f;
        auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        g.setGradientFill (ColourGradient (Colour (0xff34363c), cap.getX(), cap.getY(),
                                           Colour (0xff0b0b0d), cap.getRight(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawEllipse (cap.reduced (0.5f), 1.0f);

        const auto p1 = centre.getPointOnCircumference (capR * 0.25f, angle);
        const auto p2 = centre.getPointOnCircumference (skirtR * 0.95f, angle);
        g.setColour (enabled ? Colours::white : silkDim);
        g.drawLine ({ p1, p2 }, jmax (2.0f, size * 0.03f));
    }

    //==========================================================================
    LedChoice::LedChoice (RangedAudioParameter& p, const StringArray& l, const String& t)
        : labels (l), title (t),
          attachment (p, [this] (float v) { current = roundToInt (v); repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
    }

    void LedChoice::setAllLit (bool b)
    {
        if (b != allLit) { allLit = b; repaint(); }
    }

    Rectangle<float> LedChoice::rowBounds (int i) const
    {
        const float top = 16.0f, rowH = (getHeight() - top) / (float) labels.size();
        return { 0.0f, top + rowH * (float) i, (float) getWidth(), rowH };
    }

    int LedChoice::rowAt (Point<int> p) const
    {
        for (int i = 0; i < labels.size(); ++i)
            if (rowBounds (i).contains (p.toFloat()))
                return i;
        return -1;
    }

    void LedChoice::mouseDown (const MouseEvent& e)
    {
        const int i = rowAt (e.getPosition());
        if (i >= 0)
            attachment.setValueAsCompleteGesture ((float) i);
    }

    String LedChoice::getTooltip()
    {
        const int i = rowAt (getMouseXYRelative());
        return isPositiveAndBelow (i, tips.size()) ? tips[i] : String();
    }

    void LedChoice::paint (Graphics& g)
    {
        g.setColour (silkDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (title, Rectangle<float> (0.0f, 0.0f, (float) getWidth(), 13.0f), Justification::centredLeft);

        for (int i = 0; i < labels.size(); ++i)
        {
            auto r = rowBounds (i);
            const bool on = allLit || i == current;
            drawLed (g, { r.getX() + 6.0f, r.getCentreY() }, 8.0f, ledRed, on);
            g.setColour (i == current ? silk : silkDim);
            g.setFont (uiFont (11.0f, i == current));
            g.drawText (labels[i], r.withTrimmedLeft (17.0f), Justification::centredLeft);
        }
    }

    //==========================================================================
    LedToggle::LedToggle (RangedAudioParameter& p, const String& l, Colour c)
        : label (l), ledCol (c),
          attachment (p, [this] (float v) { on = v >= 0.5f; repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
    }

    void LedToggle::mouseDown (const MouseEvent&)
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    }

    void LedToggle::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        drawLed (g, { r.getX() + 6.0f, r.getCentreY() }, 8.0f, ledCol, on);
        g.setColour (on ? silk : silkDim);
        g.setFont (uiFont (11.0f, on));
        g.drawText (label, r.withTrimmedLeft (17.0f), Justification::centredLeft);
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
        drawScreen (g, full);

        auto plot = full.reduced (6.0f, 5.0f);
        g.setColour (silkDim);
        g.setFont (uiFont (9.0f, true));
        g.drawText ("GR", plot.removeFromTop (11.0f), Justification::centredLeft);

        constexpr float lo = -60.0f, grRange = 30.0f;
        auto grLane = plot.removeFromTop (plot.getHeight() * 0.36f);
        plot.removeFromTop (4.0f);
        g.setFont (uiFont (9.0f, true));
        g.drawText (utf8 ("INN · UT"), plot.removeFromTop (11.0f), Justification::centredLeft);
        auto lvLane = plot;

        auto ly = [&] (float db) { return lvLane.getBottom() - lvLane.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / -lo); };
        auto gy = [&] (float gr) { return grLane.getY() + grLane.getHeight() * jlimit (0.0f, 1.0f, gr / grRange); };

        g.setColour (Colour (0xff1c1e23));
        for (float db = -20.0f; db > lo; db -= 20.0f)
            g.drawHorizontalLine (roundToInt (ly (db)), lvLane.getX(), lvLane.getRight());
        g.drawHorizontalLine (roundToInt (gy (15.0f)), grLane.getX(), grLane.getRight());

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
        g.setColour (silk.withAlpha (0.85f));
        g.strokePath (outLine, PathStrokeType (1.2f));
        g.setColour (stressCol.withAlpha (0.35f));
        g.fillPath (grArea);
        g.setColour (stressCol);
        g.strokePath (grLine, PathStrokeType (1.4f));
    }

    //==========================================================================
    void GrLadder::setGr (float g)
    {
        gr = g;
        if (g >= hold) { hold = g; holdFrames = 45; }
        else if (--holdFrames <= 0) hold = jmax (g, hold - 0.3f);
        repaint();
    }

    void GrLadder::paint (Graphics& g)
    {
        static constexpr int steps[] = { 1, 2, 3, 4, 6, 10, 15, 20, 30 };
        auto r = getLocalBounds().toFloat();

        g.setColour (silkDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText ("GR", r.removeFromTop (13.0f), Justification::centred);
        g.setColour (silk);
        g.setFont (uiFont (10.0f, true));
        g.drawText (hold < 0.05f ? String ("0.0") : "-" + String (hold, 1), r.removeFromBottom (14.0f), Justification::centred);
        r.removeFromBottom (4.0f);

        drawScreen (g, r);
        r = r.reduced (3.0f, 5.0f);
        const int n = (int) std::size (steps);
        const float rowH = r.getHeight() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            auto row = Rectangle<float> (r.getX(), r.getY() + rowH * (float) i, r.getWidth(), rowH);
            const auto c = steps[i] >= 20 ? ledRed : steps[i] >= 6 ? ledAmber : ledGreen;
            const bool on = gr >= (float) steps[i] - 0.5f || std::abs (hold - (float) steps[i]) < 0.5f;
            drawLed (g, { row.getX() + 6.0f, row.getCentreY() }, 7.0f, c, on);
            g.setColour (on ? silk : silkDim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (String (steps[i]), row.withTrimmedLeft (13.0f), Justification::centredRight);
        }
    }

    //==========================================================================
    void LevelRows::setLevels (const Levels& l)
    {
        shown.inDb  = jmax (l.inDb,  shown.inDb - 1.5f);
        shown.outDb = jmax (l.outDb, shown.outDb - 1.5f);
        repaint();
    }

    void LevelRows::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        drawScreen (g, r);
        r = r.reduced (6.0f, 4.0f);

        auto row = [&] (Rectangle<float> rr, const String& name, float db)
        {
            g.setColour (silkDim);
            g.setFont (uiFont (9.0f, true));
            g.drawText (name, rr.removeFromLeft (22.0f), Justification::centredLeft);
            constexpr int n = 16;
            const float w = rr.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float segDb = -48.0f + 3.0f * (float) i;   // -48 … -3, last segment = 0 dBFS
                const auto c = segDb >= -6.0f ? ledRed : segDb >= -18.0f ? ledAmber : ledGreen;
                const bool on = db >= segDb;
                auto seg = Rectangle<float> (rr.getX() + w * (float) i, rr.getCentreY() - 3.0f, w - 2.0f, 6.0f);
                g.setColour (on ? c : c.withAlpha (0.14f));
                g.fillRoundedRectangle (seg, 1.0f);
            }
        };
        row (r.removeFromTop (r.getHeight() * 0.5f), "INN", shown.inDb);
        row (r, "UT", shown.outDb);
    }
}

//==============================================================================
StressulfEditor::StressulfEditor (StressulfProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets),
      ratioLeds (*p.apvts.getParameter (ParamIDs::ratio),
                 { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "NUKE", "OPTO" }, "RATIO"),
      distLeds (*p.apvts.getParameter (ParamIDs::dist), { "CLEAN", "DIST 2", "DIST 3" }, "DIST"),
      smashLed   (*p.apvts.getParameter (ParamIDs::smash),   "SMASH", ledRed),
      detHpLed   (*p.apvts.getParameter (ParamIDs::detHp),   "HP",    ledRed),
      bandLed    (*p.apvts.getParameter (ParamIDs::detBand), "BAND",  ledRed),
      audioHpLed (*p.apvts.getParameter (ParamIDs::audioHp), "HP",    ledRed),
      linkLed    (*p.apvts.getParameter (ParamIDs::link),    "LINK",  ledRed)
{
    setLookAndFeel (&lnf);
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    ratioLeds.setTooltips ({
        utf8 ("1:1 – ingen kompresjon, bare farge (bruk med DIST 2 / DIST 3)."),
        utf8 ("2:1 – svært myk knekk, nesten usynlig utjevning."),
        utf8 ("3:1 – myk og musikalsk."),
        utf8 ("4:1 – allsidig."),
        utf8 ("6:1 – tydelig og tett."),
        utf8 ("10:1 – nær begrensning."),
        utf8 ("20:1 – begrenser."),
        utf8 ("NUKE – nesten brickwall, svært hard knekk."),
        utf8 ("OPTO – programavhengig: rask attack, release som blir langsommere jo lenger den demper.") });
    distLeds.setTooltips ({
        utf8 ("Ren kompresjon."),
        utf8 ("DIST 2 – like overtoner (rør-aktig): varm og tykk. Øker med kompresjonen."),
        utf8 ("DIST 3 – odde overtoner (tape-aktig): kant og bitt. Øker med kompresjonen.") });
    content.addAndMakeVisible (ratioLeds);
    content.addAndMakeVisible (distLeds);

    setupToggle (smashLed,   utf8 ("SMASH: alle knapper inne. Overkomprimerer med rask attack og ekstra forvrengning – for rom og parallell knusing."));
    setupToggle (detHpLed,   utf8 ("Detektor-HP: detektoren lytter mindre på bassen, så den ikke pumper av basstromme og bass."));
    setupToggle (bandLed,    utf8 ("Band-emfase: detektoren lytter ekstra på 6 kHz. Demper S-lyder og skarpe topper – fungerer som en de-esser."));
    setupToggle (audioHpLed, utf8 ("Audio-HP: fjerner rumling under 80 Hz i selve lyden."));
    setupToggle (linkLed,    utf8 ("Stereo-link: begge kanaler dempes likt, så stereobildet står stille."));

    content.addAndMakeVisible (history);
    content.addAndMakeVisible (grLadder);
    content.addAndMakeVisible (levelRows);
    for (auto* c : std::initializer_list<Component*> { &history, &grLadder, &levelRows })
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
        k->label.setColour (Label::textColourId, silk);
        k->label.setFont (uiFont (11.0f, true));
    }

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

void StressulfEditor::setupToggle (stressulfui::LedToggle& t, const String& tip)
{
    t.setTooltip (tip);
    content.addAndMakeVisible (t);
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
    presetBar.setBounds (designW - 8 - 470, 7, 470, 26);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    faceplate = { 0, 40, designW, designH - 40 };
    const int top = faceplate.getY() + 14, bottom = designH - 14;

    // knobs, left to right like the hardware
    input.setBounds   (34,  top + 20, 112, 172);
    attack.setBounds  (150, top + 20, 92, 172);
    release.setBounds (246, top + 20, 92, 172);

    // metering
    grLadder.setBounds (350, top, 42, bottom - top);
    history.setBounds  (402, top, 168, 168);
    levelRows.setBounds (402, top + 176, 168, bottom - top - 176);

    // ratio LEDs + SMASH
    ratioLeds.setBounds (588, top, 70, 196);
    smashLed.setBounds  (588, top + 204, 70, 20);

    // character: DIST, DETEKTOR, AUDIO
    modeArea = { 670, top, 96, bottom - top };
    distLeds.setBounds (670, top, 96, 76);
    detHpLed.setBounds   (670, top + 100, 96, 20);
    bandLed.setBounds    (670, top + 120, 96, 20);
    audioHpLed.setBounds (670, top + 162, 96, 20);
    linkLed.setBounds    (670, top + 182, 96, 20);

    mix.setBounds    (776, top + 20, 82, 172);
    output.setBounds (862, top + 20, 112, 172);
}

void StressulfEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    // top bar
    auto bar = full.withHeight (40.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 40.0f }, "Stressulf", "COMPRESSOR", stressCol);

    // faceplate: dark anodised metal with a whisper of the painting behind it
    auto plate = faceplate.toFloat();
    g.setGradientFill (ColourGradient (plateTop, 0.0f, plate.getY(), plateBot, 0.0f, plate.getBottom(), false));
    g.fillRect (plate);
    if (background.isValid())
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (faceplate);
        g.setOpacity (0.10f);
        g.drawImage (background, plate, RectanglePlacement::centred | RectanglePlacement::fillDestination);
    }
    // brushed lines
    Random rng (7);
    for (float y = plate.getY(); y < plate.getBottom(); y += 2.0f)
    {
        g.setColour (Colours::white.withAlpha (0.006f + 0.012f * rng.nextFloat()));
        g.drawHorizontalLine ((int) y, plate.getX(), plate.getRight());
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine ((int) plate.getY(), plate.getX(), plate.getRight());
    g.setColour (Colours::black);
    g.drawHorizontalLine ((int) plate.getY() + 1, plate.getX(), plate.getRight());

    // rack ears with screws
    for (float x : { 0.0f, full.getRight() - 24.0f })
    {
        auto ear = Rectangle<float> (x, plate.getY(), 24.0f, plate.getHeight());
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (ear);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) (x == 0.0f ? ear.getRight() - 1.0f : ear.getX()), ear.getY(), ear.getBottom());
        for (float sy : { ear.getY() + 26.0f, ear.getBottom() - 26.0f })
        {
            auto screw = Rectangle<float> (11.0f, 11.0f).withCentre ({ ear.getCentreX(), sy });
            g.setGradientFill (ColourGradient (Colour (0xff6a6d73), screw.getX(), screw.getY(),
                                               Colour (0xff25272b), screw.getRight(), screw.getBottom(), false));
            g.fillEllipse (screw);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.drawLine ({ screw.getX() + 2.5f, screw.getCentreY(), screw.getRight() - 2.5f, screw.getCentreY() }, 1.4f);
            g.drawLine ({ screw.getCentreX(), screw.getY() + 2.5f, screw.getCentreX(), screw.getBottom() - 2.5f }, 1.4f);
        }
    }

    // section dividers
    g.setColour (Colours::black.withAlpha (0.5f));
    for (int x : { 343, 580, 664, 770 })
        g.drawVerticalLine (x, plate.getY() + 14.0f, plate.getBottom() - 14.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    for (int x : { 344, 581, 665, 771 })
        g.drawVerticalLine (x, plate.getY() + 14.0f, plate.getBottom() - 14.0f);

    // silkscreen captions for the character column
    g.setColour (silkDim);
    g.setFont (uiFont (10.0f, true));
    g.drawText ("DETEKTOR", Rectangle<int> (modeArea.getX(), detHpLed.getY() - 16, modeArea.getWidth(), 13), Justification::centredLeft);
    g.drawText ("AUDIO",    Rectangle<int> (modeArea.getX(), audioHpLed.getY() - 16, modeArea.getWidth(), 13), Justification::centredLeft);
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
    grLadder.setGr (last.grDb);
    levelRows.setLevels (last);

    const auto s = proc.currentSettings();
    ratioLeds.setAllLit (s.smash);

    const bool fixed = s.ratio == stressulf::opto && ! s.smash;
    attack.setAlpha (fixed ? 0.4f : 1.0f);
    release.setAlpha (fixed ? 0.4f : 1.0f);
}
