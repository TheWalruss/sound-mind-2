# sound-mind-benchmark: a developer-only measurement tool (see
# docs/sound-mind-benchmarking.md) - runs a representative battery of Core
# operations across varied canvas sizes/kernel sizes/stroke counts/layer
# counts/MindWave generator types, and dumps the timings to a JSON file.
# Never installed, never linked into the shipped app.

# Best-effort - "unknown" (not a build failure) if git isn't on PATH or this
# isn't a git checkout at all (e.g. a source-only tarball), matching
# SOUND_MIND_VERSION's own "never block the build over something cosmetic"
# precedent below.
find_package(Git QUIET)
if (Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE SOUND_MIND_BENCHMARK_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif ()
if (NOT SOUND_MIND_BENCHMARK_GIT_COMMIT)
    set(SOUND_MIND_BENCHMARK_GIT_COMMIT "unknown")
endif ()

add_library(sound-mind-benchmark-lib
    src/benchmark_timer.cpp
    src/benchmark_report.cpp
    src/filter_scenarios.cpp
    src/paint_scenarios.cpp
    src/mind_wave_scenarios.cpp
    src/compositing_scenarios.cpp
    include/sound_mind/benchmark/benchmark_case.h
    include/sound_mind/benchmark/benchmark_timer.h
    include/sound_mind/benchmark/benchmark_report.h
    include/sound_mind/benchmark/scenarios.h
)

target_include_directories(sound-mind-benchmark-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sound-mind-benchmark-lib
    PUBLIC
        # nlohmann_json comes along transitively - sound-mind-core already
        # links it PUBLIC, and this directory never calls find_package()
        # for it itself (no need to - naming it here too would require
        # that, since an imported target's visibility is scoped to the
        # directory tree find_package() was actually called in).
        sound-mind-core
)

target_compile_features(sound-mind-benchmark-lib PUBLIC cxx_std_23)

# $<CONFIG> (not a configure-time CMAKE_BUILD_TYPE read) so this stays
# correct under a multi-config generator too, matching how
# sound-mind-studio/CMakeLists.txt's own SOUND_MIND_VERSION definition
# reasons about build-time-vs-configure-time values.
target_compile_definitions(sound-mind-benchmark-lib
    PUBLIC
        SOUND_MIND_BENCHMARK_GIT_COMMIT="${SOUND_MIND_BENCHMARK_GIT_COMMIT}"
        SOUND_MIND_BENCHMARK_BUILD_CONFIG="$<CONFIG>"
)

add_executable(sound-mind-benchmark src/main.cpp)
target_link_libraries(sound-mind-benchmark PRIVATE sound-mind-benchmark-lib)

# Testing already enabled by the top-level CMakeLists.txt's own include(CTest).
add_subdirectory(tests)
