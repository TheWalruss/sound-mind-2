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
# Studio's windowing and widgets. find_package(JUCE...) itself is called
# once, at the top-level CMakeLists.txt, not repeated here - see its comment
# there (a real bug in vcpkg's JUCEConfig.cmake breaks a second call to it
# within the same configure).

add_library(sound-mind-core
    src/operation.cpp
    src/gradient.cpp
    src/path.cpp
    src/selection_region.cpp
    src/wand_selection.cpp
    src/tool_configuration.cpp
    src/filter_configuration.cpp
    src/tone_curve.cpp
    src/mind_wave.cpp
    src/filter_application.cpp
    src/paint_application.cpp
    src/fill_application.cpp
    src/paste_operation.cpp
    src/paste_application.cpp
    src/warp_application.cpp
    src/sequence_operation.cpp
    src/sequence_application.cpp
    src/mind_shot.cpp
    src/mind_grain.cpp
    src/convolution_kernel.cpp
    src/blend_mode_application.cpp
    src/music_theory.cpp
    src/chord_generator.cpp
    src/sequence_notation.cpp
    src/layer.cpp
    src/project_settings.cpp
    src/operation_log.cpp
    src/project.cpp
    src/compositor.cpp
    src/audio_device_list.cpp
    src/device_test_tone_player.cpp
    src/playback_engine.cpp
    src/loop_engine.cpp
    src/record_engine.cpp
    src/pooling.cpp
    src/layer_export.cpp
    src/gpu_compute_access.cpp
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
        # sound-mind-gpu, PRIVATE: filter_application.cpp's UniformBlur
        # case and compositor.cpp's layer mixing dispatch to it internally
        # (via src/gpu_compute_access.h - see its own docs), but no public
        # sound-mind-core header exposes any sound_mind::gpu type, so
        # nothing downstream (sound-mind-studio) needs to know or link
        # against it - matches sound-mind-gpu's own stated goal of
        # isolating the DirectX 12 specifics from everything above it.
        sound-mind-gpu
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
