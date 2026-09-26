#include "sound_mind/studio/tool_palette_controller.h"

#include <utility>

#include <QPointF>

#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/canvas_widget.h"
#include "sound_mind/studio/chord_generator_controller.h"
#include "sound_mind/studio/paint_controller.h"
#include "sound_mind/studio/path_controller.h"
#include "sound_mind/studio/pick_controller.h"
#include "sound_mind/studio/selection_controller.h"
#include "sound_mind/studio/tool_configuration_panel.h"

namespace sound_mind::studio {

ToolPaletteController::ToolPaletteController(CanvasWidget* canvas, ToolConfigurationPanel* toolConfigurationPanel,
                                              UndoStack* undoStack, QObject* parent)
    : QObject(parent), canvas_(canvas), toolConfigurationPanel_(toolConfigurationPanel) {
    // Basic Painting (v0.Y.24.1) - canvas_ only ever emits already-converted
    // TimeFrequencyPoints; paintController_ never reaches into canvas_
    // directly, only back through pathChanged()/contentChanged() below.
    // paintStrokeStarted isn't wired here - it needs a resolved target
    // layer, which is beginPaintStroke()'s own caller's job (see this
    // class's own docs).
    paintController_ = new PaintController(undoStack, this);
    connect(canvas_, &CanvasWidget::paintStrokeContinued, this,
            [this](sound_mind::core::TimeFrequencyPoint point) { paintController_->continueStroke(point); });
    connect(canvas_, &CanvasWidget::paintStrokeEnded, this, [this]() { paintController_->endStroke(); });
    connect(paintController_, &PaintController::pathChanged, this,
            [this]() { canvas_->setPaintPreviewPath(paintController_->currentPreviewPath()); });
    connect(paintController_, &PaintController::contentChanged, this, [this](sound_mind::core::LayerId layer) {
        canvas_->update();
        emit contentChanged(layer);
    });

    // Pick (v0.Y.24.1) - shares paintController_'s own pre-paint base cache
    // (see PickController's own docs), so it's constructed after
    // paintController_ and holds a pointer to it.
    pickController_ = new PickController(paintController_, this);
    connect(canvas_, &CanvasWidget::pickStrokeContinued, this,
            [this](sound_mind::core::TimeFrequencyPoint point) { pickController_->continueMove(point); });
    connect(canvas_, &CanvasWidget::pickStrokeEnded, this, [this]() { pickController_->endMove(); });
    connect(pickController_, &PickController::pathChanged, this, [this]() {
        canvas_->setPaintPreviewPath(pickController_->currentPreviewPath());
        canvas_->setPreviewSelectedNodeIndex(pickController_->selectedPathNodeIndex());
    });
    connect(pickController_, &PickController::contentChanged, this, [this](sound_mind::core::LayerId layer) {
        canvas_->update();
        emit contentChanged(layer);
    });
    connect(pickController_, &PickController::selectionChanged, this, [this]() {
        canvas_->setPickSelectionBounds(pickController_->selectionBounds());
        // Pre-fills the panel with the newly-picked object's own settings,
        // ready to reopen and adjust - see docs/sound-mind-design.md's
        // "Pick". Left showing whatever it last displayed on a deselect
        // (clicking empty space, deleting the selection) - reverting to
        // some prior "default" isn't attempted; the panel's job is "the
        // current brush settings", picked or not.
        if (const auto config = pickController_->selectedConfiguration(); config != nullptr) {
            toolConfigurationPanel_->setToolConfiguration(*config);
        }
        emit pickSelectionChanged();
    });

    // Selection & Fill (v0.Y.25.1) - shares paintController_'s own
    // pre-paint base cache, so it's constructed after paintController_ and
    // holds a pointer to it.
    selectionController_ = new SelectionController(paintController_, this);
    connect(canvas_, &CanvasWidget::selectStrokeContinued, this,
            [this](sound_mind::core::TimeFrequencyPoint point) { selectionController_->continueSelectionDrag(point); });
    connect(canvas_, &CanvasWidget::selectStrokeEnded, this, [this]() { selectionController_->endSelectionDrag(); });
    connect(selectionController_, &SelectionController::boundsChanged, this, [this]() {
        canvas_->setSelectionBounds(selectionController_->displayBounds());
        canvas_->setSelectionBoundary(selectionController_->displayBoundary());
        canvas_->setSelectionHasMaskShape(selectionController_->hasMaskShapedSelection());
        canvas_->setSelectionRotationHandle(selectionController_->displayRotationHandle());
    });
    connect(selectionController_, &SelectionController::contentChanged, this, [this](sound_mind::core::LayerId layer) {
        canvas_->update();
        emit contentChanged(layer);
    });
    // A fresh capture (v0.Y.33.1 Installment A) - the Tool Configuration
    // Panel's own Mind Shot picker needs to know a new entry now exists.
    connect(selectionController_, &SelectionController::mindShotCaptured, this,
            [this](sound_mind::core::MindShotId) { toolConfigurationPanel_->refreshMindShots(); });
    // Same reasoning, for a fresh Mind Grain capture (v0.Y.33.1 Installment
    // B).
    connect(selectionController_, &SelectionController::mindGrainCaptured, this,
            [this](sound_mind::core::MindGrainId) { toolConfigurationPanel_->refreshMindGrains(); });

    // Paths & Grids (v0.Y.26.1), Path tool placement - shares
    // paintController_'s own pre-paint base cache, so it's constructed
    // after paintController_ and holds a pointer to it. Reuses canvas_'s
    // own existing live-preview overlay (setPaintPreviewPath()) rather than
    // a second one - Paint/Pick/Path are mutually exclusive tool modes, so
    // only one of them ever has a real preview to show at once.
    pathController_ = new PathController(paintController_, this);
    connect(canvas_, &CanvasWidget::cursorMoved, this,
            [this](QPointF, std::optional<sound_mind::core::TimeFrequencyPoint> domainPoint) {
                if (domainPoint.has_value()) {
                    pathController_->updateCursor(*domainPoint);
                }
            });
    connect(pathController_, &PathController::pathChanged, this,
            [this]() { canvas_->setPaintPreviewPath(pathController_->currentPreviewPath()); });
    connect(pathController_, &PathController::contentChanged, this, [this](sound_mind::core::LayerId layer) {
        canvas_->update();
        emit contentChanged(layer);
    });

    // The panel's own constructed-with defaults are already a real, opaque
    // brush (not ToolConfiguration's own transparent default - see the
    // panel's own docs) - applied here so a stroke painted before ever
    // opening the panel still paints something visible.
    paintController_->setToolConfiguration(toolConfigurationPanel_->toolConfiguration().clone());
    pathController_->setToolConfiguration(toolConfigurationPanel_->toolConfiguration().clone());
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::toolConfigurationChanged, this,
            [this](const sound_mind::core::ToolConfiguration& config) {
                // Each setToolConfiguration() below takes ownership, so
                // paintController_/pathController_ each need their own
                // independent clone() - the same unique_ptr can't be
                // handed to three separate owners.
                paintController_->setToolConfiguration(config.clone());
                pathController_->setToolConfiguration(config.clone());
                // Also applies to whatever's currently Picked, if anything
                // - see PickController::applyToolConfiguration()'s own
                // docs on why this is safe to do unconditionally alongside
                // updating the pending default above. Takes a plain
                // reference (clones internally at its own single
                // construction site), so no clone() needed here.
                if (pickController_->hasSelection()) {
                    pickController_->applyToolConfiguration(config);
                }
            });
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::showBoundingBoxesChanged, canvas_,
            &CanvasWidget::setShowBoundingBoxes);
    connect(toolConfigurationPanel_, &ToolConfigurationPanel::showPathGeometryChanged, canvas_,
            &CanvasWidget::setShowPathGeometry);

    // Chords/Arpeggiator/Sequencer, Installment B (v0.0.40.2) - shares
    // paintController_'s own tool configuration (see
    // ChordGeneratorController's own docs on why there's no second,
    // parallel instrument picker), so it's constructed after
    // paintController_ and holds a pointer to it.
    chordGeneratorController_ = new ChordGeneratorController(paintController_, this);
    connect(chordGeneratorController_, &ChordGeneratorController::previewChanged, this,
            [this]() { canvas_->setChordPreview(chordGeneratorController_->previewFrequenciesHz()); });
    connect(chordGeneratorController_, &ChordGeneratorController::contentChanged, this,
            [this](sound_mind::core::LayerId layer) {
                canvas_->update();
                emit contentChanged(layer);
            });
}

