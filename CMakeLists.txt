cmake_minimum_required(VERSION 3.25)

project(SoundMind
    VERSION 0.0.0.1
    DESCRIPTION "Sound Mind Studio: a spectrogram-as-image DAW and graphics studio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Real-time audio code must stay allocation-free and lock-free (see CLAUDE.md);
# nothing here relaxes that per module, it's enforced by review, not the build.

include(CTest)

add_subdirectory(sound-mind-codec)
add_subdirectory(sound-mind-core)
add_subdirectory(sound-mind-studio)

# Not yet scaffolded: sound-mind-gpu, sound-mind-vst (deferred). Added once
# each has real content, per sound-mind-architecture.md's Build & Module Layout.

# --- Code documentation (Doxygen) --------------------------------------
# See CLAUDE.md's "Code Documentation (Doxygen)" section: this is the
# unit-level design doc, meant to be cross-checked against
# docs/sound-mind-architecture.md and against the test suite.
find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E env "SOUND_MIND_VERSION=${PROJECT_VERSION}"
                ${DOXYGEN_EXECUTABLE} docs/doxygen/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Doxygen documentation into docs/generated"
        VERBATIM
    )
else ()
    message(STATUS "Doxygen not found - the 'docs' target will not be available")
endif ()
