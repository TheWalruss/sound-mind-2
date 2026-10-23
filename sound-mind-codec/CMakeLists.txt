# sound-mind-codec: the Stream transform and file I/O (see sound-mind-architecture.md).
# The Stream codec (stream_codec.*, stream_file.*, since v0.0.2.1) and
# compressed audio export (audio_export.*, since v0.0.6.1). The Pool codec
# (NSGT-based, v0.0.5.1) was removed in v0.1.46.1 - Decision #271.

# PocketFFT (header-only, BSD-3-Clause) is the FFT backend picked for the
# codec's transform - see docs/sound-mind-architecture.md's NSGT Library
# Candidates / Decisions Needed for the reasoning (a permissive-license FFT
# backend, sidestepping FFTW3's GPL entirely). No exported CMake config
# ships with the port, so it's found and included directly, per vcpkg's own
# documented usage for this port.
find_path(POCKETFFT_INCLUDE_DIR "pocketfft_hdronly.h" REQUIRED)

# JUCE, as of v0.0.6.1 - previously only sound-mind-core linked it (for
# PlaybackEngine's device I/O). codec now links it too, for its
# juce_audio_formats module specifically: real, working Flac/Ogg encoders
# come bundled with JUCE - see audio_export.h's docs for why MP3 needs
# ffmpeg instead (JUCE's own MP3 writer is an unimplemented stub, confirmed
# by reading its actual source). find_package(JUCE...) itself is called once,
# at the top-level CMakeLists.txt, not repeated here - see its comment there.

# ffmpeg, for MP3 audio export and MP4 video export (see audio_export.h,
# video_export.h) - the only practical option for MP4 muxing available via
# vcpkg; used for MP3 too since it's already a required dependency for
# video regardless. Built with the "mp3lame" feature enabled (see
# vcpkg.json) - ffmpeg has no MP3 encoder of its own; only libmp3lame
# (LGPL-2.0-only, same compatible license family as ffmpeg's own default
# LGPL build) can encode MP3, only decode it. Its default build is LGPL -
# per the decision made before adding it, linked dynamically here (SHARED,
# an explicit exception to every other dependency in this project being
# statically linked) to stay compliant with a closed-source distribution,
# the posture already committed to via the JUCE Starter tier.
find_package(FFMPEG REQUIRED)

add_library(sound-mind-codec
    src/version.cpp
    src/stream_codec.cpp
    src/audio_resample.cpp
    src/parallel.cpp
    src/stream_incremental_encoder.cpp
    src/stream_file.cpp
    src/wav_file.cpp
    src/color_mapping.cpp
    src/rgb_image_resample.cpp
    src/polar_projection.cpp
    src/ffmpeg_audio_encoder.cpp
    src/ffmpeg_audio_decoder.cpp
    src/audio_export.cpp
    src/audio_file.cpp
    src/video_export.cpp
    src/video_frame_renderer.cpp
)

target_include_directories(sound-mind-codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${POCKETFFT_INCLUDE_DIR}
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(sound-mind-codec
    PRIVATE
        ${FFMPEG_LIBRARY_DIRS}
)

target_link_libraries(sound-mind-codec
    PRIVATE
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        ${FFMPEG_LIBRARIES}
        ole32  # CoGetApartmentType(), video_export.cpp
)

target_compile_features(sound-mind-codec PUBLIC cxx_std_23)

# See sound-mind-core/CMakeLists.txt's identical block for why these are
# needed - JUCE modules assume a full JUCE "project" context normally
# supplied by juce_add_* targets/Projucer, which sound-mind-codec isn't.
target_compile_definitions(sound-mind-codec
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
