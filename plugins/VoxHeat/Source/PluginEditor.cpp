#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace voxheat::ui;
using sbaudio::uiFont;
namespace col = sbaudio::colours;

namespace
{
    // ---- Geometry measured on the faceplate artwork (image pixels, 1592 x 988) ----
    constexpr float imgScale = 0.5f;                    // design units = image / 2
    constexpr float rotRange = 130.0f;                  // knob travel, +/- degrees

    const Rectangle<float> vuFace { 524.0f, 198.0f, 546.0f, 272.0f };
    const Point<float> vuPivot { 795.2f, 565.8f };
    constexpr float vuRadius = 285.7f;
    constexpr float vuMinDeg = -44.4f, vuMaxDeg = 45.3f;

    const Point<float> heatLamp { 230.0f, 77.0f };
    const Point<float> autoLamp { 1230.0f, 78.0f };
    constexpr float lampHalf = 40.0f;                   // lamp artwork is 80 x 80 image px
    const Rectangle<float> toggleArea { 172.0f, 700.0f, 86.0f, 160.0f };
    const Point<float> toggleNut { 215.0f, 782.0f };
    const Point<float> tgCaption { 215.0f, 685.0f }, tubeCaption { 215.0f, 871.0f };

    // SB Audio hardware palette (shared with OptoTube)
    const Colour engrave    { 0xfff5f7f9 };
    const Colour engraveSh  { 0x66223040 };
    const Colour readoutBg  { 0xff1a2028 };
    const Colour readoutTxt { 0xfff1e3b8 };
    const Colour ink        { 0xff1b1a17 };

    Rectangle<float> ui (Rectangle<float> img) { return (img * imgScale).translated (0.0f, (float) VoxHeatEditor::barH); }

    Image loadImg (const void* data, int size) { return ImageCache::getFromMemory (data, size); }

    void engravedText (Graphics& g, const String& s, Rectangle<float> r, float h, Colour c = engrave)
    {
        g.setFont (uiFont (h, true));
        g.setColour (engraveSh);
        g.drawText (s, r.translated (0.0f, 1.0f), Justification::centred, false);
        g.setColour (c);
        g.drawText (s, r, Justification::centred, false);
    }
}

//==============================================================================
FaceplateLookAndFeel::FaceplateLookAndFeel()
{
    autoOn = loadImg (BinaryData::auto_on_png, BinaryData::auto_on_pngSize);
}

void FaceplateLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                             float a0, float a1, Slider& s)
{
    // The knob body is part of the photo faceplate; only the pointer is drawn.
    // Lighting stays fixed (from the upper left) while the pointer turns.
    const bool big = s.getComponentID() == "drive";
    const float angle = a0 + pos * (a1 - a0);
    const auto c = Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) w * 0.5f / 1.12f;          // skirt radius
    const auto rot = AffineTransform::rotation (angle).translated (c);

    if (! big)
    {
        // Chicken-head nose
        Path nose;
        nose.startNewSubPath (-0.40f * R, -0.30f * R);
        nose.lineTo (-0.15f * R, -0.97f * R);
        nose.quadraticTo (0.0f, -1.02f * R, 0.15f * R, -0.97f * R);
        nose.lineTo (0.40f * R, -0.30f * R);
        nose.quadraticTo (0.0f, -0.10f * R, -0.40f * R, -0.30f * R);
        nose.closeSubPath();

        for (int i = 3; i >= 1; --i)   // soft drop shadow
        {
            g.setColour (Colours::black.withAlpha (0.10f));
            g.fillPath (nose, rot.translated (0.8f * (float) i, 1.6f * (float) i));
        }
        g.setGradientFill (ColourGradient (Colour (0xff3a3633), c.x - R * 0.5f, c.y - R * 0.6f,
                                           Colour (0xff0e0c0b), c.x + R * 0.6f, c.y + R * 0.4f, false));
        g.fillPath (nose, rot);

        Path face;                     // flat top face slightly darker than the bevels
        face.startNewSubPath (-0.22f * R, -0.32f * R);
        face.lineTo (-0.08f * R, -0.93f * R);
        face.lineTo (0.08f * R, -0.93f * R);
        face.lineTo (0.22f * R, -0.32f * R);
        face.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillPath (face, rot);

        const float lightSide = std::sin (angle + 0.6f) >= 0.0f ? -1.0f : 1.0f;
        Path edge;
        edge.startNewSubPath (lightSide * 0.40f * R, -0.30f * R);
        edge.lineTo (lightSide * 0.15f * R, -0.97f * R);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.strokePath (edge, PathStrokeType (1.0f), rot);

        Path inlay;                    // cream inlay
        inlay.addRoundedRectangle (-0.045f * R, -0.93f * R, 0.09f * R, 0.56f * R, 0.03f * R);
        g.setColour (Colour (0xff1a1715));
        g.strokePath (inlay, PathStrokeType (1.2f), rot);
        g.setGradientFill (ColourGradient (Colour (0xfff7f0dc), c.x - R * 0.3f, c.y - R * 0.6f,
                                           Colour (0xffbfb59a), c.x + R * 0.3f, c.y + R * 0.2f, false));
        g.fillPath (inlay, rot);
    }
    else
    {
        Path groove;                   // large fluted knob: recessed cream line
        groove.addRoundedRectangle (-0.048f * R, -0.86f * R, 0.096f * R, 0.50f * R, 0.04f * R);
        g.setColour (Colours::black.withAlpha (0.75f));
        g.strokePath (groove, PathStrokeType (2.2f), rot);
        g.setGradientFill (ColourGradient (Colour (0xfffbf2d8), c.x - R * 0.4f, c.y - R * 0.5f,
                                           Colour (0xffc7a974), c.x + R * 0.4f, c.y + R * 0.3f, false));
        g.fillPath (groove, rot);
        Path shade;
        shade.addRoundedRectangle (-0.048f * R, -0.86f * R, 0.096f * R, 0.06f * R, 0.03f * R);
        g.setColour (Colour (0xff4a3418).withAlpha (0.6f));
        g.fillPath (shade, rot);
    }
}

void FaceplateLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    if (b.getComponentID() != "char")
    {
        if (b.getToggleState())        // green jewel lit; "off" is what the faceplate shows
        {
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (autoOn, Rectangle<float> (0.0f, 0.0f, lampHalf, lampHalf));
        }
        return;
    }

    // Chrome bat-handle lever (up = TG, down = TUBE), drawn in image coordinates
    g.addTransform (AffineTransform::scale (imgScale));
    const auto nut = toggleNut - toggleArea.getPosition();
    const bool down = b.getToggleState();
    const float len = down ? 54.0f : 74.0f;            // pointing down = towards the viewer
    const float tipR = down ? 15.0f : 13.5f;
    const Point<float> tip (nut.x, nut.y + (down ? len : -len));

    g.setColour (Colour (0xff0d0d0e));                 // hole in the nut
    g.fillEllipse (nut.x - 13.0f, nut.y - 12.0f, 26.0f, 24.0f);
    g.setColour (Colour (0xff3b3d40));
    g.drawEllipse (nut.x - 13.0f, nut.y - 12.0f, 26.0f, 24.0f, 1.5f);

    Path shaft;
    shaft.startNewSubPath (nut.x - 8.5f, nut.y);
    shaft.lineTo (tip.x - 12.0f, tip.y);
    shaft.lineTo (tip.x + 12.0f, tip.y);
    shaft.lineTo (nut.x + 8.5f, nut.y);
    shaft.closeSubPath();
    shaft.addEllipse (tip.x - tipR, tip.y - tipR, tipR * 2, tipR * 2);
    shaft.addEllipse (nut.x - 9.5f, nut.y - 9.5f, 19.0f, 19.0f);

    g.setColour (Colours::black.withAlpha (0.18f));
    g.fillPath (shaft, AffineTransform::translation (5.0f, 7.0f));
    g.fillPath (shaft, AffineTransform::translation (3.0f, 4.0f));

    ColourGradient chrome (Colour (0xff5d6063), tip.x - 15.0f, 0, Colour (0xff45484b), tip.x + 15.0f, 0, false);
    chrome.addColour (0.30, Colour (0xfff4f5f5));
    chrome.addColour (0.48, Colour (0xffb9bcbe));
    chrome.addColour (0.75, Colour (0xff7d8083));
    g.setGradientFill (chrome);
    g.fillPath (shaft);

    g.setColour (Colours::white.withAlpha (0.85f));
    g.fillEllipse (tip.x - tipR * 0.55f, tip.y - tipR * 0.6f, tipR * 0.6f, tipR * 0.45f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (tip.x - tipR, tip.y - tipR, tipR * 2, tipR * 2, 0.8f);
}

