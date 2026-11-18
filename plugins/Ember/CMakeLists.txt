project(Ember VERSION 1.0.0)

juce_add_plugin(Ember
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.ember"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Embr
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Ember"
    DESCRIPTION                 "Multiband saturation"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Distortion
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Ember PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_link_libraries(Ember PRIVATE juce::juce_dsp)

sbaudio_configure_plugin(Ember)
