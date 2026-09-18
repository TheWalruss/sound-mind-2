#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedMindWave` within a Project - a
/// deliberate, exact duplicate of `mind_wave.h`'s own `MindWaveId` alias
/// (matching `docs/sound-mind-architecture.md`'s own Decision #59
/// "duplicated, not shared" precedent, and `layer.h`'s own identical
/// duplication for the same reason - Decision #80): including
/// `mind_wave.h` here would cycle back through `path.h` -> `operation.h`
/// -> `layer.h` -> this header. A type alias can be redeclared identically
/// in multiple headers with no ODR concern.
using MindWaveId = std::uint64_t;

/**
 * @brief Which filter algorithm a `FilterConfiguration` configures - see
 *        `docs/sound-mind-design.md`'s "Filter Layer", "Blur & focus"
 *        family plus "Tonal"/"Spectral shaping".
 *
 * @note This milestone's own confirmed scope (`docs/sound-mind-roadmap.md`'s
 *       `v0.Y.28.1`): all three "Blur & focus" variants, Sharpen, a Tone
 *       Curve, and Frequency-Axis Gradient (the Equalizer layer's own
 *       basis). `v0.Y.36.1` (Deferred Filters) Installment A added the
 *       full "Noise & distortion" family below - the design doc's
 *       remaining families (Geometric, the rest of Tonal/Spectral
 *       shaping, Space) aren't represented here yet, left for later
 *       installments of that same milestone.
 */
