#include "PluginEditor.h"

using namespace juce;
using namespace optotube::ui;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    // Faceplate palette (steel blue rack unit)
    const Colour faceTop    { 0xff93a9bd };
    const Colour faceBottom { 0xff6f879c };
    const Colour earCol     { 0xff667d92 };
    const Colour engrave    { 0xfff5f7f9 };
    const Colour engraveSh  { 0x66223040 };
    const Colour readoutBg  { 0xff1a2028 };
    const Colour readoutTxt { 0xfff1e3b8 };

    void engravedText (Graphics& g, const String& s, Rectangle<float> r, Justification j, float h, bool bold = true,
                       Colour c = engrave)
    {
        g.setFont (uiFont (h, bold));
        g.setColour (engraveSh);
        g.drawText (s, r.translated (0.0f, 1.0f), j, false);
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    void engravedLine (Graphics& g, float x1, float y1, float x2, float y2)
    {
        g.setColour (engraveSh);
        g.drawLine (x1, y1, x2, y2, 1.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawLine (x1 + 1.0f, y1 + 1.0f, x2 + 1.0f, y2 + 1.0f, 1.0f);
    }
}

//==============================================================================
void HardwareLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider&)
{
    const auto area  = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r    = jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto c     = area.getCentre();
    const float ang  = startAngle + pos * (endAngle - startAngle);
    if (r < 4.0f)
        return;

    // drop shadow
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, r * 0.08f + 1.5f)).expanded (1.5f));

    // aluminium skirt
    auto skirt = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xffe4e7ea), skirt.getX(), skirt.getY(),
                                       Colour (0xff5d6268), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse (skirt);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (skirt.reduced (0.5f), 1.0f);

    // knurling on the skirt
    g.setColour (Colours::black.withAlpha (0.12f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 36.0f;
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.80f, a), c.getPointOnCircumference (r * 0.97f, a)), 1.0f);
    }

    // black cap
    const float cr = r * 0.74f;
    auto cap = Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (cap.expanded (1.5f).translated (0.0f, 1.0f));
    g.setGradientFill (ColourGradient (Colour (0xff45494f), cap.getX(), cap.getY(),
                                       Colour (0xff0b0c0e), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x, cap.getY(),
                                       Colours::white.withAlpha (0.0f), c.x, c.y, false));
    g.fillEllipse (cap.reduced (cr * 0.12f).withTrimmedBottom (cr * 0.6f));

    // pointer (runs across cap and skirt like the hardware)
    const auto p1 = c.getPointOnCircumference (r * 0.22f, ang);
    const auto p2 = c.getPointOnCircumference (r * 0.96f, ang);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine (Line<float> (p1, p2), jmax (2.0f, r * 0.09f) + 1.5f);
    g.setColour (Colour (0xfff7f7f2));
    g.drawLine (Line<float> (p1, p2), jmax (1.6f, r * 0.07f));
}

