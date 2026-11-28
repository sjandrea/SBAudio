project(Renske VERSION 1.0.0)

juce_add_plugin(Renske
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.renske"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Rnsk
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Renske"
    DESCRIPTION                 "Audio repair suite"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Restoration
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Renske PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_link_libraries(Renske PRIVATE juce::juce_dsp)

sbaudio_configure_plugin(Renske)
