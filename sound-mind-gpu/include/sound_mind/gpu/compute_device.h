#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <wrl/client.h>

struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Fence;

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
 * @brief A live DirectX 12 compute device - `sound-mind-gpu`'s own first
 *        real code (`docs/sound-mind-architecture.md`'s Build & Module
 *        Layout has named this module since the very first architecture
 *        draft, with nothing behind it until now).
 *
 * **Deliberately pure plumbing for this installment** (`v0.Y.30.1`'s own
 * "GPU Compute Enablement", confirmed with the user ahead of
 * implementation): proves the whole device -> dispatch -> readback round
 * trip works end to end, via one trivial, DSP-meaningless operation
 * (multiplyByTwo()) - not yet a real DSP kernel. A real Filter Layer
 * operation or the compositor's own per-cell mixing, actually moved to
 * the GPU, is later, separate scope, built on top of this once it exists
 * - see this method's own docs for why proving the plumbing came first.
 *
 * Every D3D12 resource this class touches is rebuilt fresh per call
 * (root signature, PSO, buffers) rather than cached/reused across calls -
 * correct but not remotely efficient, an explicit, documented
 * simplification appropriate for "does the pipeline work at all," not a
 * pattern a real, repeatedly-dispatched DSP kernel should copy - see
 * multiplyByTwo()'s own docs.
 *
 * @note Not thread-safe, and not real-time-safe: every operation
 *       allocates, and multiplyByTwo() blocks its own calling thread on
 *       a GPU fence. Matches every other DSP path in this codebase before
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
     * @brief Doubles every element of `input`, entirely on the GPU - the
     *        one proof-of-pipeline operation this installment builds.
     *
     * Deliberately DSP-meaningless (see this class's own docs on why): a
     * trivial, hand-verifiable operation isolates whether the D3D12
     * plumbing itself (root signature embedded in the compiled shader,
     * PSO, root-descriptor buffer bindings, dispatch sizing against
     * `input.size()`, the fence wait, the readback copy) is correct,
     * without also needing to get a real DSP kernel's own math right at
     * the same time.
     *
     * Every GPU resource (root signature, PSO, the three buffers) is
     * created fresh on every call and torn down at the end of it - see
     * this class's own docs on why that's an accepted, temporary cost
     * here.
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

private:
    /// @brief Wraps an already-successfully-created device/queue/fence
    ///        triple - `create()`'s own private constructor; use
    ///        `create()` itself to obtain one.
    explicit ComputeDevice(AdapterKind adapterKind, Microsoft::WRL::ComPtr<ID3D12Device> device,
                            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
                            Microsoft::WRL::ComPtr<ID3D12Fence> fence);

    AdapterKind adapterKind_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    // Bumped once per multiplyByTwo() call (a fresh fence-value handoff
    // each time, since every call rebuilds its own command list rather
    // than reusing one - see this class's own docs) - mutable since
    // multiplyByTwo() is logically const (it doesn't change which device/
    // adapter this object represents) but still needs to advance this
    // internal counter.
    mutable std::uint64_t nextFenceValue_ = 1;
    void* fenceEvent_ = nullptr;  // HANDLE, kept void* to avoid <Windows.h> in this public header.
};

}  // namespace sound_mind::gpu