void FaceplateLookAndFeel::drawLabel (Graphics& g, Label& l)
{
    if (l.getComponentID() != "readout")
        return sbaudio::LookAndFeel::drawLabel (g, l);

    // Value readout box, same style as OptoTube
    auto r = l.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 3.0f);
    g.setColour (readoutBg);
    g.fillRoundedRectangle (r, 3.0f);
    if (! l.isBeingEdited())
    {
        g.setColour (readoutTxt);
        g.setFont (uiFont (11.0f, true));
        g.drawText (l.getText(), r, Justification::centred, false);
    }
}

//==============================================================================
VuMeter::VuMeter (VoxHeatProcessor& p)
    : proc (p), heatShown (p.heat.load()), vu (jlimit (-40.0f, 6.0f, p.outRmsDb.load() + 18.0f))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void VuMeter::timerCallback()
{
    const float target = jlimit (-40.0f, 6.0f, proc.outRmsDb.load() + 18.0f);   // 0 VU = -18 dBFS
    vu += (target - vu) * 0.28f;
    heatShown += (proc.heat.load() - heatShown) * 0.25f;
    repaint();
}

void VuMeter::paint (Graphics& g)
{
    // Component covers the VU face; draw in image coordinates relative to it.
    g.addTransform (AffineTransform::scale (imgScale));
    const auto pivot = vuPivot - vuFace.getPosition();
    const auto face = Rectangle<float> (vuFace.getWidth(), vuFace.getHeight());

    if (heatShown > 0.01f)             // backlight warms up with saturation
    {
        g.setGradientFill (ColourGradient (Colour (0xffff9a2e).withAlpha (0.20f * heatShown), face.getCentreX(), 10.0f,
                                           Colours::transparentBlack, face.getCentreX(), face.getBottom(), true));
        g.fillRect (face);
    }

    g.setColour (ink.withAlpha (0.85f));
    g.setFont (uiFont (17.0f, true));
    g.drawText (String (CharPointer_UTF8 ("SB AUDIO \xe2\x80\x93 STUDIO SERIES")),
                Rectangle<float> (18.0f, 244.0f, 320.0f, 20.0f), Justification::centredLeft);

    // Needle on a voltage-linear scale, like a real VU
    const float lo = 0.1f, hi = std::pow (10.0f, 3.0f / 20.0f);
    const float pos = jlimit (-0.03f, 1.04f, (std::pow (10.0f, vu / 20.0f) - lo) / (hi - lo));
    const float a = degreesToRadians (vuMinDeg + (vuMaxDeg - vuMinDeg) * pos);
    auto pt = [&] (float rad) { return Point<float> (pivot.x + std::sin (a) * rad, pivot.y - std::cos (a) * rad); };

    g.reduceClipRegion (face.withTrimmedBottom (2.0f).toNearestInt());
    const Line<float> needle (pt (0.0f), pt (vuRadius + 14.0f));
    g.setColour (Colours::black.withAlpha (0.16f));
    g.drawLine (needle.getStartX() + 7, needle.getStartY() + 5, needle.getEndX() + 7, needle.getEndY() + 5, 4.0f);
    g.setColour (Colour (0xff111010));
    g.drawLine (needle, 4.2f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawLine (needle.getStartX() - 1.5f, needle.getStartY(), needle.getEndX() - 1.5f, needle.getEndY(), 1.0f);
}

//==============================================================================
VoxHeatEditor::VoxHeatEditor (VoxHeatProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets), meter (p)
{
    setLookAndFeel (&lnf);
    faceplate = loadImg (BinaryData::background_jpg, BinaryData::background_jpgSize);
    heatOn    = loadImg (BinaryData::heat_on_png,    BinaryData::heat_on_pngSize);
    lampShown = p.heat.load();

    addAndMakeVisible (content);
    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    setupKnob (input,   ParamIDs::input,   { 274, 336 },  82, 478, CharPointer_UTF8 ("Nivå inn i metningen. Mer input = vokalen presses hardere inn i kurven."));
    setupKnob (output,  ParamIDs::output,  { 1318, 328 }, 83, 478, CharPointer_UTF8 ("Utgangsnivå."));
    setupKnob (density, ParamIDs::density, { 474, 756 },  81, 907, CharPointer_UTF8 ("Kompander: svake og sterke stavelser får like mye farge, mens dynamikken beholdes."));
    setupKnob (drive,   ParamIDs::drive,   { 796, 748 }, 106, 930, CharPointer_UTF8 ("Mengden metning."));
    setupKnob (tone,    ParamIDs::tone,    { 1120, 752 }, 82, 907, CharPointer_UTF8 ("Tilt-EQ etter metningen: mot venstre mørkere og mykere, mot høyre lysere med mer luft."));
    setupKnob (mix,     ParamIDs::mix,     { 1384, 758 }, 81, 907, CharPointer_UTF8 ("Blanding av tørt og mettet signal (parallell metning)."));
    drive.slider.setComponentID ("drive");

    charSwitch.setComponentID ("char");
    charSwitch.setTooltip (CharPointer_UTF8 ("TG: transistor/konsoll – framoverlent og tydelig, mest odde overtoner.\n"
                                             "TUBE: rør – rundere, varmere og fyldigere, mest like overtoner."));
    charSwitch.setMouseCursor (MouseCursor::PointingHandCursor);
    charSwitch.onStateChange = [this] { content.repaint (ui (toggleArea.expanded (60.0f, 40.0f)).getSmallestIntegerContainer()); };
    content.addAndMakeVisible (charSwitch);
    charAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::character, charSwitch);

    autoGain.setTooltip (CharPointer_UTF8 ("Auto Gain: holder lydnivået omtrent likt når du endrer Drive."));
    autoGain.setMouseCursor (MouseCursor::PointingHandCursor);
    content.addAndMakeVisible (autoGain);
    autoAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::autogain, autoGain);

    content.addAndMakeVisible (meter);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 9 / 5, designH * 9 / 5);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);
    startTimerHz (20);
}

