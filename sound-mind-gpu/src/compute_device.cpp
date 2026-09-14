// Defined before any other include - even this file's own public header,
// whose <wrl/client.h> (ComPtr) transitively pulls in <Windows.h> before
// a #define appearing any later in this file would take effect. Without
// this, Windows.h's own max/min macros shadow std::max/std::min
// throughout this file, producing a confusing "illegal token on right
// side of ::" MSVC parse error at every std::max/std::min call below.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "sound_mind/gpu/compute_device.h"

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace sound_mind::gpu {

namespace {

/// @brief Throws `std::runtime_error` (naming `what` and `hr`, in hex)
/// if `hr` is a failure code - every D3D12 call in this file goes
/// through this, so a failure is never silently ignored.
void throwIfFailed(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::ostringstream message;
        message << "sound_mind::gpu: " << what << " failed (HRESULT 0x" << std::hex
                << static_cast<unsigned long>(hr) << ")";
        throw std::runtime_error(message.str());
    }
}

/// @brief Reads a whole file's own bytes - used for the precompiled
/// shader bytecode this module's own CMakeLists.txt compiles via `dxc`
/// at build time (confirmed with the user: offline, not runtime,
/// compilation).
std::vector<std::uint8_t> readWholeFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("sound_mind::gpu: could not open shader bytecode file: " + path);
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("sound_mind::gpu: could not read shader bytecode file: " + path);
    }
    return bytes;
}

/// @brief A plain buffer resource description, `size` bytes, with the
/// given extra flags (e.g. `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`
/// for a UAV-writable buffer) - every buffer this file creates shares
/// this same shape, just different heap types/flags.
D3D12_RESOURCE_DESC bufferResourceDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    return desc;
}

/// @brief Heap properties for a plain, single-adapter heap of `type` -
/// every heap this file creates is this same shape.
D3D12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES props{};
    props.Type = type;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

/// @brief Creates an upload-heap buffer of `sizeInBytes` and copies
/// `data` into it - used for every read-only GPU input in this file,
/// bound directly as a root SRV with no separate default-heap copy (see
/// `ComputeDevice`'s own docs on why that's an accepted simplification
/// for resources read exactly once per call).
ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device* device, const void* data, UINT64 sizeInBytes) {
    const D3D12_HEAP_PROPERTIES heapProps = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC desc = bufferResourceDesc(sizeInBytes, D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> buffer;
    throwIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)),
                  "CreateCommittedResource (upload)");
    void* mapped = nullptr;
    const D3D12_RANGE noReadRange{0, 0};  // Never read back through this CPU-write-only mapping.
    throwIfFailed(buffer->Map(0, &noReadRange, &mapped), "Map (upload)");
    std::memcpy(mapped, data, static_cast<std::size_t>(sizeInBytes));
    buffer->Unmap(0, nullptr);
    return buffer;
}

/// @brief Creates a default-heap, UAV-writable buffer of `sizeInBytes`,
/// starting directly in the `UNORDERED_ACCESS` state (a valid initial
/// state for a default-heap buffer flagged for UAV use).
ComPtr<ID3D12Resource> createUavBuffer(ID3D12Device* device, UINT64 sizeInBytes) {
    const D3D12_HEAP_PROPERTIES heapProps = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC desc = bufferResourceDesc(sizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> buffer;
    throwIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                   IID_PPV_ARGS(&buffer)),
                  "CreateCommittedResource (UAV)");
    return buffer;
}

/// @brief Creates a readback-heap buffer of `sizeInBytes`, starting in
/// the `COPY_DEST` state - ready for a `CopyBufferRegion()` into it.
ComPtr<ID3D12Resource> createReadbackBuffer(ID3D12Device* device, UINT64 sizeInBytes) {
    const D3D12_HEAP_PROPERTIES heapProps = heapProperties(D3D12_HEAP_TYPE_READBACK);
    const D3D12_RESOURCE_DESC desc = bufferResourceDesc(sizeInBytes, D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> buffer;
    throwIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buffer)),
                  "CreateCommittedResource (readback)");
    return buffer;
}

