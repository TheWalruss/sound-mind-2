#pragma once

#include <vector>

#include <QString>

#include "sound_mind/core/project_settings.h"

namespace sound_mind::studio {

/**
 * @brief What the canvas's own frequency (vertical) axis labels show -
 *        see `docs/sound-mind-design.md`'s "Axis Labels".
 */
enum class VerticalAxisLabelMode {
    Off,      ///< No labels drawn at all.
    Hertz,    ///< Labeled in Hz (or kHz above 1000 Hz).
    Notes,    ///< Labeled with the nearest 12-TET note name (see `noteNameForFrequency()`).
    BinIndex  ///< Labeled with the raw (rounded) frequency-bin index.
};

/**
 * @brief What the canvas's own time (horizontal) axis labels show - see
 *        `docs/sound-mind-design.md`'s "Axis Labels".
 */
enum class HorizontalAxisLabelMode {
    Off,          ///< No labels drawn at all.
    Seconds,      ///< Labeled in seconds.
    Milliseconds,  ///< Labeled in whole milliseconds.
    FrameIndex    ///< Labeled with the raw (rounded) frame index.
};

/**
 * @brief One tick on an axis: where it sits, and what its label reads.
 */
struct AxisTick {
    /// @brief The tick's own position, in the same domain space the rest
    ///        of this codebase already uses - a frequency in Hz for a
    ///        vertical-axis tick, a time in seconds for a horizontal-axis
    ///        one. A caller converts this to a widget pixel position the
    ///        same way it converts any other `TimeFrequencyPoint` (see
    ///        `CanvasWidget::timeFrequencyToWidgetPoint()`).
    double domainValue = 0.0;

    /// @brief The text drawn next to this tick.
    QString label;
};

/**
 * @brief The frequency-axis ticks to draw for `mode`, thinned so no two
 *        sit closer than a legible minimum apart on screen.
 *
 * `Hertz`/`Notes` both walk a fixed, musically-meaningful candidate list
 * (round Hz numbers for `Hertz`; every semitone, by way of
 * `noteNameForFrequency()`, for `Notes`) rather than computing an evenly
 * bin-spaced set directly - the frequency axis is log-scaled (see
 * `frequencyToBinIndex()`'s own docs), so an evenly bin-spaced set of
 * ticks would land on visually arbitrary, unmemorable Hz values instead
 * of the round numbers/note names an artist actually orients against.
 * `BinIndex` is the one exception, since a bin index *is* the linear
 * space being labeled.
 *
 * @param mode Which labeling scheme to use; `Off` returns an empty list.
 * @param settings The project's own settings - supplies the frequency
 *        range/bin count (via `sound_mind::core::streamCodecConfigFor()`)
 *        and, for `Notes`, the tuning reference (`referenceHz`).
 * @param axisPixelLength The vertical axis's own current on-screen
 *        length, in pixels (a widget's `height()`) - candidates too
 *        close together at this length are dropped, keeping labels
 *        legible however tall or short the canvas is currently shown.
 * @return The ticks to draw, ordered from the lowest frequency to the
 *         highest.
 */
[[nodiscard]] std::vector<AxisTick> verticalAxisTicks(VerticalAxisLabelMode mode,
                                                        const sound_mind::core::ProjectSettings& settings,
                                                        double axisPixelLength);

/**
 * @brief The time-axis ticks to draw for `mode`, thinned so no two sit
 *        closer than a legible minimum apart on screen.
 *
 * Unlike the frequency axis, time is linear, so ticks land at a single
 * "nice" step (1, 2, 5, 10, 30 seconds, and so on - whichever is the
 * smallest such step that still keeps consecutive ticks legibly apart at
 * `axisPixelLength`) rather than needing a separate candidate-list walk.
 *
 * @param mode Which labeling scheme to use; `Off` returns an empty list.
 * @param settings The project's own settings - supplies the canvas's own
 *        total duration (`canvasWidth` at `timestepMs` per frame).
 * @param axisPixelLength The time axis's own current on-screen length,
 *        in pixels (a widget's `width()`).
 * @return The ticks to draw, ordered from `t=0` to the canvas's own end.
 */
[[nodiscard]] std::vector<AxisTick> horizontalAxisTicks(HorizontalAxisLabelMode mode,
                                                          const sound_mind::core::ProjectSettings& settings,
                                                          double axisPixelLength);

}  // namespace sound_mind::studio
