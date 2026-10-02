// Mixes one already-placed layer's own contribution into a running
// composite, per cell - see
// sound_mind::gpu::ComputeDevice::mixAmplitudePhaseSignal()'s own docs
// for the exact contract (and why placement/rescale isn't part of this
// shader).
//
// BlendMode selects which of sound_mind::core::applyBlendedCell()'s own
// seven per-mode formulas (blend_mode_application.cpp) this cell uses -
// v0.1.6.2, docs/sound-mind-roadmap.md's v0.Y.60.1 Installment B
// (previously this shader only ever implemented Normal's own math, the
// one mode the benchmark suite's own blend-mode sweep found GPU dispatch
// was actually accelerating). BlendMode uses the exact same ordinal
// values sound_mind::core::BlendMode's own enumerators declare - see
// ComputeDevice::mixAmplitudePhaseSignal()'s own docs.
//
// Normal (0): each channel's own dB value converts to linear, layer's
// own converted amplitude scales by Gain and MindWaveField[cell], each
// channel becomes a complex value (as a float2: x=real, y=imaginary)
// using its own signal's shared phase, the two complex values sum, and
// the result converts back to dB/phase - phase as the angle of the
// summed *mid* signal, (left + right) / 2. Unchanged from this shader's
// own pre-v0.1.6.2 behavior.
//
// Overwrite (1): a plain linear crossfade of the *raw* dB/phase values
// by Gain*MindWaveField[cell] (no dbToUnit() clamp - see
// applyBlendedCell()'s own docs on why Overwrite stays unclamped).
//
// Multiply/Screen/Overlay/Difference/Add (2-6): dbToUnit()-normalized
// per-channel amplitude, blended per mode, then a final linear crossfade
// against the base by Gain*MindWaveField[cell] - see
// applyBlendedCell()'s own docs for the exact per-mode formula and this
// shader's own BlendAmplitudeUnit()/BlendPhaseComplex() below, both
// verbatim ports of blend_mode_application.cpp's own anonymous-namespace
// helpers.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=3, b0), " \
    "SRV(t0), SRV(t1), SRV(t2), " \
    "SRV(t3), SRV(t4), SRV(t5), " \
    "SRV(t6), " \
    "UAV(u0), UAV(u1), UAV(u2)"

cbuffer Constants : register(b0) {
    uint CellCount;
    float Gain;  // layer's own opacity, as a linear gain.
    uint BlendMode;  // sound_mind::core::BlendMode's own ordinal value.
}

StructuredBuffer<float> RunningLeftDb : register(t0);
StructuredBuffer<float> RunningRightDb : register(t1);
StructuredBuffer<float> RunningPhase : register(t2);
StructuredBuffer<float> LayerLeftDb : register(t3);
StructuredBuffer<float> LayerRightDb : register(t4);
StructuredBuffer<float> LayerPhase : register(t5);
StructuredBuffer<float> MindWaveField : register(t6);  // per-cell gain multiplier - see above.
RWStructuredBuffer<float> OutLeftDb : register(u0);
RWStructuredBuffer<float> OutRightDb : register(u1);
RWStructuredBuffer<float> OutPhase : register(u2);

// The smallest linear amplitude that maps to anything but this floor's
// own dB value - avoids log10(0) for genuine silence. The same value
// sound_mind::core's own (CPU) kMinLinearAmplitude (compositor.cpp) uses.
static const float kMinLinearAmplitude = 1e-7f;

// The -96..0 dB display range blend_mode_application.cpp's own
// dbToUnit()/unitToDb() normalize into for every mode but Normal/
// Overwrite - see applyBlendedCell()'s own docs.
static const float kMinDb = -96.0f;
static const float kMaxDb = 0.0f;
static const float kMinComplexMagnitude = 1e-6f;

// sound_mind::core::BlendMode's own ordinal values - see
// ComputeDevice::mixAmplitudePhaseSignal()'s own docs.
static const uint kBlendNormal = 0;
static const uint kBlendOverwrite = 1;
static const uint kBlendMultiply = 2;
static const uint kBlendScreen = 3;
static const uint kBlendOverlay = 4;
static const uint kBlendDifference = 5;
static const uint kBlendAdd = 6;

float dbToLinear(float db) {
    return pow(10.0f, db / 20.0f);
}

float linearToDb(float amplitude) {
    return 20.0f * log10(max(amplitude, kMinLinearAmplitude));
}

float dbToUnit(float db) {
    float clamped = clamp(db, kMinDb, kMaxDb);
    return (clamped - kMinDb) / (kMaxDb - kMinDb);
}

float unitToDb(float unit) {
    return kMinDb + unit * (kMaxDb - kMinDb);
}

// Verbatim port of blend_mode_application.cpp's own blendAmplitudeUnit() -
// never called for Normal/Overwrite, both handled in their own branch
// before this would be reached.
float BlendAmplitudeUnit(uint mode, float base, float overlay) {
    if (mode == kBlendMultiply) {
        return base * overlay;
    }
    if (mode == kBlendScreen) {
        return 1.0f - (1.0f - base) * (1.0f - overlay);
    }
    if (mode == kBlendOverlay) {
        return (base < 0.5f) ? (2.0f * base * overlay) : (1.0f - 2.0f * (1.0f - base) * (1.0f - overlay));
    }
    if (mode == kBlendDifference) {
        return abs(base - overlay);
    }
    if (mode == kBlendAdd) {
        return base + overlay;
    }
    return overlay;  // Unreachable for the five modes above; a defensive fallback.
}

