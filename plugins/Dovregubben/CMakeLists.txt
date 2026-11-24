project(Dovregubben VERSION 1.0.0)

juce_add_plugin(Dovregubben
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.dovregubben"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Dovr
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Dovregubben"
    DESCRIPTION                 "Character delay: tape, BBD, digital, pitch, reverse, ghost"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Delay
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Dovregubben PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

sbaudio_configure_plugin(Dovregubben)
