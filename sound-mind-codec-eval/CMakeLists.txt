# sound-mind-codec-eval: a developer-only tool (see
# docs/sound-mind-codec-design.md) that encodes/decodes real audio files
# through both the Stream and Pool codecs across a sweep of hop lengths and
# bin counts, and reports fidelity/timing/memory metrics to a CSV report.
# Never installed, never linked into the shipped app - sibling to
# sound-mind-benchmark, but depends only on sound-mind-codec (not
# sound-mind-core), since it measures the codec's own fidelity, not Core
# operation speed.

add_library(sound-mind-codec-eval-lib
    src/fidelity_metrics.cpp
    src/eval_report.cpp
    src/process_memory.cpp
    include/sound_mind/codec_eval/fidelity_metrics.h
    include/sound_mind/codec_eval/eval_report.h
    include/sound_mind/codec_eval/process_memory.h
)

target_include_directories(sound-mind-codec-eval-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

find_path(POCKETFFT_INCLUDE_DIR "pocketfft_hdronly.h" REQUIRED)

target_include_directories(sound-mind-codec-eval-lib
    PRIVATE
        ${POCKETFFT_INCLUDE_DIR}
)

target_link_libraries(sound-mind-codec-eval-lib
    PUBLIC
        sound-mind-codec
    PRIVATE
        # GetProcessMemoryInfo() (process_memory.cpp) - Windows SDK, not a
        # new third-party dependency.
        psapi
)

target_compile_features(sound-mind-codec-eval-lib PUBLIC cxx_std_23)

add_executable(sound-mind-codec-eval src/main.cpp)
target_link_libraries(sound-mind-codec-eval PRIVATE sound-mind-codec-eval-lib)

# Testing already enabled by the top-level CMakeLists.txt's own include(CTest).
add_subdirectory(tests)
