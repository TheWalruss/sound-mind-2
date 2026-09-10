#pragma once

#include <cstddef>
#include <vector>

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief One stop in a Gradient - see `docs/sound-mind-design.md`'s
 *        "Stop Values".
 *
 * Independent intensity/opacity per channel, so a gradient can target one
 * channel only (with the other left at zero) or both identically (via
 * `Gradient::linkChannels()`).
 */
struct GradientStop {
    /// @brief Normalized position along the gradient, in `[0, 1]` - `0` is
    /// the start (t=0), `1` the end (t=1).
    float t = 0.0f;

    /// @brief The left channel's amplitude target at this stop.
    float leftIntensity = 0.0f;

    /// @brief The right channel's amplitude target at this stop.
    float rightIntensity = 0.0f;

    /// @brief How strongly `leftIntensity` is actually written at this
    /// stop - `0` leaves existing content untouched regardless of
    /// `leftIntensity`.
    float leftOpacity = 0.0f;

    /// @brief How strongly `rightIntensity` is actually written at this
    /// stop - `0` leaves existing content untouched regardless of
    /// `rightIntensity`.
    float rightOpacity = 0.0f;
};

/// @brief Serializes a stop to its JSON representation.
void to_json(nlohmann::json& json, const GradientStop& stop);

/// @brief Parses a stop from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, GradientStop& stop);

/**
 * @brief Controls how paint color and opacity vary along a Path, across a
 *        filled selection, or across the frequency axis of an Equalizer
 *        filter - see `docs/sound-mind-design.md`'s "Gradients".
 *
 * Always holds at least two stops, fixed at `t=0` and `t=1` - an interior
 * stop can be inserted anywhere between them and later removed, but the
 * two endpoints can't be removed, only edited. Values interpolate
 * linearly between consecutive stops.
 */
class Gradient {
public:
    /// @brief Constructs a gradient with just its two mandatory endpoint
    ///        stops (`t=0`, `t=1`), both fully transparent (zero intensity
    ///        and opacity) - per the design doc, a freshly created
    ///        gradient has no visible effect until values are deliberately
    ///        set, so nothing is painted by accident.
    Gradient();

    /// @brief This gradient's stops, in ascending `t` order.
    /// @return At least two stops, the first at `t=0` and the last at `t=1`.
    [[nodiscard]] const std::vector<GradientStop>& stops() const noexcept { return stops_; }

    /**
     * @brief Inserts a new interior stop at the given position.
     *
     * The new stop's values start at whatever evaluate() already produces
     * at `t` - inserting a stop alone never changes the gradient's
     * apparent shape, only adds a point that can then be pulled away from
     * it.
     *
     * @param t Where to insert the new stop; clamped to the open interval
     *        `(0, 1)` - a value at or beyond either endpoint is nudged
     *        just inside it instead of colliding with (or duplicating) an
     *        endpoint stop.
     * @return The new stop's index in stops().
     */
    std::size_t insertStop(float t);

    /**
     * @brief Removes an interior stop.
     * @param index The stop to remove, per stops()'s own indexing.
     * @return `true` and removes the stop if `index` refers to a real,
     *         interior (not the first or last) stop; `false` (no change)
     *         otherwise.
     */
    bool removeStop(std::size_t index);

    /**
     * @brief Replaces one stop's own values in place, without moving it.
     * @param index The stop to update, per stops()'s own indexing.
     * @param values The new values - `t` is ignored; a stop's position
     *        only changes via insertStop()/removeStop().
     * @return `true` and applies the change if `index` is valid; `false`
     *         (no change) otherwise.
     */
    bool setStopValues(std::size_t index, const GradientStop& values);

    /// @brief Whether editing this gradient's left channel should mirror
    ///        the edit to the right channel too - a UI-editing convenience
    ///        flag with no effect on evaluate() itself.
    /// @return `true` if channel edits are currently linked.
    [[nodiscard]] bool linkChannels() const noexcept { return linkChannels_; }

    /// @brief Sets whether editing this gradient's left channel should
    ///        mirror the edit to the right channel too.
    /// @param linked The new linked state.
    void setLinkChannels(bool linked) noexcept { linkChannels_ = linked; }

    /**
     * @brief Evaluates this gradient at a normalized position, linearly
     *        interpolating between the two stops bracketing it.
     * @param t Where to evaluate; clamped to `[0, 1]`.
     * @return The interpolated stop values at `t`.
     */
    [[nodiscard]] GradientStop evaluate(float t) const;

    friend void to_json(nlohmann::json& json, const Gradient& gradient);
    friend void from_json(const nlohmann::json& json, Gradient& gradient);

private:
    std::vector<GradientStop> stops_;
    bool linkChannels_ = false;
};

/// @brief Serializes a gradient to its JSON representation.
void to_json(nlohmann::json& json, const Gradient& gradient);

/// @brief Parses a gradient from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Gradient& gradient);

}  // namespace sound_mind::core