void ToolPaletteController::setProject(sound_mind::core::Project* project) {
    paintController_->setProject(project);
    pickController_->setProject(project);
    selectionController_->setProject(project);
    pathController_->setProject(project);
    chordGeneratorController_->setProject(project);
    toolConfigurationPanel_->setProject(project);
}

void ToolPaletteController::setChordParams(const sound_mind::core::ChordGeneratorParams& params) {
    chordGeneratorController_->setParams(params);
}

void ToolPaletteController::setChordNotation(const std::string& notation, double referenceHz, double bpm) {
    chordGeneratorController_->setNotation(notation, referenceHz, bpm);
}

void ToolPaletteController::stampChord(sound_mind::core::LayerId layer, double timeSeconds) {
    chordGeneratorController_->stampAt(layer, timeSeconds);
}

void ToolPaletteController::setGridSnapping(bool enabled, const FrequencyGridConfig& frequencyGridConfig,
                                             const TimingGridConfig& timingGridConfig) {
    pickController_->setGridSnapping(enabled, frequencyGridConfig, timingGridConfig);
    selectionController_->setGridSnapping(enabled, frequencyGridConfig, timingGridConfig);
}

void ToolPaletteController::beginPaintStroke(sound_mind::core::LayerId layer,
                                              sound_mind::core::TimeFrequencyPoint point) {
    paintController_->beginStroke(layer, point);
}

void ToolPaletteController::beginPick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    pickController_->pick(layer, point);
}

