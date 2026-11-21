project(Nordlys VERSION 1.0.0)

juce_add_plugin(Nordlys
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.nordlys"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Nrdl
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Nordlys"
    DESCRIPTION                 "Atmospheric layered synthesizer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Nordlys PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

sbaudio_configure_plugin(Nordlys)
target_link_libraries(Nordlys PRIVATE juce::juce_dsp)
