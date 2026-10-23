cmake_minimum_required(VERSION 3.22)

project(SaxHarmonizer VERSION 1.0.0 LANGUAGES C CXX)

# Match the Projucer build (C++17, see CLAUDE.md)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the JUCE checkout (the folder containing JUCE's own CMakeLists.txt).
# Override with -DJUCE_DIR=... if JUCE lives somewhere else.
set(JUCE_DIR "C:/Program Files/JUCE" CACHE PATH "Path to the JUCE source folder")
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

# ---------------------------------------------------------------------------
# Shared DSP code (everything except the GUI), used by the app and the tests
# ---------------------------------------------------------------------------
set(THIRDPARTY_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-stretch/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/signalsmith-linear/include")

set(JUCE_COMMON_DEFINITIONS
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# ---------------------------------------------------------------------------
# The app
# ---------------------------------------------------------------------------
juce_add_gui_app(SaxHarmonizer
    PRODUCT_NAME "sax-harmonizer")

# Sources include <JuceHeader.h>, so generate one (also defines ProjectInfo)
juce_generate_juce_header(SaxHarmonizer)

target_sources(SaxHarmonizer PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DSP/HarmonizerEngine.cpp
    Source/DSP/PitchDetector.cpp
    Source/DSP/PitchShifter.cpp
    Source/GUI/NoteKeyboard.cpp
    Source/Midi/NoteController.cpp
    Source/Utils/TuningUtils.cpp)

target_include_directories(SaxHarmonizer PRIVATE ${THIRDPARTY_INCLUDES})

target_compile_definitions(SaxHarmonizer PRIVATE
    ${JUCE_COMMON_DEFINITIONS}
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SaxHarmonizer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SaxHarmonizer,JUCE_VERSION>")

target_link_libraries(SaxHarmonizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests (plain console apps that return non-zero on failure; run with ctest)
# ---------------------------------------------------------------------------
option(SAX_BUILD_TESTS "Build the DSP test executables" ON)

if(SAX_BUILD_TESTS)
    enable_testing()

    function(sax_add_test_app name)
        juce_add_console_app(${name} PRODUCT_NAME "${name}")
        juce_generate_juce_header(${name})
        target_sources(${name} PRIVATE ${ARGN})
        target_include_directories(${name} PRIVATE ${THIRDPARTY_INCLUDES})
        target_compile_definitions(${name} PRIVATE ${JUCE_COMMON_DEFINITIONS})
        target_link_libraries(${name}
            PRIVATE
                juce::juce_audio_basics
                juce::juce_audio_formats
                juce::juce_core
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    sax_add_test_app(PitchDetectorTests
        Tests/PitchDetectorTests.cpp
        Source/DSP/PitchDetector.cpp)

    sax_add_test_app(PitchShifterTests
        Tests/PitchShifterTests/PitchShifterTests.cpp
        Source/DSP/PitchShifter.cpp
        Source/DSP/PitchDetector.cpp
        Source/Utils/TuningUtils.cpp)
endif()
