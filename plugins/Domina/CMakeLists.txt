project(Domina VERSION 1.0.0)

juce_add_plugin(Domina
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.domina"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Glns
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Domina"
    DESCRIPTION                 "Smart mastering: analysis, style, loudness target"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Mastering
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Domina PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_binary_data(DominaAssets SOURCES Assets/background.jpg)

sbaudio_configure_plugin(Domina)
target_link_libraries(Domina PRIVATE juce::juce_dsp DominaAssets)
