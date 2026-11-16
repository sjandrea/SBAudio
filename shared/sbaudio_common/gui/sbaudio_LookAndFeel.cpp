namespace sbaudio
{
using namespace juce;

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour (ResizableWindow::backgroundColourId, bg);
    setColour (Label::textColourId, text);
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxBackgroundColourId, well);
    setColour (Slider::textBoxOutlineColourId, panelEdge);
    setColour (Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (Slider::rotarySliderFillColourId, accent);
    setColour (TextButton::buttonColourId, well);
    setColour (TextButton::buttonOnColourId, accent);
    setColour (TextButton::textColourOffId, textDim);
    setColour (TextButton::textColourOnId, bg);
    setColour (TextEditor::backgroundColourId, well);
    setColour (TextEditor::textColourId, text);
    setColour (TextEditor::highlightColourId, accent.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, panelEdge);
    setColour (TextEditor::focusedOutlineColourId, accent);
    setColour (CaretComponent::caretColourId, accent);
    setColour (ComboBox::backgroundColourId, well);
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::outlineColourId, panelEdge);
    setColour (ComboBox::arrowColourId, textDim);
    setColour (PopupMenu::backgroundColourId, panel);
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, text);
    setColour (PopupMenu::headerTextColourId, textDim);
    setColour (TooltipWindow::backgroundColourId, panel);
    setColour (TooltipWindow::textColourId, text);
    setColour (TooltipWindow::outlineColourId, panelEdge);
    setColour (AlertWindow::backgroundColourId, panel);
    setColour (AlertWindow::textColourId, text);
    setColour (AlertWindow::outlineColourId, panelEdge);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, Slider& s)
{
    const auto area   = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size  = jmin (area.getWidth(), area.getHeight());
    const auto centre = area.getCentre();
    const float r     = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto fill   = s.findColour (Slider::rotarySliderFillColourId);
    const float arcW  = jmax (2.5f, size * 0.045f);

    // Knobs with the "bipolar" property draw their value arc from zero.
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false)
                         && s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroPos = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    const float zeroAngle = startAngle + zeroPos * (endAngle - startAngle);

    Path track;
    track.addCentredArc (centre.x, centre.y, r - arcW, r - arcW, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::well);
    g.strokePath (track, PathStrokeType (arcW + 2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (std::abs (pos - zeroPos) > 0.001f && s.isEnabled())
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, r - arcW, r - arcW, 0.0f,
                             jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
        g.setColour (fill.withAlpha (0.25f));
        g.strokePath (value, PathStrokeType (arcW + 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (fill);
        g.strokePath (value, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float br = r - arcW * 2.0f - 5.0f;
    if (br < 2.0f)
        return;
    auto body = Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.0f));
    g.setGradientFill (ColourGradient (Colour (0xff2c313b), body.getX(), body.getY(),
                                       Colour (0xff14171c), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colour (0xff3a404b));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    const auto p1 = centre.getPointOnCircumference (br * 0.30f, angle);
    const auto p2 = centre.getPointOnCircumference (br * 0.82f, angle);
    g.setColour (s.isEnabled() ? colours::text : colours::textDim);
    g.drawLine ({ p1, p2 }, jmax (2.0f, size * 0.035f));
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (TextButton::buttonOnColourId);

    if (on)
    {
        g.setColour (onCol.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.0f), 5.0f);
        g.setGradientFill (ColourGradient (onCol.brighter (0.15f), r.getX(), r.getY(),
                                           onCol.darker (0.25f), r.getX(), r.getBottom(), false));
    }
    else
    {
        g.setColour (colours::well.brighter (over ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f));
    }
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? onCol.brighter (0.3f) : colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

Font LookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (12.0f, (float) h * 0.5f), true); }
Font LookAndFeel::getLabelFont (Label& l)                 { return l.getFont(); }
Font LookAndFeel::getComboBoxFont (ComboBox&)             { return uiFont (12.0f, true); }
Font LookAndFeel::getPopupMenuFont()                      { return uiFont (13.0f); }

Label* LookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.0f, true));
    l->setColour (Label::outlineColourId, colours::panelEdge);
    l->setColour (Label::backgroundColourId, colours::well);
    l->setColour (Label::textColourId, colours::text);
    l->setWantsKeyboardFocus (false); // otherwise it opens its editor when the window gets focus
    return l;
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (arrow);
}

void LookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
}
