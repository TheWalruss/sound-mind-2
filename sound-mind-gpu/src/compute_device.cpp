#include "sound_mind/gpu/compute_device.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

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

std::vector<float> ComputeDevice::multiplyByTwo(const std::vector<float>& input) const {
    if (input.empty()) {
        return {};
    }

    const UINT elementCount = static_cast<UINT>(input.size());
    const UINT64 bufferSize = static_cast<UINT64>(input.size()) * sizeof(float);

    const auto shaderBytecode = readWholeFile(std::string(SOUND_MIND_GPU_SHADER_DIR) + "/multiply_by_two.cso");

    ComPtr<ID3D12RootSignature> rootSignature;
    throwIfFailed(
        device_->CreateRootSignature(0, shaderBytecode.data(), shaderBytecode.size(), IID_PPV_ARGS(&rootSignature)),
        "CreateRootSignature");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.CS.pShaderBytecode = shaderBytecode.data();
    psoDesc.CS.BytecodeLength = shaderBytecode.size();
    ComPtr<ID3D12PipelineState> pipelineState;
    throwIfFailed(device_->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)),
                  "CreateComputePipelineState");

    ComPtr<ID3D12CommandAllocator> commandAllocator;
    throwIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&commandAllocator)),
                  "CreateCommandAllocator");
    ComPtr<ID3D12GraphicsCommandList> commandList;
    throwIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, commandAllocator.Get(),
                                              pipelineState.Get(), IID_PPV_ARGS(&commandList)),
                  "CreateCommandList");

    // Input: an upload-heap buffer, read directly by the shader's own
    // root SRV - no separate default-heap copy. A real, repeatedly-
    // dispatched DSP kernel would want the usual default-heap-plus-copy
    // pattern instead (GPU-local memory is faster to read from
    // repeatedly); skipped here since this buffer is read exactly once
    // per call, matching this class's own documented "prove the
    // plumbing, not the efficiency" scope.
    const D3D12_HEAP_PROPERTIES uploadHeapProps = heapProperties(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC inputDesc = bufferResourceDesc(bufferSize, D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> inputBuffer;
    throwIfFailed(device_->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &inputDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS(&inputBuffer)),
                  "CreateCommittedResource (input)");
    {
        void* mapped = nullptr;
        const D3D12_RANGE noReadRange{0, 0};  // Never read back through this CPU-write-only mapping.
        throwIfFailed(inputBuffer->Map(0, &noReadRange, &mapped), "Map (input)");
        std::memcpy(mapped, input.data(), static_cast<std::size_t>(bufferSize));
        inputBuffer->Unmap(0, nullptr);
    }

    const D3D12_HEAP_PROPERTIES defaultHeapProps = heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC outputDesc = bufferResourceDesc(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> outputBuffer;
    throwIfFailed(device_->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &outputDesc,
                                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                    IID_PPV_ARGS(&outputBuffer)),
                  "CreateCommittedResource (output)");

    const D3D12_HEAP_PROPERTIES readbackHeapProps = heapProperties(D3D12_HEAP_TYPE_READBACK);
    const D3D12_RESOURCE_DESC readbackDesc = bufferResourceDesc(bufferSize, D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> readbackBuffer;
    throwIfFailed(device_->CreateCommittedResource(&readbackHeapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                    IID_PPV_ARGS(&readbackBuffer)),
                  "CreateCommittedResource (readback)");

    // Root parameter indices match the HLSL's own root signature string
    // order exactly: 0 = the element-count constant (b0), 1 = the input
    // SRV (t0), 2 = the output UAV (u0).
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetComputeRoot32BitConstant(0, elementCount, 0);
    commandList->SetComputeRootShaderResourceView(1, inputBuffer->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(2, outputBuffer->GetGPUVirtualAddress());
    commandList->SetPipelineState(pipelineState.Get());

    // Rounds up - the shader's own bounds check (see the .hlsl file)
    // handles any extra threads in a partial last group of 64.
    const UINT threadGroupCount = (elementCount + 63) / 64;
    commandList->Dispatch(threadGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = outputBuffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    commandList->ResourceBarrier(1, &barrier);

    commandList->CopyBufferRegion(readbackBuffer.Get(), 0, outputBuffer.Get(), 0, bufferSize);

    throwIfFailed(commandList->Close(), "Close (command list)");
    ID3D12CommandList* commandLists[] = {commandList.Get()};
    commandQueue_->ExecuteCommandLists(1, commandLists);

    // A fresh fence value every call - see nextFenceValue_'s own docs.
    const std::uint64_t fenceValueToWaitFor = nextFenceValue_++;
    throwIfFailed(commandQueue_->Signal(fence_.Get(), fenceValueToWaitFor), "Signal (fence)");
    if (fence_->GetCompletedValue() < fenceValueToWaitFor) {
        throwIfFailed(fence_->SetEventOnCompletion(fenceValueToWaitFor, fenceEvent_), "SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    std::vector<float> result(input.size());
    {
        void* mapped = nullptr;
        const D3D12_RANGE readRange{0, static_cast<SIZE_T>(bufferSize)};
        throwIfFailed(readbackBuffer->Map(0, &readRange, &mapped), "Map (readback)");
        std::memcpy(result.data(), mapped, static_cast<std::size_t>(bufferSize));
        const D3D12_RANGE noWriteRange{0, 0};  // Never wrote through this CPU-read-only mapping.
        readbackBuffer->Unmap(0, &noWriteRange);
    }
    return result;
}

}  // namespace sound_mind::gpu
