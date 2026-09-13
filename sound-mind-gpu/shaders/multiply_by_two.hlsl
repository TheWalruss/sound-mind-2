// A trivial, DSP-meaningless compute shader - see
// sound_mind::gpu::ComputeDevice::multiplyByTwo()'s own docs for why:
// this installment proves the D3D12 device/pipeline/dispatch/readback
// round trip works end to end via a hand-verifiable operation, before
// risking a real, harder-to-verify DSP kernel on top of unproven
// plumbing (confirmed with the user ahead of implementation).
//
// The root signature is embedded here, via the [RootSignature(...)]
// attribute, rather than described separately in C++ - the compiled
// bytecode this produces carries its own root signature blob, which
// ID3D12Device::CreateRootSignature() reads directly, keeping the one
// true definition in this file instead of two copies that could drift.
// Every binding is a root descriptor/constant (no DescriptorTable(...)),
// so no descriptor heap is needed anywhere in this installment's own
// C++ side.
#define ComputeRootSignature "RootConstants(num32BitConstants=1, b0), SRV(t0), UAV(u0)"

cbuffer Constants : register(b0) {
    uint ElementCount;
};

StructuredBuffer<float> InputBuffer : register(t0);
RWStructuredBuffer<float> OutputBuffer : register(u0);

[RootSignature(ComputeRootSignature)]
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    // Dispatch() rounds up to a whole number of 64-thread groups, so the
    // last group can run past the real element count for any ElementCount
    // not itself a multiple of 64 - guard against reading/writing past
    // either buffer's own end.
    if (dispatchThreadId.x >= ElementCount) {
        return;
    }
    OutputBuffer[dispatchThreadId.x] = InputBuffer[dispatchThreadId.x] * 2.0f;
}
