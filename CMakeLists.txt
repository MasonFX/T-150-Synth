cmake_minimum_required(VERSION 3.22)

# Mac: build one plugin that runs on both Apple Silicon and Intel, macOS 11+.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS supported")

# Windows: build the C++ runtime into the plugin, so users don't need to
# install anything else.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(T150Synth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pull JUCE automatically. If you already keep a JUCE checkout for your other
# plugins, swap this block for: add_subdirectory(path/to/JUCE JUCE)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# AU (Audio Unit, for Logic/GarageBand) only exists on Mac.
set(T150S_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND T150S_FORMATS AU)
endif()

juce_add_plugin(T150Synth
    COMPANY_NAME             "K"
    PLUGIN_MANUFACTURER_CODE Kplg
    PLUGIN_CODE              T15s
    FORMATS                  ${T150S_FORMATS}
    PRODUCT_NAME             "T150 Synth"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    VST3_CATEGORIES          Instrument Synth
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(T150Synth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(T150Synth PRIVATE Source)

target_compile_definitions(T150Synth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(T150Synth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Tests (plain C++, no JUCE needed) ----
enable_testing()
add_executable(dsp_test tests/dsp_test.cpp)
target_include_directories(dsp_test PRIVATE Source)
add_test(NAME dsp_test COMMAND dsp_test)

add_executable(synth_test tests/synth_test.cpp)
target_include_directories(synth_test PRIVATE Source)
add_test(NAME synth_test COMMAND synth_test)
