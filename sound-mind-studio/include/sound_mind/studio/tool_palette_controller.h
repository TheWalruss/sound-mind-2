#pragma once

#include <optional>
#include <string>

#include <QObject>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/grid_config.h"
#include "sound_mind/studio/selection_controller.h"

namespace sound_mind::studio {

class CanvasWidget;
class PaintController;
class PathController;
class PickController;
class SelectionController;
class ToolConfigurationPanel;
class UndoStack;

/**
 * @brief Owns the Paint/Pick/Select/Path tool controllers, and all of
 *        their wiring to `CanvasWidget`/`ToolConfigurationPanel` - extracted
 *        out of `MainWindow` as part of the Refactor & Clean Up milestone
 *        (`v0.Y.29.1`, Installment C), the same "own presentation" treatment
 *        `PlaybackController` (`v0.Y.23.1`) already received.
 *
 * Constructed with non-owning pointers to `canvas` and
 * `toolConfigurationPanel` - both stay owned by `MainWindow` (a
 * `CanvasWidget` is the window's own central widget; a
 * `ToolConfigurationPanel` is a dock widget only `QMainWindow::
 * addDockWidget()` can place) - and wires every signal *between* them and
 * its own four sub-controllers internally: `canvas_`'s own "continued"/
 * "ended" gesture signals straight to whichever controller is doing the
 * work, and each controller's own `pathChanged()`/`selectionChanged()`/
 * `boundsChanged()` straight back to `canvas_`'s preview/selection-overlay
 * setters or `toolConfigurationPanel_`'s own state.
 *
 * **Deliberately does not own the four toolbar `QAction`s, or decide tool-
 * mode exclusivity** - `MainWindow`'s own `setExclusiveToolMode()` still
 * does that (a toolbar/menu concern, not a tool-palette one), calling into
 * this class's own `cancelPaintStroke()`/`clearPickSelection()`/
 * `cancelSelectionDrag()`/`cancelPathPlacement()` first. **Deliberately
 * does not resolve "which layer" a freehand gesture starting right now
 * targets** either - `MainWindow::paintTargetLayerId()` stays that
 * decision's own owner (it depends on `LayersPanel`'s current selection,
 * a concept this class has no reason to know about), so
 * `beginPaintStroke()`/`beginPick()`/`beginSelectionDrag()`/`placePathNode()`
 * each take the resolved layer id as a plain parameter rather than
 * resolving it themselves.
 */
class ToolPaletteController : public QObject {
    Q_OBJECT

public:
    /**
     * @param canvas Non-owning; wired for input gesture signals and
     *        receives preview/selection-overlay updates. Must outlive this
     *        controller.
     * @param toolConfigurationPanel Non-owning; supplies the initial/
     *        changed tool configuration and receives Pick's own "load the
     *        picked object's settings" update. Must outlive this
     *        controller.
     * @param undoStack Non-owning; passed straight through to the internally-
     *        constructed `PaintController` (see its own docs on why this is
     *        nullable). Must outlive this controller if given.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    ToolPaletteController(CanvasWidget* canvas, ToolConfigurationPanel* toolConfigurationPanel,
                           UndoStack* undoStack = nullptr, QObject* parent = nullptr);

    /**
     * @brief Sets which project every one of the four tool controllers
     *        targets - forwards to each of their own `setProject()`, and
     *        to `toolConfigurationPanel_->setProject()` (so its own Mind
     *        Shot picker draws from the right project's own library).
     * @param project The project to target; may be `nullptr`.
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Sets Pick's and Selection's own grid-snapping state -
     *        forwards to each of their own `setGridSnapping()`.
     * @param enabled Whether Snap to Grid is currently on.
     * @param frequencyGridConfig The frequency axis's own current grid.
     * @param timingGridConfig The time axis's own current grid.
     */
    void setGridSnapping(bool enabled, const FrequencyGridConfig& frequencyGridConfig,
                          const TimingGridConfig& timingGridConfig);

