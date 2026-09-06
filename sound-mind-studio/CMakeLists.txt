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

# --- Packaging (see docs/sound-mind-roadmap.md's "Packaging & Installers") --
#
# install(TARGETS ...) plus an install(CODE ...) hook that runs windeployqt
# at install time, so `cmake --install` (and, through it, both `cpack -G ZIP`
# and `cpack -G WIX`) always produces a fully self-contained folder without
# each packaging path needing its own copy of the deployment logic.
#
# windeployqt is a host-side tool - for a cross-compiled Arm64 build it has
# to be the x64 host's copy, pointed at the Arm64 Qt install via --qtpaths
# (QT_HOST_PATH_DIR set, same as the CMakePresets.json Qt wiring); for a
# native (non-cross) build QT_ROOT_DIR's own copy is used directly.

install(TARGETS sound-mind-studio RUNTIME DESTINATION . COMPONENT Runtime)

if (DEFINED ENV{QT_HOST_PATH_DIR} AND NOT "$ENV{QT_HOST_PATH_DIR}" STREQUAL "")
    find_program(SOUND_MIND_WINDEPLOYQT windeployqt HINTS "$ENV{QT_HOST_PATH_DIR}/bin" NO_DEFAULT_PATH)
else ()
    find_program(SOUND_MIND_WINDEPLOYQT windeployqt HINTS "$ENV{QT_ROOT_DIR}/bin" NO_DEFAULT_PATH)
endif ()

find_program(SOUND_MIND_QTPATHS
    NAMES qtpaths.exe qtpaths.bat qtpaths
    HINTS "$ENV{QT_ROOT_DIR}/bin"
    NO_DEFAULT_PATH
)

if (SOUND_MIND_WINDEPLOYQT AND SOUND_MIND_QTPATHS)
    install(CODE "
        message(STATUS \"Deploying Qt runtime via windeployqt...\")
        execute_process(
            COMMAND \"${SOUND_MIND_WINDEPLOYQT}\" --qtpaths \"${SOUND_MIND_QTPATHS}\" --release --no-translations \"\${CMAKE_INSTALL_PREFIX}/sound-mind-studio.exe\"
            RESULT_VARIABLE _sound_mind_windeployqt_result
        )
        if (NOT _sound_mind_windeployqt_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed (exit code \${_sound_mind_windeployqt_result})\")
        endif ()
    " COMPONENT Runtime)
else ()
    message(WARNING "windeployqt/qtpaths not found for sound-mind-studio - "
                     "installed/packaged output will be missing its Qt runtime. "
                     "Make sure QT_ROOT_DIR (and QT_HOST_PATH_DIR for a cross-compiled Arm64 build) "
                     "are set before configuring.")
endif ()
