#pragma once

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/path.h"

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief Which family of waveform a `MindWave` generates - see `docs/
 *        sound-mind-design.md`'s "MindWave Functions".
 *
 * Only `Periodic` exists so far (`v0.Y.31.1`, Installment A) - `Envelope`,
 * `SteppedNoise`, `Spatial`, and `Fractal` are added incrementally as each
 * is actually implemented, the same one-value-per-installment shape
 * `FilterType` was built up in during Filter Layers, rather than declared
 * upfront with unimplemented branches.
 */
enum class GeneratorType {
    Periodic,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(GeneratorType, {
    {GeneratorType::Periodic, "periodic"},
})
// clang-format on

/**
 * @brief Which specific periodic shape a `GeneratorType::Periodic`
 *        `MindWave` cycles through - see `docs/sound-mind-design.md`'s
 *        "MindWave Functions": "sine, triangle, square, sawtooth, and
 *        pulse".
 *
 * Only `Sine` exists so far (`v0.Y.31.1`, Installment A) - the other four
 * are added once actually implemented, same reasoning as `GeneratorType`'s
 * own docs.
 */
enum class PeriodicWaveform {
    Sine,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(PeriodicWaveform, {
    {PeriodicWaveform::Sine, "sine"},
})
// clang-format on

/**
 * @brief Which canvas axis a `MindWave` cycles along - see `docs/
 *        sound-mind-design.md`'s "a spatial 'LFO' that varies over time
 *        (horizontal axis), frequency (vertical axis), or both".
 *
 * Only the two single-axis cases exist so far; a generator type that
 * varies across both axes at once (`GeneratorType::Spatial`, once it
 * exists) won't use this enum at all - it has no single axis to name.
 */
enum class MindWaveAxis {
    Time,
    Frequency,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(MindWaveAxis, {
    {MindWaveAxis::Time, "time"},
    {MindWaveAxis::Frequency, "frequency"},
})
// clang-format on

/**
 * @brief A parametric waveform producing a per-cell scalar field in
 *        `[0, 1]` across the canvas - see `docs/sound-mind-design.md`'s
 *        "Low Frequency Oscillations (MindWaves)".
 *
 * **`v0.Y.31.1`, Installment A**: deliberately a bare value type, not yet
 * a named, `Project`-scoped, independently-referenceable resource the way
 * the design doc's own "MindWave" ultimately is - the same "not built
 * until a real consumer needs it" gap `ToolConfiguration` currently has
 * too (embedded directly wherever it's used, no id/name/`Project` storage
 * of its own yet). Identity/storage arrives once something actually binds
 * to a `MindWave` by reference (the layer-opacity-binding installment).
 * Likewise, no superposition-stack field yet - added once superposition
 * itself is implemented, not speculatively ahead of that.
 *
 * Only one generator family (`GeneratorType::Periodic`) and one waveform
 * within it (`PeriodicWaveform::Sine`) exist yet - proving the field-
 * evaluation pipeline end to end with a single, hand-verifiable shape
 * before building out the rest of `v0.Y.31.1`'s own named catalogue
 * (`docs/sound-mind-roadmap.md`'s "narrow proof first" installment plan,
 * confirmed with the user).
 */
class MindWave {
public:
    /// @brief Constructs a real, audible-if-bound default - a 1-second
    ///        (or 1-bin, on the frequency axis) sine cycle at zero phase,
    ///        not a degenerate placeholder - matching `FilterConfiguration`'s
    ///        own "nothing happens by accident" default philosophy applied
    ///        to a MindWave instead: a freshly created one should visibly
    ///        do something the moment it's bound, not silently no-op.
    MindWave() noexcept = default;

    /// @brief Which generator family this MindWave uses.
    /// @return The current generator type.
    [[nodiscard]] GeneratorType type() const noexcept { return type_; }

    /// @brief Sets which generator family this MindWave uses.
    /// @param type The new generator type.
    void setType(GeneratorType type) noexcept { type_ = type; }

