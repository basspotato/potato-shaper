cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), runs on macOS 11 Big Sur and later
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# (APPLE is not defined before project(), so test the host system instead)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(PotatoShaper VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so the VST3 runs on any Windows 11 machine without extra redistributables
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PotatoShaper
    COMPANY_NAME "Basspotato"
    COMPANY_WEBSITE "https://github.com/basspotato/potato-shaper"
    BUNDLE_ID "com.basspotato.potatoshaper"
    PLUGIN_MANUFACTURER_CODE Bpot
    PLUGIN_CODE PtSh
    PRODUCT_NAME "Potato-Shaper"
    DESCRIPTION "Transient shaper for mixing and mastering"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    FORMATS VST3 AU
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(PotatoShaperFonts SOURCES
    Resources/Fonts/BarlowCondensed-SemiBold.ttf
    Resources/Fonts/BarlowCondensed-Bold.ttf
    Resources/Fonts/ShareTechMono-Regular.ttf)

target_sources(PotatoShaper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(PotatoShaper PRIVATE Source)

target_compile_definitions(PotatoShaper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Only used for compile tests on headless Linux machines without the full X11 dev headers
option(POTATO_MINIMAL_X11 "Disable optional X11 extensions (Linux test builds only)" OFF)
if(POTATO_MINIMAL_X11)
    target_compile_definitions(PotatoShaper PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
endif()

target_link_libraries(PotatoShaper
    PRIVATE
        PotatoShaperFonts
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (null test, transient/sustain response, level independence, CPU): -DPOTATO_TESTS=ON
option(POTATO_TESTS "Build the offline DSP test runner" OFF)
if(POTATO_TESTS)
    juce_add_console_app(PotatoShaperTests PRODUCT_NAME "PotatoShaperTests")
    target_sources(PotatoShaperTests PRIVATE tests/OfflineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(PotatoShaperTests PRIVATE Source)
    target_compile_definitions(PotatoShaperTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(PotatoShaperTests PRIVATE PotatoShaperFonts
        juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
