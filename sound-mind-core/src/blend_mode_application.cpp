#include "sound_mind/core/blend_mode_application.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace sound_mind::core {

namespace {

// The same `-96..0` dB display range every other per-cell dB<->unit
// conversion in this codebase already establishes (`filter_application.cpp`'s
// own `dbToUnit()`/`unitToDb()`, in particular) - duplicated here rather
// than shared, the same "a two-line formula isn't worth a shared header
// over" reasoning `docs/sound-mind-architecture.md`'s Decision #59 already
// gives for this exact pair.
constexpr float kMinDb = -96.0f;
constexpr float kMaxDb = 0.0f;

float dbToUnit(float db) {
    const float clamped = std::clamp(db, kMinDb, kMaxDb);
    return (clamped - kMinDb) / (kMaxDb - kMinDb);
}

float unitToDb(float unit) { return kMinDb + unit * (kMaxDb - kMinDb); }

// `compositor.cpp`'s/`filter_application.cpp`'s own private linear-
// amplitude conversion pair, duplicated here for the same reason -
// `Normal`'s own audio-style mixing needs genuine linear amplitude, a
// different domain than every other mode's own `dbToUnit()` space.
constexpr float kMinLinearAmplitude = 1e-7f;

float dbToLinearAmplitude(float db) { return std::pow(10.0f, db / 20.0f); }

float linearAmplitudeToDb(float amplitude) { return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude)); }

/// @brief The magnitude negligibility threshold below which a complex
/// blend result's own phase is meaningless (both inputs are effectively
/// silent) - matching legacy's own `1e-30` epsilon in spirit (a value far
/// below any real signal, comfortably above float rounding noise for the
/// `[0, 1]`-normalized magnitudes used here).
constexpr float kMinComplexMagnitude = 1e-6f;

/// @brief One mode's own amplitude formula, in `dbToUnit()`-normalized
/// space - see `applyBlendedCell()`'s own docs. Never called for `Normal`/
/// `Overwrite`, which have no per-channel amplitude formula of this shape.
float blendAmplitudeUnit(BlendMode mode, float base, float overlay) {
    switch (mode) {
        case BlendMode::Multiply:
            return base * overlay;
        case BlendMode::Screen:
            return 1.0f - (1.0f - base) * (1.0f - overlay);
        case BlendMode::Overlay:
            return (base < 0.5f) ? (2.0f * base * overlay) : (1.0f - 2.0f * (1.0f - base) * (1.0f - overlay));
        case BlendMode::Difference:
            return std::abs(base - overlay);
        case BlendMode::Add:
            return base + overlay;
        case BlendMode::Normal:
        case BlendMode::Overwrite:
            break;
    }
    return overlay;  // Unreachable for the five modes above; a defensive fallback.
}

/// @brief One mode's own phase formula - see `applyBlendedCell()`'s own
/// docs for the exact per-mode complex math, ported from legacy.
/// `outUnit` is the already-computed, already-opacity-blended output
/// amplitude (the average of the final left/right unit values) - used as
/// the magnitude for `Multiply`/`Screen`/`Overlay`'s own result (matching
/// legacy's own `r_out` reference); `Difference`/`Add` ignore it, using
/// their own raw complex sum/difference instead, matching legacy's own
/// decoupled amplitude/phase treatment for those two.
std::complex<float> blendPhaseComplex(BlendMode mode, std::complex<float> baseZ, std::complex<float> overlayZ,
                                       float basePhase, float overlayPhase, float baseUnit, float outUnit) {
    switch (mode) {
        case BlendMode::Multiply:
            return outUnit * std::complex<float>(std::cos(basePhase + overlayPhase), std::sin(basePhase + overlayPhase));
        case BlendMode::Screen: {
            const std::complex<float> sum = baseZ + overlayZ;
            const float phi = (std::abs(sum) > kMinComplexMagnitude) ? std::arg(sum) : basePhase;
            return outUnit * std::complex<float>(std::cos(phi), std::sin(phi));
        }
        case BlendMode::Overlay: {
            float phi;
            if (baseUnit < 0.5f) {
                phi = basePhase + overlayPhase;
            } else {
                const std::complex<float> sum = baseZ + overlayZ;
                phi = (std::abs(sum) > kMinComplexMagnitude) ? std::arg(sum) : basePhase;
            }
            return outUnit * std::complex<float>(std::cos(phi), std::sin(phi));
        }
        case BlendMode::Difference:
            return baseZ - overlayZ;
        case BlendMode::Add:
            return baseZ + overlayZ;
        case BlendMode::Normal:
        case BlendMode::Overwrite:
            break;
    }
    return overlayZ;  // Unreachable for the five modes above; a defensive fallback.
}

}  // namespace

