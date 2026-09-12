#pragma once

#include <array>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/gradient.h"

namespace sound_mind::core {

/**
 * @brief Which filter algorithm a `FilterConfiguration` configures - see
 *        `docs/sound-mind-design.md`'s "Filter Layer", "Blur & focus"
 *        family plus "Tonal"/"Spectral shaping".
 *
 * @note This milestone's own confirmed scope (`docs/sound-mind-roadmap.md`'s
 *       `v0.Y.28.1`): all three "Blur & focus" variants, Sharpen, a Tone
 *       Curve, and Frequency-Axis Gradient (the Equalizer layer's own
 *       basis) - the design doc's other Filter Layer families (Noise &
 *       distortion, Geometric, Space) aren't represented here yet, left
 *       for a later filter-set expansion.
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
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(FilterType, {
    {FilterType::UniformBlur, "uniformBlur"},
    {FilterType::EdgePreservingBlur, "edgePreservingBlur"},
    {FilterType::DirectionalBlur, "directionalBlur"},
    {FilterType::Sharpen, "sharpen"},
    {FilterType::ToneCurve, "toneCurve"},
    {FilterType::FrequencyAxisGradient, "frequencyAxisGradient"},
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
 */
class FilterConfiguration {
public:
    /// @brief Constructs a Frequency-Axis Gradient configuration with a
    ///        fresh, fully transparent default gradient (see `Gradient`'s
    ///        own docs) - a fresh Filter layer has no audible effect
    ///        until its own parameters are deliberately set, the same
    ///        "nothing happens by accident" default `ToolConfiguration`'s
    ///        own gradient already establishes for painting.
    FilterConfiguration() = default;

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
     * @brief `DirectionalBlur`'s own kernel length, in bins/columns.
     * @return The current length; meaningless unless `type()` is
     *         `DirectionalBlur`. Not clamped or validated here.
     */
    [[nodiscard]] int directionalBlurLength() const noexcept { return directionalBlurLength_; }

    /// @brief Sets `DirectionalBlur`'s own kernel length.
    /// @param length The new length, in bins/columns; intended to be positive.
    void setDirectionalBlurLength(int length) noexcept { directionalBlurLength_ = length; }

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

    friend void to_json(nlohmann::json& json, const FilterConfiguration& config);
    friend void from_json(const nlohmann::json& json, FilterConfiguration& config);

private:
    FilterType type_ = FilterType::FrequencyAxisGradient;
    float blurSigma_ = 2.0f;
    int medianSize_ = 3;
    int directionalBlurLength_ = 10;
    float directionalBlurAngleDegrees_ = 0.0f;
    float sharpenAmount_ = 1.0f;
    std::vector<std::array<float, 2>> toneCurvePoints_{{0.0f, 0.0f}, {1.0f, 1.0f}};
    Gradient frequencyGradient_;
};

/// @brief Serializes a filter configuration to its JSON representation.
void to_json(nlohmann::json& json, const FilterConfiguration& config);

/// @brief Parses a filter configuration from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, FilterConfiguration& config);

}  // namespace sound_mind::core
