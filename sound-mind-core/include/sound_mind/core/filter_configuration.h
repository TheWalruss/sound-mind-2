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
    /// @brief Shifts content from a source position offset by
    ///        `displaceDistance()`/`displaceAngleDegrees()`, bilinearly
    ///        interpolated, clamp-to-edge at the boundary. Phase untouched.
    Displace,
    /// @brief Continuously rotates left loudness, right loudness, and
    ///        phase into each other (normalized to a shared `[0, 1]`
    ///        domain first), per `channelCycleAngleDegrees()`.
    ChannelCycle,
    /// @brief A spectral reverb - each frequency bin's own time series is
    ///        convolved with an exponentially-decaying impulse response
    ///        (in linear amplitude), after per-bin absorption and
    ///        cross-frequency diffusion (both in dB), then mixed with the
    ///        dry signal. Phase untouched. Per `reverbPreDelayFrames()`/
    ///        `reverbDecayFrames()`/`reverbRoomSize()`/
    ///        `reverbDiffusion()`/`reverbAbsorption()`/`reverbMix()`.
    SpectralReverb,
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
    {FilterType::Displace, "displace"},
    {FilterType::ChannelCycle, "channelCycle"},
    {FilterType::SpectralReverb, "spectralReverb"},
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
 * `[0, 1]` output to become the *value* of.
 *
 * As of `v0.Y.38.1` (Filter Parameter Binding Completion), fifteen more
 * parameters across every filter type `v0.Y.36.1` added can also bind to a
 * MindWave, each documented on its own `*MindWave()` accessor:
 * `speckleDensity`/`speckleIntensity` (shared), `speckleThresholdDb`,
 * `noiseFloorDb`/`reductionDb`, `crushAmount`, `grainAmountDb`,
 * `feedbackAmount`, `foldGain`, `channelBalance`, `convolveAmount`,
 * `displaceDistance`/`displaceAngleDegrees`, `channelCycleAngleDegrees`,
 * and `reverbMix`. **Three parameters remain deliberately unbound** -
 * `grainSize`, `convolveKernel`/`convolveKernelSize`/`convolveNormalize`,
 * and `reverbPreDelayFrames`/`reverbDecayFrames`/`reverbRoomSize`/
 * `reverbDiffusion`/`reverbAbsorption` - each shapes a fixed-size
 * structure or a computation spanning many cells at once (a kernel
 * matrix, a block partition, a temporal impulse response/frequency-axis
 * blur), not an independent per-cell value with a well-defined "this
 * cell's own value" the way every bindable parameter above has; see
 * `docs/sound-mind-roadmap.md`'s own `v0.Y.38.1` entry for the full
 * reasoning. This is a known, permanent limitation of the current
 * per-cell binding mechanism, not a deferred future-work item the way the
 * fifteen above were before this milestone.
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
     * @brief The MindWave (if any) `speckleDensity()` is bound to -
     *        `v0.Y.38.1` (Filter Parameter Binding Completion), see
     *        `blurSigmaMindWave()`'s own docs for the general mechanism.
     *        Falls toward `0` (no speckles at all) where the wave is dark.
     *        For `SpeckleAdd`, evaluated per cell (whether a given cell is
     *        "hit" is a hash compared against this cell's own density
     *        value); for `DynamicSpeckle`, evaluated once per its own 2x2
     *        block (at the block's own top-left cell), matching that
     *        filter's own block-uniform granularity - see
     *        `applyFilter()`'s own docs.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `speckleDensity()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> speckleDensityMindWave() const noexcept { return speckleDensityMindWave_; }

    /// @brief Sets (or clears) which MindWave `speckleDensity()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setSpeckleDensityMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        speckleDensityMindWave_ = mindWaveId;
    }

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

    /// @copydoc speckleDensityMindWave()
    [[nodiscard]] std::optional<MindWaveId> speckleIntensityMindWave() const noexcept {
        return speckleIntensityMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `speckleIntensity()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setSpeckleIntensityMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        speckleIntensityMindWave_ = mindWaveId;
    }

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
     * @brief The MindWave (if any) `speckleThresholdDb()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `96` dB (larger than any real
     *        difference within this codebase's own `-96..0` dB range, so
     *        no cell ever qualifies as an outlier - a true no-op) where
     *        the wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `speckleThresholdDb()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> speckleThresholdMindWave() const noexcept {
        return speckleThresholdMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `speckleThresholdDb()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setSpeckleThresholdMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        speckleThresholdMindWave_ = mindWaveId;
    }

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
     * @brief The MindWave (if any) `noiseFloorDb()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `-96` dB (the absolute
     *        silence floor - nothing real content ever falls below it, so
     *        no attenuation is ever triggered - a true no-op) where the
     *        wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `noiseFloorDb()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> noiseFloorMindWave() const noexcept { return noiseFloorMindWave_; }

    /// @brief Sets (or clears) which MindWave `noiseFloorDb()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setNoiseFloorMindWave(std::optional<MindWaveId> mindWaveId) noexcept { noiseFloorMindWave_ = mindWaveId; }

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
     * @brief The MindWave (if any) `reductionDb()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` dB (no attenuation - a
     *        true no-op) where the wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `reductionDb()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> reductionMindWave() const noexcept { return reductionMindWave_; }

    /// @brief Sets (or clears) which MindWave `reductionDb()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setReductionMindWave(std::optional<MindWaveId> mindWaveId) noexcept { reductionMindWave_ = mindWaveId; }

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
     * @brief The MindWave (if any) `crushAmount()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` (no quantization - a
     *        true no-op) where the wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `crushAmount()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> crushAmountMindWave() const noexcept { return crushAmountMindWave_; }

    /// @brief Sets (or clears) which MindWave `crushAmount()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setCrushAmountMindWave(std::optional<MindWaveId> mindWaveId) noexcept { crushAmountMindWave_ = mindWaveId; }

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
     * @brief The MindWave (if any) `grainAmountDb()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` dB (no offset - a true
     *        no-op) where the wave is dark. Evaluated once per block (at
     *        its own top-left cell), matching `grainSize()`'s own
     *        block-uniform granularity - `grainSize()` itself has no
     *        binding of its own (a block size varying per cell has no
     *        well-defined meaning - see `docs/sound-mind-roadmap.md`'s own
     *        `v0.Y.38.1` entry).
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `grainAmountDb()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> grainAmountMindWave() const noexcept { return grainAmountMindWave_; }

    /// @brief Sets (or clears) which MindWave `grainAmountDb()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setGrainAmountMindWave(std::optional<MindWaveId> mindWaveId) noexcept { grainAmountMindWave_ = mindWaveId; }

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
     * @brief The MindWave (if any) `feedbackAmount()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` (no feedback - a true
     *        no-op) where the wave is dark. Evaluated fresh at every
     *        frame within the same per-bin recursive loop the unbound case
     *        already runs - the recursion's own coefficient simply varies
     *        by position along the way, no restructuring needed.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `feedbackAmount()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> feedbackAmountMindWave() const noexcept {
        return feedbackAmountMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `feedbackAmount()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setFeedbackAmountMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        feedbackAmountMindWave_ = mindWaveId;
    }

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
     * @brief The MindWave (if any) `foldGain()` is bound to - `v0.Y.38.1`,
     *        see `blurSigmaMindWave()`'s own docs for the general
     *        mechanism. Falls toward `1.0` (no folding - a true no-op)
     *        where the wave is dark, unlike most other bindable amounts
     *        here (which fall toward `0`) - `foldGain()`'s own no-op value
     *        is `1.0`, not `0`, matching its own docs.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `foldGain()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> foldGainMindWave() const noexcept { return foldGainMindWave_; }

    /// @brief Sets (or clears) which MindWave `foldGain()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setFoldGainMindWave(std::optional<MindWaveId> mindWaveId) noexcept { foldGainMindWave_ = mindWaveId; }

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
     * @brief The MindWave (if any) `channelBalance()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0.5` (the parameter's own
     *        structurally-neutral value - see `channelBalance()`'s own
     *        docs on why this, unlike a true no-op, still depends on the
     *        input) where the wave is dark.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `channelBalance()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> channelBalanceMindWave() const noexcept {
        return channelBalanceMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `channelBalance()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setChannelBalanceMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        channelBalanceMindWave_ = mindWaveId;
    }

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

    /**
     * @brief The MindWave (if any) `convolveAmount()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` (fully dry - a true
     *        no-op) where the wave is dark. Varies for free, the same
     *        reasoning `sharpenAmountMindWave()`'s own docs give: the
     *        kernel convolution itself (`convolveKernel()`/
     *        `convolveKernelSize()`, neither bindable - see
     *        `docs/sound-mind-roadmap.md`'s own `v0.Y.38.1` entry) runs
     *        exactly once, unaffected by this parameter; only the
     *        already-computed dry/wet blend varies per cell.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `convolveAmount()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> convolveAmountMindWave() const noexcept {
        return convolveAmountMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `convolveAmount()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setConvolveAmountMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        convolveAmountMindWave_ = mindWaveId;
    }

    /**
     * @brief `Displace`'s own shift distance, in bins/columns - confirmed
     *        with the user against the legacy Python Studio's own
     *        `offset_filter()`: content is read from a source position
     *        offset by `(distance * cos(angle), distance * sin(angle))`,
     *        the same angle convention `directionalBlurAngleDegrees()`
     *        already establishes (`0`° along the time axis/columns, `90`°
     *        along the frequency axis/bins). Clamp-to-edge at the
     *        boundary (confirmed with the user over legacy's own
     *        silence-fill, for consistency with every other spatial
     *        filter in this codebase); phase is always left untouched
     *        (confirmed with the user over legacy's own optional
     *        "apply to phase" toggle, not built here).
     * @return The current distance; meaningless unless `type()` is
     *         `Displace`. Not clamped here.
     */
    [[nodiscard]] float displaceDistance() const noexcept { return displaceDistance_; }

    /// @brief Sets `Displace`'s own shift distance.
    /// @param distance The new distance, in bins/columns.
    void setDisplaceDistance(float distance) noexcept { displaceDistance_ = distance; }

    /**
     * @brief The MindWave (if any) `displaceDistance()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0` (no displacement - a true
     *        no-op) where the wave is dark. A pixel-local parameter, per
     *        `docs/sound-mind-design.md`'s own "Filter parameters" section
     *        (an offset distance is explicitly named as an example) -
     *        varies for free, no new per-cell kernel work.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `displaceDistance()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> displaceDistanceMindWave() const noexcept {
        return displaceDistanceMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `displaceDistance()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setDisplaceDistanceMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        displaceDistanceMindWave_ = mindWaveId;
    }

    /**
     * @brief `Displace`'s own shift direction, in degrees - see
     *        `displaceDistance()`'s own docs for the exact convention.
     * @return The current angle; meaningless unless `type()` is
     *         `Displace`. Not clamped here.
     */
    [[nodiscard]] float displaceAngleDegrees() const noexcept { return displaceAngleDegrees_; }

    /// @brief Sets `Displace`'s own shift direction.
    /// @param degrees The new angle, in degrees.
    void setDisplaceAngleDegrees(float degrees) noexcept { displaceAngleDegrees_ = degrees; }

    /**
     * @brief The MindWave (if any) `displaceAngleDegrees()` is bound to -
     *        `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism, and `directionalBlurAngleMindWave()`'s own
     *        docs for why an angle falls toward `0`° (a natural default,
     *        not a claimed no-op) rather than a true no-op value. A
     *        pixel-local parameter, per `docs/sound-mind-design.md`'s own
     *        explicit "offset angle" example - varies for free.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `displaceAngleDegrees()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> displaceAngleMindWave() const noexcept { return displaceAngleMindWave_; }

    /// @brief Sets (or clears) which MindWave `displaceAngleDegrees()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setDisplaceAngleMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        displaceAngleMindWave_ = mindWaveId;
    }

    /**
     * @brief `ChannelCycle`'s own rotation angle, in degrees - `0`° (and
     *        every exact multiple of `360`°) is a true no-op (identity);
     *        every `120`° is one full step of a 3-way rotation among left
     *        loudness, right loudness, and phase (normalized to a shared
     *        `[0, 1]` domain first - see `applyFilter()`'s own docs for
     *        why), with a fractional angle linearly interpolating between
     *        adjacent steps. Confirmed with the user as the direct
     *        3-channel analog of the legacy Python Studio's own
     *        `color_rotate()`, simplified from its own configurable
     *        4-page/9-mapping system (this codebase has no separate
     *        left/right phase to make that configurability meaningful).
     * @return The current angle; meaningless unless `type()` is
     *         `ChannelCycle`. Not clamped here.
     */
    [[nodiscard]] float channelCycleAngleDegrees() const noexcept { return channelCycleAngleDegrees_; }

    /// @brief Sets `ChannelCycle`'s own rotation angle.
    /// @param degrees The new angle, in degrees.
    void setChannelCycleAngleDegrees(float degrees) noexcept { channelCycleAngleDegrees_ = degrees; }

    /**
     * @brief The MindWave (if any) `channelCycleAngleDegrees()` is bound to
     *        - `v0.Y.38.1`, see `blurSigmaMindWave()`'s own docs for the
     *        general mechanism. Falls toward `0`° (identity - a true
     *        no-op) where the wave is dark. A pixel-local parameter, per
     *        `docs/sound-mind-design.md`'s own explicit "hue-rotation
     *        angle" example (this is the direct analog for this
     *        codebase's own three channels) - varies for free.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `channelCycleAngleDegrees()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> channelCycleAngleMindWave() const noexcept {
        return channelCycleAngleMindWave_;
    }

    /// @brief Sets (or clears) which MindWave `channelCycleAngleDegrees()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setChannelCycleAngleMindWave(std::optional<MindWaveId> mindWaveId) noexcept {
        channelCycleAngleMindWave_ = mindWaveId;
    }

    /**
     * @brief `SpectralReverb`'s own pre-delay, in frames, before the
     *        impulse response's own decay begins - confirmed with the
     *        user against the legacy Python Studio's own `reverb_filter()`.
     * @return The current pre-delay; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] int reverbPreDelayFrames() const noexcept { return reverbPreDelayFrames_; }

    /// @brief Sets `SpectralReverb`'s own pre-delay.
    /// @param frames The new pre-delay, in frames; intended to be non-negative.
    void setReverbPreDelayFrames(int frames) noexcept { reverbPreDelayFrames_ = frames; }

    /**
     * @brief `SpectralReverb`'s own decay length, in frames - the
     *        impulse response's own RT60 (`-60`dB point) before scaling by
     *        `reverbRoomSize()`.
     * @return The current decay; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] int reverbDecayFrames() const noexcept { return reverbDecayFrames_; }

    /// @brief Sets `SpectralReverb`'s own decay length.
    /// @param frames The new decay, in frames; intended to be positive.
    void setReverbDecayFrames(int frames) noexcept { reverbDecayFrames_ = frames; }

    /**
     * @brief `SpectralReverb`'s own room size, in `[0, 1]` - scales
     *        `reverbDecayFrames()` down (a smaller room decays faster);
     *        `1.0` uses the full configured decay length.
     * @return The current room size; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] float reverbRoomSize() const noexcept { return reverbRoomSize_; }

    /// @brief Sets `SpectralReverb`'s own room size.
    /// @param roomSize The new room size, intended within `[0, 1]`.
    void setReverbRoomSize(float roomSize) noexcept { reverbRoomSize_ = roomSize; }

    /**
     * @brief `SpectralReverb`'s own cross-frequency diffusion, in `[0, 1]` -
     *        a Gaussian blur along the frequency axis before the impulse
     *        response is applied; `0` is a true no-op (no blur at all).
     * @return The current diffusion; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] float reverbDiffusion() const noexcept { return reverbDiffusion_; }

    /// @brief Sets `SpectralReverb`'s own diffusion.
    /// @param diffusion The new diffusion, intended within `[0, 1]`.
    void setReverbDiffusion(float diffusion) noexcept { reverbDiffusion_ = diffusion; }

    /**
     * @brief `SpectralReverb`'s own high-frequency absorption, in `[0, 1]` -
     *        `0` leaves every frequency equally loud going into the
     *        reverb tail; `1` damps the highest encoded frequency the most
     *        (real rooms absorb high frequencies fastest), ramping
     *        linearly down to no damping at the lowest encoded frequency.
     * @return The current absorption; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] float reverbAbsorption() const noexcept { return reverbAbsorption_; }

    /// @brief Sets `SpectralReverb`'s own absorption.
    /// @param absorption The new absorption, intended within `[0, 1]`.
    void setReverbAbsorption(float absorption) noexcept { reverbAbsorption_ = absorption; }

    /**
     * @brief `SpectralReverb`'s own dry/wet mix, in `[0, 1]` - `0` is a
     *        true no-op (identity); `1` is the fully reverberated result.
     * @return The current amount; meaningless unless `type()` is
     *         `SpectralReverb`. Not clamped here.
     */
    [[nodiscard]] float reverbMix() const noexcept { return reverbMix_; }

    /// @brief Sets `SpectralReverb`'s own dry/wet mix.
    /// @param mix The new mix, intended within `[0, 1]`.
    void setReverbMix(float mix) noexcept { reverbMix_ = mix; }

    /**
     * @brief The MindWave (if any) `reverbMix()` is bound to - `v0.Y.38.1`,
     *        see `blurSigmaMindWave()`'s own docs for the general
     *        mechanism. Falls toward `0` (fully dry - a true no-op) where
     *        the wave is dark. Varies for free, the same reasoning
     *        `convolveAmountMindWave()`'s own docs give: the reverb tail
     *        itself (`reverbPreDelayFrames()`/`reverbDecayFrames()`/
     *        `reverbRoomSize()`/`reverbDiffusion()`/`reverbAbsorption()`,
     *        none bindable - see `docs/sound-mind-roadmap.md`'s own
     *        `v0.Y.38.1` entry for why) is computed exactly once; only the
     *        already-computed dry/wet blend varies per cell.
     * @return The bound MindWave's id, or `std::nullopt` for a plain,
     *         uniform `reverbMix()` (the default).
     */
    [[nodiscard]] std::optional<MindWaveId> reverbMixMindWave() const noexcept { return reverbMixMindWave_; }

    /// @brief Sets (or clears) which MindWave `reverbMix()` is bound to.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setReverbMixMindWave(std::optional<MindWaveId> mindWaveId) noexcept { reverbMixMindWave_ = mindWaveId; }

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
    std::optional<MindWaveId> speckleDensityMindWave_;
    float speckleIntensity_ = 0.8f;
    std::optional<MindWaveId> speckleIntensityMindWave_;
    float speckleThresholdDb_ = 12.0f;
    std::optional<MindWaveId> speckleThresholdMindWave_;
    float noiseFloorDb_ = -60.0f;
    std::optional<MindWaveId> noiseFloorMindWave_;
    float reductionDb_ = 24.0f;
    std::optional<MindWaveId> reductionMindWave_;
    float crushAmount_ = 0.5f;
    std::optional<MindWaveId> crushAmountMindWave_;
    int grainSize_ = 4;
    float grainAmountDb_ = 6.0f;
    std::optional<MindWaveId> grainAmountMindWave_;
    float feedbackAmount_ = 0.5f;
    std::optional<MindWaveId> feedbackAmountMindWave_;
    float foldGain_ = 2.0f;
    std::optional<MindWaveId> foldGainMindWave_;
    float channelBalance_ = 0.5f;
    std::optional<MindWaveId> channelBalanceMindWave_;
    std::vector<float> convolveKernel_{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int convolveKernelSize_ = 3;
    bool convolveNormalize_ = false;
    float convolveAmount_ = 1.0f;
    std::optional<MindWaveId> convolveAmountMindWave_;
    float displaceDistance_ = 10.0f;
    std::optional<MindWaveId> displaceDistanceMindWave_;
    float displaceAngleDegrees_ = 0.0f;
    std::optional<MindWaveId> displaceAngleMindWave_;
    float channelCycleAngleDegrees_ = 0.0f;
    std::optional<MindWaveId> channelCycleAngleMindWave_;
    int reverbPreDelayFrames_ = 2;
    int reverbDecayFrames_ = 40;
    float reverbRoomSize_ = 0.6f;
    float reverbDiffusion_ = 0.5f;
    float reverbAbsorption_ = 0.4f;
    float reverbMix_ = 0.4f;
    std::optional<MindWaveId> reverbMixMindWave_;
};

/// @brief Serializes a filter configuration to its JSON representation.
void to_json(nlohmann::json& json, const FilterConfiguration& config);

/// @brief Parses a filter configuration from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, FilterConfiguration& config);

}  // namespace sound_mind::core
