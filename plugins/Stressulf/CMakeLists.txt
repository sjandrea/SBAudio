project(Stressulf VERSION 1.0.0)

juce_add_plugin(Stressulf
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.stressulf"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Strs
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "Stressulf"
    DESCRIPTION                 "Character compressor"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(Stressulf PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

sbaudio_configure_plugin(Stressulf)
target_link_libraries(Stressulf PRIVATE juce::juce_dsp)