//==============================================================================
HardwareKnob::HardwareKnob (const String& cap, std::vector<Mark> m, bool isStepped)
    : caption (cap), marks (std::move (m)), stepped (isStepped)
{
    const float span = stepped ? MathConstants<float>::pi * 0.38f : MathConstants<float>::pi * 0.78f;
    slider.setRotaryParameters (MathConstants<float>::twoPi - span, MathConstants<float>::twoPi + span, true);
    slider.setMouseDragSensitivity (stepped ? 90 : 220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

Rectangle<float> HardwareKnob::knobArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (18.0f);      // caption
    r.removeFromBottom (24.0f);   // readout
    const float margin = stepped ? 26.0f : 20.0f;
    const float d = jmin (r.getWidth() - 2.0f * margin, r.getHeight() - 2.0f * (stepped ? 22.0f : 15.0f));
    return Rectangle<float> (d, d).withCentre (r.getCentre().translated (0.0f, 3.0f));
}

void HardwareKnob::resized()
{
    slider.setBounds (knobArea().getSmallestIntegerContainer());
}

void HardwareKnob::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    engravedText (g, caption, b.removeFromTop (16.0f), Justification::centred, 11.5f);

    // scale marks
    const auto k = knobArea();
    const auto c = k.getCentre();
    const float r = k.getWidth() * 0.5f;
    const auto rp = slider.getRotaryParameters();
    for (const auto& m : marks)
    {
        const float p = (float) slider.valueToProportionOfLength (m.value);
        const float a = rp.startAngleRadians + p * (rp.endAngleRadians - rp.startAngleRadians);
        const auto t1 = c.getPointOnCircumference (r + 2.0f, a);
        const auto t2 = c.getPointOnCircumference (r + 6.0f, a);
        g.setColour (engrave.withAlpha (0.9f));
        g.drawLine (Line<float> (t1, t2), 1.3f);

        const auto lp = c.getPointOnCircumference (r + (stepped ? 17.0f : 14.0f), a);
        const float tw = stepped ? 60.0f : 36.0f;
        engravedText (g, m.text, Rectangle<float> (tw, 12.0f).withCentre (lp), Justification::centred, 9.5f, true);
    }

    // value readout
    auto ro = b.removeFromBottom (19.0f).withSizeKeepingCentre (jmin (86.0f, b.getWidth() - 4.0f), 17.0f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (ro.translated (0.0f, 1.0f), 3.0f, 1.0f);
    g.setColour (readoutBg);
    g.fillRoundedRectangle (ro, 3.0f);
    g.setColour (readoutTxt);
    g.setFont (uiFont (11.0f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), ro, Justification::centred, false);
}

//==============================================================================
float VuMeter::vuToProportion (float vu)
{
    // A VU scale is linear in voltage: -20 VU at the left end, +3 VU at the right.
    const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
    const float v = std::pow (10.0f, jlimit (-22.0f, 4.0f, vu) / 20.0f);
    return (v - lo) / (hi - lo);
}

void VuMeter::setValue (float vuDb)
{
    // ~300 ms VU ballistics at 60 Hz
    const float next = needle + 0.22f * (vuDb - needle);
    if (std::abs (next - needle) > 0.005f)
    {
        needle = next;
        repaint();
    }
}

void VuMeter::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // housing / bezel
    g.setGradientFill (ColourGradient (Colour (0xff2a2f35), b.getX(), b.getY(), Colour (0xff111316), b.getX(), b.getBottom(), false));
    g.fillRect (b);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRect (b, 1.0f);

    auto face = b.reduced (9.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff6eed5), face.getX(), face.getY(),
                                       Colour (0xffe0d2a9), face.getX(), face.getBottom(), false));
    g.fillRect (face);
    // warm backlight
    g.setGradientFill (ColourGradient (Colour (0x40ffb347), face.getCentreX(), face.getBottom(),
                                       Colour (0x00ffb347), face.getCentreX(), face.getY(), true));
    g.fillRect (face);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());

    const float a0 = -0.80f, a1 = 0.80f;
    const Point<float> pivot (face.getCentreX(), face.getBottom() - 8.0f);
    const float R = jmin (face.getHeight() - 46.0f, (face.getWidth() * 0.5f - 24.0f) / std::sin (a1) - 22.0f);
    auto angleOf = [&] (float vu) { return a0 + vuToProportion (vu) * (a1 - a0); };

    // scale arcs
    {
        Path arc;
        arc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleOf (-20.0f), angleOf (0.0f), true);
        g.setColour (Colour (0xff1b1a17));
        g.strokePath (arc, PathStrokeType (1.6f));
        Path red;
        red.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleOf (0.0f), angleOf (3.0f), true);
        g.setColour (Colour (0xffc0302a));
        g.strokePath (red, PathStrokeType (4.0f));
    }

    const std::pair<float, const char*> ticks[] = { { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" },
                                                    { -3.0f, "3" }, { -2.0f, "2" }, { -1.0f, "1" }, { 0.0f, "0" },
                                                    { 1.0f, "+1" }, { 2.0f, "+2" }, { 3.0f, "+3" } };
    g.setFont (uiFont (11.5f, true));
    for (auto& [vu, txt] : ticks)
    {
        const float a = angleOf (vu);
        const auto col = vu > 0.0f ? Colour (0xffc0302a) : Colour (0xff1b1a17);
        g.setColour (col);
        g.drawLine (Line<float> (pivot.getPointOnCircumference (R, a), pivot.getPointOnCircumference (R + 9.0f, a)), 1.4f);
        g.drawText (txt, Rectangle<float> (30.0f, 14.0f).withCentre (pivot.getPointOnCircumference (R + 19.0f, a)),
                    Justification::centred, false);
    }
    for (float vu = -19.0f; vu < 3.0f; vu += 1.0f) // minor ticks
    {
        if (vu > -10.0f && vu < -7.0f) continue;
        g.setColour (Colour (0xff1b1a17).withAlpha (0.6f));
        const float a = angleOf (vu);
        g.drawLine (Line<float> (pivot.getPointOnCircumference (R, a), pivot.getPointOnCircumference (R + 4.0f, a)), 1.0f);
    }

    g.setFont (uiFont (14.0f, true));
    g.setColour (Colour (0xff1b1a17));
    g.drawText (CharPointer_UTF8 ("−"), Rectangle<float> (20.0f, 16.0f).withCentre (pivot.getPointOnCircumference (R - 16.0f, angleOf (-20.0f) + 0.06f)), Justification::centred);
    g.setColour (Colour (0xffc0302a));
    g.drawText ("+", Rectangle<float> (20.0f, 16.0f).withCentre (pivot.getPointOnCircumference (R - 16.0f, angleOf (3.0f) - 0.06f)), Justification::centred);

    g.setColour (Colour (0xff1b1a17));
    g.setFont (uiFont (22.0f, true));
    g.drawText ("VU", Rectangle<float> (60.0f, 26.0f).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.58f }), Justification::centred);
    g.setFont (uiFont (9.5f, true));
    const char* srcName = source == GainReduction ? "GAIN REDUCTION" : source == Input ? "INPUT  (0 VU = -18 dBFS)" : "OUTPUT  (0 VU = -18 dBFS)";
    g.drawText (srcName, Rectangle<float> (face.getWidth(), 14.0f).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.72f }), Justification::centred);

    g.setFont (uiFont (10.5f, true));
    g.setColour (Colour (0xff3a352a));
    g.drawText (readout, face.reduced (8.0f, 4.0f).removeFromBottom (14.0f).translated (0.0f, -16.0f),
                Justification::centredRight, false);
    g.drawText ("SB AUDIO", face.reduced (8.0f, 4.0f).removeFromBottom (14.0f).translated (0.0f, -16.0f),
                Justification::centredLeft, false);

    // needle
    const float na = angleOf (needle);
    const auto tip = pivot.getPointOnCircumference (R + 12.0f, na);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot.translated (3.0f, 3.0f), tip.translated (3.0f, 3.0f)), 1.6f);
    g.setColour (Colour (0xff141414));
    g.drawLine (Line<float> (pivot, tip), 1.6f);

    // needle base cover
    auto base = face.withTop (face.getBottom() - 16.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3b3f45), base.getX(), base.getY(), Colour (0xff15171a), base.getX(), base.getBottom(), false));
    g.fillRect (base);

    // glass highlight
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), face.getX(), face.getY(),
                                       Colours::white.withAlpha (0.0f), face.getX(), face.getCentreY(), false));
    g.fillRect (face.withHeight (face.getHeight() * 0.5f));
}

