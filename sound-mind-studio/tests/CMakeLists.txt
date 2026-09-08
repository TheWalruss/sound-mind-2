find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(sound-mind-studio-tests
    main.cpp
    test_canvas_widget.cpp
    test_landing_page.cpp
    test_main_window.cpp
    test_recent_projects.cpp
)

target_link_libraries(sound-mind-studio-tests
    PRIVATE
        sound-mind-studio-lib
        Qt6::Test
)

# "-o -,txt" forces QTest's plain-text logger to write to stdout explicitly.
# Without it, QTest's default logger has been observed (on this Windows/Arm64
# setup) to produce no PASS/FAIL output at all once stdout is redirected or
# piped - as `ctest --output-on-failure` always does - which would otherwise
# leave a failing run with no diagnosable detail, locally or in CI.
add_test(NAME sound-mind-studio-tests COMMAND sound-mind-studio-tests -o -,txt)

# No real display in CI (or, per earlier this session, in this dev
# environment either) - the offscreen platform plugin ships with the base
# Qt install, so QWidget-derived classes can still be constructed and
# tested without one.
set_tests_properties(sound-mind-studio-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
