#include "sound_mind/studio/path_controller.h"

#include <algorithm>
#include <memory>

#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

PathController::PathController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void PathController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    cancelPath();
}

void PathController::beginPath(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr || inProgress_) {
        return;
    }
    inProgress_ = true;
    targetLayer_ = targetLayer;
    points_.assign(1, point);
    previewPath_ = sound_mind::core::Path{};
}

void PathController::continuePath(sound_mind::core::TimeFrequencyPoint point) {
    if (!inProgress_) {
        return;
    }
    points_.push_back(point);
    previewPath_ = fittedPath();
    emit pathChanged();
}

std::optional<sound_mind::core::OperationId> PathController::endPath() {
    if (!inProgress_) {
        return std::nullopt;
    }
    inProgress_ = false;
    const bool moved = std::any_of(points_.begin(), points_.end(), [this](const auto& point) {
        return point.timeSeconds != points_.front().timeSeconds || point.frequencyHz != points_.front().frequencyHz;
    });
    sound_mind::core::Path path = moved ? fittedPath() : sound_mind::core::Path{};
    points_.clear();
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
    if (project_ == nullptr || path.nodes().size() < 2) {
        return std::nullopt;
    }

    return addGuidePath(
        targetLayer_, {sound_mind::core::BranchGraft{std::move(path), std::nullopt, sound_mind::core::TimeFrequencyPoint{}}},
        tr("Drew path"));
}

std::optional<sound_mind::core::OperationId> PathController::addGuidePath(
    sound_mind::core::LayerId layer, std::vector<sound_mind::core::BranchGraft> branches, const QString& description) {
    if (project_ == nullptr || branches.empty()) {
        return std::nullopt;
    }
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::GuidePathOperation>(id, layer, std::move(branches)));
    paintController_->notifyOperationCommitted(description);
    emit contentChanged(layer);
    return id;
}

void PathController::cancelPath() {
    const bool hadPreview = inProgress_ || !previewPath_.nodes().empty();
    inProgress_ = false;
    points_.clear();
    previewPath_ = sound_mind::core::Path{};
    if (hadPreview) {
        emit pathChanged();
    }
}

sound_mind::core::Path PathController::fittedPath() const {
    if (points_.size() < 2 || project_ == nullptr) {
        return {};
    }
    return sound_mind::core::fitPathToPoints(points_, sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
                                             /*simplifyToleranceSeconds=*/0.01);
}

}  // namespace sound_mind::studio
