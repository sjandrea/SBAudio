namespace sbaudio
{
using namespace juce;

Knob::Knob (const String& caption)
{
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
    slider.setTextBoxIsEditable (true);
    slider.setDoubleClickReturnValue (false, 0.0);
    addAndMakeVisible (slider);
    label.setText (caption, dontSendNotification);
    label.setFont (uiFont (10.0f, true));
    label.setColour (Label::textColourId, colours::textDim);
    label.setJustificationType (Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (76, r.getWidth()), 18);
    slider.setBounds (r);
}
}
