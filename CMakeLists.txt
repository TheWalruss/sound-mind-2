cmake_minimum_required(VERSION 3.25)

project(SoundMind
    VERSION 0.1.0
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

# Not yet scaffolded: sound-mind-core, sound-mind-gpu, sound-mind-studio,
# sound-mind-vst (deferred). Added as each one has real content, per
# sound-mind-architecture.md's Build & Module Layout.
