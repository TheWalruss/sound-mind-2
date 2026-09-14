#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <wrl/client.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;

namespace sound_mind::gpu {

/**
 * @brief Which kind of DirectX 12 adapter a `ComputeDevice` ended up on.
 *
 * @see ComputeDevice::create()'s own docs for the fallback order this
 *      reflects.
 */
enum class AdapterKind {
    /// @brief A real, physical GPU adapter.
    Hardware,
    /// @brief Microsoft's software (CPU-emulated) D3D12 adapter - always
    ///        present on Windows, no real GPU required. Much slower than
    ///        hardware; exists here so the real D3D12 code path (device,
    ///        pipeline, dispatch, readback) can be exercised and tested
    ///        even on a machine with no GPU at all - not a performance
    ///        claim.
    Warp,
};

/**
 * @brief A dB-magnitude/shared-phase signal - the same three-array shape
 *        `sound_mind::codec::StreamImage` stores its own amplitude/phase
 *        data in, without depending on Codec directly (`sound-mind-gpu`
 *        stays below/independent of both Codec and Core - see
 *        `docs/sound-mind-architecture.md`'s Build & Module Layout).
 *
 * @see ComputeDevice::mixAmplitudePhaseSignal()'s own docs.
 */
struct AmplitudePhaseSignal {
    /// @brief The left channel's amplitude, in dB, one entry per cell.
    std::vector<float> leftMagnitudeDb;
    /// @brief The right channel's amplitude, in dB, one entry per cell -
    ///        same size as `leftMagnitudeDb`.
    std::vector<float> rightMagnitudeDb;
    /// @brief The shared left/right phase, in radians, one entry per
    ///        cell - same size as `leftMagnitudeDb`.
    std::vector<float> phaseRadians;
};

/**
 * @brief A live DirectX 12 compute device - `sound-mind-gpu`'s own first
 *        real code (`docs/sound-mind-architecture.md`'s Build & Module
 *        Layout has named this module since the very first architecture
 *        draft, with nothing behind it until Installment A).
 *
 * **Installment A** (`v0.0.29.1`) was deliberately pure plumbing - one
 * trivial, DSP-meaningless operation (multiplyByTwo()) proving the whole
 * device -> dispatch -> readback round trip works end to end before
 * risking a real kernel on unproven pipeline code. **Installment B**
 * (`v0.0.29.2`) adds the first two real DSP kernels - gaussianBlur2D()
 * (a real Filter Layer operation, `sound_mind::core::applyFilter()`'s own
 * `UniformBlur` case) and mixAmplitudePhaseSignal() (the compositor's own
 * per-cell layer-mixing math, `sound_mind::core::compositeProject()`'s
 * own `mixLayerInto()`) - each proven both *correct* (matching its own
 * CPU reference within float tolerance) and *faster* than that CPU
 * reference, per `docs/sound-mind-architecture.md`'s own Decision #4
 * testing strategy for real-time/GPU code.
 *
 * **Neither new kernel is wired into `sound-mind-core` yet** - confirmed
 * with the user ahead of implementation (the recommended option): this
 * installment proves each kernel correct and fast in isolation; actually
 * having `applyFilter()`/`compositeProject()` call into `sound-mind-gpu`
 * (with a CPU fallback when no device is available) is separate, later
 * scope, matching the "Core primitive first, wire it into the real
 * consumer after" pattern this whole project has used for every other
 * multi-step feature.
 *
 * Every D3D12 resource this class touches is rebuilt fresh per call
 * (root signature, PSO, buffers) rather than cached/reused across calls -
 * correct but not remotely efficient, an explicit, documented
 * simplification appropriate for "does the pipeline work, and is it
 * faster" - not a pattern a real, repeatedly-dispatched, wired-in kernel
 * should copy.
 *
 * @note Not thread-safe, and not real-time-safe: every operation
 *       allocates, and every method blocks its own calling thread on a
 *       GPU fence. Matches every other DSP path in this codebase before
 *       its own real-time-safe pass (`compositeProject()`,
 *       `sound_mind::codec::encode()`/`decode()`) - this is Core/Codec-
 *       adjacent worker-thread-shaped code, never meant to run on the
 *       audio callback thread as-is.
 */
class ComputeDevice {
public:
    /**
     * @brief Creates a `ComputeDevice`, trying a real hardware adapter
     *        first, then Microsoft's WARP software adapter, before
     *        giving up - confirmed with the user ahead of implementation.
     *
     * Never throws - a failed hardware adapter attempt (no DX12-capable
     * GPU, a disabled/blocked driver, a sandboxed environment with no
     * graphics support) falls through to the next tier silently; only a
     * total failure (WARP itself unavailable, which shouldn't happen on
     * any real Windows 10/11 install but is a real possibility in a
     * sufficiently locked-down container) returns `std::nullopt`. Callers
     * needing GPU acceleration should treat `std::nullopt` exactly like
     * "no GPU available" and fall back to the existing CPU path - this
     * class never being available at all is an expected, handled outcome,
     * not an error condition to propagate.
     *
     * @return A working `ComputeDevice`, or `std::nullopt` if no D3D12
     *         adapter at all (hardware or WARP) could be initialized.
     */
    [[nodiscard]] static std::optional<ComputeDevice> create();