enum class FilterType {
    /// @brief Isotropic softening - Gaussian blur, per `blurSigma()`.
    UniformBlur,
    /// @brief Softening that preserves sharp edges rather than blurring
    ///        across them - a median filter, per `medianSize()`.
    EdgePreservingBlur,
    /// @brief Softening along one direction only - a motion-blur kernel,
    ///        per `directionalBlurLength()`/`directionalBlurAngleDegrees()`.
    DirectionalBlur,
    /// @brief Sharpening (blur's own opposite) - an unsharp mask, per
    ///        `sharpenAmount()`.
    Sharpen,
    /// @brief A tone curve remapping loudness through a user-drawn curve,
    ///        per `toneCurvePoints()`.
    ToneCurve,
    /// @brief A gradient controlling loudness across the frequency axis -
    ///        the Equalizer layer's own basis, per `frequencyGradient()`.
    FrequencyAxisGradient,
    /// @brief Randomly boosts a fraction of cells toward full loudness -
    ///        deterministic per `noiseSeed()`/cell position, per
    ///        `speckleDensity()`/`speckleIntensity()`.
    SpeckleAdd,
    /// @brief Speckle Add's own inverse - replaces a cell with its local
    ///        median only where it stands out as an outlier, per
    ///        `speckleThresholdDb()`.
    SpeckleRemove,
    /// @brief A per-cell downward expander/spectral gate - attenuates
    ///        anything below `noiseFloorDb()` by up to `reductionDb()`.
    Denoise,
    /// @brief Quantizes loudness into a limited number of discrete steps -
    ///        a bitcrush-style effect, per `crushAmount()`.
    BitDepthCrush,
    /// @brief Overlays a coarse, block-based random texture - deterministic
    ///        per `noiseSeed()`/block position, per `grainSize()`/
    ///        `grainAmountDb()`.
    GranularNoise,
    /// @brief Speckle Add's own live-noise sibling - the same
    ///        `speckleDensity()`/`speckleIntensity()` knobs, but computed
    ///        over fixed 2x2 blocks with genuinely fresh randomness on
    ///        every recomposite, not `noiseSeed()`-deterministic.
    DynamicSpeckle,
    /// @brief A resonant, decaying smear along the time axis - a one-pole
    ///        recursive filter per bin, per `feedbackAmount()`.
    FeedbackDistortion,
    /// @brief A classic wavefolder - loudness beyond a threshold reflects
    ///        back into range rather than clipping, per `foldGain()`.
    SpectralWavefold,
    /// @brief Redistributes loudness between the left and right channels -
    ///        an energy-conserving pan law, per `channelBalance()`.
    ChannelBalance,
    /// @brief Inverts loudness (`out = 1 - dbToUnit(in)`) - quiet becomes
    ///        loud and loud becomes quiet. No parameters of its own.
    Invert,
    /// @brief An arbitrary, user-edited 2D convolution kernel over the
    ///        spectrogram, per `convolveKernel()`/`convolveKernelSize()`/
    ///        `convolveNormalize()`/`convolveAmount()`.
    Convolve,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(FilterType, {
    {FilterType::UniformBlur, "uniformBlur"},
    {FilterType::EdgePreservingBlur, "edgePreservingBlur"},
    {FilterType::DirectionalBlur, "directionalBlur"},
    {FilterType::Sharpen, "sharpen"},
    {FilterType::ToneCurve, "toneCurve"},
    {FilterType::FrequencyAxisGradient, "frequencyAxisGradient"},
    {FilterType::SpeckleAdd, "speckleAdd"},
    {FilterType::SpeckleRemove, "speckleRemove"},
    {FilterType::Denoise, "denoise"},
    {FilterType::BitDepthCrush, "bitDepthCrush"},
    {FilterType::GranularNoise, "granularNoise"},
    {FilterType::DynamicSpeckle, "dynamicSpeckle"},
    {FilterType::FeedbackDistortion, "feedbackDistortion"},
    {FilterType::SpectralWavefold, "spectralWavefold"},
    {FilterType::ChannelBalance, "channelBalance"},
    {FilterType::Invert, "invert"},
    {FilterType::Convolve, "convolve"},
})
// clang-format on

/**
 * @brief A Filter layer's own filter type and parameters - see
 *        `docs/sound-mind-design.md`'s "Filter Layer" and
 *        `sound_mind::core::Layer::filterConfiguration()`.
 *
 * @note Deliberately flat fields for every filter type, not a tagged
 *       union/`std::variant` - the same reasoning `docs/sound-mind-
 *       architecture.md`'s Decision #34 already gives for
 *       `ToolConfiguration` (plain typed fields are simpler and safer
 *       than an opaque bag, or a discriminated union, while the total
 *       field count stays small): six real filter types now coexist
 *       here (not the single-real-type case Decision #34 was written
 *       against), but each contributes only one or two scalar fields (or
 *       one `Gradient`), so a flat struct - only some of it meaningful
 *       depending on `type()` - stays simpler than the alternative.
 *
 * As of `v0.Y.31.1` (MindWaves v1) Installment D, each of the five
 * single-scalar parameters (`blurSigma`, `medianSize`,
 * `directionalBlurLength`, `directionalBlurAngleDegrees`,
 * `sharpenAmount`) can additionally bind to a MindWave - see each one's
 * own `*MindWave()` accessor and `docs/sound-mind-design.md`'s "Filter
 * parameters". `toneCurvePoints`/`frequencyGradient` have no such binding:
 * neither is a single number, so there's nothing for a MindWave's own
 * `[0, 1]` output to become the *value* of. **The eight `v0.Y.36.1`
 * Installment A "Noise & distortion" parameters, plus Installment B's own
 * `channelBalance`/`convolveKernel`-family fields below, all have no
 * MindWave binding either, deliberately** - matching how the original six
 * filter types shipped unbound in `v0.Y.28.1` and only gained binding in a
 * later, dedicated milestone (`v0.Y.31.1` Installment D); binding these is
 * left the same way, a known future-work item, not attempted here.
 */
class FilterConfiguration {
public:
    /// @brief Constructs a Frequency-Axis Gradient configuration with a
    ///        fresh, fully transparent default gradient (see `Gradient`'s
    ///        own docs) - a fresh Filter layer has no audible effect
    ///        until its own parameters are deliberately set, the same
    ///        "nothing happens by accident" default `ToolConfiguration`'s
    ///        own gradient already establishes for painting. Also seeds
    ///        `noiseSeed()` fresh via `std::random_device` - see its own
    ///        docs for why every configuration needs its own seed.
    FilterConfiguration() : noiseSeed_(std::random_device{}()) {}

    /// @brief Which filter algorithm this configures.
    /// @return The currently configured filter type.
    [[nodiscard]] FilterType type() const noexcept { return type_; }

    /// @brief Sets which filter algorithm this configures.
    /// @param type The new filter type.
    void setType(FilterType type) noexcept { type_ = type; }

    /**
     * @brief `UniformBlur`'s own softening radius - the Gaussian kernel's
     *        standard deviation, in bins/columns (this codebase's
     *        existing DSP already works in raw bin/column units for
     *        per-cell operations, unlike a brush's own resolution-
     *        agnostic seconds-equivalent size - a spectrogram-domain
     *        blur radius has no meaningful cross-project unit to be
     *        agnostic *about*).
     * @return The current sigma; meaningless unless `type()` is
     *         `UniformBlur`. Not clamped or validated here.
     */
    [[nodiscard]] float blurSigma() const noexcept { return blurSigma_; }

