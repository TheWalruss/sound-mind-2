cmake_minimum_required(VERSION 3.25)

project(SoundMind
    VERSION 0.0.0.2
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

# --- Packaging (CPack / WiX) --------------------------------------------
# See docs/sound-mind-roadmap.md's "Packaging & Installers": a basic
# installer, early, alongside the existing release zip - not replacing it.
if (WIN32)
    set(CPACK_PACKAGE_NAME "Sound Mind Studio")
    set(CPACK_PACKAGE_VENDOR "Sound Mind Project")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Sound Mind Studio")

    # CPack's version fields are 3-component (Major.Minor.Patch); our own
    # scheme is 4-component (X.Y.Z.W - see sound-mind-roadmap.md's
    # Versioning section). Mapped as Major=X, Minor=Y, Patch=(Z*1000 + W),
    # so every W (point release) bump still produces a strictly-increasing
    # MSI ProductVersion - needed for WiX's MajorUpgrade mechanism to
    # actually detect and replace an older install rather than silently
    # no-op. Z and W are assumed to each stay under 1000 for this to hold;
    # revisit long before that's a real risk.
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    math(EXPR CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH} * 1000 + ${PROJECT_VERSION_TWEAK}")

    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_VERSION 4)
    # Fixed forever, generated once - lets each new installer replace an
    # older install that used the same UpgradeCode. Do not regenerate this.
    set(CPACK_WIX_UPGRADE_GUID "11C9B46D-6ED2-497E-9816-A0F2408E9FFE")

    if (CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM64")
        set(CPACK_WIX_ARCHITECTURE "arm64")
        set(CPACK_SYSTEM_NAME "windows-arm64")
    else ()
        set(CPACK_WIX_ARCHITECTURE "x64")
        set(CPACK_SYSTEM_NAME "windows-x64")
    endif ()

    # CPack's default file-name template defaults CPACK_SYSTEM_NAME to a
    # generic "win64" for any 64-bit Windows target - x64 and Arm64 builds
    # would otherwise both produce an identically-named "...win64.msi" and
    # collide as release assets. Overriding the whole file name (using the
    # real 4-component PROJECT_VERSION, not the 3-component ProductVersion
    # mapping above) also matches the existing release zip's naming.
    set(CPACK_PACKAGE_FILE_NAME "sound-mind-studio-v${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")

    include(CPack)
endif ()
