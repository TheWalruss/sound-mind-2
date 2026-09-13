// One pass of a separable 2D Gaussian blur - see
// sound_mind::gpu::ComputeDevice::gaussianBlur2D()'s own docs. The same
// shader/root-signature/PSO is dispatched twice (horizontal, then
// vertical), switching axis via the Direction root constant - not two
// separate compiled shaders, since the per-pixel logic is identical
// either way, just which neighbors it samples.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=4, b0), " \
    "SRV(t0), SRV(t1), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint Width;
    uint Height;
    uint Radius;
    uint Direction;  // 0 = horizontal (blur along x/columns), 1 = vertical (blur along y/rows).
};

StructuredBuffer<float> InputGrid : register(t0);
// 2*Radius+1 entries, index 0 = the leftmost/topmost tap - the same
// normalized weights sound_mind::gpu's own (CPU-side) gaussianKernel1D()
// computes.
StructuredBuffer<float> KernelWeights : register(t1);
RWStructuredBuffer<float> OutputGrid : register(u0);

[RootSignature(ComputeRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) {
        return;
    }

    float sum = 0.0f;
    int radius = int(Radius);
    for (int k = -radius; k <= radius; ++k) {
        int sampleX = int(id.x);
        int sampleY = int(id.y);
        if (Direction == 0) {
            sampleX = clamp(sampleX + k, 0, int(Width) - 1);
        } else {
            sampleY = clamp(sampleY + k, 0, int(Height) - 1);
        }
        float weight = KernelWeights[uint(k + radius)];
        sum += InputGrid[uint(sampleY) * Width + uint(sampleX)] * weight;
    }
    OutputGrid[id.y * Width + id.x] = sum;
}
