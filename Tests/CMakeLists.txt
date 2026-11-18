juce_add_console_app(SmoothEQTests PRODUCT_NAME "SmoothEQTests")
target_sources(SmoothEQTests PRIVATE TestMain.cpp ${PROJECT_SOURCE_DIR}/Source/SmoothProcessor.cpp)
target_include_directories(SmoothEQTests PRIVATE ${PROJECT_SOURCE_DIR}/Source)
target_compile_definitions(SmoothEQTests PRIVATE
    SMOOTHEQ_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    SMOOTHEQ_VERSION="${PROJECT_VERSION}"
    JucePlugin_Name="SmoothEQTests" JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(SmoothEQTests PRIVATE
    SmoothEQFonts
    juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics juce::juce_audio_basics
    juce::juce_recommended_config_flags)
