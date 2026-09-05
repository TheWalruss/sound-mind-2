# sound-mind-studio: the Qt GUI application (see sound-mind-architecture.md).
# Currently just an empty window - a toolchain/design bring-up milestone
# (see CHANGELOG.md v0.0.0.1), proving Qt actually links and runs on this
# stack, not the real Studio UI yet.
#
# Finds Qt via CMAKE_PREFIX_PATH, set from the QT_ROOT_DIR environment
# variable each developer/CI job sets to point at their own architecture's
# Qt install (there is no single portable path - Arm64 dev and x64 CI use
# different Qt builds). Mirrors how VCPKG_ROOT is handled.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(sound-mind-studio WIN32
    src/main.cpp
)

target_link_libraries(sound-mind-studio
    PRIVATE
        Qt6::Widgets
)

target_compile_features(sound-mind-studio PRIVATE cxx_std_23)
target_compile_definitions(sound-mind-studio PRIVATE SOUND_MIND_VERSION="${PROJECT_VERSION}")
