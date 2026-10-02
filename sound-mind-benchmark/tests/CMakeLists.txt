find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-benchmark-tests
    test_benchmark_timer.cpp
    test_benchmark_report.cpp
)

target_link_libraries(sound-mind-benchmark-tests
    PRIVATE
        sound-mind-benchmark-lib
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-benchmark-tests)