BlendedCell applyBlendedCell(BlendMode mode, BlendedCell base, BlendedCell overlay, float opacity) {
    if (mode == BlendMode::Overwrite) {
        return overlay;
    }
    if (mode == BlendMode::Normal) {
        // This codebase's own pre-v0.Y.37.1 audio-style mixing, unchanged -
        // see applyBlendedCell()'s own docs on why this stays in linear-
        // amplitude space rather than the dbToUnit() space every other
        // mode uses.
        const float gain = opacity;
        const std::complex<float> baseLeft =
            dbToLinearAmplitude(base.leftMagnitudeDb) * std::complex<float>(std::cos(base.phaseRadians), std::sin(base.phaseRadians));
        const std::complex<float> baseRight =
            dbToLinearAmplitude(base.rightMagnitudeDb) * std::complex<float>(std::cos(base.phaseRadians), std::sin(base.phaseRadians));
        const std::complex<float> overlayLeft =
            dbToLinearAmplitude(overlay.leftMagnitudeDb) * gain *
            std::complex<float>(std::cos(overlay.phaseRadians), std::sin(overlay.phaseRadians));
        const std::complex<float> overlayRight =
            dbToLinearAmplitude(overlay.rightMagnitudeDb) * gain *
            std::complex<float>(std::cos(overlay.phaseRadians), std::sin(overlay.phaseRadians));

        const std::complex<float> newLeft = baseLeft + overlayLeft;
        const std::complex<float> newRight = baseRight + overlayRight;
        const std::complex<float> mid = (newLeft + newRight) / 2.0f;
        return BlendedCell{linearAmplitudeToDb(std::abs(newLeft)), linearAmplitudeToDb(std::abs(newRight)),
                            (std::abs(mid) > 0.0f) ? std::arg(mid) : 0.0f};
    }

    // Multiply/Screen/Overlay/Difference/Add - dbToUnit()-normalized space,
    // per-channel amplitude, then a final opacity crossfade - see
    // applyBlendedCell()'s own docs.
    const float baseLeftUnit = dbToUnit(base.leftMagnitudeDb);
    const float baseRightUnit = dbToUnit(base.rightMagnitudeDb);
    const float overlayLeftUnit = dbToUnit(overlay.leftMagnitudeDb);
    const float overlayRightUnit = dbToUnit(overlay.rightMagnitudeDb);

    const float blendedLeftUnit = std::clamp(blendAmplitudeUnit(mode, baseLeftUnit, overlayLeftUnit), 0.0f, 1.0f);
    const float blendedRightUnit = std::clamp(blendAmplitudeUnit(mode, baseRightUnit, overlayRightUnit), 0.0f, 1.0f);

    const float finalLeftUnit = baseLeftUnit * (1.0f - opacity) + blendedLeftUnit * opacity;
    const float finalRightUnit = baseRightUnit * (1.0f - opacity) + blendedRightUnit * opacity;

    // Phase: (left + right) / 2's own unit magnitude as each side's
    // complex weight - this codebase's own existing Normal-mode phase
    // derivation's own convention, not legacy's "always the first
    // amplitude channel" one - see applyBlendedCell()'s own docs.
    const float baseAvgUnit = (baseLeftUnit + baseRightUnit) / 2.0f;
    const float overlayAvgUnit = (overlayLeftUnit + overlayRightUnit) / 2.0f;
    const float outUnit = (finalLeftUnit + finalRightUnit) / 2.0f;
    const std::complex<float> baseZ =
        baseAvgUnit * std::complex<float>(std::cos(base.phaseRadians), std::sin(base.phaseRadians));
    const std::complex<float> overlayZ =
        overlayAvgUnit * std::complex<float>(std::cos(overlay.phaseRadians), std::sin(overlay.phaseRadians));

    const std::complex<float> blendedZ =
        blendPhaseComplex(mode, baseZ, overlayZ, base.phaseRadians, overlay.phaseRadians, baseAvgUnit, outUnit);
    const std::complex<float> resultZ = (1.0f - opacity) * baseZ + opacity * blendedZ;
    const float resultPhase = (std::abs(resultZ) > kMinComplexMagnitude) ? std::arg(resultZ) : base.phaseRadians;

    return BlendedCell{unitToDb(finalLeftUnit), unitToDb(finalRightUnit), resultPhase};
}

}  // namespace sound_mind::core