//==============================================================================
namespace
{
    using M = HardwareKnob::Mark;
}

OptoTubeEditor::OptoTubeEditor (OptoTubeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      presetBar (*p.presets),
      threshold ("THRESHOLD", { { -40, "-40" }, { -30, "-30" }, { -20, "-20" }, { -10, "-10" }, { 0, "0" } }),
      ratio     ("RATIO",     { { 1.5, "1.5" }, { 2, "2" }, { 3, "3" }, { 4, "4" }, { 6, "6" }, { 10, "10" } }),
      attack    ("ATTACK ms", { { 0.5, ".5" }, { 2, "2" }, { 10, "10" }, { 30, "30" }, { 100, "100" }, { 300, "300" } }),
      release   ("RELEASE s", { { 50, ".05" }, { 200, ".2" }, { 500, ".5" }, { 1000, "1" }, { 3000, "3" }, { 10000, "10" } }),
      gain      ("GAIN dB",   { { -12, "-12" }, { -6, "-6" }, { 0, "0" }, { 6, "+6" }, { 12, "+12" }, { 18, "+18" }, { 24, "+24" } }),
      mode      ("MODE",      { { 0, "OPTO" }, { 1, "MAN" }, { 2, "BOTH" } }, true),
      hpf       ("DET. HPF",  { { 0, "OFF" }, { 1, "60" }, { 2, "100" }, { 3, "150" } }, true),
      tube      ("TUBE",      { { 0, "0" }, { 25, "2.5" }, { 50, "5" }, { 75, "7.5" }, { 100, "10" } }),
      mix       ("MIX",       { { 0, "DRY" }, { 50, "50" }, { 100, "WET" } })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    const std::pair<HardwareKnob*, const char*> knobs[] = {
        { &threshold, ParamIDs::threshold }, { &ratio, ParamIDs::ratio }, { &attack, ParamIDs::attack },
        { &release, ParamIDs::release }, { &gain, ParamIDs::gain }, { &mode, ParamIDs::mode },
        { &hpf, ParamIDs::hpf }, { &tube, ParamIDs::tube }, { &mix, ParamIDs::mix } };

    for (auto& [knob, id] : knobs)
    {
        content.addAndMakeVisible (knob);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, knob->slider));
        if (auto* param = proc.apvts.getParameter (id))
            knob->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    threshold.slider.setTooltip (CharPointer_UTF8 ("Nivået der kompresjonen starter (Peak Reduction på LA-2A). Lavere = mer kompresjon."));
    ratio.slider.setTooltip (CharPointer_UTF8 ("Hvor kraftig signalet over terskelen dempes. 2–4 er typisk for vokal og gitar, 10 nærmer seg limiting."));
    attack.slider.setTooltip (CharPointer_UTF8 ("Hvor raskt den manuelle kompressoren griper inn. Brukes i MAN og BOTH."));
    release.slider.setTooltip (CharPointer_UTF8 ("Hvor raskt den manuelle kompressoren slipper. Brukes i MAN og BOTH."));
    mode.slider.setTooltip (CharPointer_UTF8 ("OPTO: programavhengig optisk kompresjon (LA-2A-stil), attack/release styres automatisk.\n"
                                              "MAN: attack og release som du stiller dem (CL 1B-stil).\n"
                                              "BOTH: opto og manuell i parallell – den som demper mest bestemmer. Fanger raske topper og rir nivået."));
    hpf.slider.setTooltip (CharPointer_UTF8 ("Filtrerer bassen bort fra detektoren (ikke fra lyden), så kompressoren ikke pumper av "
                                             "proximity-effekt, plopp eller gitarkroppens boom."));
    tube.slider.setTooltip (CharPointer_UTF8 ("Rørkarakter: myk metning med mest 2. harmoniske. 0 = helt rent."));
    gain.slider.setTooltip (CharPointer_UTF8 ("Make-up gain på det komprimerte signalet."));
    mix.slider.setTooltip (CharPointer_UTF8 ("Parallellkompresjon: blander inn ukomprimert signal. WET = bare komprimert."));

    content.addAndMakeVisible (meter);
    meter.setOpaque (true);
    meter.setInterceptsMouseClicks (false, false);

    int i = 0;
    for (auto* b : { &meterGr, &meterIn, &meterOut })
    {
        content.addAndMakeVisible (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setColour (TextButton::buttonOnColourId, Colour (0xfff1e3b8));
        b->setColour (TextButton::textColourOnId, Colour (0xff1a2028));
        b->setColour (TextButton::textColourOffId, Colour (0xffc5cdd6));
        b->onClick = [this, i] { if (auto* btn = i == 0 ? &meterGr : i == 1 ? &meterIn : &meterOut; btn->getToggleState()) setMeterSource (i); };
        ++i;
    }
    meterGr.setTooltip ("Viser hvor mye kompressoren demper (gain reduction)");
    meterIn.setTooltip (CharPointer_UTF8 ("Viser inngangsnivå (0 VU = −18 dBFS)"));
    meterOut.setTooltip (CharPointer_UTF8 ("Viser utgangsnivå (0 VU = −18 dBFS)"));

    const int src = jlimit (0, 2, (int) proc.apvts.state.getProperty ("meterMode", 0));
    (src == 0 ? meterGr : src == 1 ? meterIn : meterOut).setToggleState (true, dontSendNotification);
    meter.setSource (src);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 9 / 5, designH * 9 / 5);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);

    updateEnablement();
    startTimerHz (60);
}

