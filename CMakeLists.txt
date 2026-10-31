cmake_minimum_required(VERSION 3.22)
project(HZDrummer VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

# Engine: reines C++ ohne JUCE-Abhängigkeit, damit testbar und später in Plugin/Mobile wiederverwendbar
add_library(hzengine STATIC
    src/engine/Meter.cpp
    src/engine/Sequencer.cpp
    src/engine/ClickSynth.cpp
    src/engine/SamplePlayer.cpp
    src/engine/Resampler.cpp
    src/engine/MeterCatalog.cpp
    src/engine/Pattern.cpp
    src/engine/TabParser.cpp
    src/engine/PatternPlayer.cpp
    src/engine/PatternEdit.cpp)
target_include_directories(hzengine PUBLIC src)

juce_add_gui_app(HZDrummer
    PRODUCT_NAME "HZ-Drummer"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(HZDrummer PRIVATE
    src/app/Main.cpp
    src/app/MainComponent.cpp
    src/app/MainComponentMenus.cpp
    src/app/MainComponentEditor.cpp
    src/app/AudioEngine.cpp
    src/app/KitManager.cpp
    src/app/KitLoader.cpp
    src/app/Library.cpp
    src/app/LibraryFiles.cpp
    src/app/FolderTree.cpp
    src/app/FolderPanel.cpp
    src/app/PlanFactory.cpp
    src/app/KeyBindings.cpp
    src/app/NavigationMenus.cpp
    src/app/MixState.cpp
    src/app/MeterLibrary.cpp
    src/app/HelpOverlay.cpp
    src/app/PatternIO.cpp
    src/app/PatternEditor.cpp
    src/app/PatternEditorLanes.cpp
    src/app/PatternEditorFile.cpp
    src/app/WorkshopWindow.cpp
    src/app/MixerPanel.cpp
    src/app/AudioSettingsPanel.cpp
    src/app/Song.cpp
    src/app/SongRename.cpp
    src/app/MainComponentRename.cpp
    src/app/ArrangerModel.cpp
    src/app/BadgeRegistry.cpp
    src/app/SearchIndex.cpp
    src/app/SearchQuery.cpp
    src/app/MainComponentArranger.cpp
    src/app/MainComponentCountIn.cpp
    src/app/arranger/FindView.cpp
    src/app/arranger/ScratchView.cpp
    src/app/arranger/SequenceView.cpp
    src/app/arranger/ArrangerPanel.cpp
    src/app/arranger/ArrangerPanelEdit.cpp
    src/app/arranger/ArrangerPanelSong.cpp
    src/app/QuickSearch.cpp
    src/app/views/StatusPanels.cpp
    src/app/views/SongStrip.cpp
    src/app/views/SongLayout.cpp
    src/app/views/SourceBadge.cpp
    src/app/views/HZLook.cpp
    src/app/views/InWindowDialog.cpp
    src/app/views/HelpLine.cpp
    src/app/views/ButtonSymbols.cpp
    src/app/views/Watermark.cpp
    src/app/views/GhostMenu.cpp
    src/app/BrowseOrder.cpp
    src/app/views/BeatDisplay.cpp)

target_compile_definitions(HZDrummer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=1
    JUCE_ALSA=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:HZDrummer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:HZDrummer,JUCE_VERSION>")

# version v0.2.<number of commits>, counted anew on every build (System window, app log)
add_custom_target(hzversion
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DOUTPUT=${CMAKE_BINARY_DIR}/generated/Version.h
            -P ${CMAKE_SOURCE_DIR}/cmake/Version.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/Version.h)
add_dependencies(HZDrummer hzversion)
target_include_directories(HZDrummer PRIVATE ${CMAKE_BINARY_DIR}/generated)

# the complete drum kit of the icon, drawn faintly behind the main window (views/Watermark)
juce_add_binary_data(hzassets SOURCES assets/drumkit.png)

target_link_libraries(HZDrummer
    PRIVATE
        hzengine
        hzassets
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Link-time optimisation makes every link take minutes (JUCE's plain -flto even links serially
# on GCC) and gains nothing audible here. Off by default; -DHZ_LTO=ON for a release build.
option(HZ_LTO "Link-time optimisation (slow link)" OFF)
if(HZ_LTO)
    target_link_libraries(HZDrummer PUBLIC juce::juce_recommended_lto_flags)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(HZDrummer PRIVATE -flto=auto)
    endif()
endif()

enable_testing()
add_executable(engine_tests tests/engine_tests.cpp tests/engine_tests_player.cpp)
target_link_libraries(engine_tests PRIVATE hzengine)
add_test(NAME engine_tests COMMAND engine_tests)

# Werkzeug: DrumGizmo-Kit -> vorgemischte Stereo-Samples
juce_add_console_app(kitprep PRODUCT_NAME "kitprep")
target_sources(kitprep PRIVATE tools/kitprep/Main.cpp)
target_compile_definitions(kitprep PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(kitprep
    PRIVATE
        hzengine
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Werkzeug: Drum-Tabs -> JSON-Pattern-Bibliothek
juce_add_console_app(patternc PRODUCT_NAME "patternc")
target_sources(patternc PRIVATE tools/patternc/Main.cpp src/app/PatternIO.cpp)
target_compile_definitions(patternc PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(patternc
    PRIVATE
        hzengine
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Tests der App-Schicht (Dateiformat), laufen im Projektordner
juce_add_console_app(app_tests PRODUCT_NAME "app_tests")
target_sources(app_tests PRIVATE tests/app_tests.cpp tests/song_tests.cpp tests/folder_tests.cpp src/app/FolderTree.cpp src/app/BrowseOrder.cpp src/app/PatternIO.cpp src/app/LibraryFiles.cpp src/app/QuickSearch.cpp src/app/SearchQuery.cpp src/app/views/SourceBadge.cpp src/app/Song.cpp src/app/SongRename.cpp src/app/ArrangerModel.cpp src/app/BadgeRegistry.cpp src/app/MixState.cpp)
target_compile_definitions(app_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(app_tests PRIVATE hzengine juce::juce_core juce::juce_audio_basics juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME app_tests COMMAND app_tests ${CMAKE_SOURCE_DIR}/library-src)