/// @brief Reads `count` floats back out of a mapped readback buffer -
/// the common tail end of every GPU method in this file.
std::vector<float> readBackFloats(ID3D12Resource* readbackBuffer, std::size_t count) {
    std::vector<float> result(count);
    const UINT64 sizeInBytes = static_cast<UINT64>(count) * sizeof(float);
    void* mapped = nullptr;
    const D3D12_RANGE readRange{0, static_cast<SIZE_T>(sizeInBytes)};
    throwIfFailed(readbackBuffer->Map(0, &readRange, &mapped), "Map (readback)");
    std::memcpy(result.data(), mapped, static_cast<std::size_t>(sizeInBytes));
    const D3D12_RANGE noWriteRange{0, 0};  // Never wrote through this CPU-read-only mapping.
    readbackBuffer->Unmap(0, &noWriteRange);
    return result;
}

/// @brief A `D3D12_RESOURCE_BARRIER` transitioning `resource` from
/// `before` to `after` - every barrier in this file is this same shape.
D3D12_RESOURCE_BARRIER transitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                          D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

/// @brief A compiled shader's own root signature + pipeline state,
/// loaded together since the PSO needs the root signature and both come
/// from the same bytecode blob.
struct ComputePipeline {
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
};

/// @brief Loads `shaderFileName` (from `SOUND_MIND_GPU_SHADER_DIR`,
/// this module's own build-time dxc output - see `CMakeLists.txt`) and
/// builds its root signature (read directly out of the bytecode's own
/// embedded `[RootSignature(...)]` blob - see the `.hlsl` files' own
/// docs) and compute PSO.
ComputePipeline loadComputePipeline(ID3D12Device* device, const std::string& shaderFileName) {
    const auto bytecode = readWholeFile(std::string(SOUND_MIND_GPU_SHADER_DIR) + "/" + shaderFileName);

    ComputePipeline pipeline;
    throwIfFailed(
        device->CreateRootSignature(0, bytecode.data(), bytecode.size(), IID_PPV_ARGS(&pipeline.rootSignature)),
        "CreateRootSignature");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = pipeline.rootSignature.Get();
    psoDesc.CS.pShaderBytecode = bytecode.data();
    psoDesc.CS.BytecodeLength = bytecode.size();
    throwIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline.pipelineState)),
                  "CreateComputePipelineState");
    return pipeline;
}

/// @brief A command allocator plus an already-recording command list of
/// type COMPUTE, using `pipeline`'s own initial pipeline state - the
/// boilerplate every GPU method in this file needs before it can record
/// anything.
struct CommandRecorder {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
};

CommandRecorder createCommandRecorder(ID3D12Device* device, ID3D12PipelineState* initialState) {
    CommandRecorder recorder;
    throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&recorder.allocator)),
                  "CreateCommandAllocator");
    throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, recorder.allocator.Get(),
                                             initialState, IID_PPV_ARGS(&recorder.list)),
                  "CreateCommandList");
    return recorder;
}

/// @brief The 1D Gaussian kernel weights `gaussianBlur2D()`'s own two
/// passes both use - the exact same formula `sound_mind::core`'s own
/// (CPU) `gaussianKernel1D()` (`filter_application.cpp`) computes:
/// `sigma` floored at `0.1`, radius truncated at 4 standard deviations,
/// normalized to sum to 1.
std::vector<float> gaussianKernel1D(float sigma) {
    const float s = std::max(0.1f, sigma);
    const int radius = std::max(1, static_cast<int>(std::ceil(4.0f * s)));
    std::vector<float> kernel(static_cast<std::size_t>(radius) * 2 + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        const float weight = std::exp(-(static_cast<float>(i) * static_cast<float>(i)) / (2.0f * s * s));
        kernel[static_cast<std::size_t>(i + radius)] = weight;
        sum += weight;
    }
    for (float& weight : kernel) {
        weight /= sum;
    }
    return kernel;
}

