# sound-mind-gpu: the DirectX 12 compute wrapper (see
# sound-mind-architecture.md's Build & Module Layout - named there since
# the very first architecture draft, with no content until Installment A).
# No dependency on sound-mind-codec/core/studio, and (as of Installment B)
# nothing depends on it either yet - see ComputeDevice's own docs: its two
# real DSP kernels (gaussianBlur2D()/mixAmplitudePhaseSignal()) are proven
# correct and fast here, but not yet wired into sound-mind-core's own
# applyFilter()/compositeProject() (confirmed with the user - separate,
# later scope).
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

# Compiles shaders/<name>.hlsl to ${SOUND_MIND_GPU_SHADER_DIR}/<name>.cso
# via dxc, and appends that .cso path to SOUND_MIND_GPU_COMPILED_SHADERS
# (in the parent scope) - one add_custom_command() per shader, shared by
# every .hlsl file this module has, rather than repeating this block per
# shader.
set(SOUND_MIND_GPU_COMPILED_SHADERS "")
function(sound_mind_gpu_compile_shader name)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.hlsl)
    set(output ${SOUND_MIND_GPU_SHADER_DIR}/${name}.cso)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SOUND_MIND_GPU_SHADER_DIR}
        COMMAND ${DXC_EXECUTABLE} -T cs_6_0 -E CSMain -Fo ${output} ${source}
        DEPENDS ${source}
        COMMENT "Compiling ${name}.hlsl (dxc, offline)"
        VERBATIM
    )
    set(SOUND_MIND_GPU_COMPILED_SHADERS ${SOUND_MIND_GPU_COMPILED_SHADERS} ${output} PARENT_SCOPE)
endfunction()

sound_mind_gpu_compile_shader(multiply_by_two)
sound_mind_gpu_compile_shader(gaussian_blur)
sound_mind_gpu_compile_shader(mix_amplitude_phase_signal)
sound_mind_gpu_compile_shader(gaussian_blur_varying)
sound_mind_gpu_compile_shader(median_blur_varying)
sound_mind_gpu_compile_shader(directional_blur_varying)
sound_mind_gpu_compile_shader(fractal_field)
sound_mind_gpu_compile_shader(colored_noise_field)

add_custom_target(sound-mind-gpu-shaders DEPENDS ${SOUND_MIND_GPU_COMPILED_SHADERS})

# Every compiled shader becomes a byte array in a generated source file built
# into this library (v0.1.41.6), so an installed copy needs no shader files -
# the earlier loose-.cso-in-the-build-tree approach only ever worked on the
# machine that built it.
set(SOUND_MIND_GPU_EMBEDDED_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.cpp)
add_custom_command(
    OUTPUT ${SOUND_MIND_GPU_EMBEDDED_SHADERS}
    COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${SOUND_MIND_GPU_EMBEDDED_SHADERS}"
            "-DSHADERS=${SOUND_MIND_GPU_COMPILED_SHADERS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
    DEPENDS ${SOUND_MIND_GPU_COMPILED_SHADERS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
    COMMENT "Embedding compiled shaders"
    VERBATIM
)

add_library(sound-mind-gpu
    src/compute_device.cpp
    src/embedded_shaders.h
    ${SOUND_MIND_GPU_EMBEDDED_SHADERS}
)
target_include_directories(sound-mind-gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
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


if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
