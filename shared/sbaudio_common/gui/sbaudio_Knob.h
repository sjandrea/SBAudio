#pragma once

namespace sbaudio
{
    /** Rotary knob with a caption above and an editable value box below. */
    struct Knob : public juce::Component
    {
        explicit Knob (const juce::String& caption);
        void resized() override;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
    };
}
