# sound-mind-core: project model, operation log, compositor, and the
# MindWave/Instrument/Generator/Analysis engines (see sound-mind-architecture.md).
# Real Project/Layer/ProjectSettings/OperationLog data model, a first
# Compositor, and (as of v0.0.4.1) a real-time PlaybackEngine backed by
# JUCE's audio device I/O - not the full data model yet.

find_package(nlohmann_json CONFIG REQUIRED)

# JUCE tier: the free Starter license, as of v0.0.4.1 - see
# docs/sound-mind-architecture.md's Decisions Made for the reasoning and
# docs/tech-stack-decisions.md for the dependency record. Only the audio
# engine modules are linked, per that same doc's Build & Module Layout -
# never juce_gui_basics/juce_gui_extra, since Qt (not JUCE) owns the
# Studio's windowing and widgets.
find_package(JUCE CONFIG REQUIRED)

add_library(sound-mind-core
    src/operation.cpp
    src/layer.cpp
    src/project_settings.cpp
    src/operation_log.cpp
    src/project.cpp
    src/compositor.cpp
    src/playback_engine.cpp
)

target_include_directories(sound-mind-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sound-mind-core
    PUBLIC
        nlohmann_json::nlohmann_json
        sound-mind-codec
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_events
        juce::juce_audio_devices
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_features(sound-mind-core PUBLIC cxx_std_23)

# JUCE modules assume they're compiled as part of a JUCE "project" that
# defines these preprocessor flags (normally supplied by juce_add_*
# targets/Projucer) - set the minimal set sound-mind-core actually needs
# directly, since it isn't a full JUCE application target.
target_compile_definitions(sound-mind-core
    PUBLIC
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