    /// @brief Move-constructs from `other`, leaving `other` in a valid,
    ///        destructible-but-otherwise-unusable state (its own fence
    ///        event handle transfers to `this`, not duplicated).
    /// @param other The device to move from.
    ComputeDevice(ComputeDevice&& other) noexcept;

    /// @brief Move-assigns from `other`, releasing this object's own
    ///        prior fence event handle first - see the move constructor's
    ///        own docs for what state `other` is left in.
    /// @param other The device to move from.
    /// @return `*this`.
    ComputeDevice& operator=(ComputeDevice&& other) noexcept;

    /// @brief Not copyable - a `ComputeDevice` owns a live D3D12 device/
    ///        queue/fence and a Win32 event handle; copying one would
    ///        mean either two objects racing to close the same handle or
    ///        silently creating a second, independent device underneath
    ///        what looks like a copy.
    ComputeDevice(const ComputeDevice&) = delete;

    /// @brief Not copy-assignable - see the copy constructor's own docs.
    ComputeDevice& operator=(const ComputeDevice&) = delete;

    /// @brief Releases this device's own fence event handle, if any (a
    ///        moved-from instance holds none). The D3D12 COM objects
    ///        (`device_`/`commandQueue_`/`fence_`) release themselves via
    ///        `Microsoft::WRL::ComPtr`'s own destructor - nothing extra
    ///        to do for those here.
    ~ComputeDevice();

    /// @brief Which kind of adapter this device ended up on.
    /// @return `Hardware` or `Warp` - see `AdapterKind`'s own docs.
    [[nodiscard]] AdapterKind adapterKind() const noexcept { return adapterKind_; }

    /**
     * @brief Doubles every element of `input`, entirely on the GPU -
     *        Installment A's own proof-of-pipeline operation.
     *
     * Deliberately DSP-meaningless (see this class's own docs on why): a
     * trivial, hand-verifiable operation isolates whether the D3D12
     * plumbing itself (root signature embedded in the compiled shader,
     * PSO, root-descriptor buffer bindings, dispatch sizing against
     * `input.size()`, the fence wait, the readback copy) is correct,
     * without also needing to get a real DSP kernel's own math right at
     * the same time.
     *
     * @param input The values to double. An empty vector is a valid,
     *        immediate no-op (no GPU dispatch happens at all).
     * @return `input`, each element multiplied by `2.0f`, same size and
     *         order.
     * @throws std::runtime_error if any D3D12 call fails - shouldn't
     *         happen against a device `create()` already proved works,
     *         but a device can still be lost (driver reset, removal)
     *         between calls.
     */
    [[nodiscard]] std::vector<float> multiplyByTwo(const std::vector<float>& input) const;

