cmake_minimum_required(VERSION 3.22)

project(MasterSuite VERSION 1.1.0 LANGUAGES C CXX)

# Static MSVC runtime: no Visual C++ Redistributable needed on the user PC
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_LOCAL_PATH)
    add_subdirectory(${JUCE_LOCAL_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(MS_BUILD_TESTS "Build the offline test runner" OFF)

juce_add_plugin(MasterSuite
    COMPANY_NAME                "ROHIT JI"
    BUNDLE_ID                   com.rohitji.mastersuite
    PLUGIN_MANUFACTURER_CODE    Rhji
    PLUGIN_CODE                 Mstr
    PRODUCT_NAME                "Master Suite"
    DESCRIPTION                 "Mastering suite: Mid/Side EQ, multiband dynamics + glue, exciter, imager, rebalance, loudness (LUFS) maximizer"
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Mastering)

set(MS_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_sources(MasterSuite PRIVATE ${MS_SOURCES})

set(MS_DEFINITIONS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
if(WIN32)
    list(APPEND MS_DEFINITIONS WINVER=0x0A00 _WIN32_WINNT=0x0A00 JUCE_USE_WINDOWS_MEDIA_FORMAT=0)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND MS_DEFINITIONS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XINPUT=0
         JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0)
endif()
target_compile_definitions(MasterSuite PUBLIC ${MS_DEFINITIONS})

target_link_libraries(MasterSuite
    PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(MS_BUILD_TESTS)
    juce_add_console_app(MSTests PRODUCT_NAME "MSTests")
    target_sources(MSTests PRIVATE ${MS_SOURCES} Tests/TestMain.cpp)
    target_compile_definitions(MSTests PRIVATE ${MS_DEFINITIONS} JucePlugin_Name="Master Suite"
        JucePlugin_Build_VST3=0 MS_TEST_BUILD=1 MS_SHOTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_include_directories(MSTests PRIVATE Source)
    target_link_libraries(MSTests PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
