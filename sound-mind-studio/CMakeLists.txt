# sound-mind-studio: the Qt GUI application (see sound-mind-architecture.md).
#
# Finds Qt via CMAKE_PREFIX_PATH, set from the QT_ROOT_DIR environment
# variable each developer/CI job sets to point at their own architecture's
# Qt install (there is no single portable path - Arm64 dev and x64 CI use
# different Qt builds). Mirrors how VCPKG_ROOT is handled.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
# Compiles assets/app.qrc (the embedded logo PNG - see theme.h) into a
# generated qrc_app.cpp automatically, the same way AUTOMOC handles moc -
# no manual qt_add_resources()/rcc invocation needed.
set(CMAKE_AUTORCC ON)

# The GUI classes live in a library, separate from the thin main.cpp entry
# point, so the test executable (see tests/) can link against them without
# recompiling or duplicating anything the real app also builds. The Qt
# resource file lives here too, not on the executable target, since it's
# LandingPage/theme.cpp (library code) that actually reads from it - see
# their own docs.
#
# The Q_OBJECT-bearing headers are listed here too, not just their .cpp
# files: AUTOMOC only auto-associates a header with a source file when they
# share a basename *and* live in the same directory, and our headers live
# under include/sound_mind/studio/ while the sources live under src/ - so
# without this, AUTOMOC silently never scans them and moc never runs.
add_library(sound-mind-studio-lib
    src/canvas_widget.cpp
    src/landing_page.cpp
    src/main_window.cpp
    src/recent_projects.cpp
    src/theme.cpp
    include/sound_mind/studio/canvas_widget.h
    include/sound_mind/studio/landing_page.h
    include/sound_mind/studio/main_window.h
    include/sound_mind/studio/recent_projects.h
    include/sound_mind/studio/theme.h
    assets/app.qrc
)

target_include_directories(sound-mind-studio-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sound-mind-studio-lib
    PUBLIC
        Qt6::Widgets
        sound-mind-core
)

target_compile_features(sound-mind-studio-lib PUBLIC cxx_std_23)
target_compile_definitions(sound-mind-studio-lib PUBLIC SOUND_MIND_VERSION="${PROJECT_VERSION}")

# resources/app.rc gives the built .exe its own icon (Explorer, taskbar,
# Alt+Tab) via the RC compiler - a build-time step entirely separate from
# assets/app.qrc above (Qt's own resource system, compiled into the lib and
# read at runtime) - see that file's docs for why the two aren't merged.
# RC isn't in the top-level project()'s LANGUAGES (this whole project is
# Windows-only anyway, per CLAUDE.md, so there's no cross-platform reason
# to enable it any earlier) - the Ninja generator needs it explicitly
# enabled before a target can list a .rc source, unlike the Visual Studio
# generator, which understands .rc files intrinsically via MSBuild.
enable_language(RC)

add_executable(sound-mind-studio WIN32
    src/main.cpp
    resources/app.rc
)

target_link_libraries(sound-mind-studio
    PRIVATE
        sound-mind-studio-lib
)

target_compile_features(sound-mind-studio PRIVATE cxx_std_23)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

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

# windeployqt needs to be told which Qt DLL variant to copy - it doesn't
# infer this from the target executable. Hardcoding --release here would
# silently deploy release Qt DLLs (Qt6Widgets.dll) alongside a Debug-
# configured build linked against debug ones (Qt6Widgetsd.dll), the same
# class of CRT/Qt debug-vs-release mismatch diagnosed for the test suite
# in v0.0.1.1's development. Harmless today only because packaging is
# always done from the "release" preset - but a real bug waiting to
# surface the first time someone packages a Debug build.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SOUND_MIND_WINDEPLOYQT_CONFIG_FLAG "--debug")
else ()
    set(SOUND_MIND_WINDEPLOYQT_CONFIG_FLAG "--release")
endif ()

if (SOUND_MIND_WINDEPLOYQT AND SOUND_MIND_QTPATHS)
    install(CODE "
        message(STATUS \"Deploying Qt runtime via windeployqt...\")
        execute_process(
            COMMAND \"${SOUND_MIND_WINDEPLOYQT}\" --qtpaths \"${SOUND_MIND_QTPATHS}\" ${SOUND_MIND_WINDEPLOYQT_CONFIG_FLAG} --no-translations \"\${CMAKE_INSTALL_PREFIX}/sound-mind-studio.exe\"
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