    /**
     * @brief A separable 2D Gaussian blur over `data` (`width` x `height`,
     *        row-major), entirely on the GPU - the same algorithm
     *        `sound_mind::core`'s own (CPU) `gaussianBlur2D()` (in
     *        `filter_application.cpp`) implements for `FilterType::
     *        UniformBlur`: `sigma` floored at `0.1`, kernel truncated at 4
     *        standard deviations, clamp-to-edge boundary handling on both
     *        passes.
     *
     * Two dispatches (horizontal then vertical), ping-ponging between two
     * GPU buffers, matching the CPU version's own separable-pass shape -
     * a real, representative GPU DSP pattern (as opposed to a single
     * O(radius^2)-per-pixel 2D convolution, which would also be correct
     * but wouldn't reflect how a real, efficient GPU blur is actually
     * written).
     *
     * @param data The `width * height` values to blur, row-major (row =
     *        `y`, column = `x`) - `data[y * width + x]`.
     * @param width The row length. `width * height` must equal
     *        `data.size()`.
     * @param height The number of rows.
     * @param sigma The Gaussian kernel's own standard deviation, in
     *        cells - see `sound_mind::core::FilterConfiguration::
     *        blurSigma()`'s own docs for the same unit convention.
     * @return The blurred result, same size/shape as `data`.
     * @throws std::runtime_error if `width * height != data.size()`, or
     *         if any D3D12 call fails.
     */
    [[nodiscard]] std::vector<float> gaussianBlur2D(const std::vector<float>& data, std::uint32_t width,
                                                     std::uint32_t height, float sigma) const;

    /**
     * @brief A genuine, non-separable 2D Gaussian blur with `sigma`
     *        evaluated fresh per output cell, entirely on the GPU -
     *        `v0.Y.31.1` (MindWaves v1) Installment D2's own GPU-aware
     *        follow-up to Installment D1's CPU-only `sound_mind::core`
     *        `gaussianBlur2DVarying()` (`filter_application.cpp`), which
     *        this matches exactly: `sigma` floored at `0.1`, kernel radius
     *        `max(1, ceil(4*sigma))`, clamp-to-edge boundary handling -
     *        computed directly per output cell (`O(radius^2)` work each),
     *        since separability (`gaussianBlur2D()`'s own two-pass shape)
     *        only holds for one shared sigma across the whole image.
     *
     * A single dispatch over the whole `width x height` grid - each GPU
     * thread computes its own cell's own kernel independently, needing no
     * cross-thread coordination despite each one potentially using a
     * different radius (confirmed with the user: a real dedicated GPU
     * path, conditioned on a sufficiently large single dispatch actually
     * being achievable - one dispatch per `applyFilter()` call, covering
     * every cell in the composite, comfortably clears that bar for any
     * project-sized canvas).
     *
     * @param data The `width * height` values to blur, row-major.
     * @param width The row length. `width * height` must equal `data.size()`.
     * @param height The number of rows.
     * @param sigmaPerCell Each cell's own sigma (already resolved from
     *        whatever MindWave binding produced it, or a uniform ceiling
     *        if unbound - `sound-mind-gpu` has no dependency on
     *        `sound-mind-core`/`MindWave` itself, so this arrives as a
     *        plain array), same size as `data`.
     * @return The blurred result, same size/shape as `data`.
     * @throws std::runtime_error if `width * height` doesn't equal both
     *         `data.size()` and `sigmaPerCell.size()`, or if any D3D12
     *         call fails.
     */
    [[nodiscard]] std::vector<float> gaussianBlur2DVarying(const std::vector<float>& data, std::uint32_t width,
                                                            std::uint32_t height,
                                                            const std::vector<float>& sigmaPerCell) const;

