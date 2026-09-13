#include "sound_mind/studio/pick_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

namespace {

/// @brief Whether `point` falls within `bounds`, expanded by `timePadding`/
/// `frequencyPadding` on every side - see pick()'s own docs for why a
/// raw, unpadded bounds() isn't always enough on its own.
bool containsPoint(const sound_mind::core::TimeFrequencyRect& bounds, sound_mind::core::TimeFrequencyPoint point,
                    double timePadding, double frequencyPadding) {
    return point.timeSeconds >= bounds.startTimeSeconds - timePadding &&
           point.timeSeconds <= bounds.endTimeSeconds + timePadding &&
           point.frequencyHz >= bounds.lowFrequencyHz - frequencyPadding &&
           point.frequencyHz <= bounds.highFrequencyHz + frequencyPadding;
}

/// @brief The minimum click forgiveness `pick()` gives every candidate,
/// regardless of type - in the same seconds-equivalent normalized space
/// `PathEditSession`'s own `kNodeHitToleranceSeconds` (`path_edit_session.cpp`)
/// uses for node/handle hit-testing. `PaintOperation`'s own brush-size
/// padding (see `pick()`'s own docs) is usually generous enough on its
/// own, but a `FillOperation`/`PasteOperation`'s bounds() already exactly
/// matches its real footprint, which used to mean *zero* padding for
/// those - fine for a visibly-colored Fill or a visibly-different Paste,
/// where a real mouse click naturally lands well inside the visible
/// shape, but not for a Cut's own silence Fill: with nothing rendered to
/// see, there's no way to click confidently away from the exact edge, and
/// a real mouse is far less precise than an automated test's exact pixel.
constexpr double kMinimumPickPaddingSeconds = 0.015;

/// @brief The bin-space distance between `from` and `to`'s own
/// frequencies - what a mouse drag's own frequency delta needs to be
/// measured in, not raw Hz, before handing it to `Path::translated()`/
/// `sound_mind::core::translated(TimeFrequencyRect, ...)`/
/// `Operation::translatedCopy()`. The frequency axis is log-scaled (see
/// `frequencyToBinIndex()`'s own docs), so a fixed Hz difference between
/// two mouse positions doesn't correspond to the same on-screen distance
/// everywhere in the frequency range - only the bin-space distance does,
/// which is what keeps a dragged object tracking the mouse 1:1 instead of
/// visibly changing its own shape (or, near `minFrequencyHz`, even
/// inverting a bound past zero) as it moves vertically.
///
/// `PathEditSession::continueDrag()`'s own node/handle drag needs this
/// exact same formula and keeps its own copy - a two-line formula isn't
/// worth promoting to a shared header over, the same "duplicated, not
/// shared" reasoning `docs/sound-mind-architecture.md`'s Decision #59
/// already gives for `dbToLinearAmplitude()`/`linearAmplitudeToDb()`.
double frequencyBinDelta(sound_mind::core::TimeFrequencyPoint from, sound_mind::core::TimeFrequencyPoint to,
                          const sound_mind::codec::StreamCodecConfig& config) {
    return sound_mind::core::frequencyToBinIndex(static_cast<float>(to.frequencyHz), config) -
           sound_mind::core::frequencyToBinIndex(static_cast<float>(from.frequencyHz), config);
}

/// @brief A closed, four-corner rectangular outline Path tracing `rect` -
/// the live drag preview for a moving non-`PaintOperation` (no real Path
/// of its own to preview) - see currentPreviewPath()'s own docs.
sound_mind::core::Path outlinePathFor(const sound_mind::core::TimeFrequencyRect& rect) {
    sound_mind::core::Path path;
    const auto addCorner = [&path](double timeSeconds, double frequencyHz) {
        sound_mind::core::PathNode node;
        node.anchor = sound_mind::core::TimeFrequencyPoint{timeSeconds, frequencyHz};
        node.type = sound_mind::core::PathNodeType::Corner;
        path.addNode(node);
    };
    addCorner(rect.startTimeSeconds, rect.lowFrequencyHz);
    addCorner(rect.endTimeSeconds, rect.lowFrequencyHz);
    addCorner(rect.endTimeSeconds, rect.highFrequencyHz);
    addCorner(rect.startTimeSeconds, rect.highFrequencyHz);
    addCorner(rect.startTimeSeconds, rect.lowFrequencyHz);  // closes the loop.
    return path;
}

}  // namespace

PickController::PickController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void PickController::setGridSnapping(bool enabled, const FrequencyGridConfig& frequencyGridConfig,
                                       const TimingGridConfig& timingGridConfig) {
    gridSnappingEnabled_ = enabled;
    frequencyGridConfig_ = frequencyGridConfig;
    timingGridConfig_ = timingGridConfig;
}

void PickController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    clearSelection();
}

