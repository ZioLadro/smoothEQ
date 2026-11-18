cmake_minimum_required(VERSION 3.22)

# These two must come before project(), or the plugins are built for the macOS of the build machine.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

project(SmoothEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SMOOTHEQ_BUILD_AAX "Also build AAX (Pro Tools; needs PACE signing to load in release Pro Tools)" OFF)
option(SMOOTHEQ_BUILD_TESTS "Build the headless test runner" OFF)
option(SMOOTHEQ_COPY_AFTER_BUILD "Install the plugins into the user plug-in folders after building" ON)
set(SMOOTHEQ_VST2_SDK "" CACHE PATH "Steinberg VST2 SDK folder: if set, a VST2 is built too (the SDK is no longer distributed)")
set(SMOOTHEQ_AAX_COPY_DIR "" CACHE PATH "Where the AAX is copied after building (empty = JUCE default, the system Avid folder, needs write access)")

# ---- JUCE: -DJUCE_DIR=<path>, or ./JUCE, or downloaded automatically (needs git)
set(SMOOTHEQ_JUCE_TAG "8.0.10" CACHE STRING "JUCE version fetched when no local copy is given")
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/JUCE)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${SMOOTHEQ_JUCE_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)
    set(JUCE_DIR ${juce_SOURCE_DIR})
endif()

set(SMOOTHEQ_FORMATS VST3)
if(APPLE)
    list(APPEND SMOOTHEQ_FORMATS AU)
endif()
if(SMOOTHEQ_VST2_SDK)
    juce_set_vst2_sdk_path(${SMOOTHEQ_VST2_SDK})
    list(APPEND SMOOTHEQ_FORMATS VST)
endif()
if(SMOOTHEQ_BUILD_AAX)
    if(EXISTS ${JUCE_DIR}/modules/juce_audio_plugin_client/AAX/SDK)
        juce_set_aax_sdk_path(${JUCE_DIR}/modules/juce_audio_plugin_client/AAX/SDK)
    endif()
    list(APPEND SMOOTHEQ_FORMATS AAX)
endif()

set(SMOOTHEQ_EXTRA_PLUGIN_ARGS)
if(SMOOTHEQ_AAX_COPY_DIR)
    list(APPEND SMOOTHEQ_EXTRA_PLUGIN_ARGS AAX_COPY_DIR "${SMOOTHEQ_AAX_COPY_DIR}")
endif()

juce_add_plugin(SmoothEQ
    ${SMOOTHEQ_EXTRA_PLUGIN_ARGS}
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Gianluca Sofia"
    BUNDLE_ID "com.gianlucasofia.smootheq"
    PLUGIN_MANUFACTURER_CODE Gsof
    PLUGIN_CODE Smeq
    PRODUCT_NAME "SMOOTHEQ"
    FORMATS ${SMOOTHEQ_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SMOOTHEQ_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_EQ)

# Embedded UI fonts (Rubik, Silkscreen; SIL Open Font License, see Resources/fonts)
juce_add_binary_data(SmoothEQFonts HEADER_NAME SmoothFonts.h NAMESPACE SmoothFonts SOURCES
    Resources/fonts/Rubik-Medium.ttf
    Resources/fonts/Rubik-Bold.ttf
    Resources/fonts/Rubik-BlackItalic.ttf
    Resources/fonts/Silkscreen-Regular.ttf)
set_target_properties(SmoothEQFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(SmoothEQ PRIVATE Source/SmoothProcessor.cpp)
target_include_directories(SmoothEQ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions(SmoothEQ PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    SMOOTHEQ_VERSION="${PROJECT_VERSION}")
target_link_libraries(SmoothEQ
    PRIVATE SmoothEQFonts juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(SMOOTHEQ_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
