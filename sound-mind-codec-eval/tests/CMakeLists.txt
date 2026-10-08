find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-codec-eval-tests
    test_fidelity_metrics.cpp
    test_eval_report.cpp
)

target_link_libraries(sound-mind-codec-eval-tests
    PRIVATE
        sound-mind-codec-eval-lib
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-codec-eval-tests)