bool PickController::pick(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr) {
        clearSelection();
        return false;
    }
    if (pathEditSession_.isActive()) {
        const bool selected =
            pathEditSession_.selectNodeNear(point, sound_mind::core::frequencyToTimeScaleFor(project_->settings()));
        emit pathChanged();
        return selected;
    }

    const auto operations = project_->operationLog().activeOperationsTargeting(layer);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());

    // Every candidate under this point, most-recent-first (an overlapping
    // newer stroke ordinarily wins over an older one underneath it) -
    // activeOperationsTargeting() itself returns them in stack (back-to-
    // front) order. Any concrete Operation kind is a candidate, not just
    // PaintOperation - only PaintOperation gets its own brush-size
    // padding, since every other kind's bounds() already exactly matches
    // its real footprint (see pick()'s own docs).
    std::vector<const sound_mind::core::Operation*> candidates;
    for (auto it = operations.rbegin(); it != operations.rend(); ++it) {
        const sound_mind::core::Operation* operation = *it;
        double timePadding = kMinimumPickPaddingSeconds;
        if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(operation)) {
            timePadding = std::max(timePadding, paint->config().size());
        }
        const double frequencyPadding = timePadding * scale;
        if (containsPoint(operation->bounds(), point, timePadding, frequencyPadding)) {
            candidates.push_back(operation);
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
    const sound_mind::core::Operation* toSelect = candidates.front();
    if (pickedOperationId_.has_value()) {
        const auto currentIt = std::find_if(
            candidates.begin(), candidates.end(),
            [this](const sound_mind::core::Operation* op) { return op->id() == *pickedOperationId_; });
        if (currentIt != candidates.end()) {
            const auto nextIt = std::next(currentIt);
            toSelect = (nextIt != candidates.end()) ? *nextIt : candidates.front();
        }
    }

    pickedOperationId_ = toSelect->id();
    pickedLayer_ = layer;
    pickedOperation_ = toSelect;
    dragAnchor_ = point;
    dragCurrent_ = point;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit selectionChanged();
    return true;
}

bool PickController::selectOperation(sound_mind::core::LayerId layer, sound_mind::core::OperationId id) {
    if (project_ == nullptr) {
        clearSelection();
        return false;
    }

    const auto operations = project_->operationLog().activeOperationsTargeting(layer);
    const auto it = std::find_if(operations.begin(), operations.end(),
                                   [id](const sound_mind::core::Operation* op) { return op->id() == id; });
    if (it == operations.end()) {
        clearSelection();
        return false;
    }

    pickedOperationId_ = id;
    pickedLayer_ = layer;
    pickedOperation_ = *it;
    // No real click point to arm a drag from - the same "nothing to
    // preview yet" state a fresh pick() itself starts in.
    dragAnchor_ = sound_mind::core::TimeFrequencyPoint{};
    dragCurrent_ = dragAnchor_;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    pathEditSession_.end();
    emit selectionChanged();
    return true;
}

void PickController::clearSelection() {
    const bool hadSelection = pickedOperationId_.has_value();
    pickedOperationId_.reset();
    pickedOperation_ = nullptr;
    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    // Discards any in-progress path edit too, without committing it - a
    // cleared selection has nothing left to be editing the Path of.
    pathEditSession_.end();
    if (hadSelection) {
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::ToolConfiguration> PickController::selectedConfiguration() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return std::nullopt;
    }
    return paint->config();
}

std::optional<sound_mind::core::TimeFrequencyRect> PickController::selectionBounds() const {
    if (!pickedOperationId_.has_value()) {
        return std::nullopt;
    }
    return pickedOperation_->bounds();
}

void PickController::continueMove(sound_mind::core::TimeFrequencyPoint point) {
    if (pathEditSession_.isActive()) {
        if (gridSnappingEnabled_) {
            point = snapToGrid(point, frequencyGridConfig_, timingGridConfig_, project_->settings());
        }
        if (pathEditSession_.continueDrag(point, sound_mind::core::streamCodecConfigFor(project_->settings()))) {
            emit pathChanged();
        }
        return;
    }
    if (!pickedOperationId_.has_value()) {
        return;
    }
    if (gridSnappingEnabled_) {
        point = snapToGrid(point, frequencyGridConfig_, timingGridConfig_, project_->settings());
    }
    dragMoved_ = true;
    dragCurrent_ = point;
    const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
    const double deltaTimeSeconds = point.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyBins = frequencyBinDelta(dragAnchor_, point, config);

    if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_)) {
        previewPath_ = paint->path().translated(deltaTimeSeconds, deltaFrequencyBins, config);
    } else {
        previewPath_ = outlinePathFor(
            sound_mind::core::translated(pickedOperation_->bounds(), deltaTimeSeconds, deltaFrequencyBins, config));
    }
    emit pathChanged();
}

void PickController::endMove() {
    if (pathEditSession_.isActive()) {
        // continueDrag() already applied the drag directly to the
        // session's own preview - nothing further to do until
        // commitPathEdit()/cancelPathEdit() - see this method's own docs.
        return;
    }
    if (!pickedOperationId_.has_value() || !dragMoved_) {
        dragMoved_ = false;
        if (!previewPath_.nodes().empty()) {
            previewPath_ = sound_mind::core::Path{};
            emit pathChanged();
        }
        return;
    }

    const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
    const double deltaTimeSeconds = dragCurrent_.timeSeconds - dragAnchor_.timeSeconds;
    const double deltaFrequencyBins = frequencyBinDelta(dragAnchor_, dragCurrent_, config);

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    commitReplacement(pickedOperation_->translatedCopy(newId, deltaTimeSeconds, deltaFrequencyBins, config));

    dragMoved_ = false;
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
    emit selectionChanged();
}

