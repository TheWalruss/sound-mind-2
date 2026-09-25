#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <QImage>
#include <QObject>
#include <QString>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

class CanvasWidget;
class FilterConfigurationPanel;
class LayersPanel;
class PlaybackController;
class UndoStack;

/**
 * @brief Owns layer-stack lookup, mutation, and `LayersPanel`/
 *        `FilterConfigurationPanel` refresh - extracted out of `MainWindow`
 *        as part of the Refactor & Clean Up milestone (`v0.Y.29.1`,
 *        Installment D), the same "own presentation" treatment
 *        `PlaybackController`/`ToolPaletteController` already received.
 *
 * Constructed with non-owning pointers to `canvas`, `playbackController`,
 * `layersPanel`, and `filterConfigurationPanel` - all four stay owned by
 * `MainWindow` (a `CanvasWidget` is the window's own central widget; the
 * two panels are dock widgets only `QMainWindow::addDockWidget()` can
 * place; `PlaybackController` is itself already a sibling extraction) -
 * and calls into all four itself wherever a mutation needs to refresh the
 * canvas, invalidate cached playback audio, or push the current layer
 * stack into the panel.
 *
 * **Deliberately does not own `LayersPanel`'s/`FilterConfigurationPanel`'s
 * own signal connections** - `MainWindow`'s own existing `connect()` calls
 * (wired to its own public slots, unchanged) still route every panel
 * gesture here; this class's own public methods are what those slots now
 * forward to, one call each. **Deliberately does not know about
 * `LoopEngine`** either - `addEmptyLayer()` takes its own placeholder
 * content as a parameter rather than deriving it itself, since only
 * `MainWindow` holds the current project's `LoopEngine` (and only it needs
 * to, for Loop Mode's own reasons).
 *
 * **Six property setters are undoable** - `toggleLayerVisibility()`,
 * `setLayerOpacity()`, `setLayerOpacityMindWave()`, `setLayerTranslation()`,
 * `setLayerRescale()`, `setLayerBlendMode()` each push a matching
 * `UndoCommand` onto the shared
 * `UndoStack` after applying the change, so `MainWindow`'s Edit > Undo/Redo
 * covers them - see `UndoStack`'s own class docs for why this is a
 * separate mechanism from `sound_mind::core::OperationLog`. The other
 * mutations here (rename, delete, add, reorder, `FilterConfiguration`
 * edits) are **not** undoable yet - a deliberately narrower scope than
 * "every layer mutation," confirmed with the user alongside this fix.
 */
class LayerController : public QObject {
    Q_OBJECT

public:
    /**
     * @param canvas Non-owning; repainted after any mutation that changes
     *        what the canvas shows. Must outlive this controller.
     * @param playbackController Non-owning; invalidated after any
     *        mutation that could change the project's own composited
     *        audio. Must outlive this controller.
     * @param layersPanel Non-owning; refreshed after any mutation, and
     *        queried for the current row selection. Must outlive this
     *        controller.
     * @param filterConfigurationPanel Non-owning; kept in sync with
     *        whichever Filter/Equalizer layer is currently selected. Must
     *        outlive this controller.
     * @param undoStack Non-owning; every undoable property setter (see
     *        the class's own docs) pushes onto it. Must outlive this
     *        controller.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    LayerController(CanvasWidget* canvas, PlaybackController* playbackController, LayersPanel* layersPanel,
                     FilterConfigurationPanel* filterConfigurationPanel, UndoStack* undoStack,
                     QObject* parent = nullptr);

    /// @brief Sets which project this controller looks up/mutates layers
    ///        in. Does *not* itself refresh the Layers Panel - `MainWindow`'s
    ///        own `setProject()` calls `refreshLayersPanel()` separately,
    ///        once its own per-project reset (show/hide, `hasUnsavedChanges()`,
    ///        etc.) is otherwise complete.
    /// @param project The project to target; may be `nullptr` (every
    ///        lookup/mutation below becomes a no-op/`nullptr`/
    ///        `std::nullopt` until `refreshLayersPanel()` is next called
    ///        to reflect it).
    void setProject(sound_mind::core::Project* project);

    /// @brief The topmost *visible* layer with content, if any - the same
    ///        notion of "the composite" `exportTopmostLayerAudioNow()`/
    ///        `exportTopmostLayerVideoNow()`/`poolTopmostLayerNow()` all
    ///        share.
    /// @return A mutable pointer to that layer, or `nullptr` if none
    ///         qualifies, or no project is set.
    [[nodiscard]] sound_mind::core::Layer* topmostLayerWithContent();

    /// @brief The layer with the given id, if the current project has one.
    /// @param id The layer to look up.
    /// @return A mutable pointer to that layer, or `nullptr` if no
    ///         project is set or no layer in it has this id.
    [[nodiscard]] sound_mind::core::Layer* layerById(sound_mind::core::LayerId id);

    /**
     * @brief Which layer a freehand stroke started right now would paint
     *        into.
     *
     * `layersPanel`'s own selected row, if any - the "active layer" a
     * user has actually clicked. Falls back to the *bottommost* layer in
     * the stack (the Background layer, always present) whenever nothing
     * is selected - a fresh project, or a selection that was cleared -
     * not `topmostLayerWithContent()` (which requires existing content
     * and would make a fresh, still-empty new layer unpaintable), and not
     * `.back()` (always the locked, content-less Equalizer layer, never a
     * sensible paint target).
     *
     * @return That layer's id, or `std::nullopt` if no project is set.
     */
    [[nodiscard]] std::optional<sound_mind::core::LayerId> paintTargetLayerId() const;