OptoTubeEditor::~OptoTubeEditor()
{
    setLookAndFeel (nullptr);
}

void OptoTubeEditor::setMeterSource (int s)
{
    proc.apvts.state.setProperty ("meterMode", s, nullptr);
    meter.setSource (s);
}

void OptoTubeEditor::updateEnablement()
{
    const int m = (int) proc.apvts.getRawParameterValue (ParamIDs::mode)->load();
    const float a = m == optotube::Opto ? 0.45f : 1.0f;
    attack.setAlpha (a);
    release.setAlpha (a);
}

void OptoTubeEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
}

void OptoTubeEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void OptoTubeEditor::layoutContent()
{
    const int top = 44;
    faceplate = { 0, top, designW, designH - top };

    presetBar.setBounds (190, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    const int rowA = top + 52, rowH = 150;
    const int rowB = rowA + rowH + 4;

    threshold.setBounds (54, rowA, 144, rowH);
    ratio.setBounds (198, rowA, 134, rowH);
    attack.setBounds (54, rowB, 139, rowH);
    release.setBounds (193, rowB, 139, rowH);

    meterArea = { 346, rowA + 6, 268, 196 };
    meter.setBounds (meterArea);
    auto btns = Rectangle<int> (meterArea.getX() + 40, meterArea.getBottom() + 30, meterArea.getWidth() - 80, 26);
    const int bw = btns.getWidth() / 3;
    meterGr.setBounds (btns.removeFromLeft (bw).reduced (3, 0));
    meterIn.setBounds (btns.removeFromLeft (bw).reduced (3, 0));
    meterOut.setBounds (btns.reduced (3, 0));

    mode.setBounds (628, rowA, 140, rowH);
    gain.setBounds (768, rowA, 140, rowH);
    hpf.setBounds (628, rowB, 100, rowH);
    tube.setBounds (728, rowB, 90, rowH);
    mix.setBounds (818, rowB, 90, rowH);

    leftGroup  = { 46, rowA - 6, 294, rowH * 2 + 14 };
    rightGroup = { 620, rowA - 6, 296, rowH * 2 + 14 };
}

void OptoTubeEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();

    // ---- top bar (shared SB Audio style) ----
    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    g.setFont (uiFont (18.0f, true));
    g.setColour (col::text);
    g.drawText ("Opto", Rectangle<float> (14.0f, 0.0f, 50.0f, 44.0f), Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("Tube", Rectangle<float> (57.0f, 0.0f, 50.0f, 44.0f), Justification::centredLeft);
    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText ("SB AUDIO", Rectangle<float> (106.0f, 0.0f, 70.0f, 44.0f), Justification::centredLeft);

    // ---- faceplate ----
    auto fp = faceplate.toFloat();
    g.setGradientFill (ColourGradient (faceTop, 0.0f, fp.getY(), faceBottom, 0.0f, fp.getBottom(), false));
    g.fillRect (fp);

    // brushed texture (deterministic)
    Random rnd (1234);
    for (float y = fp.getY(); y < fp.getBottom(); y += 1.0f)
    {
        const float a = rnd.nextFloat() * 0.045f;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (a));
        g.fillRect (fp.getX(), y, fp.getWidth(), 1.0f);
    }

    // rack ears with screws
    for (auto ear : { fp.withWidth (36.0f), fp.withLeft (fp.getRight() - 36.0f) })
    {
        g.setColour (earCol.withAlpha (0.55f));
        g.fillRect (ear);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRect (ear.getX() == 0.0f ? ear.withLeft (ear.getRight() - 1.0f) : ear.withWidth (1.0f));
        for (float sy : { ear.getY() + 34.0f, ear.getBottom() - 34.0f })
        {
            auto slot = Rectangle<float> (22.0f, 12.0f).withCentre ({ ear.getCentreX(), sy });
            g.setColour (Colour (0xff20262c));
            g.fillRoundedRectangle (slot, 6.0f);
            auto head = Rectangle<float> (14.0f, 14.0f).withCentre (slot.getCentre());
            g.setGradientFill (ColourGradient (Colour (0xffe9ecef), head.getX(), head.getY(),
                                               Colour (0xff70767c), head.getRight(), head.getBottom(), false));
            g.fillEllipse (head);
            g.setColour (Colour (0xff3a3f45));
            g.drawLine (head.getX() + 3.0f, head.getCentreY(), head.getRight() - 3.0f, head.getCentreY(), 1.6f);
            g.drawLine (head.getCentreX(), head.getY() + 3.0f, head.getCentreX(), head.getBottom() - 3.0f, 1.6f);
        }
    }

    // title row
    engravedText (g, "OptoTube", Rectangle<float> (56.0f, fp.getY() + 10.0f, 200.0f, 30.0f), Justification::centredLeft, 26.0f);
    engravedText (g, "OPTICAL TUBE COMPRESSOR", Rectangle<float> (200.0f, fp.getY() + 16.0f, 260.0f, 24.0f),
                  Justification::centredLeft, 10.5f, true, engrave.withAlpha (0.85f));
    engravedText (g, "SB AUDIO", Rectangle<float> (fp.getRight() - 256.0f, fp.getY() + 10.0f, 200.0f, 30.0f),
                  Justification::centredRight, 18.0f);
    engravedText (g, "MODEL OT-1", Rectangle<float> (fp.getRight() - 256.0f, fp.getY() + 31.0f, 200.0f, 14.0f),
                  Justification::centredRight, 9.5f, true, engrave.withAlpha (0.8f));

    // section separators
    const float sy1 = (float) leftGroup.getY() + 4.0f, sy2 = (float) leftGroup.getBottom() - 4.0f;
    engravedLine (g, 340.0f, sy1, 340.0f, sy2);
    engravedLine (g, 620.0f, sy1, 620.0f, sy2);
    engravedLine (g, 60.0f, (float) attack.getY() - 4.0f, 328.0f, (float) attack.getY() - 4.0f);
    engravedLine (g, 632.0f, (float) hpf.getY() - 4.0f, 904.0f, (float) hpf.getY() - 4.0f);

    engravedText (g, "METER", Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getBottom() + 8.0f,
                                                (float) meterArea.getWidth(), 16.0f), Justification::centred, 10.5f);
}

