cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Nave VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DNAVE_JUCE_DIR=/path/to/JUCE
set(NAVE_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(NAVE_JUCE_DIR)
    add_subdirectory(${NAVE_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Signal path (no JUCE) -----------------------------------------------------
# The reverb machines, the engine and the tail probe. The tests build against
# this alone.
add_library(NaveDSP STATIC
    Source/Engine/Types.cpp
    Source/Engine/Blocks.cpp
    Source/Engine/Space.cpp
    Source/Engine/Plate.cpp
    Source/Engine/Spring.cpp
    Source/Engine/Reflections.cpp
    Source/Engine/Nonlinear.cpp
    Source/Engine/Tape.cpp
    Source/Engine/Engine.cpp
    Source/Engine/Probe.cpp)
target_include_directories(NaveDSP PUBLIC Source)
set_target_properties(NaveDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(NaveDSP PRIVATE /utf-8)
endif()

# --- Plugin -------------------------------------------------------------------
# Copy the built plugin into the system VST3 folder after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3
#   Linux   ~/.vst3
#   Windows C:\Program Files\Common Files\VST3  (needs an administrator shell,
#           so it is off by default there)
if(WIN32)
    set(_nave_copy_default OFF)
else()
    set(_nave_copy_default ON)
endif()
option(NAVE_COPY_PLUGIN "Install the plugin into the system VST3 folder after building" ${_nave_copy_default})

juce_add_plugin(Nave
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.nave
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Nave
    PRODUCT_NAME                "Nave"
    DESCRIPTION                 "Reverb suite"
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Reverb
    COPY_PLUGIN_AFTER_BUILD     ${NAVE_COPY_PLUGIN})

set(NAVE_PLUGIN_SOURCES
    Source/Settings.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp
    Source/UI/Views.cpp)

target_sources(Nave PRIVATE ${NAVE_PLUGIN_SOURCES})
target_include_directories(Nave PRIVATE Source)
if(MSVC)
    target_compile_options(Nave PRIVATE /utf-8)
endif()
target_compile_definitions(Nave PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Nave
    PRIVATE
        NaveDSP
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(NaveEngineTests Tests/EngineTests.cpp)
target_link_libraries(NaveEngineTests PRIVATE NaveDSP)
add_test(NAME engine COMMAND NaveEngineTests)

# --- Optional dev tool --------------------------------------------------------
#   NaveSnapshot out.png [wavFolder] [input.wav] [preset] [browser]
#   renders the panel (and every preset) headless
option(NAVE_BUILD_TOOLS "Build the snapshot tool" OFF)
if(NAVE_BUILD_TOOLS)
    juce_add_console_app(NaveSnapshot PRODUCT_NAME "NaveSnapshot")
    target_sources(NaveSnapshot PRIVATE Tools/Snapshot.cpp ${NAVE_PLUGIN_SOURCES})
    target_include_directories(NaveSnapshot PRIVATE Source)
    if(MSVC)
        target_compile_options(NaveSnapshot PRIVATE /utf-8)
    endif()
    target_compile_definitions(NaveSnapshot PRIVATE
        JucePlugin_Name="Nave"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(NaveSnapshot PRIVATE
        NaveDSP
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
