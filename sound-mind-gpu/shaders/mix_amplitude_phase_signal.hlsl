// Mixes one already-placed layer's own contribution into a running
// composite, per cell - see
// sound_mind::gpu::ComputeDevice::mixAmplitudePhaseSignal()'s own docs
// for the exact contract (and why placement/rescale isn't part of this
// shader). The same per-cell math sound_mind::core's own (CPU)
// mixLayerInto() (compositor.cpp) implements: each channel's own dB
// value converts to linear, layer's own converted amplitude scales by
// Gain, each channel becomes a complex value (as a float2: x=real,
// y=imaginary) using its own signal's shared phase, the two complex
// values sum, and the result converts back to dB/phase - phase as the
// angle of the summed *mid* signal, (left + right) / 2.
#define ComputeRootSignature \
    "RootConstants(num32BitConstants=2, b0), " \
    "SRV(t0), SRV(t1), SRV(t2), " \
    "SRV(t3), SRV(t4), SRV(t5), " \
    "UAV(u0), UAV(u1), UAV(u2)"

cbuffer Constants : register(b0) {
    uint CellCount;
    float Gain;  // layer's own opacity, as a linear gain.
};

StructuredBuffer<float> RunningLeftDb : register(t0);
StructuredBuffer<float> RunningRightDb : register(t1);
StructuredBuffer<float> RunningPhase : register(t2);
StructuredBuffer<float> LayerLeftDb : register(t3);
StructuredBuffer<float> LayerRightDb : register(t4);
StructuredBuffer<float> LayerPhase : register(t5);
RWStructuredBuffer<float> OutLeftDb : register(u0);
RWStructuredBuffer<float> OutRightDb : register(u1);
RWStructuredBuffer<float> OutPhase : register(u2);

// The smallest linear amplitude that maps to anything but this floor's
// own dB value - avoids log10(0) for genuine silence. The same value
// sound_mind::core's own (CPU) kMinLinearAmplitude (compositor.cpp) uses.
static const float kMinLinearAmplitude = 1e-7f;

float dbToLinear(float db) {
    return pow(10.0f, db / 20.0f);
}

float linearToDb(float amplitude) {
    return 20.0f * log10(max(amplitude, kMinLinearAmplitude));
}

[RootSignature(ComputeRootSignature)]
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= CellCount) {
        return;
    }
    uint cell = id.x;

    float layerLeftLinear = dbToLinear(LayerLeftDb[cell]) * Gain;
    float layerRightLinear = dbToLinear(LayerRightDb[cell]) * Gain;
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
}
