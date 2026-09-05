find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-codec-tests
    test_version.cpp
)

target_link_libraries(sound-mind-codec-tests
    PRIVATE
        sound-mind-codec
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-codec-tests)