VoxHeatEditor::~VoxHeatEditor()
{
    setLookAndFeel (nullptr);
}

void VoxHeatEditor::setupKnob (Knob& k, const char* id, Point<float> centre, float radius,
                               float valueY, const String& tip)
{
    k.centre = centre; k.radius = radius; k.valueY = valueY;

    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    k.slider.setRotaryParameters (degreesToRadians (-rotRange), degreesToRadians (rotRange), true);
    k.slider.setTooltip (tip);
    k.slider.setMouseCursor (MouseCursor::PointingHandCursor);
    content.addAndMakeVisible (k.slider);
    k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.slider.setNumDecimalPlacesToDisplay (1);
    const String pid (id);
    k.slider.setTextValueSuffix ((pid == ParamIDs::input || pid == ParamIDs::output || pid == ParamIDs::tone) ? " dB" : " %");
    if (auto* param = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    // Value readout (double-click to type a value)
    k.value.setComponentID ("readout");
    k.value.setFont (uiFont (11.0f, true));
    k.value.setJustificationType (Justification::centred);
    k.value.setEditable (false, true, false);
    k.value.setColour (Label::textWhenEditingColourId, readoutTxt);
    k.value.setColour (TextEditor::textColourId, readoutTxt);
    k.value.setColour (TextEditor::backgroundColourId, readoutBg);
    k.value.setColour (TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    k.value.setColour (TextEditor::focusedOutlineColourId, col::accent);
    k.value.setText (k.slider.getTextFromValue (k.slider.getValue()), dontSendNotification);
    auto* kp = &k;
    k.slider.onValueChange = [kp] { kp->value.setText (kp->slider.getTextFromValue (kp->slider.getValue()), dontSendNotification); };
    k.value.onTextChange = [kp]
    {
        kp->slider.setValue (kp->slider.getValueFromText (kp->value.getText()), sendNotificationSync);
        kp->value.setText (kp->slider.getTextFromValue (kp->slider.getValue()), dontSendNotification);
    };
    content.addAndMakeVisible (k.value);
}

void VoxHeatEditor::timerCallback()
{
    const float h = proc.heat.load();
    if (std::abs (h - lampShown) > 0.005f)
    {
        lampShown += (h - lampShown) * 0.35f;
        content.repaint (ui (Rectangle<float> (lampHalf * 2, lampHalf * 2).withCentre (heatLamp)).getSmallestIntegerContainer());
    }
}

void VoxHeatEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
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
    presetBar.setBounds (170, 8, 440, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    for (auto* k : { &input, &output, &density, &drive, &tone, &mix })
    {
        const float half = k->radius * 1.12f;
        k->slider.setBounds (ui (Rectangle<float> (half * 2, half * 2).withCentre (k->centre)).getSmallestIntegerContainer());
        k->value.setBounds (ui (Rectangle<float> (150.0f, 40.0f).withCentre ({ k->centre.x, k->valueY })).getSmallestIntegerContainer());
    }
    charSwitch.setBounds (ui (toggleArea).getSmallestIntegerContainer());
    autoGain.setBounds (ui (Rectangle<float> (autoLamp.x - lampHalf, autoLamp.y - lampHalf, 220.0f, lampHalf * 2)).getSmallestIntegerContainer());
    meter.setBounds (ui (vuFace).getSmallestIntegerContainer());
}

void VoxHeatEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();

    // ---- top bar (shared SB Audio style) ----
    auto bar = full.withHeight ((float) barH);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    const auto nameFont = uiFont (18.0f, true);
    const float voxW = GlyphArrangement::getStringWidth (nameFont, "Vox");
    const float heatW = GlyphArrangement::getStringWidth (nameFont, "Heat");
    g.setFont (nameFont);
    g.setColour (col::text);
    g.drawText ("Vox", Rectangle<float> (14.0f, 0.0f, voxW + 4.0f, (float) barH), Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("Heat", Rectangle<float> (14.0f + voxW, 0.0f, heatW + 4.0f, (float) barH), Justification::centredLeft);
    g.setFont (uiFont (9.0f, true));
    g.setColour (col::textDim);
    g.drawText ("SB AUDIO", Rectangle<float> (14.0f + voxW + heatW + 10.0f, 0.0f, 70.0f, (float) barH), Justification::centredLeft);

    // ---- faceplate ----
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (faceplate, ui (Rectangle<float> (1592.0f, 988.0f)));

    // HEAT pilot lamp: crossfade from the unlit faceplate to the lit jewel
    const float glow = jlimit (0.0f, 1.0f, lampShown * 1.6f);
    if (glow > 0.01f)
    {
        g.setOpacity (glow);
        g.drawImage (heatOn, ui (Rectangle<float> (lampHalf * 2, lampHalf * 2).withCentre (heatLamp)));
        g.setOpacity (1.0f);
    }

    // TG / TUBE captions: the selected position is lit
    const bool tube = charSwitch.getToggleState();
    engravedText (g, "TG",   ui (Rectangle<float> (120.0f, 34.0f).withCentre (tgCaption)),   12.0f, engrave.withAlpha (tube ? 0.45f : 1.0f));
    engravedText (g, "TUBE", ui (Rectangle<float> (160.0f, 34.0f).withCentre (tubeCaption)), 12.0f, engrave.withAlpha (tube ? 1.0f : 0.45f));
}