void PickController::applyToolConfiguration(const sound_mind::core::ToolConfiguration& config) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return;  // Only a PaintOperation has a tool configuration to reapply - see this method's own docs.
    }

    sound_mind::core::Path newPath = paint->path();
    // Re-seeded from the new configuration's own default, exactly like a
    // fresh stroke - see ToolConfiguration::defaultGradient()'s own docs.
    newPath.gradient() = config.defaultGradient();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    commitReplacement(std::make_unique<sound_mind::core::PaintOperation>(newId, pickedLayer_, std::move(newPath),
                                                                            config, pickedOperationId_));
}

void PickController::deleteSelection() {
    if (pathEditSession_.isActive() && pathEditSession_.selectedNodeIndex().has_value()) {
        deleteSelectedPathNode();
        return;
    }
    if (!pickedOperationId_.has_value()) {
        return;
    }

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();

    std::unique_ptr<sound_mind::core::Operation> tombstone;
    if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_)) {
        tombstone = std::make_unique<sound_mind::core::PaintOperation>(
            newId, pickedLayer_, sound_mind::core::Path{}, paint->config(), pickedOperationId_);
    } else {
        // FillOperation/PasteOperation can't reduce to a literal zero-
        // effect copy of themselves - see this method's own docs.
        tombstone = std::make_unique<sound_mind::core::FillOperation>(
            newId, pickedLayer_, pickedOperation_->bounds(), sound_mind::core::silenceGradient(), pickedOperationId_);
    }

    commitReplacement(std::move(tombstone));
    clearSelection();
}

void PickController::bringToFront() { reorderSelection(&sound_mind::core::OperationLog::bringToFront); }

void PickController::sendToBack() { reorderSelection(&sound_mind::core::OperationLog::sendToBack); }

void PickController::bringForward() { reorderSelection(&sound_mind::core::OperationLog::bringForward); }

void PickController::sendBackward() { reorderSelection(&sound_mind::core::OperationLog::sendBackward); }

void PickController::reorderSelection(bool (sound_mind::core::OperationLog::*reorder)(sound_mind::core::OperationId)) {
    if (!pickedOperationId_.has_value()) {
        return;
    }
    if ((project_->operationLog().*reorder)(*pickedOperationId_)) {
        paintController_->rebuildLayerContent(pickedLayer_);
        emit contentChanged(pickedLayer_);
    }
}

bool PickController::beginPathEdit() {
    if (!pickedOperationId_.has_value()) {
        return false;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        return false;
    }
    pathEditSession_.begin(paint->path());
    emit pathChanged();
    return true;
}

void PickController::deleteSelectedPathNode() {
    if (pathEditSession_.deleteSelectedNode()) {
        emit pathChanged();
    }
}

void PickController::toggleSelectedPathNodeType() {
    if (!pathEditSession_.isActive() || !pathEditSession_.selectedNodeIndex().has_value()) {
        return;
    }
    if (pathEditSession_.toggleSelectedNodeType(project_->settings())) {
        emit pathChanged();
    }
}

void PickController::commitPathEdit() {
    if (!pathEditSession_.isActive()) {
        return;
    }
    const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(pickedOperation_);
    if (paint == nullptr) {
        // Defensive: beginPathEdit() only ever activates for a
        // PaintOperation, and nothing else changes pickedOperation_
        // while a path edit is active.
        cancelPathEdit();
        return;
    }

    sound_mind::core::Path editedPath = pathEditSession_.previewPath();
    // The original's own gradient carries over unchanged - editing
    // geometry shouldn't silently reset color or opacity.
    editedPath.gradient() = paint->path().gradient();
    const sound_mind::core::ToolConfiguration config = paint->config();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();

    pathEditSession_.end();

    commitReplacement(std::make_unique<sound_mind::core::PaintOperation>(newId, pickedLayer_, std::move(editedPath),
                                                                            config, pickedOperationId_));
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
}

void PickController::cancelPathEdit() {
    if (!pathEditSession_.isActive()) {
        return;
    }
    pathEditSession_.end();
    previewPath_ = sound_mind::core::Path{};
    emit pathChanged();
}

sound_mind::core::OperationId PickController::commitReplacement(
    std::unique_ptr<sound_mind::core::Operation> replacement) {
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = replacement->id();
    log.append(std::move(replacement));

    // The just-appended entry is always the log's own new last element -
    // re-resolved from there rather than kept from the moved-from local,
    // since ownership now belongs to the log.
    pickedOperationId_ = newId;
    pickedOperation_ = &log.at(log.size() - 1);
    pickedLayer_ = *pickedOperation_->targetLayer();

    paintController_->rebuildLayerContent(pickedLayer_);
    emit contentChanged(pickedLayer_);
    return newId;
}

}  // namespace sound_mind::studio
