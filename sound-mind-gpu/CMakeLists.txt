# sound-mind-gpu: the DirectX 12 compute wrapper (see
# sound-mind-architecture.md's Build & Module Layout - named there since
# the very first architecture draft, with no content until now). No
# dependency on sound-mind-codec/core/studio, and (for this first
# installment) nothing depends on it either - see ComputeDevice's own
# docs for why this pass is pure plumbing, not a real DSP operation yet.
#
# d3d12.lib/dxgi.lib/dxguid.lib are Windows SDK components, not a vcpkg
# package - always available with the MSVC toolchain this project already
# requires, so linking them isn't a new third-party dependency. Likewise
# dxc.exe (the DirectX Shader Compiler, invoked below to compile .hlsl to
# .cso at build time - confirmed with the user: offline compilation, not
# runtime) ships with the Windows SDK itself.
find_program(DXC_EXECUTABLE NAMES dxc REQUIRED
    DOC "The DirectX Shader Compiler, from the Windows SDK - must be on PATH (a Visual Studio Developer environment puts it there)."
)

set(SOUND_MIND_GPU_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SOUND_MIND_GPU_COMPILED_SHADER ${SOUND_MIND_GPU_SHADER_DIR}/multiply_by_two.cso)

add_custom_command(
    OUTPUT ${SOUND_MIND_GPU_COMPILED_SHADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SOUND_MIND_GPU_SHADER_DIR}
    COMMAND ${DXC_EXECUTABLE} -T cs_6_0 -E CSMain -Fo ${SOUND_MIND_GPU_COMPILED_SHADER}
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/multiply_by_two.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/multiply_by_two.hlsl
    COMMENT "Compiling multiply_by_two.hlsl (dxc, offline)"
    VERBATIM
)
add_custom_target(sound-mind-gpu-shaders DEPENDS ${SOUND_MIND_GPU_COMPILED_SHADER})

add_library(sound-mind-gpu
    src/compute_device.cpp
)
add_dependencies(sound-mind-gpu sound-mind-gpu-shaders)

target_include_directories(sound-mind-gpu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(sound-mind-gpu
    PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
)

target_compile_features(sound-mind-gpu PUBLIC cxx_std_23)

# No JUCE, no Qt - this module's whole reason to exist (per the
# architecture doc) is isolating the DirectX 12 specifics from both, so
# it gets its own explicit warning level rather than inheriting either
# framework's own recommended flags bundle.
if (MSVC)
    target_compile_options(sound-mind-gpu PRIVATE /W4)
endif ()

# The compiled shader's own build-time location - multiplyByTwo() reads
# it from here at runtime. A loose file next to the build tree, not
# embedded into the binary - a deliberate, temporary simplification for
# this installment's own "prove the plumbing" scope (see ComputeDevice's
# own docs); a real shipped feature would need this revisited (embedded
# bytecode, or an installer-packaged shaders folder).
target_compile_definitions(sound-mind-gpu
    PRIVATE
        SOUND_MIND_GPU_SHADER_DIR="${SOUND_MIND_GPU_SHADER_DIR}"
)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