void OptoTubeEditor::timerCallback()
{
    const float gr = proc.meterGrDb.exchange (-1.0f);
    const float in = proc.meterInMs.exchange (-1.0f);
    const float out = proc.meterOutMs.exchange (-1.0f);

    if (gr >= 0.0f || in >= 0.0f || out >= 0.0f)
    {
        framesWithoutAudio = 0;
        if (gr >= 0.0f)  lastGr = gr;
        if (in >= 0.0f)  lastIn = in;
        if (out >= 0.0f) lastOut = out;
    }
    else if (++framesWithoutAudio > 20) // host stopped calling processBlock: let the needle rest
    {
        lastGr = 0.0f; lastIn = 0.0f; lastOut = 0.0f;
    }

    const int src = meterGr.getToggleState() ? 0 : meterIn.getToggleState() ? 1 : 2;
    auto vuOf = [] (float ms) { return 10.0f * std::log10 (ms + 1.0e-12f) + 18.0f; };

    if (src == 0)
    {
        meter.setValue (-lastGr);
        meter.setReadout (String (CharPointer_UTF8 ("GR ")) + String (lastGr, 1) + " dB");
    }
    else
    {
        const float ms = src == 1 ? lastIn : lastOut;
        const float dbfs = 10.0f * std::log10 (ms + 1.0e-12f);
        meter.setValue (vuOf (ms));
        meter.setReadout (dbfs < -90.0f ? String ("-inf dBFS") : String (dbfs, 1) + " dBFS");
    }

    updateEnablement();
}
