project(PrismEQ VERSION 1.0.0)

juce_add_plugin(PrismEQ
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.prismeq"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Prsm
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "PrismEQ"
    DESCRIPTION                 "Dynamic parametric equalizer"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx EQ
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

target_sources(PrismEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EqGraph.cpp)

target_link_libraries(PrismEQ PRIVATE juce::juce_dsp)

sbaudio_configure_plugin(PrismEQ)