    /**
     * @brief Pushes the current project's layer stack into the Layers
     *        Panel - an empty list (not a no-op) when no project is set.
     *
     * Each row's own thumbnail (`v0.Y.44.1`, Layers Panel Redesign) comes
     * from a per-layer cache, not a fresh render every call - refreshing
     * runs after nearly every canvas edit (`ToolPaletteController::
     * contentChanged()`'s own connection in `MainWindow`), so re-rendering
     * *every* layer's own thumbnail on *every* edit would cost real,
     * needless work as layer count grows, for every layer except the one
     * that actually just changed.
     *
     * @param changedContentLayer If given, that layer's own cached
     *        thumbnail is discarded first, so this call re-renders a fresh
     *        one for it - pass the id of whichever layer a paint/pick/
     *        fill/paste/chord-stamp/undo/redo operation just changed
     *        (see `ToolPaletteController::contentChanged()`'s own docs).
     *        `std::nullopt` (the default) leaves every cached thumbnail
     *        as-is - correct for every mutation here that doesn't touch a
     *        layer's own raw content (opacity, translation, rescale, blend
     *        mode, visibility, rename, add, delete, reorder - none of
     *        which change what `Layer::content()` itself holds).
     */
    void refreshLayersPanel(std::optional<sound_mind::core::LayerId> changedContentLayer = std::nullopt);

    /// @brief Sets whether the layer with the given id contributes to the
    ///        project. Repaints the canvas and invalidates cached
    ///        playback audio ("topmost layer with content" may have
    ///        changed), then refreshes the Layers Panel. Does nothing if
    ///        no layer with this id exists. **Undoable** - a no-op call
    ///        (the same visibility it already had) still applies but
    ///        pushes no undo entry (see the class's own docs).
    /// @param id The layer to change.
    /// @param visible The new visibility.
    void toggleLayerVisibility(sound_mind::core::LayerId id, bool visible);

    /// @brief Sets the opacity of the layer with the given id. Repaints
    ///        the canvas, then refreshes the Layers Panel. Does nothing if
    ///        no layer with this id exists. **Undoable** - see
    ///        toggleLayerVisibility()'s own docs on no-op calls.
    /// @param id The layer to change.
    /// @param opacity The new opacity, intended to be in `[0, 1]`.
    void setLayerOpacity(sound_mind::core::LayerId id, float opacity);

    /// @brief Sets (or clears) which MindWave the layer with the given id's
    ///        own opacity is bound to - see `sound_mind::core::Layer::
    ///        opacityMindWave()`'s own docs. Repaints the canvas (the
    ///        binding changes what the composite actually looks like),
    ///        then refreshes the Layers Panel. Does nothing if no layer
    ///        with this id exists. **Undoable** - see
    ///        toggleLayerVisibility()'s own docs on no-op calls.
    /// @param id The layer to change.
    /// @param mindWaveId The new binding, or `std::nullopt` to unbind.
    void setLayerOpacityMindWave(sound_mind::core::LayerId id,
                                  std::optional<sound_mind::core::MindWaveId> mindWaveId);

