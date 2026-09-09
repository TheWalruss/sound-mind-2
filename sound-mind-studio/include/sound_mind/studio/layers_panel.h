#pragma once

#include <cstdint>
#include <vector>

#include <QDockWidget>
#include <QString>

#include "sound_mind/core/layer.h"

class QListWidget;

namespace sound_mind::studio {

/**
 * @brief A dockable panel listing the current project's layer stack - see
 *        `docs/sound-mind-roadmap.md`'s Layers Panel milestone
 *        (`v0.Y.13.1`).
 *
 * Adapted from the legacy Studio's Layers panel (drag-handle reorder,
 * per-row visibility toggle, name, opacity, add/delete), scoped down to
 * what `sound_mind::core::Layer` actually supports today: no blend-mode
 * combo, no MindWave-link combo, no transform controls, and no settings/
 * gear button - none of those concepts exist in the engine yet.
 *
 * Purely presentational, the same division of responsibility as
 * `LandingPage`: every row action is a signal `MainWindow` connects to
 * its own handlers - no `Layer`/`Project` mutation happens here. Shows
 * the topmost layer first (conventional "top of stack at the top of the
 * list"), the reverse of `Project::layers()`'s own bottom-to-top order -
 * setLayers() and reorderRequested() both use `Project::layers()`'s own
 * bottom-to-top convention so callers never have to reverse anything
 * themselves.
 *
 * `Background` and `Equalizer` layers (`sound_mind::core::LayerType`) are
 * locked: a lock icon instead of a drag handle (so they can't be
 * reordered), no delete button, and - for `Background` specifically - a
 * disabled, always-on visibility toggle (matching the legacy panel's own
 * "locked bottom layer" treatment).
 */
class LayersPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief One row's worth of display data - a Qt-friendly mirror of
    /// just the `sound_mind::core::Layer` fields this panel needs.
    struct RowData {
        /// @brief Mirrors `sound_mind::core::Layer::id()`.
        sound_mind::core::LayerId id = 0;

        /// @brief Mirrors `sound_mind::core::Layer::name()`.
        QString name;

        /// @brief Mirrors `sound_mind::core::Layer::type()`.
        sound_mind::core::LayerType type = sound_mind::core::LayerType::Normal;

        /// @brief Mirrors `sound_mind::core::Layer::opacity()`.
        float opacity = 1.0f;

        /// @brief Mirrors `sound_mind::core::Layer::visible()`.
        bool visible = true;
    };

    /// @brief Builds the panel with an initially-empty layer list.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit LayersPanel(QWidget* parent = nullptr);

    /**
     * @brief Replaces the displayed rows.
     * @param layersBottomToTop Layers in `Project::layers()`'s own
     *        bottom-to-top order - displayed reversed (top layer first).
     */
    void setLayers(const std::vector<RowData>& layersBottomToTop);

signals:
    /// @brief A row's visibility toggle was clicked.
    void visibilityToggled(sound_mind::core::LayerId id, bool visible);

    /// @brief A row's opacity slider changed.
    void opacityChanged(sound_mind::core::LayerId id, float opacity);

    /// @brief A row's name was double-clicked.
    void renameRequested(sound_mind::core::LayerId id);

    /// @brief A row's delete button was clicked.
    void deleteRequested(sound_mind::core::LayerId id);

    /**
     * @brief A drag-reorder finished with a valid result (a locked
     *        layer's position wasn't disturbed - see the class docs).
     *
     * An invalid drag (one that would have moved a locked layer) is
     * rejected internally instead: the panel snaps its own display back
     * to the last `setLayers()` call's order, and this signal never
     * fires.
     *
     * @param newOrderBottomToTop Every layer id, in the new order -
     *        `Project::layers()`'s own bottom-to-top convention.
     */
    void reorderRequested(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop);

private slots:
    /// @brief `list_`'s `rowsMoved` handler - validates the drag (see
    ///        reorderRequested()'s docs) and either emits it or reverts
    ///        the display back to `currentRows_`.
    void handleRowsMoved();

private:
    QListWidget* list_ = nullptr;

    /// @brief The rows as of the last setLayers() call, bottom-to-top -
    /// used to rebuild the list display when an invalid drag is rejected,
    /// and to recover each row's id/type from handleRowsMoved()'s new
    /// visual order (the QListWidgetItems themselves carry the id via
    /// Qt::UserRole, but not the type - see handleRowsMoved()'s impl).
    std::vector<RowData> currentRows_;
};

}  // namespace sound_mind::studio
