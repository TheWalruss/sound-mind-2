#pragma once

#include <optional>
#include <vector>

#include <QColor>
#include <Qt>

#include "sound_mind/core/path.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::studio {

/**
 * @brief Which source(s) currently feed the Frequency Grid, and how its
 *        lines are drawn - see `docs/sound-mind-design.md`'s "Overlay
 *        Grids" > "Frequency Grid".
 *
 * Any combination of the three sources can be active at once, per that
 * section's own "built from any combination of" wording - unlike
 * `TimingGridConfig::mode` below, which the design doc phrases as
 * either/or. Session-only Studio UI state, the same as
 * `VerticalAxisLabelMode`/`HorizontalAxisLabelMode` (`axis_labels.h`) and
 * `ToolConfigurationPanel`'s own "Show bounding boxes"/"Show path
 * geometry" checkboxes: none of those persist in the project file
 * either, since none affect encoding, decoding, or any stored pixel
 * data - only what the canvas currently draws as a reference aid, and
 * (for Snap to Grid - see `nearestFrequencyGridLineHz()`) where a drag
 * momentarily lands.
 *
 * **Deliberately free-entry only for now**: the design doc also mentions
 * curated preset sets (alternate tuning references for the note grid;
 * "commonly-cited reference-tone frequency" sets for Custom
 * Frequencies) - neither doc names specific values, so this installment
 * ships manual entry only (the note grid uses the project's own
 * existing `ProjectSettings::referenceHz`; Custom Frequencies is a
 * user-typed Hz list), deferring a built-in preset picker.
 */
struct FrequencyGridConfig {
    /// @brief Every semitone (12-TET, against `ProjectSettings::referenceHz`)
    /// within the project's encoded frequency range.
    bool noteGridEnabled = false;

    /// @brief Every integer multiple of `harmonicFundamentalHz` within the
    /// project's encoded frequency range.
    bool harmonicSeriesEnabled = false;

    /// @brief The harmonic series' own fundamental, in Hz - must be
    /// positive for harmonicSeriesEnabled to produce any lines.
    double harmonicFundamentalHz = 110.0;

    /// @brief Whether the user-entered frequency list below is active.
    bool customFrequenciesEnabled = false;
    /// @brief A user-entered list of specific frequencies, in Hz.
    std::vector<double> customFrequenciesHz;

    /// @brief How Frequency Grid lines are drawn on the canvas.
    QColor lineColor = QColor(Qt::lightGray);
    /// @brief Line width, in pixels.
    double lineWidthPixels = 1.0;
    /// @brief Line dash style.
    Qt::PenStyle lineStyle = Qt::SolidLine;

    /// @brief Whether any source above would actually produce a line -
    /// "no grid active" per Snap to Grid's own docs ("with no grid
    /// active, snapping has nothing to snap to").
    /// @return `true` if at least one source is enabled with valid
    ///         parameters to actually produce a line.
    [[nodiscard]] bool isActive() const noexcept {
        return noteGridEnabled || (harmonicSeriesEnabled && harmonicFundamentalHz > 0.0) ||
               (customFrequenciesEnabled && !customFrequenciesHz.empty());
    }
};

/**
 * @brief Which single source currently feeds the Timing Grid - see
 *        `docs/sound-mind-design.md`'s "Overlay Grids" > "Timing Grid":
 *        "either at a fixed interval or as a beat/bar grid" - either/or,
 *        unlike `FrequencyGridConfig`'s own combinable sources above.
 */
enum class TimingGridMode {
    Off,
    /// @brief Lines every `TimingGridConfig::intervalSeconds`.
    Interval,
    /// @brief Lines every `TimingGridConfig::tempoBeatFraction` of a
    /// quarter-note beat, at the project's own `ProjectSettings::defaultTempoBpm`.
    Tempo,
};

/**
 * @brief The Timing Grid's own current mode and line style - see
 *        `TimingGridMode`'s own docs, and `FrequencyGridConfig`'s own
 *        docs on why this is session-only Studio UI state, not persisted
 *        in the project file.
 */
struct TimingGridConfig {
    /// @brief Which single source is currently active - see `TimingGridMode`'s own docs.
    TimingGridMode mode = TimingGridMode::Off;

    /// @brief Line spacing in seconds, while `mode` is `Interval`. Must
    /// be positive to produce any lines.
    double intervalSeconds = 1.0;

    /// @brief Line spacing in fractions of a quarter-note beat, while
    /// `mode` is `Tempo` - `1.0` = every beat (a quarter note), `0.5` =
    /// every eighth note, and so on down to "the finest rhythmic value
    /// in use" per the design doc's own phrasing. Must be positive to
    /// produce any lines.
    double tempoBeatFraction = 1.0;

    /// @brief How Timing Grid lines are drawn on the canvas.
    QColor lineColor = QColor(Qt::lightGray);
    /// @brief Line width, in pixels.
    double lineWidthPixels = 1.0;
    /// @brief Line dash style.
    Qt::PenStyle lineStyle = Qt::SolidLine;

