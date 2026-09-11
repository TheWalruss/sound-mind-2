#include "sound_mind/studio/path_controller.h"

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

PathController::PathController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void PathController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    placementActive_ = false;
    targetLayer_ = 0;
    path_ = sound_mind::core::Path{};
    lastCursor_.reset();
    emit pathChanged();
}

void PathController::placeNode(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr) {
        return;
    }
    if (!placementActive_) {
        placementActive_ = true;
        targetLayer_ = targetLayer;
        path_ = sound_mind::core::Path{};
    }

    sound_mind::core::PathNode node;
    node.anchor = point;
    node.type = defaultNodeType_;
    if (defaultNodeType_ == sound_mind::core::PathNodeType::Smooth) {
        // Collapsed onto the anchor itself - a freshly placed node with no
        // curve pulled out of it yet, per PathNode's own docs. Real handle
        // dragging is a follow-up installment (see the class's own docs).
        node.handleIn = point;
        node.handleOut = point;
    }
    path_.addNode(node);
    // lastCursor_ is deliberately left alone here, not set to `point` -
    // the live rubber-band segment currentPreviewPath() draws only
    // reflects an actual updateCursor() call, never synthesized from the
    // node just placed (which would draw a spurious zero-length segment
    // until the cursor genuinely moves again).

    emit pathChanged();
}

void PathController::updateCursor(sound_mind::core::TimeFrequencyPoint point) {
    if (!placementActive_) {
        return;
    }
    lastCursor_ = point;
    emit pathChanged();
}

void PathController::finishPath() {
    if (!placementActive_ || path_.nodes().empty()) {
        return;
    }
    placementActive_ = false;

    sound_mind::core::Path finalPath = path_;
    // Seed the new path's own gradient from the tool's default - see
    // PaintController::endStroke()'s own docs for the identical precedent.
    finalPath.gradient() = toolConfig_.defaultGradient();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(
        std::make_unique<sound_mind::core::PaintOperation>(id, targetLayer_, std::move(finalPath), toolConfig_));

    const sound_mind::core::LayerId paintedLayer = targetLayer_;
    path_ = sound_mind::core::Path{};
    lastCursor_.reset();

    paintController_->rebuildLayerContent(paintedLayer);
    emit contentChanged(paintedLayer);
    emit pathChanged();
}

void PathController::cancelPath() {
    if (!placementActive_) {
        return;
    }
    placementActive_ = false;
    path_ = sound_mind::core::Path{};
    lastCursor_.reset();
    emit pathChanged();
}

sound_mind::core::Path PathController::currentPreviewPath() const {
    if (!placementActive_) {
        return sound_mind::core::Path{};
    }
    sound_mind::core::Path preview = path_;
    if (lastCursor_.has_value() && !path_.nodes().empty()) {
        sound_mind::core::PathNode cursorNode;
        cursorNode.anchor = *lastCursor_;
        cursorNode.type = sound_mind::core::PathNodeType::Corner;
        preview.addNode(cursorNode);
    }
    return preview;
}

}  // namespace sound_mind::studio
