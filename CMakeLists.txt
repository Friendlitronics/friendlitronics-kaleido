cmake_minimum_required(VERSION 3.22)

# --- macOS build settings (must be set before project()) ---
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

project(FriendlitronicsKaleido VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------
# Use a local checkout if there is one; otherwise fetch the pinned release, so
# a fresh clone builds with nothing but CMake and a compiler. JUCE 7.0.12
# predates the macOS 15 SDK, which removed an API it calls, hence the patch.
set(JUCE_TAG "7.0.12" CACHE STRING "JUCE release tag to fetch")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "using local JUCE at ./JUCE")
    add_subdirectory(JUCE)
else()
    message(STATUS "fetching JUCE ${JUCE_TAG}")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${JUCE_TAG}
        GIT_SHALLOW    TRUE
        PATCH_COMMAND  ${CMAKE_COMMAND}
                       -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-7.0.12-macos15.patch
                       -DWORK_DIR=<SOURCE_DIR>
                       -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyPatch.cmake)
    FetchContent_MakeAvailable(JUCE)
endif()

# Installing into ~/Library on every build is what you want at a desk and not
# what you want on a build machine.
if(DEFINED ENV{CI})
    set(COPY_AFTER_BUILD FALSE)
else()
    set(COPY_AFTER_BUILD TRUE)
endif()

juce_add_plugin(FriendlitronicsKaleido
    COMPANY_NAME            "Friendlitronics"
    BUNDLE_ID               "dev.elegancia.friendlitronicskaleido"
    PLUGIN_MANUFACTURER_CODE Frnd
    PLUGIN_CODE             Kald
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "Friendlitronics Kaleido"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    # The Standalone is a live-input effect, and macOS hands an app silent
    # input unless it declares microphone use. Notarisation needs the hardened
    # runtime, which in turn denies audio input unless it is asked for here.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Friendlitronics Kaleido processes audio from your input device."
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS "com.apple.security.device.audio-input"
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD})

juce_generate_juce_header(FriendlitronicsKaleido)

# DSP and engine are header-only; only the plugin shell and the look-and-feel
# need translation units.
target_sources(FriendlitronicsKaleido PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/KaleidoLookAndFeel.cpp)

target_compile_definitions(FriendlitronicsKaleido PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FriendlitronicsKaleido PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_compile_options(FriendlitronicsKaleido PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra>)

#==============================================================================
# Headless engine test. Built only on demand:
#   cmake --build build --target KaleidoEngineTest
#==============================================================================
juce_add_console_app(KaleidoEngineTest PRODUCT_NAME "KaleidoEngineTest")
set_target_properties(KaleidoEngineTest PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_sources(KaleidoEngineTest PRIVATE tests/EngineTest.cpp)
juce_generate_juce_header(KaleidoEngineTest)

target_compile_definitions(KaleidoEngineTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(KaleidoEngineTest PRIVATE
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_gui_basics
    juce::juce_core
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
