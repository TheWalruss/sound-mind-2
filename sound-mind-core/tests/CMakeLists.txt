find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-core-tests
    test_operation.cpp
    test_layer.cpp
)

target_link_libraries(sound-mind-core-tests
    PRIVATE
        sound-mind-core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-core-tests)
