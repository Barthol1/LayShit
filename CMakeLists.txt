cmake_minimum_required(VERSION 3.22)
project(LayShit VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(libs/JUCE)

option(LAYSHIT_BUILD_PLUGIN "Build the plugin targets" ON)
if(LAYSHIT_BUILD_PLUGIN)
# ---------------------------------------------------------------- plugin
juce_add_plugin(LayShit
    COMPANY_NAME "LayShit Audio"
    PLUGIN_MANUFACTURER_CODE Lysh
    PLUGIN_CODE Lysh
    FORMATS VST3 Standalone
    PRODUCT_NAME "LayShit"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay)

target_sources(LayShit PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/gfx/BowlRenderer.cpp
    Source/gfx/Meshes.cpp
    Source/gfx/OldMan.cpp
    Source/ui/LayShitLookAndFeel.cpp
    Source/ui/ChainStrip.cpp
    Source/ui/PresetBar.cpp
    Source/ui/FlushButton.cpp)

target_include_directories(LayShit PRIVATE Source)

target_compile_definitions(LayShit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LayShit
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

endif()

# ---------------------------------------------------------------- DSP tests
juce_add_console_app(LayShitTests PRODUCT_NAME "LayShitTests")

target_sources(LayShitTests PRIVATE
    tests/TestMain.cpp
    tests/DspTests.cpp
    tests/OldManTests.cpp
    Source/gfx/OldMan.cpp)

target_include_directories(LayShitTests PRIVATE Source)

target_compile_definitions(LayShitTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=1)

target_link_libraries(LayShitTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