    /// @brief Sets `UniformBlur`'s own softening radius.
    /// @param sigma The new sigma, in bins/columns; intended to be positive.
    void setBlurSigma(float sigma) noexcept { blurSigma_ = sigma; }

    /**
     * @brief The MindWave (if any) `blurSigma()` is bound to - see
     *        `docs/sound-mind-design.md`'s "Filter parameters": a bound
     *        kernel-shape parameter genuinely varies per cell, evaluated
     *        at that cell's own canvas position, with `blurSigma()` as
     *        the ceiling it reaches where the wave is brightest (falling
     *        toward `0` - no blur - where it's dark), not a blend of two
     *        whole-image results.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `blurSigma()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> blurSigmaMindWave() const noexcept { return blurSigmaMindWave_; }

    /// @brief Sets (or clears) which MindWave `blurSigma()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setBlurSigmaMindWave(std::optional<MindWaveId> mindWaveId) noexcept { blurSigmaMindWave_ = mindWaveId; }

    /**
     * @brief `EdgePreservingBlur`'s own median-filter window size, in
     *        bins/columns.
     * @return The current size; meaningless unless `type()` is
     *         `EdgePreservingBlur`. Not clamped or validated here, but
     *         an even value behaves as if rounded up to the next odd one
     *         (see `applyFilter()`'s own docs in `filter_application.h`).
     */
    [[nodiscard]] int medianSize() const noexcept { return medianSize_; }

    /// @brief Sets `EdgePreservingBlur`'s own median-filter window size.
    /// @param size The new size, in bins/columns; intended to be a
    ///        positive odd number of at least 3.
    void setMedianSize(int size) noexcept { medianSize_ = size; }

    /**
     * @brief The MindWave (if any) `medianSize()` is bound to - see
     *        `blurSigmaMindWave()`'s own docs for the general mechanism.
     *        Falls toward `1` (a single-cell window - identity, no
     *        filtering at all) where the wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `medianSize()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> medianSizeMindWave() const noexcept { return medianSizeMindWave_; }

    /// @brief Sets (or clears) which MindWave `medianSize()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setMedianSizeMindWave(std::optional<MindWaveId> mindWaveId) noexcept { medianSizeMindWave_ = mindWaveId; }

    /**
     * @brief `DirectionalBlur`'s own kernel length, in bins/columns.
     * @return The current length; meaningless unless `type()` is
     *         `DirectionalBlur`. Not clamped or validated here.
     */
    [[nodiscard]] int directionalBlurLength() const noexcept { return directionalBlurLength_; }

    /// @brief Sets `DirectionalBlur`'s own kernel length.
    /// @param length The new length, in bins/columns; intended to be positive.
    void setDirectionalBlurLength(int length) noexcept { directionalBlurLength_ = length; }

    /**
     * @brief The MindWave (if any) `directionalBlurLength()` is bound to -
     *        see `blurSigmaMindWave()`'s own docs for the general
     *        mechanism. Falls toward `0` (no blur - identity) where the
     *        wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `directionalBlurLength()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> directionalBlurLengthMindWave() const noexcept {
        return directionalBlurLengthMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `directionalBlurLength()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setDirectionalBlurLengthMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        directionalBlurLengthMindWave_ = mindWaveId;
    }

    /**
     * @brief `DirectionalBlur`'s own direction, in degrees - `0`° blurs
     *        along the time axis (columns), `90`° along the frequency
     *        axis (rows), matching standard math convention (see
     *        `applyFilter()`'s own docs for the exact kernel).
     * @return The current angle; meaningless unless `type()` is
     *         `DirectionalBlur`. Not clamped or validated here.
     */
    [[nodiscard]] float directionalBlurAngleDegrees() const noexcept { return directionalBlurAngleDegrees_; }

    /// @brief Sets `DirectionalBlur`'s own direction.
    /// @param degrees The new angle, in degrees.
    void setDirectionalBlurAngleDegrees(float degrees) noexcept { directionalBlurAngleDegrees_ = degrees; }

