cmake_minimum_required(VERSION 3.22)
project(PseudoStereo VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PseudoStereo
    COMPANY_NAME "Pseudostereo Port"
    PLUGIN_MANUFACTURER_CODE Psvt
    PLUGIN_CODE Pss1
    PRODUCT_NAME "Pseudo-Stereo"
    FORMATS VST3
    VST3_CATEGORIES Fx Spatial
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)   # copy the .vst3 yourself (see README)

target_sources(PseudoStereo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PseudoStereoDSP.h)

target_compile_definitions(PseudoStereo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PseudoStereo
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