void ToolPaletteController::beginSelectionDrag(sound_mind::core::LayerId layer,
                                                sound_mind::core::TimeFrequencyPoint point) {
    selectionController_->beginSelectionDrag(layer, point);
}

void ToolPaletteController::placePathNode(sound_mind::core::LayerId layer,
                                           sound_mind::core::TimeFrequencyPoint point) {
    pathController_->placeNode(layer, point);
}

void ToolPaletteController::cancelPaintStroke() { paintController_->cancelStroke(); }

void ToolPaletteController::clearPickSelection() { pickController_->clearSelection(); }

void ToolPaletteController::cancelSelectionDrag() { selectionController_->cancelSelectionDrag(); }

void ToolPaletteController::setSelectionShape(SelectionShape shape) { selectionController_->setSelectionShape(shape); }

void ToolPaletteController::setSelectionCombineMode(SelectionCombineMode mode) {
    selectionController_->setSelectionCombineMode(mode);
}

void ToolPaletteController::setWandTolerance(double tolerancePercent) {
    selectionController_->setWandTolerance(tolerancePercent);
}

void ToolPaletteController::setWandHarmonicsAware(bool harmonicsAware) {
    selectionController_->setWandHarmonicsAware(harmonicsAware);
}

void ToolPaletteController::beginRotateDrag(sound_mind::core::TimeFrequencyPoint point) {
    selectionController_->beginRotateDrag(point);
}

void ToolPaletteController::continueRotateDrag(sound_mind::core::TimeFrequencyPoint point) {
    selectionController_->continueRotateDrag(point);
}

void ToolPaletteController::endRotateDrag() { selectionController_->endRotateDrag(); }

void ToolPaletteController::cancelRotateDrag() { selectionController_->cancelRotateDrag(); }

void ToolPaletteController::cancelPathPlacement() { pathController_->cancelPath(); }

void ToolPaletteController::undo() { paintController_->undo(); }

void ToolPaletteController::redo() { paintController_->redo(); }

void ToolPaletteController::deleteSelection() { pickController_->deleteSelection(); }

void ToolPaletteController::bringToFront() { pickController_->bringToFront(); }

void ToolPaletteController::sendToBack() { pickController_->sendToBack(); }

void ToolPaletteController::bringForward() { pickController_->bringForward(); }

void ToolPaletteController::sendBackward() { pickController_->sendBackward(); }

void ToolPaletteController::beginPathEdit() { pickController_->beginPathEdit(); }

void ToolPaletteController::toggleSelectedPathNodeType() { pickController_->toggleSelectedPathNodeType(); }

void ToolPaletteController::commitPathEdit() { pickController_->commitPathEdit(); }

void ToolPaletteController::cancelPathEdit() { pickController_->cancelPathEdit(); }

void ToolPaletteController::clearSelection() { selectionController_->clearSelection(); }

void ToolPaletteController::fill(const sound_mind::core::Gradient& gradient) { selectionController_->fill(gradient); }

void ToolPaletteController::applyFilterToSelection(const sound_mind::core::FilterConfiguration& config) {
    selectionController_->applyFilterToSelection(config);
}

std::optional<sound_mind::core::Path> ToolPaletteController::selectedPath() const {
    return pickController_->selectedPath();
}

std::optional<sound_mind::core::BlendMode> ToolPaletteController::selectedPasteBlendMode() const {
    return pickController_->selectedPasteBlendMode();
}

void ToolPaletteController::applyPasteBlendMode(sound_mind::core::BlendMode mode) {
    pickController_->applyPasteBlendMode(mode);
}

bool ToolPaletteController::hasSelection() const { return selectionController_->hasSelection(); }

void ToolPaletteController::copySelection() { selectionController_->copySelection(); }

void ToolPaletteController::cutSelection() { selectionController_->cutSelection(); }

std::optional<sound_mind::core::MindShotId> ToolPaletteController::captureMindShot(const std::string& name) {
    return selectionController_->captureMindShot(name);
}

std::optional<sound_mind::core::MindGrainId> ToolPaletteController::captureMindGrain(const std::string& name) {
    return selectionController_->captureMindGrain(name);
}

std::optional<sound_mind::core::OperationId> ToolPaletteController::pasteInto(sound_mind::core::LayerId targetLayer,
                                                                                sound_mind::core::BlendMode blendMode) {
    return selectionController_->pasteInto(targetLayer, blendMode);
}

void ToolPaletteController::selectOperation(sound_mind::core::LayerId layer, sound_mind::core::OperationId operationId) {
    pickController_->selectOperation(layer, operationId);
}

void ToolPaletteController::finishPath() { pathController_->finishPath(); }

void ToolPaletteController::cancelPath() { pathController_->cancelPath(); }

void ToolPaletteController::setPathPlacesSmoothNodes(bool smooth) {
    pathController_->setDefaultNodeType(smooth ? sound_mind::core::PathNodeType::Smooth
                                                : sound_mind::core::PathNodeType::Corner);
}

}  // namespace sound_mind::studio
