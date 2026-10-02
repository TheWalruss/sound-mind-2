// Evaluates sound_mind::core's own recursive-midpoint-displacement
// GeneratorType::Fractal MindWave generator across a flat array of
// already-resolved per-cell "t" (loop-fraction, already folded into
// [0, 1)) values - see sound_mind::gpu::ComputeDevice::fractalField()'s
// own docs for the exact contract. v0.1.6.4,
// docs/sound-mind-roadmap.md's v0.Y.60.1 Installment D.
//
// A verbatim, iterative port of sound_mind::core::midpointDisplacement()'s
// own tail-recursive algorithm (mind_wave.cpp) - each recursive call there
// already does nothing after its own recursive call returns (a true tail
// call), so unrolling it into a loop changes nothing about the result,
// only how many stack frames it takes. HLSL has no practical support for
// a runtime (not compile-time-constant) recursion depth, which this
// generator's own Iterations is - hence the loop.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=4, b0), " \
    "SRV(t0), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint CellCount;
    uint Iterations;
    float Roughness;
    uint Seed;
}

StructuredBuffer<float> TValues : register(t0);
RWStructuredBuffer<float> Output : register(u0);

// Verbatim ports of mind_wave.cpp's own anonymous-namespace
// hashUint32()/hashCoord()/hashCoord2D()/hashToUnitSigned() - duplicated
// here rather than shared (sound-mind-gpu has no dependency on
// sound-mind-core, the same reasoning every other shader's own duplicated
// helper in this directory already follows).
uint HashUint32(uint value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

uint HashCoord(int coordinate, uint seed) {
    return HashUint32(uint(coordinate) * 0x9e3779b1U ^ HashUint32(seed));
}

uint HashCoord2D(int x, int y, uint seed) {
    return HashUint32(HashCoord(x, seed) ^ HashUint32(uint(y) * 0x85ebca6bU));
}

float HashToUnitSigned(uint h) {
    return (float(h) / 4294967295.0f) * 2.0f - 1.0f;
}

[RootSignature(ComputeRootSignature)]
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= CellCount) {
        return;
    }
    float t = TValues[id.x];

    // The same (left, right, leftValue, rightValue, level, nodeIndex)
    // state midpointDisplacement()'s own recursion threads through its
    // own parameter list, updated in place each iteration instead of
    // passed to a fresh recursive call.
    float left = 0.0f;
    float right = 1.0f;
    float leftValue = 0.5f;
    float rightValue = 0.5f;
    int level = 0;
    uint nodeIndex = 0;

    for (uint i = 0; i < Iterations; ++i) {
        float mid = (left + right) * 0.5f;
        float amplitude = pow(Roughness, float(level + 1));
        uint midHash = HashCoord2D(int(nodeIndex), level, Seed);
        float midValue = (leftValue + rightValue) * 0.5f + HashToUnitSigned(midHash) * amplitude;
        if (t < mid) {
            right = mid;
            rightValue = midValue;
            nodeIndex = nodeIndex * 2;
        } else {
            left = mid;
            leftValue = midValue;
            nodeIndex = nodeIndex * 2 + 1;
        }
        level += 1;
    }

    float span = right - left;
    float localT = (span > 0.0f) ? (t - left) / span : 0.0f;
    Output[id.x] = leftValue + (rightValue - leftValue) * localT;
}