    /// @brief Sets the horizontal translation of the layer with the given
    ///        id. Repaints the canvas, then refreshes the Layers Panel.
    ///        Does nothing if no layer with this id exists. **Undoable** -
    ///        see toggleLayerVisibility()'s own docs on no-op calls.
    /// @param id The layer to change.
    /// @param translationColumns The new shift, in spectrogram columns.
    void setLayerTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns);

    /// @brief Sets the horizontal rescale of the layer with the given id.
    ///        Repaints the canvas, then refreshes the Layers Panel. Does
    ///        nothing if no layer with this id exists. **Undoable** - see
    ///        toggleLayerVisibility()'s own docs on no-op calls.
    /// @param id The layer to change.
    /// @param rescaleFactor The new ratio.
    void setLayerRescale(sound_mind::core::LayerId id, double rescaleFactor);

    /// @brief Sets the blend mode of the layer with the given id - see
    ///        `sound_mind::core::Layer::blendMode()`'s own docs.
    ///        `v0.Y.37.1` (Deferred Blend Modes). Repaints the canvas (the
    ///        blend mode changes what the composite actually looks like),
    ///        then refreshes the Layers Panel. Does nothing if no layer
    ///        with this id exists. **Undoable** - see
    ///        toggleLayerVisibility()'s own docs on no-op calls.
    /// @param id The layer to change.
    /// @param mode The new blend mode.
    void setLayerBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode);

    /// @brief Renames the layer with the given id, without prompting -
    ///        the non-prompting core behind `MainWindow::renameLayer()`'s
    ///        own `QInputDialog`. Refreshes the Layers Panel on success.
    ///        The applied name is passed through
    ///        `Project::uniqueLayerName()` first (`v0.Y.44.1`, Layers Panel
    ///        Redesign) - excluding `id` itself from the collision check,
    ///        so renaming a layer to the exact name it already has is a
    ///        no-op rename, not a needless "(2)" suffix - so the layer may
    ///        end up with a slightly different name than `newName` if it
    ///        collided with another layer's own current name.
    /// @param id The layer to rename.
    /// @param newName The new name - an empty name is rejected.
    /// @return `true` on success; `false` if no layer with this id
    ///         exists, or `newName` is empty.
    bool renameLayerTo(sound_mind::core::LayerId id, const QString& newName);

    /// @brief Deletes the layer with the given id. Refuses (no-op) for a
    ///        `Background`/`Equalizer` layer, or if no layer with this id
    ///        exists. Also refuses - showing an explanatory modal instead -
    ///        if any active Mind Grain stroke elsewhere in the project
    ///        reads its own content live from this layer (see
    ///        `sound_mind::core::mindGrainOperationsBrokenByRemovingLayer()`'s
    ///        own docs); the deletion is cancelled outright, not just
    ///        warned about. Repaints the canvas and invalidates cached
    ///        playback audio, then refreshes the Layers Panel, on success.
    /// @param id The layer to delete.
    void deleteLayer(sound_mind::core::LayerId id);

    /// @brief Adds a new, empty `Normal` layer to the current project,
    ///        selecting it immediately in the Layers Panel. Repaints the
    ///        canvas and invalidates cached playback audio, then
    ///        refreshes the Layers Panel. A no-op if no project is set.
    /// @param placeholderContent The new layer's own starting content - a
    ///        silent, project-dimensioned placeholder (`MainWindow`'s own
    ///        `LoopEngine::emptyImage()`, in practice - this class knows
    ///        nothing about `LoopEngine` itself).
    void addEmptyLayer(sound_mind::codec::StreamImage placeholderContent);

    /// @brief Adds a new `Filter`-type layer to the current project,
    ///        seeded from `pendingFilterConfiguration()` (see its own
    ///        docs) rather than a blank default, and selecting it
    ///        immediately in the Layers Panel. Repaints the canvas and
    ///        invalidates cached playback audio, then refreshes the
    ///        Layers Panel. A no-op if no project is set.
    void addFilterLayer();

    /// @brief Reacts to the Layers Panel's own selection changing -
    ///        keeps `FilterConfigurationPanel` in sync. Loads the newly
    ///        selected layer's own configuration, in the right display
    ///        mode, if it's a Filter/Equalizer layer; otherwise loads
    ///        `pendingFilterConfiguration()` instead (real-world testing
    ///        pass, 2026-09-20, finding #13) - the panel stays enabled
    ///        either way (as long as a project is set), rather than
    ///        disabling whenever the selection isn't a Filter/Equalizer
    ///        layer, so a filter can be configured before one exists.
    /// @param id The newly selected layer's id, or `std::nullopt` if the
    ///        selection was cleared.
    void handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id);

    /// @brief Applies `FilterConfigurationPanel`'s own edited
    ///        configuration. If the panel is currently editing a real,
    ///        still-selected Filter/Equalizer layer, writes onto that
    ///        layer directly (repainting the canvas and invalidating
    ///        cached playback audio, as before). Otherwise (real-world
    ///        testing pass, 2026-09-20, finding #13) updates
    ///        `pendingFilterConfiguration()` instead, so an edit made with
    ///        nothing (or a non-Filter layer) selected isn't simply
    ///        discarded - it seeds whatever Filter layer gets added next.
    ///        A no-op only if no project is set at all.
    /// @param config The panel's own new, complete configuration.
    void applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config);

    /// @brief The configuration `addFilterLayer()` will seed its next new
    ///        layer with, and what `FilterConfigurationPanel` shows/edits
    ///        whenever the current selection isn't a real Filter/Equalizer
    ///        layer - real-world testing pass, 2026-09-20, finding #13
    ///        ("allow editing Filter Configuration before a Filter layer
    ///        is added, not only after"). Resets to a fresh default in
    ///        `setProject()`; otherwise persists across however many
    ///        `addFilterLayer()` calls consume it, so dialing in a favorite
    ///        setting once seeds every filter added afterward, not just
    ///        the next one.
    /// @return The current pending configuration.
    [[nodiscard]] const sound_mind::core::FilterConfiguration& pendingFilterConfiguration() const noexcept {
        return pendingFilterConfiguration_;
    }

    /// @brief Reorders the current project's layer stack. Refuses -
    ///        showing an explanatory modal, and cancelling outright rather
    ///        than just warning - if the requested order would put any
    ///        active Mind Grain stroke's own target layer at-or-below its
    ///        own source layer (see
    ///        `sound_mind::core::mindGrainOperationsBrokenByReorder()`'s own
    ///        docs). Refreshes the Layers Panel either way (even a
    ///        rejected reorder needs the panel snapped back to the
    ///        authoritative order); repaints the canvas only if the
    ///        reorder actually applied.
    /// @param newOrderBottomToTop Every current layer's id, exactly once
    ///        each, in the desired new bottom-to-top order.
    void reorderLayers(const std::vector<sound_mind::core::LayerId>& newOrderBottomToTop);

