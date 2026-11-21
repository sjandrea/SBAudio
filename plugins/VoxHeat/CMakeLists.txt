project(VoxHeat VERSION 1.0.0)

juce_add_plugin(VoxHeat
    COMPANY_NAME                "${SBAUDIO_COMPANY}"
    BUNDLE_ID                   "com.sbaudio.voxheat"
    PLUGIN_MANUFACTURER_CODE    ${SBAUDIO_MANUFACTURER}
    PLUGIN_CODE                 Vxht
    FORMATS                     ${SBAUDIO_FORMATS}
    PRODUCT_NAME                "VoxHeat"
    DESCRIPTION                 "Vocal saturator"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_SANDBOX_SAFE             TRUE
    VST3_CATEGORIES             Fx Distortion
    COPY_PLUGIN_AFTER_BUILD     ${SBAUDIO_COPY})

# Faceplate artwork (made from artwork/source.png by artwork/make_assets.py)
juce_add_binary_data(VoxHeatAssets SOURCES
    Assets/background.jpg
    Assets/heat_on.png
    Assets/auto_on.png)

target_sources(VoxHeat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

sbaudio_configure_plugin(VoxHeat)
target_link_libraries(VoxHeat PRIVATE VoxHeatAssets juce::juce_dsp)