    /**
     * @brief The MindWave (if any) `directionalBlurAngleDegrees()` is
     *        bound to - see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Unlike the other four bindable
     *        parameters, an angle has no "no effect" value to fall toward
     *        (every angle blurs *some* direction) - it falls toward `0`
     *        degrees where the wave is dark, a natural default rather
     *        than a claimed no-op.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `directionalBlurAngleDegrees()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> directionalBlurAngleMindWave() const noexcept {
        return directionalBlurAngleMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `directionalBlurAngleDegrees()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setDirectionalBlurAngleMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        directionalBlurAngleMindWave_ = mindWaveId;
    }

    /**
     * @brief `Sharpen`'s own strength - an unsharp-mask amount, where
     *        `1.0` is a typical/moderate sharpen and higher values push
     *        further.
     * @return The current amount; meaningless unless `type()` is
     *         `Sharpen`. Not clamped or validated here.
     */
    [[nodiscard]] float sharpenAmount() const noexcept { return sharpenAmount_; }

    /// @brief Sets `Sharpen`'s own strength.
    /// @param amount The new amount; intended to be positive.
    void setSharpenAmount(float amount) noexcept { sharpenAmount_ = amount; }

    /**
     * @brief The MindWave (if any) `sharpenAmount()` is bound to - see
     *        `blurSigmaMindWave()`'s own docs for the general mechanism.
     *        Falls toward `0` (no sharpening - identity) where the wave
     *        is dark. Unlike the other four bindable parameters, this one
     *        varies per cell for free: `sharpenAmount()` only scales an
     *        already-fixed difference term (see `applyFilter()`'s own
     *        docs), so binding it needs no new per-cell kernel work.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `sharpenAmount()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> sharpenAmountMindWave() const noexcept { return sharpenAmountMindWave_; }

    /// @brief Sets (or clears) which MindWave `sharpenAmount()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setSharpenAmountMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        sharpenAmountMindWave_ = mindWaveId;
    }

    /**
     * @brief `ToneCurve`'s own control points, each `{input, output}` in
     *        `[0, 1]` (normalized against this codebase's own `-96..0`
     *        dB display range, the same range `color_mapping.cpp`'s
     *        `dbToByte()`/`byteToDb()` already establish) - ordered by
     *        input position, always at least the two endpoints `{0, 0}`
     *        and `{1, 1}` (the identity curve - no effect).
     * @return The current control points; meaningless unless `type()` is
     *         `ToneCurve`. Not validated here (an out-of-order or
     *         out-of-range list is the caller's own responsibility).
     */
    [[nodiscard]] const std::vector<std::array<float, 2>>& toneCurvePoints() const noexcept {
        return toneCurvePoints_;
    }

    /// @brief Mutable access to `ToneCurve`'s own control points, for
    ///        in-place edits.
    /// @return The current control points.
    [[nodiscard]] std::vector<std::array<float, 2>>& toneCurvePoints() noexcept { return toneCurvePoints_; }

    /// @brief Sets `ToneCurve`'s own control points wholesale.
    /// @param points The new control points - see toneCurvePoints()'s own docs.
    void setToneCurvePoints(std::vector<std::array<float, 2>> points) { toneCurvePoints_ = std::move(points); }

    /**
     * @brief `FrequencyAxisGradient`'s own gradient, evaluated across the
     *        frequency axis (`t=0` at the lowest encoded frequency,
     *        `t=1` at the highest) exactly the way a Path's own gradient
     *        evaluates along its length - see `Gradient`'s own docs and
     *        `applyFilter()`'s own docs in `filter_application.h` for
     *        the exact per-bin blend.
     * @return The current gradient; meaningless unless `type()` is
     *         `FrequencyAxisGradient`.
     */
    [[nodiscard]] const Gradient& frequencyGradient() const noexcept { return frequencyGradient_; }

    /// @brief Mutable access to `FrequencyAxisGradient`'s own gradient,
    ///        for in-place edits.
    /// @return The current gradient.
    [[nodiscard]] Gradient& frequencyGradient() noexcept { return frequencyGradient_; }

    /**
     * @brief The seed `SpeckleAdd`/`GranularNoise` hash against cell
     *        position for their own deterministic noise - see
     *        `applyFilter()`'s own docs. Generated fresh (via
     *        `std::random_device`) whenever a `FilterConfiguration` is
     *        default-constructed, then persisted like any other field, so
     *        a given Filter layer's own noise pattern stays stable across
     *        every recomposite (any edit, scroll, or repaint) and reload,
     *        while a *different* Filter layer (or the same one recreated)
     *        doesn't look identical to it. `DynamicSpeckle` deliberately
     *        ignores this - see its own docs.
     * @return The current seed.
     */
    [[nodiscard]] std::uint32_t noiseSeed() const noexcept { return noiseSeed_; }

