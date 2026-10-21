#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "sound_mind/studio/text_controller.h"
#include "sound_mind/core/resonant_instrument.h"
#include "sound_mind/core/filter_preview.h"
#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/filter_configuration.h"
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
class ChordGeneratorController;
class PaintController;
class PathController;
class TextController;
class PickController;
class SelectionController;
class ToolConfigurationPanel;
class ProjectHistory;

/**
 * @brief Owns the Paint/Pick/Select/Path tool controllers, and all of
 *        their wiring to `CanvasWidget`/`ToolConfigurationPanel` - extracted
 *        out of `MainWindow` as part of the Refactor & Clean Up milestone
 *        (`v0.Y.29.1`, Installment C), the same "own presentation" treatment
 *        `PlaybackController` (`v0.Y.23.1`) already received.
 *
 * **As of `v0.0.40.2` (Chords/Arpeggiator/Sequencer, Installment B):** also
 * owns a fifth, `ChordGeneratorController` - constructed the same way
 * `pickController_`/`selectionController_`/`pathController_` are (sharing
 * `paintController_`'s own tool configuration - see
 * `ChordGeneratorController`'s own docs), with its own `previewChanged()`
 * wired straight to `canvas_->setChordPreview()` and its own
 * `contentChanged()` merged into this class's own, the same "internal
 * wiring, one merged signal out" treatment every other sub-controller
 * already gets. Unlike the other four, it has no toolbar `QAction`/tool
 * mode of its own to cancel on deactivation (`ChordStamp` mode's own single-
 * press gesture has no in-progress state to begin with - see
 * `CanvasWidget::ToolMode::ChordStamp`'s own docs) and isn't driven by
 * `ToolConfigurationPanel` at all, only by the separate `ChordGeneratorPanel`
 * (via `setChordParams()`, called directly by `MainWindow` - this class
 * doesn't hold a pointer to that panel the way it does `toolConfigurationPanel_`,
 * since nothing else here needs to reach into it).
 *
 * Constructed with non-owning pointers to `canvas` and
 * `toolConfigurationPanel` - both stay owned by `MainWindow` (a
 * `CanvasWidget` is the window's own central widget; a
 * `ToolConfigurationPanel` is a dock widget only `QMainWindow::
 * addDockWidget()` can place) - and wires every signal *between* them and
 * its own sub-controllers internally: `canvas_`'s own "continued"/
 * "ended" gesture signals straight to whichever controller is doing the
 * work, and each controller's own `pathChanged()`/`selectionChanged()`/
 * `boundsChanged()`/`previewChanged()` straight back to `canvas_`'s preview/
 * selection-overlay setters or `toolConfigurationPanel_`'s own state.
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
     * @param history Non-owning; passed straight through to the internally-
     *        constructed `PaintController` (see its own docs on why this is
     *        nullable). Must outlive this controller if given.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    ToolPaletteController(CanvasWidget* canvas, ToolConfigurationPanel* toolConfigurationPanel,
                           ProjectHistory* history = nullptr, QObject* parent = nullptr);

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

    /// @brief Sets how the *next* Select drag combines with the existing
    ///        selection - forwards to
    ///        `SelectionController::setSelectionCombineMode()`. The actual
    ///        work behind holding Shift/Alt while starting a new selection
    ///        gesture, resolved by the caller from the canvas's own mouse-
    ///        press modifiers.
    /// @param mode The combine mode to use for the next completed drag.
    void setSelectionCombineMode(SelectionCombineMode mode);

    /// @brief Sets Wand's own tolerance - forwards to
    ///        `SelectionController::setWandTolerance()`. The actual work
    ///        behind the Selection Configuration Panel's own Tolerance
    ///        spin box.
    /// @param tolerancePercent The new tolerance.
    void setWandTolerance(double tolerancePercent);

    /// @brief Sets whether Wand extends along harmonic rows - forwards to
    ///        `SelectionController::setWandHarmonicsAware()`. The actual
    ///        work behind the Selection Configuration Panel's own
    ///        Harmonics-aware checkbox.
    /// @param harmonicsAware The new value.
    void setWandHarmonicsAware(bool harmonicsAware);

    /// @brief Starts dragging the committed selection's own rotate handle
    ///        - forwards to `SelectionController::beginRotateDrag()`.
    /// @param point Where the drag starts.
    void beginRotateDrag(sound_mind::core::TimeFrequencyPoint point);

    /// @brief Continues an in-progress rotate drag - forwards to
    ///        `SelectionController::continueRotateDrag()`.
    /// @param point The cursor's current position.
    void continueRotateDrag(sound_mind::core::TimeFrequencyPoint point);

    /// @brief Ends an in-progress rotate drag - forwards to
    ///        `SelectionController::endRotateDrag()`.
    void endRotateDrag();

    /// @brief Cancels any in-progress rotate drag - forwards to
    ///        `SelectionController::cancelRotateDrag()`. Called by
    ///        `MainWindow` before turning Select mode off.
    void cancelRotateDrag();

    /// @brief Starts a Path tool stroke - forwards to
    ///        `PathController::beginPath()`.
    /// @param layer Which layer the guide path will belong to - resolved
    ///        by the caller, not by this class.
    /// @param point The press position.
    void beginPathStroke(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Continues the Path tool stroke - forwards to
    ///        `PathController::continuePath()`.
    /// @param point The cursor's new position.
    void continuePathStroke(sound_mind::core::TimeFrequencyPoint point);

    /// @brief Ends the Path tool stroke, committing a guide path - forwards
    ///        to `PathController::endPath()`.
    void endPathStroke();

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

    /// @brief Discards any in-progress Path tool stroke - forwards to
    ///        `PathController::cancelPath()`. Called by `MainWindow`
    ///        before turning Path mode off.
    void cancelPathStroke();

    /// @brief Undoes the most recent paint stroke - forwards to
    ///        `PaintController::undo()`.
    void undo();

    /// @brief Redoes the most recently undone paint stroke - forwards to
    ///        `PaintController::redo()`.
    void redo();

    /// @brief Invalidates every layer's own cached paint-content replay -
    ///        `v0.1.8.1` - forwards to
    ///        `PaintController::invalidateContentCaches()`. Call whenever
    ///        any MindWave's own value changes anywhere in the project's
    ///        library - see that method's own docs.
    void invalidateContentCaches();

    /// @brief Deletes the currently Picked object - forwards to
    ///        `PickController::deleteSelection()`.
    void deleteSelection();

    /// @brief Forwards to `PaintController::deleteOperations()`.
    /// @param ids The operations to delete, as one undoable step.
    /// @param description The History Panel's row text.
    void deleteOperations(const std::vector<sound_mind::core::OperationId>& ids, const QString& description);

    /// @brief Forwards to `PaintController::forgetLayer()`.
    /// @param layer A layer that has just been removed from the project.
    void forgetLayer(sound_mind::core::LayerId layer);

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

    /// @brief Applies `config`'s own filter to the current selection -
    ///        forwards to `SelectionController::applyFilterToSelection()`,
    ///        `v0.Y.46.1` Installment E ("Layers Panel & Editing
    ///        Enhancements v2").
    /// @param config Which filter to apply, and its own parameters.
    void applyFilterToSelection(const sound_mind::core::FilterConfiguration& config);

    /// @brief The currently Picked object's own `Path`, if any - forwards
    ///        to `PickController::selectedPath()`. The actual mechanism
    ///        behind `MainWindow::usePickedPathAsMindWaveShape()` - see
    ///        its own docs.
    /// @return The selected path, or `std::nullopt` if nothing suitable
    ///         is Picked.
    [[nodiscard]] std::optional<sound_mind::core::Path> selectedPath() const;

    /// @brief The currently Picked object's own stable identity, if any -
    ///        forwards to `PickController::selectedOperationId()`. The
    ///        Branching Curve session's own way to recognize the same
    ///        stroke being re-Picked across several clicks - see
    ///        `BranchCurveSession`'s own docs.
    /// @return The selected `Operation::id()`, or `std::nullopt` if
    ///         nothing is currently Picked.
    [[nodiscard]] std::optional<sound_mind::core::OperationId> selectedOperationId() const;
    /// @brief The currently Picked object's own paste blend mode, if any -
    ///        forwards to `PickController::selectedPasteBlendMode()`. The
    ///        mechanism behind pre-filling Selection Configuration's own
    ///        Paste Blend Mode combo when a pasted region is Picked (real-
    ///        world testing pass, 2026-09-20, finding #5) - `MainWindow`
    ///        calls this from its own `pickSelectionChanged()` handler.
    /// @return The selected object's own blend mode, or `std::nullopt` if
    ///         nothing suitable is Picked.
    [[nodiscard]] std::optional<sound_mind::core::BlendMode> selectedPasteBlendMode() const;

    /// @brief Forwards to `PickController::selectedTextSettings()`.
    /// @return The Picked text's settings, if text is Picked.
    [[nodiscard]] std::optional<TextSettings> selectedTextSettings() const;

    /// @brief Forwards to `PickController::applyTextSettings()`.
    /// @param settings The new settings.
    /// @return Whether the Picked text changed.
    bool applyTextSettingsToPicked(const TextSettings& settings);

    /// @brief Applies a new blend mode to the currently Picked object -
    ///        forwards to `PickController::applyPasteBlendMode()`. A no-op
    ///        if nothing is Picked, or the selection isn't a pasted region
    ///        (real-world testing pass, 2026-09-20, finding #5).
    /// @param mode The new blend mode to apply.
    void applyPasteBlendMode(sound_mind::core::BlendMode mode);

    /// @brief Whether there's a committed selection right now - forwards
    ///        to `SelectionController::hasSelection()`.
    /// @return `true` if there's a selection to fill/copy/cut.
    [[nodiscard]] bool hasSelection() const;

    /// @brief Whether there's anything on the clipboard to paste - forwards
    ///        to `SelectionController::hasClipboard()`.
    /// @return `true` after a Copy/Cut.
    [[nodiscard]] bool hasClipboard() const;

    /// @brief The Text tool's controller - see `TextController`. Owned by
    ///        this object; wired to the canvas's Text-mode signals and its
    ///        text preview.
    /// @return The controller.
    [[nodiscard]] TextController* textController() const noexcept { return textController_; }

    /// @brief Whether anything is Picked - one object or several.
    /// @return `true` with a single or multi Pick selection.
    [[nodiscard]] bool hasPickedObjects() const;

    /// @brief How many objects are Picked.
    /// @return `0`, `1`, or the multi-selection's size.
    [[nodiscard]] std::size_t pickedObjectCount() const;

    /// @brief Copies the Picked objects - forwards to
    ///        `PickController::copySelectedObjects()`, and makes Paste paste
    ///        objects (not a region) until a region is copied again.
    /// @return How many objects were copied.
    std::size_t copyPickedObjects();

    /// @brief Whether Paste should paste copied objects: some were copied,
    ///        more recently than any region.
    /// @return `true` when it should.
    [[nodiscard]] bool pastesObjects() const;

    /// @brief Pastes the copied objects onto `layer` - forwards to
    ///        `PickController::pasteObjects()`.
    /// @param layer The layer to paste onto.
    /// @return The pasted objects' ids.
    std::vector<sound_mind::core::OperationId> pastePickedObjects(sound_mind::core::LayerId layer);

    /// @brief Forwards to `PickController::canMergeSelection()`.
    /// @return `true` if two or more Picked objects have a path.
    [[nodiscard]] bool canMergePickedPaths() const;

    /// @brief Forwards to `PickController::mergeSelectionIntoBranchingPath()`.
    /// @return The new branching path's id, or `std::nullopt`.
    std::optional<sound_mind::core::OperationId> mergePickedPaths();

    /// @brief Forwards to `PickController::selectedBranches()`.
    /// @return The Picked object's branches; empty with nothing suitable Picked.
    [[nodiscard]] std::vector<sound_mind::core::BranchGraft> selectedBranches() const;

    /// @brief Forwards to `PickController::selectedIsGuidePath()`.
    /// @return `true` if a guide path is Picked.
    [[nodiscard]] bool selectedIsGuidePath() const;

    /// @brief Forwards to `PickController::selectedLayer()`.
    /// @return The Picked object's layer, or `std::nullopt` with nothing Picked.
    [[nodiscard]] std::optional<sound_mind::core::LayerId> selectedLayer() const;

    /// @brief Whether a Picked path's nodes are being edited.
    /// @return `PickController::isPathEditActive()`.
    [[nodiscard]] bool isPathEditActive() const;

    /// @brief Whether a node is selected in the path being edited.
    /// @return `true` while a path edit has a selected node.
    [[nodiscard]] bool hasSelectedPathNode() const;

    /**
     * @brief Paints the current tool configuration along every branch of
     *        the Picked object (normally a guide path), on its own layer, as
     *        one undoable step - "Stroke Path with Current Tool". The guide
     *        path itself stays. See `PaintController::strokePaths()` for
     *        when it refuses.
     * @return How many strokes were painted.
     */
    std::size_t strokePickedPath();

    /// @brief Commits a guide path built elsewhere - forwards to
    ///        `PathController::addGuidePath()`.
    /// @param layer The layer it belongs to.
    /// @param branches Its strokes, trunk first.
    /// @param description The History Panel's row text.
    /// @return The new operation's id, or `std::nullopt` if nothing was committed.
    std::optional<sound_mind::core::OperationId> addGuidePath(sound_mind::core::LayerId layer,
                                                              std::vector<sound_mind::core::BranchGraft> branches,
                                                              const QString& description);

    /// @brief Forwards to `SelectionController::committedSelection()`.
    /// @return The committed selection, or `std::nullopt` if there isn't one.
    [[nodiscard]] std::optional<sound_mind::core::FilterPreviewSelection> committedSelection() const;

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
    /// @param fundamentalFrequencyHz See `SelectionController::
    ///        captureMindShot()`'s own identical parameter docs. Defaults
    ///        to `0.0` ("not set").
    /// @param startTimeOffsetSeconds See `SelectionController::
    ///        captureMindShot()`'s own identical parameter docs. Defaults
    ///        to `0.0` ("no offset").
    /// @return The new entry's own id, or `std::nullopt` if there was no
    ///         committed selection to capture.
    std::optional<sound_mind::core::MindShotId> captureMindShot(const std::string& name,
                                                                    double fundamentalFrequencyHz = 0.0,
                                                                    double startTimeOffsetSeconds = 0.0);

    /// @brief Captures the current selection's own `{layer, bounds}` into a
    ///        new, named Mind Grain - forwards to
    ///        `SelectionController::captureMindGrain()`.
    /// @param name Display name for the new library entry.
    /// @param fundamentalFrequencyHz See `SelectionController::
    ///        captureMindGrain()`'s own identical parameter docs. Defaults
    ///        to `0.0`.
    /// @param startTimeOffsetSeconds See `SelectionController::
    ///        captureMindGrain()`'s own identical parameter docs. Defaults
    ///        to `0.0`.
    /// @return The new entry's own id, or `std::nullopt` if there was no
    ///         committed selection to capture.
    std::optional<sound_mind::core::MindGrainId> captureMindGrain(const std::string& name,
                                                                     double fundamentalFrequencyHz = 0.0,
                                                                     double startTimeOffsetSeconds = 0.0);

    /// @brief Pastes the clipboard onto `targetLayer` - forwards to
    ///        `SelectionController::pasteInto()`.
    /// @param targetLayer Which layer to paste into.
    /// @param blendMode How the pasted clip combines with what's already at
    ///        the target - `v0.Y.37.1` (Deferred Blend Modes), forwarded
    ///        straight through. `sound_mind::core::BlendMode::Overwrite`
    ///        (the default) reproduces this method's own pre-`v0.Y.37.1`
    ///        hard-overwrite behavior exactly.
    /// @return The newly-created `PasteOperation`'s own id, or
    ///         `std::nullopt` if there was nothing on the clipboard.
    [[nodiscard]] std::optional<sound_mind::core::OperationId> pasteInto(
        sound_mind::core::LayerId targetLayer,
        sound_mind::core::BlendMode blendMode = sound_mind::core::BlendMode::Overwrite);

    /// @brief Selects a specific operation as Pick's own current
    ///        selection - forwards to `PickController::selectOperation()`.
    /// @param layer The operation's own target layer.
    /// @param operationId The operation to select.
    void selectOperation(sound_mind::core::LayerId layer, sound_mind::core::OperationId operationId);
    /**
     * @brief Sets the Chord Generator's own current parameters - forwards
     *        to `ChordGeneratorController::setParams()`.
     *
     * Also keeps the Chord Overlay (`CanvasWidget::setChordPreview()`) in
     * sync, wired internally the same way pathChanged()/selectionChanged()
     * are for the other three tool controllers (see this class's own
     * docs) - callers only ever need to call this one method, not a
     * second one to refresh the overlay themselves.
     *
     * @param params `ChordGeneratorPanel`'s own new, complete parameters.
     */
    void setChordParams(const sound_mind::core::ChordGeneratorParams& params);

    /// @brief Sets the Chord Generator's own current notation text - forwards
    ///        to `ChordGeneratorController::setNotation()`; keeps the Chord
    ///        Overlay in sync the same way setChordParams() does.
    /// @param notation `ChordGeneratorPanel`'s own current notation text -
    ///        see `sound_mind::core::parseSequenceNotation()`'s own docs on
    ///        the grammar; may be invalid.
    /// @param referenceHz The tuning reference to resolve note names against.
    /// @param bpm The tempo to resolve beats-suffixed durations against.
    void setChordNotation(const std::string& notation, double referenceHz, double bpm);

    /// @brief Stamps the Chord Generator's own current chord/arpeggio -
    ///        forwards to `ChordGeneratorController::stampAt()`.
    /// @param layer Which layer to stamp into - resolved by the caller
    ///        (`MainWindow`), not by this class.
    /// @param timeSeconds When the chord starts, in seconds - wherever the
    ///        user clicked on the canvas's own time axis.
    void stampChord(sound_mind::core::LayerId layer, double timeSeconds);

    /**
     * @brief Bakes `layer`'s own currently-active operations into real
     *        pixel content - forwards to
     *        `PaintController::rebuildLayerContent()`.
     *
     * The one narrow point of access this class exposes onto its own
     * private `paintController_`, for a caller that appended new
     * `Operation`s to a layer through some route other than an interactive
     * paint/pick/stamp gesture (`MainWindow::importMidiFile()`, in
     * particular - see `sound_mind::studio::importMidiChannelsInto()`'s
     * own docs on why it can't call this itself) and needs the resulting
     * content to actually render rather than sitting unrendered in the
     * operation log. Emits contentChanged() the same as every other
     * content-mutating method here, via `PaintController::contentChanged()`
     * already being forwarded through (see this class's own docs).
     *
     * @param layer The layer to rebuild - a no-op if it doesn't exist, or
     *        no project is set (see `PaintController::rebuildLayerContent()`'s
     *        own docs).
     */
    void rebuildLayerContent(sound_mind::core::LayerId layer);

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

    /// @brief Emitted whenever Pick's own current selection changes - a new
    ///        pick, a clear, or a committed edit. `MainWindow` connects
    ///        this to pre-fill Selection Configuration's own Paste Blend
    ///        Mode combo via selectedPasteBlendMode() (real-world testing
    ///        pass, 2026-09-20, finding #5) - `ToolConfigurationPanel`'s
    ///        own equivalent pre-fill (selectedConfiguration()) is handled
    ///        internally instead, since this controller already owns that
    ///        panel directly, unlike `SelectionConfigurationPanel`.
    void pickSelectionChanged();

private:
    CanvasWidget* canvas_;
    ToolConfigurationPanel* toolConfigurationPanel_;

    PaintController* paintController_ = nullptr;
    PickController* pickController_ = nullptr;
    SelectionController* selectionController_ = nullptr;
    PathController* pathController_ = nullptr;
    TextController* textController_ = nullptr;
    /// @brief Whether objects were copied more recently than a region - see
    ///        pastesObjects().
    bool objectsCopiedLast_ = false;
    ChordGeneratorController* chordGeneratorController_ = nullptr;
};

}  // namespace sound_mind::studio
