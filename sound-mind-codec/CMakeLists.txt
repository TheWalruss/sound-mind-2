# sound-mind-codec: Pool/Stream transforms and file I/O (see sound-mind-architecture.md).
# Both the Stream codec (stream_codec.*, stream_file.*, since v0.0.2.1) and
# the Pool codec (pool_codec.*, pool_file.*, since v0.0.5.1 - NSGT-based,
# near-lossless) are real.

# PocketFFT (header-only, BSD-3-Clause) is the FFT backend picked for both
# codecs' transforms - see docs/sound-mind-architecture.md's NSGT Library
# Candidates / Decisions Needed for the reasoning (a permissive-license FFT
# backend, sidestepping FFTW3's GPL entirely). No exported CMake config
# ships with the port, so it's found and included directly, per vcpkg's own
# documented usage for this port.
find_path(POCKETFFT_INCLUDE_DIR "pocketfft_hdronly.h" REQUIRED)

# libtiff for the Pool file format - a standard TIFF 6.0 container (see
# pool_file.h). Chosen over lighter alternatives (e.g. tinytiff, which is
# LGPL-3.0 - a real copyleft consideration for static linking, unlike
# libtiff's permissive license) for its complete, native LZW/multi-page/
# tiled-layout support.
find_package(TIFF REQUIRED)

add_library(sound-mind-codec
    src/version.cpp
    src/stream_codec.cpp
    src/stream_file.cpp
    src/wav_file.cpp
    src/color_mapping.cpp
    src/pool_codec.cpp
    src/pool_file.cpp
)

target_include_directories(sound-mind-codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${POCKETFFT_INCLUDE_DIR}
)

target_link_libraries(sound-mind-codec
    PRIVATE
        TIFF::TIFF
)

target_compile_features(sound-mind-codec PUBLIC cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