    /**
     * @brief A 2D median filter with the window size evaluated fresh per
     *        output cell, entirely on the GPU - Installment D2's own
     *        GPU-aware follow-up to `sound_mind::core`'s own (CPU)
     *        `medianBlur2DVarying()`. A per-cell size of `1` or less
     *        skips windowing entirely for that cell (the identity a
     *        `1`-cell "window" already is), matching the CPU version's
     *        own baseline handling exactly.
     *
     * The per-cell window size is clamped to at most `31` (`961` samples)
     * here **and** in the CPU version - a deliberate, matching safety
     * bound added alongside this GPU kernel (see `sound_mind::core::
     * FilterConfiguration::medianSize()`'s own Studio UI range, which
     * never exceeds this anyway): the GPU shader gathers a window's own
     * samples into a fixed-size local array before sorting (no
     * `std::nth_element` equivalent exists in HLSL - a plain insertion
     * sort of up to `961` elements instead), so an unbounded window size
     * would risk a real out-of-bounds write, not just a slow one.
     *
     * @param data The `width * height` values to filter, row-major.
     * @param width The row length. `width * height` must equal `data.size()`.
     * @param height The number of rows.
     * @param sizePerCell Each cell's own window size (already resolved,
     *        same convention as `gaussianBlur2DVarying()`'s own
     *        `sigmaPerCell`), same size as `data`.
     * @return The filtered result, same size/shape as `data`.
     * @throws std::runtime_error if `width * height` doesn't equal both
     *         `data.size()` and `sizePerCell.size()`, or if any D3D12
     *         call fails.
     */
    [[nodiscard]] std::vector<float> medianBlur2DVarying(const std::vector<float>& data, std::uint32_t width,
                                                          std::uint32_t height,
                                                          const std::vector<float>& sizePerCell) const;

    /**
     * @brief A directional (motion-blur-style) kernel with length and
     *        angle both evaluated fresh per output cell, entirely on the
     *        GPU - Installment D2's own GPU-aware follow-up to
     *        `sound_mind::core`'s own (CPU) `directionalBlur2DVarying()`.
     *        A per-cell length of `0` or less skips convolution entirely
     *        for that cell (the identity a zero-length kernel already
     *        is), matching the CPU version's own baseline handling
     *        exactly.
     *
     * Accumulates each of a cell's own `2n+1` unit steps directly into a
     * running weighted sum (`weight = 1/(2n+1)` each), rather than first
     * grouping steps that land on the same integer offset the way the CPU
     * version's own `directionalBlurOffsets()` does (no associative
     * container exists in HLSL) - mathematically equivalent by
     * associativity of summation (grouping-then-normalizing and summing-
     * individually-weighted-contributions compute the identical total),
     * so this still agrees with the CPU version within ordinary floating-
     * point tolerance, just via a different accumulation order.
     *
     * @param data The `width * height` values to filter, row-major.
     * @param width The row length. `width * height` must equal `data.size()`.
     * @param height The number of rows.
     * @param lengthPerCell Each cell's own kernel length, in bins/columns
     *        (already resolved, same convention as `gaussianBlur2DVarying()`'s
     *        own `sigmaPerCell`), same size as `data`.
     * @param angleDegreesPerCell Each cell's own kernel angle, in degrees
     *        (already resolved the same way), same size as `data`.
     * @return The filtered result, same size/shape as `data`.
     * @throws std::runtime_error if `width * height` doesn't equal
     *         `data.size()`, `lengthPerCell.size()`, and
     *         `angleDegreesPerCell.size()` all alike, or if any D3D12
     *         call fails.
     */
    [[nodiscard]] std::vector<float> directionalBlur2DVarying(const std::vector<float>& data, std::uint32_t width,
                                                               std::uint32_t height,
                                                               const std::vector<float>& lengthPerCell,
                                                               const std::vector<float>& angleDegreesPerCell) const;

