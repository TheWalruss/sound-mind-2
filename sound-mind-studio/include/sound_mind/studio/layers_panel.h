#pragma once

#include <cstdint>
#include <optional>
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
 * per-row visibility toggle, name, opacity, add/delete - the "add" half
 * is a "+ Add Layer" button above the list, added once painting existed
 * to give a new layer content; see `MainWindow::addEmptyLayer()`'s own
 * docs), scoped down to
 * what `sound_mind::core::Layer` actually supports today: no blend-mode
 * combo, no MindWave-link combo, and no settings/gear button - none of
 * those concepts exist in the engine yet. As of `v0.Y.21.1` (Layer Time
 * Alignment), two transform controls *do* exist - translation and
 * rescale, both horizontal-axis-only (see `sound_mind::core::Layer`'s own
 * docs for the narrower-than-legacy scope).
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
 * "locked bottom layer" treatment) and no opacity slider or transform
 * controls at all (confirmed with the user: neither concept applies to
 * the always-opaque, always-first-in-time floor of the stack).
 *
 * As of Basic Painting's layer-selection fix, a single row can be the
 * "active layer" - clicking a row's name selects it (single-click, not
 * the existing double-click-to-rename), shown via the list's own native
 * selection highlight. Unlike every other row action above, this is
 * *not* forwarded as a signal for `MainWindow` to apply to the `Project`
 * - there's nothing in `Project`/`Layer` for a selection to mutate, it's
 * purely local UI state a caller (`MainWindow::paintTargetLayerId()`)
 * reads back via selectedLayerId(). setLayers() preserves it across a
 * refresh as long as the same id is still present in the new rows, and
 * drops it (back to `std::nullopt`) the moment it isn't - e.g. the
 * selected layer was just deleted. A caller switching to an entirely
 * different `Project` must still call clearSelection() itself *before*
 * the switch's own setLayers() call, since a new project's own
 * `LayerId`s can coincidentally reuse values from the old one - a
 * same-numbered id in the new rows would otherwise be mistaken for the
 * old selection surviving the switch, rather than a fresh coincidence.
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

        /// @brief Mirrors `sound_mind::core::Layer::translationColumns()`.
        std::int64_t translationColumns = 0;

        /// @brief Mirrors `sound_mind::core::Layer::rescaleFactor()`.
        double rescaleFactor = 1.0;
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

    /// @brief The currently selected row's layer id, if any - see the
    ///        class's own docs on what "selected" means here.
    /// @return That layer's id, or `std::nullopt` if no row is selected.
    [[nodiscard]] std::optional<sound_mind::core::LayerId> selectedLayerId() const { return selectedLayerId_; }

    /// @brief Clears the current selection - see the class's own docs on
    ///        why a caller switching `Project`s must call this itself.
    void clearSelection();

    /// @brief Selects `id` as if its row had been clicked - a no-op if no
    ///        row currently has that id. Used by `MainWindow` to
    ///        immediately select a layer it just added (see
    ///        addLayerRequested()'s own docs), without requiring an extra
    ///        click before it can be painted into.
    /// @param id The layer to select.
    void selectLayer(sound_mind::core::LayerId id);

signals:
    /// @brief A row's visibility toggle was clicked.
    void visibilityToggled(sound_mind::core::LayerId id, bool visible);

    /// @brief A row's opacity slider changed.
    void opacityChanged(sound_mind::core::LayerId id, float opacity);

    /// @brief A row's translation spin box changed - see
    /// `sound_mind::core::Layer::translationColumns()`'s docs.
    void translationChanged(sound_mind::core::LayerId id, std::int64_t translationColumns);

    /// @brief A row's rescale spin box changed - see
    /// `sound_mind::core::Layer::rescaleFactor()`'s docs.
    void rescaleChanged(sound_mind::core::LayerId id, double rescaleFactor);

    /// @brief A row's name was double-clicked.
    void renameRequested(sound_mind::core::LayerId id);

    /// @brief A row's delete button was clicked.
    void deleteRequested(sound_mind::core::LayerId id);

    /// @brief The "+ Add Layer" button was clicked - `MainWindow` responds
    ///        by adding a new, silent, project-sized `Normal` layer (see
    ///        `MainWindow::addEmptyLayer()`'s own docs) and selecting it
    ///        via selectLayer(), ready to paint into immediately.
    void addLayerRequested();

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

    /// @brief See selectedLayerId()'s own docs.
    std::optional<sound_mind::core::LayerId> selectedLayerId_;
};

}  // namespace sound_mind::studio