// Verbatim port of blend_mode_application.cpp's own blendPhaseComplex() -
// outUnit is the already-computed, already-opacity-blended output
// amplitude (the average of the final left/right unit values).
float2 BlendPhaseComplex(uint mode, float2 baseZ, float2 overlayZ, float basePhase, float overlayPhase,
                          float baseUnit, float outUnit) {
    if (mode == kBlendMultiply) {
        float phi = basePhase + overlayPhase;
        return outUnit * float2(cos(phi), sin(phi));
    }
    if (mode == kBlendScreen) {
        float2 sum = baseZ + overlayZ;
        float phi = (length(sum) > kMinComplexMagnitude) ? atan2(sum.y, sum.x) : basePhase;
        return outUnit * float2(cos(phi), sin(phi));
    }
    if (mode == kBlendOverlay) {
        float phi;
        if (baseUnit < 0.5f) {
            phi = basePhase + overlayPhase;
        } else {
            float2 sum = baseZ + overlayZ;
            phi = (length(sum) > kMinComplexMagnitude) ? atan2(sum.y, sum.x) : basePhase;
        }
        return outUnit * float2(cos(phi), sin(phi));
    }
    if (mode == kBlendDifference) {
        return baseZ - overlayZ;
    }
    if (mode == kBlendAdd) {
        return baseZ + overlayZ;
    }
    return overlayZ;  // Unreachable for the five modes above; a defensive fallback.
}

[RootSignature(ComputeRootSignature)]
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= CellCount) {
        return;
    }
    uint cell = id.x;
    float cellGain = Gain * MindWaveField[cell];

    if (BlendMode == kBlendNormal) {
        float layerLeftLinear = dbToLinear(LayerLeftDb[cell]) * cellGain;
        float layerRightLinear = dbToLinear(LayerRightDb[cell]) * cellGain;
        float layerPhaseValue = LayerPhase[cell];
        float2 layerDirection = float2(cos(layerPhaseValue), sin(layerPhaseValue));

        float runningLeftLinear = dbToLinear(RunningLeftDb[cell]);
        float runningRightLinear = dbToLinear(RunningRightDb[cell]);
        float runningPhaseValue = RunningPhase[cell];
        float2 runningDirection = float2(cos(runningPhaseValue), sin(runningPhaseValue));

        float2 newLeft = runningLeftLinear * runningDirection + layerLeftLinear * layerDirection;
        float2 newRight = runningRightLinear * runningDirection + layerRightLinear * layerDirection;

        OutLeftDb[cell] = linearToDb(length(newLeft));
        OutRightDb[cell] = linearToDb(length(newRight));
        float2 mid = (newLeft + newRight) * 0.5f;
        float midLength = length(mid);
        OutPhase[cell] = (midLength > 0.0f) ? atan2(mid.y, mid.x) : 0.0f;
        return;
    }

    if (BlendMode == kBlendOverwrite) {
        OutLeftDb[cell] = RunningLeftDb[cell] * (1.0f - cellGain) + LayerLeftDb[cell] * cellGain;
        OutRightDb[cell] = RunningRightDb[cell] * (1.0f - cellGain) + LayerRightDb[cell] * cellGain;
        float2 baseZ = float2(cos(RunningPhase[cell]), sin(RunningPhase[cell]));
        float2 overlayZ = float2(cos(LayerPhase[cell]), sin(LayerPhase[cell]));
        float2 resultZ = (1.0f - cellGain) * baseZ + cellGain * overlayZ;
        OutPhase[cell] = (length(resultZ) > kMinComplexMagnitude) ? atan2(resultZ.y, resultZ.x) : LayerPhase[cell];
        return;
    }

    // Multiply/Screen/Overlay/Difference/Add.
    float baseLeftUnit = dbToUnit(RunningLeftDb[cell]);
    float baseRightUnit = dbToUnit(RunningRightDb[cell]);
    float overlayLeftUnit = dbToUnit(LayerLeftDb[cell]);
    float overlayRightUnit = dbToUnit(LayerRightDb[cell]);

    float blendedLeftUnit = clamp(BlendAmplitudeUnit(BlendMode, baseLeftUnit, overlayLeftUnit), 0.0f, 1.0f);
    float blendedRightUnit = clamp(BlendAmplitudeUnit(BlendMode, baseRightUnit, overlayRightUnit), 0.0f, 1.0f);

    float finalLeftUnit = baseLeftUnit * (1.0f - cellGain) + blendedLeftUnit * cellGain;
    float finalRightUnit = baseRightUnit * (1.0f - cellGain) + blendedRightUnit * cellGain;

    float baseAvgUnit = (baseLeftUnit + baseRightUnit) * 0.5f;
    float overlayAvgUnit = (overlayLeftUnit + overlayRightUnit) * 0.5f;
    float outUnit = (finalLeftUnit + finalRightUnit) * 0.5f;
    float2 baseZ = baseAvgUnit * float2(cos(RunningPhase[cell]), sin(RunningPhase[cell]));
    float2 overlayZ = overlayAvgUnit * float2(cos(LayerPhase[cell]), sin(LayerPhase[cell]));

    float2 blendedZ =
        BlendPhaseComplex(BlendMode, baseZ, overlayZ, RunningPhase[cell], LayerPhase[cell], baseAvgUnit, outUnit);
    float2 resultZ = (1.0f - cellGain) * baseZ + cellGain * blendedZ;
    OutPhase[cell] = (length(resultZ) > kMinComplexMagnitude) ? atan2(resultZ.y, resultZ.x) : RunningPhase[cell];

    OutLeftDb[cell] = unitToDb(finalLeftUnit);
    OutRightDb[cell] = unitToDb(finalRightUnit);
}
