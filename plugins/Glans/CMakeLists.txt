project(Glans VERSION 1.0.0)

juce_add_plugin(Glans
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.glans"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Glns
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Glans"
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

target_sources(Glans PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_link_libraries(Glans PRIVATE juce::juce_dsp)

sbaudio_configure_plugin(Glans)