/// @brief Tries to create a D3D12 device from `adapter` at feature level
/// 11.0 (a broad compute-capable baseline) - returns null, without
/// throwing, on failure: a failed hardware attempt is an expected,
/// handled outcome here (the next fallback tier), not an error - see
/// `ComputeDevice::create()`'s own docs.
ComPtr<ID3D12Device> tryCreateDevice(IDXGIAdapter* adapter) {
    ComPtr<ID3D12Device> device;
    D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    return device;
}

/// @brief The first real, non-software hardware adapter a D3D12 device
/// can actually be created from, or null if none works.
ComPtr<ID3D12Device> createHardwareDevice(IDXGIFactory4* factory) {
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;  // DXGI's own software-adapter listing - WARP is requested explicitly below instead.
        }
        if (ComPtr<ID3D12Device> device = tryCreateDevice(adapter.Get())) {
            return device;
        }
    }
    return nullptr;
}

/// @brief Creates a device from Microsoft's WARP software adapter - see
/// `AdapterKind::Warp`'s own docs for why this fallback tier exists.
ComPtr<ID3D12Device> createWarpDevice(IDXGIFactory4* factory) {
    ComPtr<IDXGIAdapter> warpAdapter;
    if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)))) {
        return nullptr;
    }
    return tryCreateDevice(warpAdapter.Get());
}

}  // namespace

std::optional<ComputeDevice> ComputeDevice::create() {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return std::nullopt;
    }

    AdapterKind adapterKind = AdapterKind::Hardware;
    ComPtr<ID3D12Device> device = createHardwareDevice(factory.Get());
    if (!device) {
        adapterKind = AdapterKind::Warp;
        device = createWarpDevice(factory.Get());
    }
    if (!device) {
        return std::nullopt;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    ComPtr<ID3D12CommandQueue> commandQueue;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) {
        return std::nullopt;
    }

    ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        return std::nullopt;
    }

    return ComputeDevice(adapterKind, std::move(device), std::move(commandQueue), std::move(fence));
}

ComputeDevice::ComputeDevice(AdapterKind adapterKind, Microsoft::WRL::ComPtr<ID3D12Device> device,
                              Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
                              Microsoft::WRL::ComPtr<ID3D12Fence> fence)
    : adapterKind_(adapterKind),
      device_(std::move(device)),
      commandQueue_(std::move(commandQueue)),
      fence_(std::move(fence)),
      fenceEvent_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {
    if (fenceEvent_ == nullptr) {
        throw std::runtime_error("sound_mind::gpu: CreateEventW failed");
    }
}

ComputeDevice::ComputeDevice(ComputeDevice&& other) noexcept
    : adapterKind_(other.adapterKind_),
      device_(std::move(other.device_)),
      commandQueue_(std::move(other.commandQueue_)),
      fence_(std::move(other.fence_)),
      nextFenceValue_(other.nextFenceValue_),
      fenceEvent_(other.fenceEvent_) {
    other.fenceEvent_ = nullptr;
}

ComputeDevice& ComputeDevice::operator=(ComputeDevice&& other) noexcept {
    if (this != &other) {
        if (fenceEvent_ != nullptr) {
            CloseHandle(fenceEvent_);
        }
        adapterKind_ = other.adapterKind_;
        device_ = std::move(other.device_);
        commandQueue_ = std::move(other.commandQueue_);
        fence_ = std::move(other.fence_);
        nextFenceValue_ = other.nextFenceValue_;
        fenceEvent_ = other.fenceEvent_;
        other.fenceEvent_ = nullptr;
    }
    return *this;
}

ComputeDevice::~ComputeDevice() {
    if (fenceEvent_ != nullptr) {
        CloseHandle(fenceEvent_);
    }
}

