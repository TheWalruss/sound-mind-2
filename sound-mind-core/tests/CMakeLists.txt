find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-core-tests
    test_operation.cpp
    test_layer.cpp
    test_project_settings.cpp
    test_project.cpp
    test_compositor.cpp
    test_playback_engine.cpp
    test_loop_engine.cpp
    test_record_engine.cpp
    test_pooling.cpp
    test_layer_export.cpp
)

target_link_libraries(sound-mind-core-tests
    PRIVATE
        sound-mind-core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(sound-mind-core-tests)
