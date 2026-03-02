cmake_minimum_required(VERSION 3.22)

project(FFTFreezer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FFTFreezer
    COMPANY_NAME "VBFreezer"
    BUNDLE_ID "com.vbfreezer.fftfreezer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE VbFz
    PLUGIN_CODE FrZr
    FORMATS VST3 Standalone
    PRODUCT_NAME "FFT Freezer"
)

juce_generate_juce_header(FFTFreezer)

target_sources(FFTFreezer
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_compile_definitions(FFTFreezer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_compile_options(FFTFreezer PRIVATE
    -Wall
    -Wno-sign-conversion
    -Wno-overloaded-virtual
    -Wno-unused-parameter
)

target_link_libraries(FFTFreezer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
