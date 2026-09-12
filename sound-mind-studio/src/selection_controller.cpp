#include "sound_mind/studio/selection_controller.h"

#include <algorithm>
#include <memory>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paste_application.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

namespace {

/// @brief The normalized rectangle spanning two corner points, in either
/// order - a drag can go in any of the four diagonal directions.
sound_mind::core::TimeFrequencyRect rectFromCorners(sound_mind::core::TimeFrequencyPoint a,
                                                      sound_mind::core::TimeFrequencyPoint b) {
    sound_mind::core::TimeFrequencyRect rect;
    rect.startTimeSeconds = std::min(a.timeSeconds, b.timeSeconds);
    rect.endTimeSeconds = std::max(a.timeSeconds, b.timeSeconds);
    rect.lowFrequencyHz = std::min(a.frequencyHz, b.frequencyHz);
    rect.highFrequencyHz = std::max(a.frequencyHz, b.frequencyHz);
    return rect;
}

}  // namespace

SelectionController::SelectionController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void SelectionController::setGridSnapping(bool enabled, const FrequencyGridConfig& frequencyGridConfig,
                                             const TimingGridConfig& timingGridConfig) {
    gridSnappingEnabled_ = enabled;
    frequencyGridConfig_ = frequencyGridConfig;
    timingGridConfig_ = timingGridConfig;
}

void SelectionController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    dragActive_ = false;
    dragMoved_ = false;
    const bool hadSelection = committedBounds_.has_value();
    committedBounds_.reset();
    clipboard_.reset();
    clipboardBounds_.reset();
    emit boundsChanged();
    if (hadSelection) {
        emit selectionChanged();
    }
}

void SelectionController::beginSelectionDrag(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    dragActive_ = true;
    dragMoved_ = false;
    dragAnchor_ = point;
    selectionLayer_ = layer;
    dragPreviewBounds_ = rectFromCorners(point, point);
    emit boundsChanged();
}

void SelectionController::continueSelectionDrag(sound_mind::core::TimeFrequencyPoint point) {
    if (!dragActive_) {
        return;
    }
    if (gridSnappingEnabled_ && project_ != nullptr) {
        point = snapToGrid(point, frequencyGridConfig_, timingGridConfig_, project_->settings());
    }
    dragMoved_ = true;
    dragPreviewBounds_ = rectFromCorners(dragAnchor_, point);
    emit boundsChanged();
}

void SelectionController::endSelectionDrag() {
    if (!dragActive_) {
        return;
    }
    dragActive_ = false;

    if (!dragMoved_) {
        // A plain click, not a drag - deselect, matching Pick's own
        // "clicking empty space clears the selection" convention.
        const bool hadSelection = committedBounds_.has_value();
        committedBounds_.reset();
        emit boundsChanged();
        if (hadSelection) {
            emit selectionChanged();
        }
        return;
    }

    committedBounds_ = dragPreviewBounds_;
    emit boundsChanged();
    emit selectionChanged();
}

void SelectionController::cancelSelectionDrag() {
    if (!dragActive_) {
        return;
    }
    dragActive_ = false;
    dragMoved_ = false;
    emit boundsChanged();  // reverts the display back to the committed selection (or none).
}

void SelectionController::clearSelection() {
    const bool hadSelection = committedBounds_.has_value();
    committedBounds_.reset();
    if (hadSelection) {
        emit boundsChanged();
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::TimeFrequencyRect> SelectionController::displayBounds() const {
    if (dragActive_) {
        return dragPreviewBounds_;
    }
    return committedBounds_;
}

void SelectionController::fill(const sound_mind::core::Gradient& gradient) {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return;
    }
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::FillOperation>(id, selectionLayer_, *committedBounds_, gradient));
    paintController_->rebuildLayerContent(selectionLayer_);
    emit contentChanged(selectionLayer_);
}

void SelectionController::copySelection() {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return;
    }
    // Ensure selectionLayer_ has real content to capture from - a layer
    // never painted on yet has none until rebuildLayerContent() lazily
    // synthesizes its silent base, the same precedent PaintController's
    // own docs already establish.
    paintController_->rebuildLayerContent(selectionLayer_);
    const sound_mind::core::Layer* layer = project_->layerById(selectionLayer_);
    if (layer == nullptr || !layer->content().has_value()) {
        return;
    }

    clipboard_ = sound_mind::core::captureClip(*layer->content(), *committedBounds_);
    clipboardBounds_ = *committedBounds_;
}

void SelectionController::cutSelection() {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return;
    }
    copySelection();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::FillOperation>(id, selectionLayer_, *committedBounds_,
                                                                    sound_mind::core::silenceGradient()));
    paintController_->rebuildLayerContent(selectionLayer_);
    emit contentChanged(selectionLayer_);
}

std::optional<sound_mind::core::OperationId> SelectionController::pasteInto(sound_mind::core::LayerId targetLayer) {
    if (!clipboard_.has_value() || !clipboardBounds_.has_value() || project_ == nullptr) {
        return std::nullopt;
    }
    // Ensure targetLayer has real content to paste onto - same reasoning
    // as copySelection()'s own call.
    paintController_->rebuildLayerContent(targetLayer);

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::PasteOperation>(id, targetLayer, *clipboardBounds_, *clipboard_));
    paintController_->rebuildLayerContent(targetLayer);
    emit contentChanged(targetLayer);

    // The pasted region becomes the new committed selection, on the layer
    // it was actually pasted onto - visual confirmation of both where it
    // landed and (via selectionLayer_) which layer that was, the same way
    // a freshly drawn selection would be.
    selectionLayer_ = targetLayer;
    committedBounds_ = *clipboardBounds_;
    emit boundsChanged();
    emit selectionChanged();

    return id;
}

}  // namespace sound_mind::studio
