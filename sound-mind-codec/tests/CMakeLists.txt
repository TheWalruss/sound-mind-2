find_package(Catch2 3 CONFIG REQUIRED)

add_executable(sound-mind-codec-tests
    test_version.cpp
    test_stream_codec.cpp
    test_stream_file.cpp
    test_wav_file.cpp
    test_color_mapping.cpp
    test_pool_codec.cpp
    test_pool_file.cpp
    test_audio_export.cpp
    test_video_export.cpp
)

target_include_directories(sound-mind-codec-tests
    PRIVATE
        # test_video_export.cpp reads exported files back via ffmpeg's own
        # demuxing API directly (verification-only - see its own docs).
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(sound-mind-codec-tests
    PRIVATE
        ${FFMPEG_LIBRARY_DIRS}
)

target_link_libraries(sound-mind-codec-tests
    PRIVATE
        sound-mind-codec
        Catch2::Catch2WithMain
        # test_audio_export.cpp reads exported files back via JUCE's own
        # AudioFormatManager/MP3AudioFormat directly (verification-only,
        # not part of sound-mind-codec's public API - see its own docs) -
        # codec links juce_audio_formats PRIVATE, so it doesn't propagate.
        juce::juce_audio_formats
        ${FFMPEG_LIBRARIES}
)


# test_audio_export.cpp includes juce_audio_formats/juce_audio_formats.h
# directly (to read exported files back for verification) - same JUCE
# "standalone project" definitions as sound-mind-codec's own CMakeLists.txt
# needs for the same header, and for the same reason.
target_compile_definitions(sound-mind-codec-tests
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

include(Catch)
catch_discover_tests(sound-mind-codec-tests)
