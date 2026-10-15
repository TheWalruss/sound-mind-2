#pragma once

#include <functional>
#include <optional>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/selection_region.h"

namespace sound_mind::core {

/**
 * @brief A committed canvas selection, as "Apply Filter to Selection"
 *        would use it - which layer it was drawn on, its bounding box, and
 *        (for a non-rectangular selection) its exact shape. The same three
 *        values a `FilterOperation` is built from.
 */
struct FilterPreviewSelection {
    /// @brief The layer the selection was drawn on - the one the filter edits.
    LayerId layer = 0;
    /// @brief The selection's bounding box.
    TimeFrequencyRect bounds;
    /// @brief The selection's exact shape, or `std::nullopt` for a rectangle.
    std::optional<SelectionRegion> boundary;
};

/**
 * @brief What the filter preview should play: an in-progress filter
 *        configuration, where it would apply, and which few seconds to loop.
 *
 * Exactly one of three targets applies, checked in this order:
 * - **`filterLayer`** - a Filter or Equalizer layer is selected: that layer's
 *   configuration is replaced by `config` and the whole project is heard, so
 *   the preview is exactly what playback will sound like.
 * - **`selection`** - otherwise, with a canvas selection: `config` is baked
 *   into the selection's layer, inside the selection only, exactly as "Apply
 *   Filter to Selection" would (`applyFilterOperation()`).
 * - **neither** - `config` is applied to the whole mix, as if a Filter layer
 *   sat at the top of the stack.
 */
struct FilterPreviewRequest {
    /// @brief The in-progress filter configuration to hear.
    FilterConfiguration config;
    /// @brief The selected Filter/Equalizer layer, if any - see above.
    std::optional<LayerId> filterLayer;
    /// @brief The committed canvas selection, if any - see above.
    std::optional<FilterPreviewSelection> selection;
    /// @brief Where the looped window starts, in seconds from the project start.
    double startSeconds = 0.0;
    /// @brief Where the looped window ends, in seconds; past `startSeconds`.
    double endSeconds = 0.0;
};

/// @brief How long the filter preview loops when there's no selection to
///        take a length from, in seconds.
inline constexpr double kDefaultFilterPreviewSeconds = 4.0;

/// @brief The shortest window the filter preview loops, in seconds - a
///        shorter selection is lengthened to this from its own start.
inline constexpr double kMinimumFilterPreviewSeconds = 0.5;

/// @brief The fade-in/fade-out at each end of the preview, in seconds, so
///        the loop seam doesn't click.
inline constexpr double kFilterPreviewFadeSeconds = 0.01;

/**
 * @brief Picks the few seconds the filter preview loops.
 *
 * With a selection, its own time range (lengthened to
 * `kMinimumFilterPreviewSeconds` if shorter). Otherwise
 * `kDefaultFilterPreviewSeconds` starting at the playback position, moved
 * back so it ends at the canvas end if it would run past it. Always clamped
 * to `[0, canvasSeconds]`; a canvas shorter than the wanted length is looped
 * whole.
 *
 * @param playbackPositionSeconds The current playback position.
 * @param selection The selection's bounding box, if any.
 * @param canvasSeconds The canvas's length in seconds; must be positive.
 * @return `{start, end}` in seconds, `end > start`.
 */
[[nodiscard]] std::pair<double, double> filterPreviewWindow(double playbackPositionSeconds,
                                                             const std::optional<TimeFrequencyRect>& selection,
                                                             double canvasSeconds);

/**
 * @brief Renders the audio the filter preview loops - see
 *        `FilterPreviewRequest`'s docs for what's heard.
 *
 * Composites a copy of `project` (respecting mute, like playback), cuts out
 * `[startSeconds, endSeconds)` (rounded to whole frames), decodes it, and
 * fades both ends over `kFilterPreviewFadeSeconds`. The same request on the
 * same project always yields the same length, so successive renders can be
 * swapped into a looping `PlaybackEngine` with `stageAudio()`.
 *
 * Costs one full composite of the project per call (the same as a Repeat
 * playback refresh); run it off the UI thread.
 *
 * @param project The project to render; not modified.
 * @param request What to render.
 * @param shouldCancel Polled between layers - see `compositeProject()`.
 * @return The stereo preview audio at the project's sample rate, or an
 *         empty buffer if the project has no content at all.
 * @throws CompositeCancelled if `shouldCancel` returns `true`.
 * @note Thread-safe for distinct `project` objects; never touches the
 *       audio thread.
 */
[[nodiscard]] sound_mind::codec::AudioBuffer renderFilterPreview(const Project& project,
                                                                 const FilterPreviewRequest& request,
                                                                 const std::function<bool()>& shouldCancel = {});

}  // namespace sound_mind::core
