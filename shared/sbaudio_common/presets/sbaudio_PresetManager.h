#pragma once

namespace sbaudio
{
    /** A built-in preset: parameter ID -> plain (unnormalised) value.
        Parameters that are not listed are reset to their default. */
    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    /** Generic preset handling for plugins built on AudioProcessorValueTreeState.
        Factory presets are compiled in; user presets are XML files in
        ~/Library/Audio/Presets/SB Audio/<plugin>/ (the standard macOS location). */
    class PresetManager
    {
    public:
        PresetManager (juce::AudioProcessorValueTreeState& state,
                       const juce::String& pluginName,
                       const juce::String& fileExtension,
                       std::vector<FactoryPreset> factory,
                       juce::Array<juce::Identifier> keepOnLoad = {});

        struct Entry { juce::String name; bool factory; int index; };

        int getNumFactoryPresets() const { return (int) factory.size(); }
        juce::String getFactoryPresetName (int) const;
        void loadFactoryPreset (int);

        juce::File getUserPresetDirectory() const;
        juce::StringArray getUserPresetNames() const;
        bool saveUserPreset (const juce::String& name);
        bool loadUserPreset (const juce::String& name);
        bool deleteUserPreset (const juce::String& name);
        bool userPresetExists (const juce::String& name) const;

        /** Factory presets followed by user presets. */
        std::vector<Entry> getAllPresets() const;
        void load (const Entry&);
        /** Steps through getAllPresets() relative to the current preset (wraps around). */
        void step (int delta);

        juce::String getCurrentPresetName() const;
        bool isCurrentFactory() const;

    private:
        void setCurrent (const juce::String& name, bool isFactory);
        void setParam (juce::RangedAudioParameter&, float normalised);

        juce::AudioProcessorValueTreeState& apvts;
        juce::String pluginName, extension;
        std::vector<FactoryPreset> factory;
        juce::Array<juce::Identifier> keepOnLoad;
    };
}
