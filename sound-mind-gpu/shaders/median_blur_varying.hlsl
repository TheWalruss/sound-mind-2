// A 2D median filter with the window size evaluated fresh per output
// cell - see sound_mind::gpu::ComputeDevice::medianBlur2DVarying()'s own
// docs, including why the per-cell window size is clamped to 31 (961
// samples) here.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=2, b0), " \
    "SRV(t0), SRV(t1), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint Width;
    uint Height;
};

StructuredBuffer<float> InputGrid : register(t0);
StructuredBuffer<float> SizePerCell : register(t1);
RWStructuredBuffer<float> OutputGrid : register(u0);

// 31x31 - matches sound_mind::core::FilterConfiguration::medianSize()'s
// own Studio UI ceiling (never exceeded through normal use); a fixed
// local array needs a compile-time size, and no std::nth_element
// equivalent exists in HLSL to avoid needing one.
#define MAX_WINDOW_SAMPLES 961

// HLSL's own round() uses round-half-to-even; the CPU reference this
// kernel must agree with uses std::lround() (round-half-away-from-zero) -
// see directional_blur_varying.hlsl's own docs for the real mismatch this
// caused there, applied here defensively for the same reason even though
// medianSize()'s own typical values are less likely to land exactly on a
// .5 boundary.
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
    float rawSize = SizePerCell[cell];
    if (rawSize <= 1.0f) {
        OutputGrid[cell] = InputGrid[cell];  // Baseline: a 1-cell window is the identity.
        return;
    }

    int windowSize = max(3, int(roundHalfAwayFromZero(rawSize)) | 1);
    windowSize = min(windowSize, 31);
    int half = windowSize / 2;

    float samples[MAX_WINDOW_SAMPLES];
    int count = 0;
    for (int dy = -half; dy <= half; ++dy) {
        int sampleY = clamp(int(id.y) + dy, 0, int(Height) - 1);
        for (int dx = -half; dx <= half; ++dx) {
            int sampleX = clamp(int(id.x) + dx, 0, int(Width) - 1);
            samples[count] = InputGrid[uint(sampleY) * Width + uint(sampleX)];
            count = count + 1;
        }
    }

    // A plain insertion sort - correct and simple for a window this
    // small; no std::nth_element equivalent exists in HLSL.
    for (int i = 1; i < count; ++i) {
        float key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j = j - 1;
        }
        samples[j + 1] = key;
    }
    OutputGrid[cell] = samples[count / 2];
}