    /**
     * @brief Mixes one layer's own (already-placed) contribution into a
     *        running composite, entirely on the GPU - the same per-cell
     *        math `sound_mind::core`'s own (CPU) `mixLayerInto()` (in
     *        `compositor.cpp`) implements: each channel's own dB value
     *        converts to linear, `layer`'s own converted amplitude scales
     *        by `layerGain` (a layer's own `opacity()`, as a linear
     *        gain), each channel becomes a complex value using its own
     *        signal's shared phase, `running`'s own and `layer`'s own
     *        complex values sum, and the result converts back to
     *        dB/phase (phase as the angle of the summed *mid* signal,
     *        `(left + right) / 2`).
     *
     * **Deliberately excludes `mixLayerInto()`'s own placement/rescale
     *        step** (`sourceColumnFor()`'s own timeline-remapping math) -
     *        `layer` here must already be sized/aligned identically to
     *        `running` (the caller's own responsibility, matching
     *        `mixLayerInto()`'s own precondition once a layer's content
     *        has been placed onto the canvas). Scoped this way
     *        deliberately: this installment proves the per-cell complex-
     *        mixing math on the GPU, which is the actual DSP content of
     *        `mixLayerInto()` - the placement geometry is a separate
     *        concern, left for whichever later installment actually
     *        wires this into `compositeProject()`.
     *
     * @param running The current composite. All three of its own arrays
     *        must be the same size.
     * @param layer The one layer's own contribution to mix in - all
     *        three of its own arrays must be the same size as `running`'s
     *        own.
     * @param layerGain `layer`'s own opacity, as a linear gain (matching
     *        `mixLayerInto()`'s own `layer.opacity()` usage) - not
     *        clamped or validated here.
     * @param mindWaveField A per-cell multiplier applied alongside
     *        `layerGain` (`sound_mind::core::Layer::opacityMindWave()`'s
     *        own per-cell field, evaluated by the caller - `sound-mind-gpu`
     *        has no dependency on `sound-mind-core`/`MindWave` itself, so
     *        this arrives as a plain array), same size as `running`'s/
     *        `layer`'s own arrays. Confirmed with the user (`v0.Y.31.1`
     *        Installment C1) as an additional per-cell buffer alongside
     *        the existing scalar `layerGain`, not a replacement for it - a
     *        caller with no MindWave binding passes an all-`1.0` array
     *        (no effect), matching `layerGain`'s own "not clamped or
     *        validated" scalar contract extended to a per-cell one.
     * @return The updated composite, same shape as `running`.
     * @throws std::runtime_error if `running`'s, `layer`'s, and
     *         `mindWaveField`'s own arrays aren't all the same size, or if
     *         any D3D12 call fails.
     */
    [[nodiscard]] AmplitudePhaseSignal mixAmplitudePhaseSignal(const AmplitudePhaseSignal& running,
                                                                const AmplitudePhaseSignal& layer, float layerGain,
                                                                const std::vector<float>& mindWaveField) const;

private:
    /// @brief Wraps an already-successfully-created device/queue/fence
    ///        triple - `create()`'s own private constructor; use
    ///        `create()` itself to obtain one.
    explicit ComputeDevice(AdapterKind adapterKind, Microsoft::WRL::ComPtr<ID3D12Device> device,
                            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
                            Microsoft::WRL::ComPtr<ID3D12Fence> fence);

    /// @brief Closes, executes, and blocks the calling thread until
    ///        `commandList` has actually finished running on the GPU -
    ///        the shared tail end of every method in this class that
    ///        records and submits its own commands.
    /// @param commandList The command list to close and execute -
    ///        already fully recorded.
    void executeAndWait(ID3D12GraphicsCommandList* commandList) const;

    AdapterKind adapterKind_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    // Bumped once per executeAndWait() call (a fresh fence-value handoff
    // each time, since every call rebuilds its own command list rather
    // than reusing one - see this class's own docs) - mutable since
    // every public method here is logically const (none change which
    // device/adapter this object represents) but still needs to advance
    // this internal counter.
    mutable std::uint64_t nextFenceValue_ = 1;
    void* fenceEvent_ = nullptr;  // HANDLE, kept void* to avoid <Windows.h> in this public header.
};

}  // namespace sound_mind::gpu