    /// @brief Starts a freehand paint stroke - forwards to
    ///        `PaintController::beginStroke()`.
    /// @param layer Which layer to paint into - resolved by the caller
    ///        (`MainWindow::paintTargetLayerId()`), not by this class.
    /// @param point Where the stroke starts.
    void beginPaintStroke(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Starts a Pick gesture - forwards to `PickController::pick()`.
    /// @param layer Which layer to pick within - resolved by the caller,
    ///        not by this class.
    /// @param point Where the gesture starts.
    void beginPick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Starts a Select drag - forwards to `SelectionController::
    ///        beginSelectionDrag()`.
    /// @param layer Which layer to select within - resolved by the
    ///        caller, not by this class.
    /// @param point Where the drag starts.
    void beginSelectionDrag(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Sets which shape the *next* Select drag produces - forwards
    ///        to `SelectionController::setSelectionShape()`. The actual
    ///        work behind the Selection Configuration Panel's own
    ///        Selection Type dropdown.
    /// @param shape The shape to draw next.
    void setSelectionShape(SelectionShape shape);

    /// @brief Places the next Path tool node - forwards to
    ///        `PathController::placeNode()`.
    /// @param layer Which layer the path is being placed onto - resolved
    ///        by the caller, not by this class.
    /// @param point Where to place the node.
    void placePathNode(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Cancels any in-progress freehand stroke - forwards to
    ///        `PaintController::cancelStroke()`. Called by `MainWindow`
    ///        before turning Paint mode off.
    void cancelPaintStroke();

    /// @brief Clears the current Pick selection - forwards to
    ///        `PickController::clearSelection()`. Called by `MainWindow`
    ///        before turning Pick mode off.
    void clearPickSelection();

    /// @brief Cancels any in-progress selection drag - forwards to
    ///        `SelectionController::cancelSelectionDrag()`. Called by
    ///        `MainWindow` before turning Select mode off.
    void cancelSelectionDrag();

    /// @brief Discards any in-progress Path node placement - forwards to
    ///        `PathController::cancelPath()`. Called by `MainWindow`
    ///        before turning Path mode off.
    void cancelPathPlacement();

    /// @brief Undoes the most recent paint stroke - forwards to
    ///        `PaintController::undo()`.
    void undo();

    /// @brief Redoes the most recently undone paint stroke - forwards to
    ///        `PaintController::redo()`.
    void redo();

    /// @brief Deletes the currently Picked object - forwards to
    ///        `PickController::deleteSelection()`.
    void deleteSelection();

    /// @brief Moves the currently Picked object to the top of its own
    ///        layer's stack - forwards to `PickController::bringToFront()`.
    void bringToFront();

    /// @brief Moves the currently Picked object to the bottom of its own
    ///        layer's stack - forwards to `PickController::sendToBack()`.
    void sendToBack();

    /// @brief Swaps the currently Picked object forward one position -
    ///        forwards to `PickController::bringForward()`.
    void bringForward();

    /// @brief Swaps the currently Picked object backward one position -
    ///        forwards to `PickController::sendBackward()`.
    void sendBackward();

    /// @brief Enters direct node/handle editing of the currently Picked
    ///        stroke's own Path - forwards to
    ///        `PickController::beginPathEdit()`.
    void beginPathEdit();

    /// @brief Converts the currently selected path-edit node between
    ///        `Corner` and `Smooth` - forwards to `PickController::
    ///        toggleSelectedPathNodeType()`.
    void toggleSelectedPathNodeType();

    /// @brief Commits the active path edit - forwards to
    ///        `PickController::commitPathEdit()`.
    void commitPathEdit();

    /// @brief Discards the active path edit - forwards to
    ///        `PickController::cancelPathEdit()`.
    void cancelPathEdit();

    /// @brief Clears the current rectangular selection - forwards to
    ///        `SelectionController::clearSelection()`.
    void clearSelection();

    /// @brief Fills the current selection with `gradient` - forwards to
    ///        `SelectionController::fill()`.
    /// @param gradient The color (or gradient) to fill with.
    void fill(const sound_mind::core::Gradient& gradient);

    /// @brief Whether there's a committed selection right now - forwards
    ///        to `SelectionController::hasSelection()`.
    /// @return `true` if there's a selection to fill/copy/cut.
    [[nodiscard]] bool hasSelection() const;

    /// @brief Copies the current selection's own pixels onto the
    ///        clipboard - forwards to `SelectionController::
    ///        copySelection()`.
    void copySelection();

    /// @brief Copies the current selection and clears its own source
    ///        pixels - forwards to `SelectionController::cutSelection()`.
    void cutSelection();

    /// @brief Captures the current selection into a new, named Mind Shot -
    ///        forwards to `SelectionController::captureMindShot()`.
    /// @param name Display name for the new library entry.
    /// @return The new entry's own id, or `std::nullopt` if there was no
    ///         committed selection to capture.
    std::optional<sound_mind::core::MindShotId> captureMindShot(const std::string& name);

    /// @brief Captures the current selection's own `{layer, bounds}` into a
    ///        new, named Mind Grain - forwards to
    ///        `SelectionController::captureMindGrain()`.
    /// @param name Display name for the new library entry.
    /// @return The new entry's own id, or `std::nullopt` if there was no
    ///         committed selection to capture.
    std::optional<sound_mind::core::MindGrainId> captureMindGrain(const std::string& name);

    /// @brief Pastes the clipboard onto `targetLayer` - forwards to
    ///        `SelectionController::pasteInto()`.
    /// @param targetLayer Which layer to paste into.
    /// @return The newly-created `PasteOperation`'s own id, or
    ///         `std::nullopt` if there was nothing on the clipboard.
    [[nodiscard]] std::optional<sound_mind::core::OperationId> pasteInto(sound_mind::core::LayerId targetLayer);

    /// @brief Selects a specific operation as Pick's own current
    ///        selection - forwards to `PickController::selectOperation()`.
    /// @param layer The operation's own target layer.
    /// @param operationId The operation to select.
    void selectOperation(sound_mind::core::LayerId layer, sound_mind::core::OperationId operationId);

    /// @brief Finishes the Path tool's own in-progress node placement -
    ///        forwards to `PathController::finishPath()`.
    void finishPath();

    /// @brief Discards the Path tool's own in-progress node placement -
    ///        forwards to `PathController::cancelPath()`.
    void cancelPath();

    /// @brief Sets which node type the Path tool places next - forwards
    ///        to `PathController::setDefaultNodeType()`.
    /// @param smooth `true` for `PathNodeType::Smooth`; `false` for
    ///        `PathNodeType::Corner`.
    void setPathPlacesSmoothNodes(bool smooth);

signals:
    /// @brief Emitted whenever any of the four tool controllers changes a
    ///        layer's own rendered content, as a result of painting,
    ///        picking, filling, pasting, or any of their own undo()/
    ///        redo() - merges all four controllers' own `contentChanged()`
    ///        signals into one, so `MainWindow` needs only a single
    ///        connection (to mark `hasUnsavedChanges()`/refresh the Layers
    ///        Panel) instead of four. This controller has already called
    ///        `canvas_->update()` itself by the time this is emitted.
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    CanvasWidget* canvas_;
    ToolConfigurationPanel* toolConfigurationPanel_;

    PaintController* paintController_ = nullptr;
    PickController* pickController_ = nullptr;
    SelectionController* selectionController_ = nullptr;
    PathController* pathController_ = nullptr;
};

}  // namespace sound_mind::studio
