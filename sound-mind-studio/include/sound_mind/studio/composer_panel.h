#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QDockWidget>
#include <QImage>
#include <QString>

#include "sound_mind/core/layer.h"

class QVBoxLayout;

namespace sound_mind::studio {

/**
 * @brief A read-only, dockable panel showing Composer Mode's own DAW-style
 *        track view - `docs/sound-mind-design.md`'s "Composer Mode",
 *        `v0.Y.48.1` Installment A.
 *
 * Each visible layer becomes one track row, topmost layer first (matching
 * `LayersPanel`'s own display convention). Every track has its own
 * background-style selector (Clean/Amplitude/Thumbnail) - purely local,
 * session-only UI state this panel owns entirely by itself
 * (`docs/sound-mind-architecture.md`'s "Composer Mode Fit" section: "Which
 * background a track is currently showing is Studio view-state, not
 * project data"), not round-tripped through `MainWindow`/`Project` at all.
 * Every active `Operation` targeting a track's own layer renders as a
 * read-only box, positioned by its own `bounds()` against the project's
 * total canvas duration.
 *
 * **Installment A is view-only**: no retiming, reordering, moving an
 * operation between layers, or editing a track's/object's own settings
 * from here yet - each is deliberately left for its own later installment
 * (see `docs/sound-mind-roadmap.md`'s `v0.Y.48.1` entry). Purely
 * presentational, the same "MainWindow computes, panel only displays"
 * division of responsibility `LayersPanel`/`HistoryPanel` already follow -
 * this panel does no `Core` computation of its own at all.
 */
class ComposerPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief One operation's own box, already positioned as a
    ///        `[0, 1]`-normalized fraction of the project's own total
    ///        canvas duration - see `setTracks()`'s own docs for how
    ///        `MainWindow` computes these from `Operation::bounds()`.
    struct OperationBox {
        double startFraction = 0.0;
        double endFraction = 0.0;
    };

    /// @brief One track's own displayed data - see `setTracks()`'s own
    ///        docs.
    struct TrackData {
        sound_mind::core::LayerId id{0};
        QString name;
        /// @brief `sound_mind::core::renderLayerAmplitudeSummary()`'s own
        ///        result, converted to a `QImage` - `std::nullopt` if the
        ///        layer has no content yet.
        std::optional<QImage> amplitudeImage;
        /// @brief `sound_mind::core::renderLayerThumbnail()`'s own result,
        ///        converted to a `QImage` - `std::nullopt` if the layer
        ///        has no content yet.
        std::optional<QImage> thumbnailImage;
        /// @brief Every operation currently targeting this track's own
        ///        layer (`OperationLog::activeOperationsTargeting()`), in
        ///        no particular order.
        std::vector<OperationBox> operations;
    };

    /// @brief Builds an empty panel (no tracks) - call `setTracks()` once
    ///        a project exists.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ComposerPanel(QWidget* parent = nullptr);

    /**
     * @brief Replaces every displayed track.
     *
     * Each track row keeps its own previously-selected background style
     * across a refresh, matched by `TrackData::id` - a track whose id is
     * new (never seen before) starts at `Clean`, the same "safe, cheap
     * default" every other first-seen UI state in this codebase starts
     * from.
     *
     * @param tracks Every visible layer's own track data, topmost layer
     *        first.
     */
    void setTracks(const std::vector<TrackData>& tracks);

private:
    QVBoxLayout* trackLayout_;
};

}  // namespace sound_mind::studio
