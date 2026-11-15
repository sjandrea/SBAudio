# Shared settings for all SB Audio plugins.

set(SBAUDIO_COMPANY      "SB Audio")
set(SBAUDIO_MANUFACTURER Sbau)

if(APPLE)
    set(SBAUDIO_FORMATS AU VST3 Standalone)
    set(SBAUDIO_COPY TRUE)
else()
    set(SBAUDIO_FORMATS VST3 Standalone)
    set(SBAUDIO_COPY FALSE)
endif()

# Call after juce_add_plugin(): common defines and links for every plugin.
function(sbaudio_configure_plugin target)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            sbaudio_common
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
