# sound-mind-core: project model, operation log, compositor, and the
# MindWave/Instrument/Generator/Analysis engines (see sound-mind-architecture.md).
# Currently the Operation base class plus the first real Project/Layer/
# ProjectSettings/OperationLog data model (see CHANGELOG.md, v0.0.1.1) -
# not the full data model yet.

find_package(nlohmann_json CONFIG REQUIRED)

add_library(sound-mind-core
    src/operation.cpp
    src/layer.cpp
    src/project_settings.cpp
    src/operation_log.cpp
    src/project.cpp
)

target_include_directories(sound-mind-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sound-mind-core
    PUBLIC
        nlohmann_json::nlohmann_json
)

target_compile_features(sound-mind-core PUBLIC cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
