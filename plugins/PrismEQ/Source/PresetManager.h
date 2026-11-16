#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PrismEQProcessor;

/** Factory presets (built in) and user presets (XML files in
    ~/Library/Audio/Presets/SB Audio/PrismEQ). */
class PresetManager
{
public:
    explicit PresetManager (PrismEQProcessor&);

    struct Entry { juce::String name; bool factory; int index; };

    int getNumFactoryPresets() const;
    juce::String getFactoryPresetName (int) const;
    void loadFactoryPreset (int);

    static juce::File getUserPresetDirectory();
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    bool userPresetExists (const juce::String& name) const;

    /** Factory presets followed by user presets. */
    std::vector<Entry> getAllPresets() const;
    void load (const Entry&);
    /** Steps through getAllPresets() relative to the current preset. */
    void step (int delta);

    juce::String getCurrentPresetName() const;
    bool isCurrentFactory() const;

    static constexpr const char* fileExtension = ".prismpreset";

private:
    void setCurrent (const juce::String& name, bool factory);
    PrismEQProcessor& proc;
};
