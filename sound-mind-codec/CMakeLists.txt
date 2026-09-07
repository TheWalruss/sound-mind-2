# sound-mind-codec: Pool/Stream transforms and file I/O (see sound-mind-architecture.md).
# The Stream codec (stream_codec.*, stream_file.*) is real as of v0.0.2.1;
# the Pool codec (NSGT-based, lossless) doesn't exist yet.

# PocketFFT (header-only, BSD-3-Clause) is the FFT backend picked for the
# Stream codec's transform - see docs/sound-mind-architecture.md's NSGT
# Library Candidates / Decisions Needed for the reasoning (a permissive-
# license FFT backend, sidestepping FFTW3's GPL entirely). No exported
# CMake config ships with the port, so it's found and included directly,
# per vcpkg's own documented usage for this port.
find_path(POCKETFFT_INCLUDE_DIR "pocketfft_hdronly.h" REQUIRED)

add_library(sound-mind-codec
    src/version.cpp
    src/stream_codec.cpp
    src/stream_file.cpp
    src/wav_file.cpp
    src/color_mapping.cpp
)

target_include_directories(sound-mind-codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${POCKETFFT_INCLUDE_DIR}
)

target_compile_features(sound-mind-codec PUBLIC cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
