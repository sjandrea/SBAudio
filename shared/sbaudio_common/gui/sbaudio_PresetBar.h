#pragma once

namespace sbaudio
{
    /** Inline "save preset" dialog drawn over the whole editor. */
    class PresetSaveOverlay : public juce::Component
    {
    public:
        PresetSaveOverlay();
        void show (const juce::String& initialName);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override { setVisible (false); }

        std::function<void (const juce::String&)> onSave;
        std::function<bool (const juce::String&)> exists;

    private:
        juce::Rectangle<int> box() const;
        void updateHint();
        juce::TextEditor name;
        juce::TextButton saveBtn { "LAGRE" }, cancelBtn { "AVBRYT" };
        juce::Label hint;
    };

    /** Preset strip:  < [preset name ▾] >  LAGRE
        Call attachOverlayTo (editor) once, and keep the overlay sized to the editor
        with getOverlay().setBounds (getLocalBounds()) in the editor's resized(). */
    class PresetBar : public juce::Component, private juce::Timer
    {
    public:
        explicit PresetBar (PresetManager&);
        ~PresetBar() override;

        void attachOverlayTo (juce::Component& editor);
        PresetSaveOverlay& getOverlay() { return overlay; }
        void refresh();

        /** Called after the user loads a preset from the bar. */
        std::function<void()> onPresetLoaded;

        void resized() override;

    private:
        void timerCallback() override;
        void showMenu();

        PresetManager& presets;
        juce::TextButton prevBtn { "<" }, nextBtn { ">" }, nameBtn, saveBtn { "LAGRE" };
        PresetSaveOverlay overlay;
        juce::String shown;
        bool shownFactory = false;
    };
}