    /// @brief Which periodic shape this MindWave cycles through - only
    ///        meaningful while `type()` is `GeneratorType::Periodic`.
    /// @return The current periodic waveform.
    [[nodiscard]] PeriodicWaveform periodicWaveform() const noexcept { return periodicWaveform_; }

    /// @brief Sets which periodic shape this MindWave cycles through.
    /// @param waveform The new periodic waveform.
    void setPeriodicWaveform(PeriodicWaveform waveform) noexcept { periodicWaveform_ = waveform; }

    /// @brief Which canvas axis this MindWave cycles along.
    /// @return The current axis.
    [[nodiscard]] MindWaveAxis axis() const noexcept { return axis_; }

    /// @brief Sets which canvas axis this MindWave cycles along.
    /// @param axis The new axis.
    void setAxis(MindWaveAxis axis) noexcept { axis_ = axis; }

    /**
     * @brief How long one full cycle takes, in the current axis's own
     *        natural unit: seconds for `MindWaveAxis::Time`, bins (not
     *        Hz) for `MindWaveAxis::Frequency` - see `evaluate()`'s own
     *        docs for why bins, not Hz.
     * @return The current period.
     */
    [[nodiscard]] double period() const noexcept { return period_; }

    /// @brief Sets the cycle length - see `period()`'s own docs for the
    ///        unit. Values at or below zero are treated as a small
    ///        positive floor by `evaluate()` (defensive only - not a
    ///        musically meaningful minimum), rather than dividing by zero.
    /// @param period The new period.
    void setPeriod(double period) noexcept { period_ = period; }

    /// @brief The cycle's own phase offset, in radians - `0` starts the
    ///        cycle at its own defined origin; matches `sharedPhaseRadians`'s
    ///        own unit convention elsewhere in this codebase, even though
    ///        it's a different concept (a generator parameter, not a
    ///        signal's own phase).
    /// @return The current phase offset, in radians.
    [[nodiscard]] double phaseRadians() const noexcept { return phaseRadians_; }

    /// @brief Sets the cycle's own phase offset - see `phaseRadians()`'s
    ///        own docs for the unit.
    /// @param phaseRadians The new phase offset, in radians.
    void setPhaseRadians(double phaseRadians) noexcept { phaseRadians_ = phaseRadians; }

    /**
     * @brief This MindWave's own field value at `point`.
     *
     * The frequency axis is log-scaled (see `frequencyToBinIndex()`'s own
     * docs), so a `period()` expressed in raw Hz would pack a different
     * number of visible cycles into the same on-screen space depending on
     * where in the frequency range it sits - the same reasoning
     * `translateFrequencyByBins()`/`PickController`'s own frequency-axis
     * drag math already established for why bins, not Hz, are this axis's
     * own natural, evenly-spaced unit. `period()` on `MindWaveAxis::Frequency`
     * is therefore in bins, converted from `point`'s own Hz via
     * `frequencyToBinIndex()` before dividing by it.
     *
     * @param point The canvas position to evaluate - only the component
     *        matching `axis()` is actually used.
     * @param config Interprets `point`'s own Hz against `config`'s own
     *        frequency range/bin count, needed only when `axis()` is
     *        `MindWaveAxis::Frequency`.
     * @return This MindWave's own field value at `point`, in `[0, 1]`.
     */
    [[nodiscard]] float evaluate(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const;

private:
    GeneratorType type_ = GeneratorType::Periodic;
    PeriodicWaveform periodicWaveform_ = PeriodicWaveform::Sine;
    MindWaveAxis axis_ = MindWaveAxis::Time;
    double period_ = 1.0;
    double phaseRadians_ = 0.0;
};

/// @brief Serializes a MindWave to its JSON representation.
void to_json(nlohmann::json& json, const MindWave& mindWave);

/// @brief Parses a MindWave from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, MindWave& mindWave);

}  // namespace sound_mind::core
