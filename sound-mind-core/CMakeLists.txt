# sound-mind-core: project model, operation log, compositor, and the
# MindWave/Instrument/Generator/Analysis engines (see sound-mind-architecture.md).
# Currently just the Operation and Layer base classes - a design/toolchain
# bring-up step, not the full data model yet.

add_library(sound-mind-core
    src/operation.cpp
)

target_include_directories(sound-mind-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(sound-mind-core PUBLIC cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
