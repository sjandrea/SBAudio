cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) for Logic Pro. Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(SBAudio LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched once for all plugins. Use a local checkout with -DJUCE_DIR=/path/to/JUCE.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

include(cmake/SBAudio.cmake)

# Shared SB Audio code (look & feel, colours, meters …)
juce_add_module(shared/sbaudio_common)

# ---- Plugins -------------------------------------------------------------
# Build a single plugin only: cmake -B build -DSBAUDIO_ONLY=VoxPilot
set(SBAUDIO_ONLY "" CACHE STRING "Configure only this plugin (empty = all)")

file(GLOB SBAUDIO_PLUGIN_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*)
foreach(dir ${SBAUDIO_PLUGIN_DIRS})
    get_filename_component(name ${dir} NAME)
    if(IS_DIRECTORY ${dir} AND EXISTS ${dir}/CMakeLists.txt AND NOT name MATCHES "^_")
        if(SBAUDIO_ONLY STREQUAL "" OR SBAUDIO_ONLY STREQUAL name)
            add_subdirectory(${dir})
        endif()
    endif()
endforeach()
