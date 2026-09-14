// A genuine, non-separable 2D Gaussian blur, with sigma evaluated fresh
// per output cell - see
// sound_mind::gpu::ComputeDevice::gaussianBlur2DVarying()'s own docs for
// why this can't reuse gaussian_blur.hlsl's own separable two-pass shape
// (separability only holds for one shared sigma across the whole image).
// Each thread computes its own cell's own weighted sum directly over its
// own (sigma-dependent) neighborhood - real O(radius^2) work per cell,
// but no cross-thread coordination needed despite the varying radius.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=2, b0), " \
    "SRV(t0), SRV(t1), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint Width;
    uint Height;
};

StructuredBuffer<float> InputGrid : register(t0);
// One entry per cell - already resolved (MindWave-evaluated and lerped
// toward the configured ceiling, or a uniform ceiling if unbound) on the
// CPU side.
StructuredBuffer<float> SigmaPerCell : register(t1);
RWStructuredBuffer<float> OutputGrid : register(u0);

[RootSignature(ComputeRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) {
        return;
    }
    uint cell = id.y * Width + id.x;
    float sigma = max(0.1f, SigmaPerCell[cell]);
    int radius = max(1, int(ceil(4.0f * sigma)));
    float twoSigmaSquared = 2.0f * sigma * sigma;

    float weightedSum = 0.0f;
    float weightTotal = 0.0f;
    for (int dy = -radius; dy <= radius; ++dy) {
        int sampleY = clamp(int(id.y) + dy, 0, int(Height) - 1);
        for (int dx = -radius; dx <= radius; ++dx) {
            int sampleX = clamp(int(id.x) + dx, 0, int(Width) - 1);
            float distanceSquared = float(dx * dx + dy * dy);
            float weight = exp(-distanceSquared / twoSigmaSquared);
            weightedSum += InputGrid[uint(sampleY) * Width + uint(sampleX)] * weight;
            weightTotal += weight;
        }
    }
    OutputGrid[cell] = weightedSum / weightTotal;
}
