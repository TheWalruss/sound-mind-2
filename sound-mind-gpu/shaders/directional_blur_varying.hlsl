// A directional (motion-blur-style) kernel with length and angle both
// evaluated fresh per output cell - see
// sound_mind::gpu::ComputeDevice::directionalBlur2DVarying()'s own docs,
// including why this accumulates each step directly rather than grouping
// duplicate integer offsets the way the CPU version's own
// directionalBlurOffsets() does.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=2, b0), " \
    "SRV(t0), SRV(t1), SRV(t2), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint Width;
    uint Height;
};

StructuredBuffer<float> InputGrid : register(t0);
StructuredBuffer<float> LengthPerCell : register(t1);
StructuredBuffer<float> AngleDegreesPerCell : register(t2);
RWStructuredBuffer<float> OutputGrid : register(u0);

static const float kPi = 3.14159265358979323846f;

// HLSL's own round() uses round-half-to-even; the CPU reference this
// kernel must agree with (sound_mind::core's own directionalBlurOffsets()/
// directionalBlur2DVarying(), and this file's own test-side reference)
// uses std::lround() - round-half-away-from-zero. The two conventions only
// disagree exactly at a .5 boundary, but that boundary is hit often here
// (sin(30 degrees) is exactly 0.5, so any odd step at a 30/150-degree-
// family angle lands exactly on one) - found via a real GPU/CPU mismatch,
// not a hypothetical one. This matches std::lround() instead of HLSL's
// own built-in.
float roundHalfAwayFromZero(float x) {
    return (x >= 0.0f) ? floor(x + 0.5f) : ceil(x - 0.5f);
}

[RootSignature(ComputeRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) {
        return;
    }
    uint cell = id.y * Width + id.x;
    float rawLength = LengthPerCell[cell];
    if (rawLength <= 0.0f) {
        OutputGrid[cell] = InputGrid[cell];  // Baseline: a zero-length kernel is the identity.
        return;
    }

    int n = max(1, int(roundHalfAwayFromZero(rawLength)));
    float angleRadians = AngleDegreesPerCell[cell] * kPi / 180.0f;
    float cosA = cos(angleRadians);
    float sinA = sin(angleRadians);

    float sum = 0.0f;
    int totalSteps = 2 * n + 1;
    for (int step = -n; step <= n; ++step) {
        int colOffset = int(roundHalfAwayFromZero(float(step) * cosA));
        int rowOffset = int(roundHalfAwayFromZero(-float(step) * sinA));
        int sampleX = clamp(int(id.x) + colOffset, 0, int(Width) - 1);
        int sampleY = clamp(int(id.y) + rowOffset, 0, int(Height) - 1);
        sum += InputGrid[uint(sampleY) * Width + uint(sampleX)];
    }
    OutputGrid[cell] = sum / float(totalSteps);
}