    /// @brief Whether `mode` would actually produce a line - see
    /// `FrequencyGridConfig::isActive()`'s own docs.
    /// @return `true` if `mode` is active with valid parameters to
    ///         actually produce a line.
    [[nodiscard]] bool isActive() const noexcept {
        if (mode == TimingGridMode::Off) {
            return false;
        }
        if (mode == TimingGridMode::Interval) {
            return intervalSeconds > 0.0;
        }
        return tempoBeatFraction > 0.0;
    }
};

/**
 * @brief The Frequency Grid's own active line positions, in Hz, for
 *        drawing - one entry per active source's own lines within
 *        `settings`' encoded frequency range (`minFrequencyHz`/
 *        `maxFrequencyHz`), merged, deduplicated, and sorted ascending.
 *
 * @param config Which source(s) are active, and their own parameters.
 * @param settings The project settings to derive the frequency range
 *        (and, for the note grid, `referenceHz`) from.
 * @return The active line positions; empty if `config.isActive()` is
 *         `false`.
 */
[[nodiscard]] std::vector<double> frequencyGridLinesHz(const FrequencyGridConfig& config,
                                                          const sound_mind::core::ProjectSettings& settings);

/**
 * @brief The Timing Grid's own active line positions, in seconds, for
 *        drawing - every line from `0` up to and including
 *        `durationSeconds`.
 *
 * @param config The Timing Grid's own current mode/parameters.
 * @param settings The project settings to derive `defaultTempoBpm` from,
 *        while `config.mode` is `Tempo`.
 * @param durationSeconds The project's own total duration, in seconds -
 *        the range to fill with lines.
 * @return The active line positions; empty if `config.isActive()` is
 *         `false`, or `durationSeconds` isn't positive.
 */
[[nodiscard]] std::vector<double> timingGridLinesSeconds(const TimingGridConfig& config,
                                                            const sound_mind::core::ProjectSettings& settings,
                                                            double durationSeconds);

/**
 * @brief The nearest active Frequency Grid line to `frequencyHz` - the
 *        actual work behind Snap to Grid's own frequency-axis snapping
 *        (see `docs/sound-mind-design.md`'s "Snap to Grid").
 *
 * Considers every enabled source independently (the nearest semitone,
 * the nearest harmonic, and the nearest custom frequency), and returns
 * whichever single candidate sits closest to `frequencyHz` - "whichever
 * active grid is finest at that point", per the design doc's own
 * phrasing, in practice means "the globally nearest active line" once
 * more than one source can be active at once.
 *
 * @param frequencyHz The frequency to snap.
 * @param config Which source(s) are active, and their own parameters.
 * @param settings The project settings to derive `referenceHz` from, for
 *        the note grid.
 * @return The nearest active line's own frequency, in Hz; `std::nullopt`
 *         if `config.isActive()` is `false` (nothing to snap to).
 */
[[nodiscard]] std::optional<double> nearestFrequencyGridLineHz(double frequencyHz, const FrequencyGridConfig& config,
                                                                  const sound_mind::core::ProjectSettings& settings);

/**
 * @brief The nearest active Timing Grid line to `seconds` - the actual
 *        work behind Snap to Grid's own time-axis snapping.
 *
 * @param seconds The time to snap.
 * @param config The Timing Grid's own current mode/parameters.
 * @param settings The project settings to derive `defaultTempoBpm` from,
 *        while `config.mode` is `Tempo`.
 * @return The nearest active line's own time, in seconds; `std::nullopt`
 *         if `config.isActive()` is `false` (nothing to snap to).
 */
[[nodiscard]] std::optional<double> nearestTimingGridLineSeconds(double seconds, const TimingGridConfig& config,
                                                                    const sound_mind::core::ProjectSettings& settings);

/**
 * @brief Snaps `point` to whichever active grid lines are nearest it, on
 *        each axis independently - the actual work behind Snap to Grid
 *        (see `docs/sound-mind-design.md`'s "Snap to Grid"), shared by
 *        every drag interaction it applies to (`PickController`'s
 *        whole-object move and Path node drag, `SelectionController`'s
 *        selection drag).
 *
 * Each axis snaps only if its own grid is active - "with no grid
 * active, snapping has nothing to snap to" applies per-axis, not as a
 * single all-or-nothing switch, so e.g. an active Frequency Grid with
 * the Timing Grid off still snaps frequency while leaving time free.
 *
 * @param point The point to snap.
 * @param frequencyGridConfig Which Frequency Grid source(s) are active.
 * @param timingGridConfig The Timing Grid's own current mode.
 * @param settings The project settings both nearestFrequencyGridLineHz()
 *        and nearestTimingGridLineSeconds() need.
 * @return `point`, with whichever coordinate(s) have an active grid
 *         replaced by their own nearest line.
 */
[[nodiscard]] sound_mind::core::TimeFrequencyPoint snapToGrid(sound_mind::core::TimeFrequencyPoint point,
                                                                  const FrequencyGridConfig& frequencyGridConfig,
                                                                  const TimingGridConfig& timingGridConfig,
                                                                  const sound_mind::core::ProjectSettings& settings);

}  // namespace sound_mind::studio
