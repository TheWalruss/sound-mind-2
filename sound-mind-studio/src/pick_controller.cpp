#include "sound_mind/studio/pick_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

namespace {

/// @brief Whether `point` falls within `bounds`, expanded by `timePadding`/
/// `frequencyPadding` on every side - see pick()'s own docs for why a
/// raw, unpadded bounds() isn't enough on its own.
bool containsPoint(const sound_mind::core::TimeFrequencyRect& bounds, sound_mind::core::TimeFrequencyPoint point,
                    double timePadding, double frequencyPadding) {
    return point.timeSeconds >= bounds.startTimeSeconds - timePadding &&
           point.timeSeconds <= bounds.endTimeSeconds + timePadding &&
           point.frequencyHz >= bounds.lowFrequencyHz - frequencyPadding &&
           point.frequencyHz <= bounds.highFrequencyHz + frequencyPadding;
}

}  // namespace

PickController::PickController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void PickController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    clearSelection();
}

bool PickController::pick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr) {
        clearSelection();
        return false;
    }

    const auto operations = project_->operationLog().activeOperationsTargeting(layer);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());

    // Every candidate under this point, most-recent-first (an overlapping
    // newer stroke ordinarily wins over an older one underneath it) -
    // activeOperationsTargeting() itself returns them in log (oldest-
    // first) order.
    std::vector<const sound_mind::core::PaintOperation*> candidates;
    for (auto it = operations.rbegin(); it != operations.rend(); ++it) {
        const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(*it);
        if (paint == nullptr) {
            continue;
        }
        const double timePadding = paint->config().size();
        const double frequencyPadding = timePadding * scale;
        if (containsPoint(paint->bounds(), point, timePadding, frequencyPadding)) {
            candidates.push_back(paint);
        }
    }

    if (candidates.empty()) {
        clearSelection();
        return false;
    }

    // Clicking a fresh spot selects whatever's topmost there, same as
    // always - but if the *currently selected* object is itself among
    // this click's own candidates, clicking it again cycles to the next
    // one underneath instead of re-selecting the same topmost object
    // every time, which is the only way an object entirely occluded by a
    // larger one on top of it could ever be reached at all. Wraps back to
    // the topmost after the last (occluded-most) candidate.
    const sound_mind::core::PaintOperation* toSelect = candidates.front();
    if (pickedOperationId_.has_value()) {
        const auto currentIt =
            std::find_if(candidates.begin(), candidates.end(),
                          [this](const sound_mind::core::PaintOperation* op) { return op->id() == *pickedOperationId_; });
        if (currentIt != candidates.end()) {
            const auto nextIt = std::next(currentIt);
            toSelect = (nextIt != candidates.end()) ? *nextIt : candidates.front();
        }
    }

    pickedOperationId_ = toSelect->id();
    pickedLayer_ = layer;
    pickedPath_ = toSelect->path();
    pickedConfig_ = toSelect->config();
    dragAnchor_ = point;
    dragCurrent_ = point;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit selectionChanged();
    return true;
}

void PickController::clearSelection() {
    const bool hadSelection = pickedOperationId_.has_value();
    pickedOperationId_.reset();
    pickedPath_ = sound_mind::core::Path{};
    pickedConfig_ = sound_mind::core::ToolConfiguration{};
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    if (hadSelection) {
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::ToolConfiguration> PickController::selectedConfiguration() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    return pickedConfig_;
}

std::optional<sound_mind::core::TimeFrequencyRect> PickController::selectionBounds() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    return pickedPath_.bounds();
}

void PickController::continueMove(sound_mind::core::TimeFrequencyPoint point) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    dragMoved_ = true;
    dragCurrent_ = point;
    previewPath_ = pickedPath_.translated(point.timeSeconds - dragAnchor_.timeSeconds,
                                           point.frequencyHz - dragAnchor_.frequencyHz);
    emit pathChanged();
}

void PickController::endMove() {
    if (!pickedOperationId_.has_value() || !dragMoved_) {
        dragMoved_ = false;
        if (!previewPath_.nodes().empty()) {
            previewPath_ = sound_mind::core::Path{};
            emit pathChanged();
        }
        return;
    }

    sound_mind::core::Path movedPath = pickedPath_.translated(dragCurrent_.timeSeconds - dragAnchor_.timeSeconds,
                                                                dragCurrent_.frequencyHz - dragAnchor_.frequencyHz);
    const sound_mind::core::OperationId newId = commitReplacement(movedPath, pickedConfig_);

    pickedOperationId_ = newId;
    pickedPath_ = std::move(movedPath);
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
    emit selectionChanged();
}

void PickController::applyToolConfiguration(const sound_mind::core::ToolConfiguration& config) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    sound_mind::core::Path newPath = pickedPath_;
    // Re-seeded from the new configuration's own default, exactly like a
    // fresh stroke - see ToolConfiguration::defaultGradient()'s own docs.
    newPath.gradient() = config.defaultGradient();

    const sound_mind::core::OperationId newId = commitReplacement(newPath, config);

    pickedOperationId_ = newId;
    pickedPath_ = std::move(newPath);
    pickedConfig_ = config;
}

void PickController::deleteSelection() {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    commitReplacement(sound_mind::core::Path{}, pickedConfig_);
    clearSelection();
}

sound_mind::core::OperationId PickController::commitReplacement(sound_mind::core::Path newPath,
                                                                    sound_mind::core::ToolConfiguration newConfig) {
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    log.append(std::make_unique<sound_mind::core::PaintOperation>(newId, pickedLayer_, std::move(newPath),
                                                                     std::move(newConfig), pickedOperationId_));
    paintController_->rebuildLayerContent(pickedLayer_);
    emit contentChanged(pickedLayer_);
    return newId;
}

}  // namespace sound_mind::studio
