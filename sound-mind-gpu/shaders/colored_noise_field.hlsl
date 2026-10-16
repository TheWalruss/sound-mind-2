// Evaluates sound_mind::core's GeneratorType::Noise MindWave over a whole
// field - see sound_mind::gpu::ComputeDevice::coloredNoiseField()'s own docs
// for the contract. v0.1.41.5, roadmap slot v0.Y.80.2 step 5.
//
// A port of sound_mind::core::coloredNoiseSample() (generator.cpp): white
// noise from a SplitMix64-mixed hash of (seed, stream, step, row), either
// blended toward its own first difference (alpha >= 0) or summed over ten
// octave generators each held 2^k steps (alpha < 0). The hash is exact
// (64-bit integers - shader model 6.0 with Int64ShaderOps); the arithmetic
// after it is float, where the CPU uses double, so values agree to about
// 1e-6. The caller resolves each row's and column's integer coordinates.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=20, b0), " \
    "SRV(t0), " \
    "SRV(t1), " \
    "UAV(u0)"

cbuffer Constants : register(b0) {
    uint BinCount;
    uint FrameCount;
    uint StepAlongFrames;  // 1: the noise steps along frames (time axis); 0: along bins.
    uint SeedLow;
    uint SeedHigh;
    uint UseOctaves;       // 1: alpha < 0 (octave sum); 0: white toward its first difference.
    float Toward;          // alpha >= 0: how far toward the first difference.
    float InverseSpread;   // alpha < 0: 1 / the octave sum's three-sigma spread.
    // alpha < 0: the ten octave weights, as float4s (a cbuffer array would
    // pad every element to 16 bytes and break the packed root constants).
    float4 WeightsLow;     // octaves 0-3.
    float4 WeightsMiddle;  // octaves 4-7.
    float4 WeightsHigh;    // octaves 8-9 in .xy.
}

StructuredBuffer<int64_t> BinCoordinate : register(t0);    // one per bin.
StructuredBuffer<int64_t> FrameCoordinate : register(t1);  // one per frame.
RWStructuredBuffer<float> Output : register(u0);

uint64_t MakeU64(uint high, uint low) {
    return (uint64_t(high) << 32) | uint64_t(low);
}

// SplitMix64's finalizer - generator.cpp's mix64().
uint64_t Mix64(uint64_t value) {
    value += MakeU64(0x9E3779B9u, 0x7F4A7C15u);
    value = (value ^ (value >> 30)) * MakeU64(0xBF58476Du, 0x1CE4E5B9u);
    value = (value ^ (value >> 27)) * MakeU64(0x94D049BBu, 0x133111EBu);
    return value ^ (value >> 31);
}

// generator.cpp's hashedUnitNoise(), in [0, 1) - its top 24 bits, where the
// CPU keeps 53.
float HashedUnit(uint64_t seed, uint64_t stream, int64_t x, int64_t y) {
    uint64_t hash = Mix64(seed ^ Mix64(stream));
    hash = Mix64(hash ^ uint64_t(x));
    hash = Mix64(hash ^ uint64_t(y));
    return float(uint(hash >> 40)) * (1.0f / 16777216.0f);
}

[RootSignature(ComputeRootSignature)]
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    const uint frame = id.x;
    const uint bin = id.y;
    if (frame >= FrameCount || bin >= BinCount) {
        return;
    }
    const uint64_t seed = MakeU64(SeedHigh, SeedLow);
    const int64_t step = StepAlongFrames != 0 ? FrameCoordinate[frame] : BinCoordinate[bin];
    const int64_t row = StepAlongFrames != 0 ? BinCoordinate[bin] : FrameCoordinate[frame];

    float value;
    if (UseOctaves == 0) {
        const float now = HashedUnit(seed, 0, step, row) - 0.5f;
        const float before = HashedUnit(seed, 0, step - 1, row) - 0.5f;
        value = 0.5f + (1.0f - Toward) * now + Toward * 0.5f * (now - before);
    } else {
        const float weights[10] = {WeightsLow.x,    WeightsLow.y,    WeightsLow.z,    WeightsLow.w,
                                   WeightsMiddle.x, WeightsMiddle.y, WeightsMiddle.z, WeightsMiddle.w,
                                   WeightsHigh.x,   WeightsHigh.y};
        float sum = 0.0f;
        for (uint k = 0; k < 10; ++k) {
            // Arithmetic shift: floor(step / 2^k), negative steps included.
            const int64_t held = step >> k;
            sum += weights[k] * (HashedUnit(seed, 1 + k, held, row) - 0.5f);
        }
        value = 0.5f + 0.5f * sum * InverseSpread;
    }
    Output[bin * FrameCount + frame] = saturate(value);
}