    /// @brief Sets `noiseSeed()` explicitly - mainly for deterministic
    ///        testing; a fresh `FilterConfiguration` already seeds itself
    ///        randomly, so there's no ordinary-use reason to call this.
    /// @param seed The new seed.
    void setNoiseSeed(std::uint32_t seed) noexcept { noiseSeed_ = seed; }

    /**
     * @brief `SpeckleAdd`/`DynamicSpeckle`'s own shared density - the
     *        fraction of cells (`SpeckleAdd`) or 2x2 blocks
     *        (`DynamicSpeckle`) that get hit on a given application, in
     *        `[0, 1]`.
     * @return The current density; meaningless unless `type()` is
     *         `SpeckleAdd` or `DynamicSpeckle`. Not clamped here.
     */
    [[nodiscard]] float speckleDensity() const noexcept { return speckleDensity_; }

    /// @brief Sets `SpeckleAdd`/`DynamicSpeckle`'s own shared density.
    /// @param density The new density, intended within `[0, 1]`.
    void setSpeckleDensity(float density) noexcept { speckleDensity_ = density; }

    /**
     * @brief `SpeckleAdd`/`DynamicSpeckle`'s own shared intensity - how far
     *        a hit cell blends toward full loudness (`0`dB), in `[0, 1]`
     *        (`0` = no change even when hit, `1` = jumps all the way to
     *        `0`dB).
     * @return The current intensity; meaningless unless `type()` is
     *         `SpeckleAdd` or `DynamicSpeckle`. Not clamped here.
     */
    [[nodiscard]] float speckleIntensity() const noexcept { return speckleIntensity_; }

    /// @brief Sets `SpeckleAdd`/`DynamicSpeckle`'s own shared intensity.
    /// @param intensity The new intensity, intended within `[0, 1]`.
    void setSpeckleIntensity(float intensity) noexcept { speckleIntensity_ = intensity; }

    /**
     * @brief `SpeckleRemove`'s own outlier threshold, in dB - a cell more
     *        than this many dB louder or quieter than its own local
     *        (3x3) median is replaced by that median; everything else is
     *        left untouched.
     * @return The current threshold; meaningless unless `type()` is
     *         `SpeckleRemove`. Not clamped here.
     */
    [[nodiscard]] float speckleThresholdDb() const noexcept { return speckleThresholdDb_; }

    /// @brief Sets `SpeckleRemove`'s own outlier threshold.
    /// @param thresholdDb The new threshold, in dB; intended to be positive.
    void setSpeckleThresholdDb(float thresholdDb) noexcept { speckleThresholdDb_ = thresholdDb; }

    /**
     * @brief `Denoise`'s own noise-floor threshold, in dB - cells at or
     *        below this loudness (with a soft knee, not a hard cutoff)
     *        are attenuated by up to `reductionDb()`.
     * @return The current floor; meaningless unless `type()` is `Denoise`.
     *         Not clamped here.
     */
    [[nodiscard]] float noiseFloorDb() const noexcept { return noiseFloorDb_; }

    /// @brief Sets `Denoise`'s own noise-floor threshold.
    /// @param floorDb The new floor, in dB.
    void setNoiseFloorDb(float floorDb) noexcept { noiseFloorDb_ = floorDb; }

    /**
     * @brief `Denoise`'s own maximum attenuation, in dB - applied in full
     *        to a cell well below `noiseFloorDb()`, ramping to none at
     *        the floor's own soft-knee edge.
     * @return The current reduction; meaningless unless `type()` is
     *         `Denoise`. Not clamped here.
     */
    [[nodiscard]] float reductionDb() const noexcept { return reductionDb_; }

    /// @brief Sets `Denoise`'s own maximum attenuation.
    /// @param reductionDb The new reduction, in dB; intended to be positive.
    void setReductionDb(float reductionDb) noexcept { reductionDb_ = reductionDb; }

    /**
     * @brief `BitDepthCrush`'s own strength, in `[0, 1]` - `0` is a true
     *        no-op (identity, no quantization at all); `1` quantizes down
     *        to a harsh, roughly 2-level loudness range.
     * @return The current amount; meaningless unless `type()` is
     *         `BitDepthCrush`. Not clamped here.
     */
    [[nodiscard]] float crushAmount() const noexcept { return crushAmount_; }

