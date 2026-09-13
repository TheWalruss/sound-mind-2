find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-gpu-tests
    test_compute_device.cpp
)

target_link_libraries(sound-mind-gpu-tests
    PRIVATE
        sound-mind-gpu
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-gpu-tests)