signals:
    /// @brief Emitted whenever a mutation above actually took effect -
    ///        `MainWindow`'s own cue to mark `hasUnsavedChanges()`.
    void layersChanged();

private:
    /// @brief The actual visibility mutation + side effects, shared by
    ///        toggleLayerVisibility() and its own pushed UndoCommand's
    ///        undo()/redo() callbacks - see the class's own docs.
    void applyVisibility(sound_mind::core::LayerId id, bool visible);

    /// @brief The actual opacity mutation + side effects, shared by
    ///        setLayerOpacity() and its own pushed UndoCommand's
    ///        undo()/redo() callbacks - see the class's own docs.
    void applyOpacity(sound_mind::core::LayerId id, float opacity);

    /// @brief The actual opacity-MindWave-binding mutation + side effects,
    ///        shared by setLayerOpacityMindWave() and its own pushed
    ///        UndoCommand's undo()/redo() callbacks - see the class's own
    ///        docs.
    void applyOpacityMindWave(sound_mind::core::LayerId id, std::optional<sound_mind::core::MindWaveId> mindWaveId);

    /// @brief The actual translation mutation + side effects, shared by
    ///        setLayerTranslation() and its own pushed UndoCommand's
    ///        undo()/redo() callbacks - see the class's own docs.
    void applyTranslation(sound_mind::core::LayerId id, std::int64_t translationColumns);

    /// @brief The actual rescale mutation + side effects, shared by
    ///        setLayerRescale() and its own pushed UndoCommand's
    ///        undo()/redo() callbacks - see the class's own docs.
    void applyRescale(sound_mind::core::LayerId id, double rescaleFactor);

    /// @brief The actual blend mode mutation + side effects, shared by
    ///        setLayerBlendMode() and its own pushed UndoCommand's
    ///        undo()/redo() callbacks - see the class's own docs.
    void applyBlendMode(sound_mind::core::LayerId id, sound_mind::core::BlendMode mode);

    /// @brief `layer`'s own thumbnail (`v0.Y.44.1`, Layers Panel Redesign) -
    ///        `thumbnailCache_`'s cached image if one exists, or a freshly
    ///        rendered one (cached for next time) otherwise. A null `QImage`
    ///        for a layer with no content at all (Filter/Background/
    ///        Equalizer, or a brand-new empty layer) - never cached, so a
    ///        later `setContent()` on the same layer id renders a real
    ///        thumbnail the next time this is asked for it.
    /// @param layer The layer to get a thumbnail for.
    /// @return That layer's own thumbnail, or a null `QImage`.
    [[nodiscard]] QImage thumbnailFor(const sound_mind::core::Layer& layer);

    CanvasWidget* canvas_;
    PlaybackController* playbackController_;
    LayersPanel* layersPanel_;
    FilterConfigurationPanel* filterConfigurationPanel_;
    UndoStack* undoStack_;
    sound_mind::core::Project* project_ = nullptr;

    /// @brief See pendingFilterConfiguration()'s own docs.
    sound_mind::core::FilterConfiguration pendingFilterConfiguration_;

    /// @brief See thumbnailFor()'s own docs - discarded per-id by
    ///        refreshLayersPanel()'s own `changedContentLayer` parameter,
    ///        and per-id by deleteLayer() on removal (tidiness, not
    ///        correctness - a stale entry for a since-deleted id is simply
    ///        never looked up again).
    std::unordered_map<sound_mind::core::LayerId, QImage> thumbnailCache_;
};

}  // namespace sound_mind::studio