void ComputeDevice::executeAndWait(ID3D12GraphicsCommandList* commandList) const {
    throwIfFailed(commandList->Close(), "Close (command list)");
    ID3D12CommandList* commandLists[] = {commandList};
    commandQueue_->ExecuteCommandLists(1, commandLists);

    const std::uint64_t fenceValueToWaitFor = nextFenceValue_++;
    throwIfFailed(commandQueue_->Signal(fence_.Get(), fenceValueToWaitFor), "Signal (fence)");
    if (fence_->GetCompletedValue() < fenceValueToWaitFor) {
        throwIfFailed(fence_->SetEventOnCompletion(fenceValueToWaitFor, fenceEvent_), "SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

std::vector<float> ComputeDevice::multiplyByTwo(const std::vector<float>& input) const {
    if (input.empty()) {
        return {};
    }

    const UINT elementCount = static_cast<UINT>(input.size());
    const UINT64 bufferSize = static_cast<UINT64>(input.size()) * sizeof(float);

    const ComputePipeline pipeline = loadComputePipeline(device_.Get(), "multiply_by_two.cso");
    CommandRecorder recorder = createCommandRecorder(device_.Get(), pipeline.pipelineState.Get());

    // Input: an upload-heap buffer, read directly by the shader's own
    // root SRV - no separate default-heap copy (see this class's own
    // docs on why that's an accepted simplification for a buffer read
    // exactly once per call).
    const ComPtr<ID3D12Resource> inputBuffer = createUploadBuffer(device_.Get(), input.data(), bufferSize);
    const ComPtr<ID3D12Resource> outputBuffer = createUavBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> readbackBuffer = createReadbackBuffer(device_.Get(), bufferSize);

    // Root parameter indices match the HLSL's own root signature string
    // order exactly: 0 = the element-count constant (b0), 1 = the input
    // SRV (t0), 2 = the output UAV (u0).
    recorder.list->SetComputeRootSignature(pipeline.rootSignature.Get());
    recorder.list->SetComputeRoot32BitConstant(0, elementCount, 0);
    recorder.list->SetComputeRootShaderResourceView(1, inputBuffer->GetGPUVirtualAddress());
    recorder.list->SetComputeRootUnorderedAccessView(2, outputBuffer->GetGPUVirtualAddress());

    // Rounds up - the shader's own bounds check (see the .hlsl file)
    // handles any extra threads in a partial last group of 64.
    const UINT threadGroupCount = (elementCount + 63) / 64;
    recorder.list->Dispatch(threadGroupCount, 1, 1);

    const D3D12_RESOURCE_BARRIER barrier = transitionBarrier(
        outputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    recorder.list->ResourceBarrier(1, &barrier);
    recorder.list->CopyBufferRegion(readbackBuffer.Get(), 0, outputBuffer.Get(), 0, bufferSize);

    executeAndWait(recorder.list.Get());
    return readBackFloats(readbackBuffer.Get(), input.size());
}

std::vector<float> ComputeDevice::gaussianBlur2D(const std::vector<float>& data, std::uint32_t width,
                                                  std::uint32_t height, float sigma) const {
    if (static_cast<std::uint64_t>(width) * height != data.size()) {
        throw std::runtime_error("sound_mind::gpu: gaussianBlur2D() - width * height must equal data.size()");
    }
    if (data.empty()) {
        return {};
    }

    const auto kernel = gaussianKernel1D(sigma);
    const UINT radius = static_cast<UINT>(kernel.size() / 2);
    const UINT64 bufferSize = static_cast<UINT64>(data.size()) * sizeof(float);
    const UINT64 kernelSize = static_cast<UINT64>(kernel.size()) * sizeof(float);

    const ComputePipeline pipeline = loadComputePipeline(device_.Get(), "gaussian_blur.cso");
    CommandRecorder recorder = createCommandRecorder(device_.Get(), pipeline.pipelineState.Get());

    const ComPtr<ID3D12Resource> inputStaging = createUploadBuffer(device_.Get(), data.data(), bufferSize);
    const ComPtr<ID3D12Resource> kernelBuffer = createUploadBuffer(device_.Get(), kernel.data(), kernelSize);
    // Two default-heap buffers, ping-ponged between the horizontal and
    // vertical passes - see this method's own docs on why two real
    // dispatches, not one, matches the CPU algorithm's own separable
    // shape.
    const ComPtr<ID3D12Resource> bufferA = createUavBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> bufferB = createUavBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> readbackBuffer = createReadbackBuffer(device_.Get(), bufferSize);

    recorder.list->SetComputeRootSignature(pipeline.rootSignature.Get());
    // bufferA starts life in UNORDERED_ACCESS (createUavBuffer()'s own
    // valid creation state) - it must transition to COPY_DEST before
    // CopyBufferRegion() can write into it as a destination.
    {
        const D3D12_RESOURCE_BARRIER toCopyDest =
            transitionBarrier(bufferA.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        recorder.list->ResourceBarrier(1, &toCopyDest);
    }
    recorder.list->CopyBufferRegion(bufferA.Get(), 0, inputStaging.Get(), 0, bufferSize);
    {
        const D3D12_RESOURCE_BARRIER barrier =
            transitionBarrier(bufferA.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        recorder.list->ResourceBarrier(1, &barrier);
    }

    const UINT groupsX = (width + 7) / 8;
    const UINT groupsY = (height + 7) / 8;

    // Root parameter indices match gaussian_blur.hlsl's own root
    // signature string order: 0 = constants (b0: width, height, radius,
    // direction), 1 = the data SRV (t0), 2 = the kernel SRV (t1), 3 = the
    // output UAV (u0).
    const auto dispatchPass = [&](ID3D12Resource* source, ID3D12Resource* destination, UINT direction) {
        const UINT constants[4] = {width, height, radius, direction};
        recorder.list->SetComputeRoot32BitConstants(0, 4, constants, 0);
        recorder.list->SetComputeRootShaderResourceView(1, source->GetGPUVirtualAddress());
        recorder.list->SetComputeRootShaderResourceView(2, kernelBuffer->GetGPUVirtualAddress());
        recorder.list->SetComputeRootUnorderedAccessView(3, destination->GetGPUVirtualAddress());
        recorder.list->Dispatch(groupsX, groupsY, 1);
    };

    // Pass 1: horizontal (bufferA -> bufferB).
    dispatchPass(bufferA.Get(), bufferB.Get(), 0);
    {
        D3D12_RESOURCE_BARRIER barriers[2] = {
            transitionBarrier(bufferB.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
            transitionBarrier(bufferA.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        };
        recorder.list->ResourceBarrier(2, barriers);
    }
    // Pass 2: vertical (bufferB -> bufferA).
    dispatchPass(bufferB.Get(), bufferA.Get(), 1);

    {
        const D3D12_RESOURCE_BARRIER barrier = transitionBarrier(
            bufferA.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        recorder.list->ResourceBarrier(1, &barrier);
    }
    recorder.list->CopyBufferRegion(readbackBuffer.Get(), 0, bufferA.Get(), 0, bufferSize);

    executeAndWait(recorder.list.Get());
    return readBackFloats(readbackBuffer.Get(), data.size());
}

AmplitudePhaseSignal ComputeDevice::mixAmplitudePhaseSignal(const AmplitudePhaseSignal& running,
                                                             const AmplitudePhaseSignal& layer, float layerGain,
                                                             const std::vector<float>& mindWaveField) const {
    const std::size_t cellCount = running.leftMagnitudeDb.size();
    const bool sameShape = running.rightMagnitudeDb.size() == cellCount && running.phaseRadians.size() == cellCount &&
                            layer.leftMagnitudeDb.size() == cellCount && layer.rightMagnitudeDb.size() == cellCount &&
                            layer.phaseRadians.size() == cellCount && mindWaveField.size() == cellCount;
    if (!sameShape) {
        throw std::runtime_error(
            "sound_mind::gpu: mixAmplitudePhaseSignal() - running's, layer's, and mindWaveField's own arrays must "
            "all be the same size");
    }
    if (cellCount == 0) {
        return {};
    }

    const UINT64 bufferSize = static_cast<UINT64>(cellCount) * sizeof(float);
    const ComputePipeline pipeline = loadComputePipeline(device_.Get(), "mix_amplitude_phase_signal.cso");
    CommandRecorder recorder = createCommandRecorder(device_.Get(), pipeline.pipelineState.Get());

    const ComPtr<ID3D12Resource> runningLeft =
        createUploadBuffer(device_.Get(), running.leftMagnitudeDb.data(), bufferSize);
    const ComPtr<ID3D12Resource> runningRight =
        createUploadBuffer(device_.Get(), running.rightMagnitudeDb.data(), bufferSize);
    const ComPtr<ID3D12Resource> runningPhase =
        createUploadBuffer(device_.Get(), running.phaseRadians.data(), bufferSize);
    const ComPtr<ID3D12Resource> layerLeft = createUploadBuffer(device_.Get(), layer.leftMagnitudeDb.data(), bufferSize);
    const ComPtr<ID3D12Resource> layerRight =
        createUploadBuffer(device_.Get(), layer.rightMagnitudeDb.data(), bufferSize);
    const ComPtr<ID3D12Resource> layerPhase = createUploadBuffer(device_.Get(), layer.phaseRadians.data(), bufferSize);
    const ComPtr<ID3D12Resource> mindWaveFieldBuffer =
        createUploadBuffer(device_.Get(), mindWaveField.data(), bufferSize);

    const ComPtr<ID3D12Resource> outLeft = createUavBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> outRight = createUavBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> outPhase = createUavBuffer(device_.Get(), bufferSize);

    const ComPtr<ID3D12Resource> readbackLeft = createReadbackBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> readbackRight = createReadbackBuffer(device_.Get(), bufferSize);
    const ComPtr<ID3D12Resource> readbackPhase = createReadbackBuffer(device_.Get(), bufferSize);

    // Root parameter indices match mix_amplitude_phase_signal.hlsl's own
    // root signature string order: 0 = constants (b0: cell count, gain),
    // 1-3 = running's own left/right/phase SRVs (t0-t2), 4-6 = layer's
    // own left/right/phase SRVs (t3-t5), 7 = the MindWaveField SRV (t6 -
    // v0.Y.31.1 Installment C1, a per-cell gain multiplier alongside the
    // scalar gain above), 8-10 = the three output UAVs (u0-u2).
    recorder.list->SetComputeRootSignature(pipeline.rootSignature.Get());
    struct Constants {
        UINT cellCount;
        float gain;
    } constants{static_cast<UINT>(cellCount), layerGain};
    recorder.list->SetComputeRoot32BitConstants(0, 2, &constants, 0);
    recorder.list->SetComputeRootShaderResourceView(1, runningLeft->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(2, runningRight->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(3, runningPhase->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(4, layerLeft->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(5, layerRight->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(6, layerPhase->GetGPUVirtualAddress());
    recorder.list->SetComputeRootShaderResourceView(7, mindWaveFieldBuffer->GetGPUVirtualAddress());
    recorder.list->SetComputeRootUnorderedAccessView(8, outLeft->GetGPUVirtualAddress());
    recorder.list->SetComputeRootUnorderedAccessView(9, outRight->GetGPUVirtualAddress());
    recorder.list->SetComputeRootUnorderedAccessView(10, outPhase->GetGPUVirtualAddress());

    const UINT threadGroupCount = (static_cast<UINT>(cellCount) + 63) / 64;
    recorder.list->Dispatch(threadGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER barriers[3] = {
        transitionBarrier(outLeft.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
        transitionBarrier(outRight.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
        transitionBarrier(outPhase.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
    };
    recorder.list->ResourceBarrier(3, barriers);
    recorder.list->CopyBufferRegion(readbackLeft.Get(), 0, outLeft.Get(), 0, bufferSize);
    recorder.list->CopyBufferRegion(readbackRight.Get(), 0, outRight.Get(), 0, bufferSize);
    recorder.list->CopyBufferRegion(readbackPhase.Get(), 0, outPhase.Get(), 0, bufferSize);

    executeAndWait(recorder.list.Get());

    AmplitudePhaseSignal result;
    result.leftMagnitudeDb = readBackFloats(readbackLeft.Get(), cellCount);
    result.rightMagnitudeDb = readBackFloats(readbackRight.Get(), cellCount);
    result.phaseRadians = readBackFloats(readbackPhase.Get(), cellCount);
    return result;
}

}  // namespace sound_mind::gpu
