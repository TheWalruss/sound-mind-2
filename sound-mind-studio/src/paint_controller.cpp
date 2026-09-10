#include "sound_mind/studio/paint_controller.h"

#include <algorithm>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"

namespace sound_mind::studio {

namespace {

/// @brief Finds a layer by id within `project`'s own layer stack.
/// @return A pointer to the layer, or `nullptr` if no layer with that id exists.
sound_mind::core::Layer* findLayer(sound_mind::core::Project& project, sound_mind::core::LayerId id) {
    for (auto& layer : project.layers()) {
        if (layer.id() == id) {
            return &layer;
        }
    }
    return nullptr;
}

}  // namespace

PaintController::PaintController(QObject* parent) : QObject(parent) {}

void PaintController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    strokeInProgress_ = false;
    strokePoints_.clear();
    previewPath_ = sound_mind::core::Path{};
    baseContent_.clear();
}

void PaintController::beginStroke(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr || strokeInProgress_) {
        return;
    }
    strokeInProgress_ = true;
    strokeTargetLayer_ = targetLayer;
    strokePoints_.clear();
    strokePoints_.push_back(point);
    previewPath_ = sound_mind::core::Path{};
}

void PaintController::continueStroke(sound_mind::core::TimeFrequencyPoint point) {
    if (!strokeInProgress_) {
        return;
    }
    strokePoints_.push_back(point);
    if (strokePoints_.size() >= 2) {
        // Cheap enough to refit on every sample - see this method's own
        // docs; fitPathToPoints() only ever runs over the current
        // stroke's own point count, not anything session-length.
        previewPath_ = sound_mind::core::fitPathToPoints(strokePoints_, frequencyToTimeScale(), /*simplifyToleranceSeconds=*/0.01);
    }
    emit pathChanged();
}

void PaintController::endStroke() {
    if (!strokeInProgress_) {
        return;
    }
    strokeInProgress_ = false;

    if (project_ == nullptr || strokePoints_.empty()) {
        strokePoints_.clear();
        previewPath_ = sound_mind::core::Path{};
        emit pathChanged();
        return;
    }

    sound_mind::core::Path finalPath;
    if (strokePoints_.size() == 1) {
        // A tap, not a drag - see this method's own docs. A single Corner
        // node still paints, as a single stamp (applyPaintOperation()'s
        // own path-sampling handles a one-node Path already).
        sound_mind::core::PathNode node;
        node.anchor = strokePoints_.front();
        node.type = sound_mind::core::PathNodeType::Corner;
        finalPath.addNode(node);
    } else {
        finalPath =
            sound_mind::core::fitPathToPoints(strokePoints_, frequencyToTimeScale(), /*simplifyToleranceSeconds=*/0.01);
    }
    // Seed the new stroke's own gradient from the tool's default - see
    // ToolConfiguration::defaultGradient()'s own docs.
    finalPath.gradient() = toolConfig_.defaultGradient();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::PaintOperation>(id, strokeTargetLayer_, std::move(finalPath),
                                                                    toolConfig_));

    const sound_mind::core::LayerId paintedLayer = strokeTargetLayer_;
    strokePoints_.clear();
    previewPath_ = sound_mind::core::Path{};

    rebuildLayerContent(paintedLayer);
    emit pathChanged();
}

void PaintController::cancelStroke() {
    if (!strokeInProgress_) {
        return;
    }
    strokeInProgress_ = false;
    strokePoints_.clear();
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
}

bool PaintController::canUndo() const noexcept { return project_ != nullptr && project_->operationLog().canUndo(); }

bool PaintController::canRedo() const noexcept { return project_ != nullptr && project_->operationLog().canRedo(); }

void PaintController::undo() {
    if (!canUndo()) {
        return;
    }
    project_->operationLog().undo();
    for (const auto& [layerId, base] : baseContent_) {
        rebuildLayerContent(layerId);
    }
}

void PaintController::redo() {
    if (!canRedo()) {
        return;
    }
    project_->operationLog().redo();
    for (const auto& [layerId, base] : baseContent_) {
        rebuildLayerContent(layerId);
    }
}

void PaintController::rebuildLayerContent(sound_mind::core::LayerId layer) {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::Layer* target = findLayer(*project_, layer);
    if (target == nullptr) {
        return;
    }

    // Capture this layer's pre-paint base exactly once - see this
    // method's own docs.
    if (!baseContent_.contains(layer)) {
        if (!target->content().has_value()) {
            return;  // nothing to paint onto yet (never imported/rendered).
        }
        baseContent_.emplace(layer, *target->content());
    }

    const auto activeOperations = project_->operationLog().activeOperationsTargeting(layer);
    sound_mind::codec::StreamImage rebuilt =
        sound_mind::core::rebuildPaintedContent(baseContent_.at(layer), activeOperations, frequencyToTimeScale());
    target->setContent(std::move(rebuilt));

    emit contentChanged(layer);
}

double PaintController::frequencyToTimeScale() const noexcept {
    if (project_ == nullptr) {
        return 1000.0;  // an arbitrary, always-positive fallback - never actually used (callers all guard on project_).
    }
    const auto& settings = project_->settings();
    const double durationSeconds = static_cast<double>(settings.canvasWidth) * settings.timestepMs / 1000.0;
    const double frequencyRangeHz = static_cast<double>(settings.maxFrequencyHz) - static_cast<double>(settings.minFrequencyHz);
    if (durationSeconds <= 0.0) {
        return 1000.0;
    }
    return frequencyRangeHz / durationSeconds;
}

}  // namespace sound_mind::studio