    /// @brief Sets `BitDepthCrush`'s own strength.
    /// @param amount The new amount, intended within `[0, 1]`.
    void setCrushAmount(float amount) noexcept { crushAmount_ = amount; }

    /**
     * @brief `GranularNoise`'s own block size, in bins/columns - the
     *        composite is divided into `grainSize()` x `grainSize()`
     *        blocks, each getting one random dB offset applied uniformly
     *        across every cell in it.
     * @return The current size; meaningless unless `type()` is
     *         `GranularNoise`. Not clamped here.
     */
    [[nodiscard]] int grainSize() const noexcept { return grainSize_; }

    /// @brief Sets `GranularNoise`'s own block size.
    /// @param size The new size, in bins/columns; intended to be positive.
    void setGrainSize(int size) noexcept { grainSize_ = size; }

    /**
     * @brief `GranularNoise`'s own per-block offset range, in dB - each
     *        block's own random offset falls within `[-grainAmountDb(),
     *        +grainAmountDb()]`.
     * @return The current amount; meaningless unless `type()` is
     *         `GranularNoise`. Not clamped here.
     */
    [[nodiscard]] float grainAmountDb() const noexcept { return grainAmountDb_; }

    /// @brief Sets `GranularNoise`'s own per-block offset range.
    /// @param amountDb The new amount, in dB; intended to be positive.
    void setGrainAmountDb(float amountDb) noexcept { grainAmountDb_ = amountDb; }

    /**
     * @brief `FeedbackDistortion`'s own resonance amount, in `[0, 1)` - a
     *        one-pole recursive filter along the time axis per bin
     *        (`y[frame] = (1 - amount) * x[frame] + amount * y[frame -
     *        1]`); `0` is a true no-op (identity), values approaching `1`
     *        ring/smear for progressively longer. Internally clamped to
     *        `0.99` regardless of what's stored here, to guarantee
     *        stability.
     * @return The current amount; meaningless unless `type()` is
     *         `FeedbackDistortion`. Not clamped here.
     */
    [[nodiscard]] float feedbackAmount() const noexcept { return feedbackAmount_; }

    /// @brief Sets `FeedbackDistortion`'s own resonance amount.
    /// @param amount The new amount, intended within `[0, 1)`.
    void setFeedbackAmount(float amount) noexcept { feedbackAmount_ = amount; }

    /**
     * @brief `SpectralWavefold`'s own pre-fold gain - `1.0` is a true
     *        no-op (identity); higher values push loudness further past
     *        the fold threshold, producing progressively more folds (and
     *        harsher, more harmonically dense distortion).
     * @return The current gain; meaningless unless `type()` is
     *         `SpectralWavefold`. Not clamped here.
     */
    [[nodiscard]] float foldGain() const noexcept { return foldGain_; }

    /// @brief Sets `SpectralWavefold`'s own pre-fold gain.
    /// @param gain The new gain; intended to be at least `1.0`.
    void setFoldGain(float gain) noexcept { foldGain_ = gain; }

    /**
     * @brief `ChannelBalance`'s own balance, in `[0, 1]` - confirmed with
     *        the user against the legacy Python Studio's own
     *        `channel_balance()`, an energy-conserving pan law rather than
     *        independent per-channel gain: at each cell, `total = left +
     *        right`, then `left = total * (1 - balance)`, `right = total *
     *        balance`. `0.5` is a true no-op only on an already-balanced
     *        signal (`left == right` at every cell) - unlike every other
     *        filter's own "no-op" value, this one depends on the input,
     *        not just the parameter.
     * @return The current balance; meaningless unless `type()` is
     *         `ChannelBalance`. Not clamped here.
     */
    [[nodiscard]] float channelBalance() const noexcept { return channelBalance_; }

    /// @brief Sets `ChannelBalance`'s own balance.
    /// @param balance The new balance, intended within `[0, 1]`.
    void setChannelBalance(float balance) noexcept { channelBalance_ = balance; }

    /**
     * @brief `Convolve`'s own kernel, row-major, exactly
     *        `convolveKernelSize() * convolveKernelSize()` entries - see
     *        `NamedConvolutionKernel`'s own docs for the identical
     *        representation a saved library entry uses (loading one here
     *        is a one-time copy, not a live reference).
     * @return The current kernel; meaningless unless `type()` is
     *         `Convolve`. Not validated here.
     */
    [[nodiscard]] const std::vector<float>& convolveKernel() const noexcept { return convolveKernel_; }

