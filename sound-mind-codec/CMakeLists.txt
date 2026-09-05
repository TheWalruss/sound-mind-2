# sound-mind-codec: Pool/Stream transforms and file I/O (see sound-mind-architecture.md).
# Currently a toolchain-validation stub - no real codec logic yet.

add_library(sound-mind-codec
    src/version.cpp
)

target_include_directories(sound-mind-codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(sound-mind-codec PUBLIC cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
