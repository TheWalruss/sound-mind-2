#pragma once

#include <cstdint>
#include <optional>
#include <vector>

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
 * **Five property setters are undoable** - `toggleLayerVisibility()`,
 * `setLayerOpacity()`, `setLayerOpacityMindWave()`, `setLayerTranslation()`,
 * `setLayerRescale()` each push a matching `UndoCommand` onto the shared
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

    /// @brief Pushes the current project's layer stack into the Layers
    ///        Panel - an empty list (not a no-op) when no project is set.
    void refreshLayersPanel();

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

    /// @brief Renames the layer with the given id, without prompting -
    ///        the non-prompting core behind `MainWindow::renameLayer()`'s
    ///        own `QInputDialog`. Refreshes the Layers Panel on success.
    /// @param id The layer to rename.
    /// @param newName The new name - an empty name is rejected.
    /// @return `true` on success; `false` if no layer with this id
    ///         exists, or `newName` is empty.
    bool renameLayerTo(sound_mind::core::LayerId id, const QString& newName);

    /// @brief Deletes the layer with the given id. Refuses (no-op) for a
    ///        `Background`/`Equalizer` layer, or if no layer with this id
    ///        exists. Repaints the canvas and invalidates cached playback
    ///        audio, then refreshes the Layers Panel, on success.
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
    ///        selecting it immediately in the Layers Panel. Repaints the
    ///        canvas and invalidates cached playback audio, then
    ///        refreshes the Layers Panel. A no-op if no project is set.
    void addFilterLayer();

    /// @brief Reacts to the Layers Panel's own selection changing -
    ///        keeps `FilterConfigurationPanel` in sync (loads the newly
    ///        selected layer's own configuration, in the right display
    ///        mode, if it's a Filter/Equalizer layer; disables the panel
    ///        entirely otherwise).
    /// @param id The newly selected layer's id, or `std::nullopt` if the
    ///        selection was cleared.
    void handleLayerSelectionChanged(std::optional<sound_mind::core::LayerId> id);

    /// @brief Applies `FilterConfigurationPanel`'s own edited
    ///        configuration back onto whichever layer it's currently
    ///        editing. Repaints the canvas and invalidates cached
    ///        playback audio. A no-op if no project is set, or the panel
    ///        isn't currently editing a real, still-selected Filter/
    ///        Equalizer layer.
    /// @param config The panel's own new, complete configuration.
    void applyFilterConfiguration(const sound_mind::core::FilterConfiguration& config);

    /// @brief Reorders the current project's layer stack. Refreshes the
    ///        Layers Panel either way (even a rejected reorder needs the
    ///        panel snapped back to the authoritative order); repaints
    ///        the canvas only if the reorder actually applied.
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

    CanvasWidget* canvas_;
    PlaybackController* playbackController_;
    LayersPanel* layersPanel_;
    FilterConfigurationPanel* filterConfigurationPanel_;
    UndoStack* undoStack_;
    sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