    /// @brief Mutable access to `Convolve`'s own kernel, for in-place edits.
    /// @return The current kernel.
    [[nodiscard]] std::vector<float>& convolveKernel() noexcept { return convolveKernel_; }

    /// @brief Sets `Convolve`'s own kernel wholesale.
    /// @param kernel The new kernel - see convolveKernel()'s own docs.
    void setConvolveKernel(std::vector<float> kernel) { convolveKernel_ = std::move(kernel); }

    /**
     * @brief `Convolve`'s own kernel side length - always odd, so the
     *        kernel has a well-defined center cell (matching
     *        `NamedConvolutionKernel::size`'s own docs).
     * @return The current size; meaningless unless `type()` is `Convolve`.
     *         Not forced odd here - `applyFilter()`'s own docs cover how
     *         an even value is handled.
     */
    [[nodiscard]] int convolveKernelSize() const noexcept { return convolveKernelSize_; }

    /// @brief Sets `Convolve`'s own kernel side length.
    /// @param size The new size; intended to be a positive odd number.
    void setConvolveKernelSize(int size) noexcept { convolveKernelSize_ = size; }

    /**
     * @brief Whether `Convolve` divides its own kernel by the sum of its
     *        positive coefficients before applying it - see
     *        `applyFilter()`'s own docs for why (a pure-positive kernel
     *        would otherwise brighten/darken the whole image by that sum).
     * @return The current setting; meaningless unless `type()` is
     *         `Convolve`.
     */
    [[nodiscard]] bool convolveNormalize() const noexcept { return convolveNormalize_; }

    /// @brief Sets `Convolve`'s own normalize setting.
    /// @param normalize The new setting.
    void setConvolveNormalize(bool normalize) noexcept { convolveNormalize_ = normalize; }

    /**
     * @brief `Convolve`'s own dry/wet mix, in `[0, 1]` - `0` is a true
     *        no-op (identity, regardless of the kernel); `1` is the fully
     *        convolved result.
     * @return The current amount; meaningless unless `type()` is
     *         `Convolve`. Not clamped here.
     */
    [[nodiscard]] float convolveAmount() const noexcept { return convolveAmount_; }

    /// @brief Sets `Convolve`'s own dry/wet mix.
    /// @param amount The new amount, intended within `[0, 1]`.
    void setConvolveAmount(float amount) noexcept { convolveAmount_ = amount; }

    friend void to_json(nlohmann::json& json, const FilterConfiguration& config);
    friend void from_json(const nlohmann::json& json, FilterConfiguration& config);

private:
    FilterType type_ = FilterType::FrequencyAxisGradient;
    float blurSigma_ = 2.0f;
    std::optional<MindWaveId> blurSigmaMindWave_;
    int medianSize_ = 3;
    std::optional<MindWaveId> medianSizeMindWave_;
    int directionalBlurLength_ = 10;
    std::optional<MindWaveId> directionalBlurLengthMindWave_;
    float directionalBlurAngleDegrees_ = 0.0f;
    std::optional<MindWaveId> directionalBlurAngleMindWave_;
    float sharpenAmount_ = 1.0f;
    std::optional<MindWaveId> sharpenAmountMindWave_;
    std::vector<std::array<float, 2>> toneCurvePoints_{{0.0f, 0.0f}, {1.0f, 1.0f}};
    Gradient frequencyGradient_;
    std::uint32_t noiseSeed_;
    float speckleDensity_ = 0.05f;
    float speckleIntensity_ = 0.8f;
    float speckleThresholdDb_ = 12.0f;
    float noiseFloorDb_ = -60.0f;
    float reductionDb_ = 24.0f;
    float crushAmount_ = 0.5f;
    int grainSize_ = 4;
    float grainAmountDb_ = 6.0f;
    float feedbackAmount_ = 0.5f;
    float foldGain_ = 2.0f;
    float channelBalance_ = 0.5f;
    std::vector<float> convolveKernel_{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int convolveKernelSize_ = 3;
    bool convolveNormalize_ = false;
    float convolveAmount_ = 1.0f;
};

/// @brief Serializes a filter configuration to its JSON representation.
void to_json(nlohmann::json& json, const FilterConfiguration& config);

/// @brief Parses a filter configuration from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, FilterConfiguration& config);

}  // namespace sound_mind::core
