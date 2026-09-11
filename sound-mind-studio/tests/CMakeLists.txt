find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(sound-mind-studio-tests
    main.cpp
    test_audio_snippet_picker_dialog.cpp
    test_canvas_widget.cpp
    test_create_project_wizard.cpp
    test_image_scale_picker_dialog.cpp
    test_import_export.cpp
    test_import_helpers.cpp
    test_landing_page.cpp
    test_layers_panel.cpp
    test_loop_panel.cpp
    test_main_window.cpp
    test_paint_controller.cpp
    test_path_controller.cpp
    test_pick_controller.cpp
    test_playback_controller.cpp
    test_selection_controller.cpp
    test_playback_panel.cpp
    test_record_panel.cpp
    test_recent_projects.cpp
    test_theme.cpp
    test_tool_configuration_panel.cpp
)

target_link_libraries(sound-mind-studio-tests
    PRIVATE
        sound-mind-studio-lib
        Qt6::Test
)

# "-o -,txt" forces QTest's plain-text logger to write to stdout explicitly,
# rather than silently falling back to whatever its own terminal-detection
# picks. It does NOT reliably fix the underlying issue, though: on this
# Windows/Arm64 setup, QTest's text logger has been confirmed (`v0.0.26.1`,
# see docs/sound-mind-architecture.md's Decisions Made) to still produce no
# PASS/FAIL detail at all once stdout is redirected or piped - true
# regardless of shell (PowerShell, cmd.exe, Git Bash all show the same
# silence) - which leaves ctest --output-on-failure with nothing
# diagnosable for a failing run either. When that happens, the reliable
# fallback is QTest's own `-o file,xml` structured logger pointed at a
# real file (not stdout) - written deterministically regardless of
# console/tty state - run against a *temporarily* stripped-down main.cpp
# containing only the one suspect QObject test class (this binary drives
# several through one shared main(), so a shared -o target only ever keeps
# the *last* class's own results otherwise); restore main.cpp immediately
# after, and `touch` it - `mv`/`cp` alone can leave a restored file's own
# mtime *older* than an already-built binary, silently skipping the
# recompile back to the real, multi-class version.
add_test(NAME sound-mind-studio-tests COMMAND sound-mind-studio-tests -o -,txt)

# No real display in CI (or, per earlier this session, in this dev
# environment either) - the offscreen platform plugin ships with the base
# Qt install, so QWidget-derived classes can still be constructed and
# tested without one.
set_tests_properties(sound-mind-studio-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
