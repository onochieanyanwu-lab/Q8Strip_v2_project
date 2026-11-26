cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime so the VST3 has no VC++ redistributable dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Q8Strip VERSION 0.2.1)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Q8Strip
    COMPANY_NAME "DIYAudio"
    PLUGIN_MANUFACTURER_CODE Dyau
    PLUGIN_CODE Q8s2
    FORMATS VST3
    PRODUCT_NAME "Q8 Strip V2"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# SOURCES: files live next to this CMakeLists.txt (flat repo layout).
# If you put them in a "Source" folder instead, change these two lines to Source/PluginProcessor.cpp etc.
target_sources(Q8Strip PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(Q8Strip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Q8Strip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
