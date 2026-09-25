#include "sound_mind/studio/selection_controller.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paste_application.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/wand_selection.h"
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

/// @brief `range`'s own bounding TimeFrequencyRect, converting its
/// absolute frame/bin corners back to real time/frequency via
/// frameIndexToTime()/binIndexToFrequency() - the inverse of rangeFor(),
/// needed wherever a Wand/combined selection's own mask range has to
/// become a plain bounding box (committedBounds_ is always one,
/// regardless of shape - see the class's own docs).
sound_mind::core::TimeFrequencyRect boundsOfRange(sound_mind::core::FrameBinRange range,
                                                    const sound_mind::codec::StreamCodecConfig& config) {
    sound_mind::core::TimeFrequencyRect rect;
    rect.startTimeSeconds = sound_mind::core::frameIndexToTime(range.frameLow, config);
    rect.endTimeSeconds = sound_mind::core::frameIndexToTime(range.frameHigh, config);
    rect.lowFrequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(range.binLow), config);
    rect.highFrequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(range.binHigh), config);
    return rect;
}

/// @brief The smallest FrameBinRange enclosing both `a` and `b` - Add's own
/// result footprint needs to cover everything either operand selects,
/// unlike Subtract/Intersect (which never exceed the first operand's own
/// range - see SelectionRegion::combine()'s own docs).
sound_mind::core::FrameBinRange unionRange(sound_mind::core::FrameBinRange a, sound_mind::core::FrameBinRange b) {
    sound_mind::core::FrameBinRange result;
    result.frameLow = std::min(a.frameLow, b.frameLow);
    result.frameHigh = std::max(a.frameHigh, b.frameHigh);
    result.binLow = std::min(a.binLow, b.binLow);
    result.binHigh = std::max(a.binHigh, b.binHigh);
    return result;
}

/// @brief `rect`'s own center point.
sound_mind::core::TimeFrequencyPoint rectCenter(const sound_mind::core::TimeFrequencyRect& rect) {
    return sound_mind::core::TimeFrequencyPoint{(rect.startTimeSeconds + rect.endTimeSeconds) / 2.0,
                                                 (rect.lowFrequencyHz + rect.highFrequencyHz) / 2.0};
}

/// @brief The angle (radians) from `center` to `point`, in the same
/// frequencyToTimeScale-normalized space rotatedRectangle() itself
/// rotates in - the shared geometry beginRotateDrag()/continueRotateDrag()
/// use to turn a dragged point into an angle.
double angleOfPoint(sound_mind::core::TimeFrequencyPoint point, sound_mind::core::TimeFrequencyPoint center,
                    double frequencyToTimeScale) {
    const double dt = point.timeSeconds - center.timeSeconds;
    const double df = (point.frequencyHz - center.frequencyHz) / frequencyToTimeScale;
    return std::atan2(df, dt);
}

/// @brief How far above rect's own top edge, as a fraction of its own
/// normalized height, the rotate handle sits - see
/// SelectionController::displayRotationHandle()'s own docs. A floor
/// height (in normalized units) keeps the handle from crowding a very
/// short/flat rectangle right up against its own top edge.
constexpr double kRotationHandleHeightFraction = 0.2;
constexpr double kRotationHandleMinNormalizedOffset = 0.5;

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
    lassoRawPoints_.clear();
    lassoPreviewPath_ = sound_mind::core::Path{};
    pendingWandRegion_.reset();
    pendingWandBounds_.reset();
    rotateDragActive_ = false;
    committedRotationRadians_.reset();
    const bool hadSelection = committedBounds_.has_value();
    committedBounds_.reset();
    committedBoundary_.reset();
    clipboard_.reset();
    clipboardBounds_.reset();
    clipboardBoundary_.reset();
    emit boundsChanged();
    if (hadSelection) {
        emit selectionChanged();
    }
}

void SelectionController::setSelectionShape(SelectionShape shape) {
    cancelSelectionDrag();
    currentShape_ = shape;
}

void SelectionController::beginSelectionDrag(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    dragActive_ = true;
    dragMoved_ = false;
    selectionLayer_ = layer;
    if (currentShape_ == SelectionShape::Lasso) {
        lassoRawPoints_.clear();
        lassoRawPoints_.push_back(point);
        lassoPreviewPath_ = sound_mind::core::Path{};
    } else if (currentShape_ == SelectionShape::Wand) {
        // A single-click gesture - the whole flood fill runs right here,
        // at the anchor, rather than waiting for endSelectionDrag() - see
        // this class's own docs on why continueSelectionDrag() then
        // ignores any further movement entirely.
        pendingWandRegion_.reset();
        pendingWandBounds_.reset();
        if (project_ != nullptr) {
            paintController_->rebuildLayerContent(layer);
            if (const sound_mind::core::Layer* layerPtr = project_->layerById(layer);
                layerPtr != nullptr && layerPtr->content().has_value()) {
                sound_mind::core::SelectionRegion region = sound_mind::core::selectByAmplitudeSimilarity(
                    *layerPtr->content(), point, wandTolerancePercent_, wandHarmonicsAware_);
                if (!region.maskCells().empty()) {
                    const sound_mind::core::FrameBinRange range{region.maskFrameLow(), region.maskFrameHigh(),
                                                                  region.maskBinLow(), region.maskBinHigh()};
                    pendingWandBounds_ = boundsOfRange(range, layerPtr->content()->config);
                    pendingWandRegion_ = std::move(region);
                }
            }
        }
    } else {
        dragAnchor_ = point;
        dragPreviewBounds_ = rectFromCorners(point, point);
    }
    emit boundsChanged();
}

void SelectionController::continueSelectionDrag(sound_mind::core::TimeFrequencyPoint point) {
    if (!dragActive_) {
        return;
    }
    if (currentShape_ == SelectionShape::Wand) {
        return;  // Ignored entirely - see beginSelectionDrag()'s own docs.
    }
    if (gridSnappingEnabled_ && project_ != nullptr) {
        point = snapToGrid(point, frequencyGridConfig_, timingGridConfig_, project_->settings());
    }
    dragMoved_ = true;
    if (currentShape_ == SelectionShape::Lasso) {
        lassoRawPoints_.push_back(point);
        // Cheap enough to refit on every sample - the same
        // "PaintController::continueStroke()'s own precedent" this
        // class's own docs already cite.
        if (lassoRawPoints_.size() >= 2 && project_ != nullptr) {
            lassoPreviewPath_ = sound_mind::core::fitPathToPoints(
                lassoRawPoints_, sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
                /*simplifyToleranceSeconds=*/0.01);
        }
    } else {
        dragPreviewBounds_ = rectFromCorners(dragAnchor_, point);
    }
    emit boundsChanged();
}

void SelectionController::endSelectionDrag() {
    if (!dragActive_) {
        return;
    }
    dragActive_ = false;

    // Resolve the just-finished drag's own result, uniformly across all
    // three shapes, into a plain (bounds, boundary) pair - std::nullopt
    // bounds means "drew nothing meaningful" (the same convention every
    // shape already used before Wand/combine existed).
    std::optional<sound_mind::core::TimeFrequencyRect> newBounds;
    std::optional<sound_mind::core::SelectionRegion> newBoundary;

    if (currentShape_ == SelectionShape::Wand) {
        newBounds = pendingWandBounds_;
        newBoundary = std::move(pendingWandRegion_);
        pendingWandRegion_.reset();
        pendingWandBounds_.reset();
    } else if (currentShape_ == SelectionShape::Lasso) {
        // Refit once more from the final point set (continueSelectionDrag()
        // may never have run at all for a plain click) - fewer than 3
        // resulting nodes can't enclose any area, so that's treated the
        // same as "drew nothing".
        sound_mind::core::Path fitted;
        if (dragMoved_ && lassoRawPoints_.size() >= 3 && project_ != nullptr) {
            fitted = sound_mind::core::fitPathToPoints(
                lassoRawPoints_, sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
                /*simplifyToleranceSeconds=*/0.01);
        }
        lassoRawPoints_.clear();
        lassoPreviewPath_ = sound_mind::core::Path{};
        if (fitted.nodes().size() >= 3) {
            newBounds = fitted.bounds();
            newBoundary = sound_mind::core::SelectionRegion(std::move(fitted));
        }
    } else if (dragMoved_) {
        newBounds = dragPreviewBounds_;
        // newBoundary stays std::nullopt - a plain rectangle.
    }

    if (!newBounds.has_value()) {
        // A combining gesture (Add/Subtract/Intersect) that produced
        // nothing leaves the existing selection untouched entirely - a
        // "whiffed" combine shouldn't destroy what it was trying to
        // modify. Only Replace mode's own "drew nothing" clears, matching
        // every shape's own pre-existing "click empty space" convention.
        if (currentCombineMode_ == SelectionCombineMode::Replace) {
            const bool hadSelection = committedBounds_.has_value();
            committedBounds_.reset();
            committedBoundary_.reset();
            committedRotationRadians_.reset();
            emit boundsChanged();
            if (hadSelection) {
                emit selectionChanged();
            }
        }
        return;
    }

    const bool effectivelyReplacing = currentCombineMode_ == SelectionCombineMode::Replace || !committedBounds_.has_value();
    if (effectivelyReplacing) {
        committedBounds_ = newBounds;
        committedBoundary_ = std::move(newBoundary);
    } else {
        // A boolean-combined result is always Mask-kind (see
        // SelectionRegion::combine()'s own docs) - never rotatable,
        // regardless of what shape either operand started out as.
        combineIntoCommittedSelection(*newBounds, newBoundary);
    }
    // Only a plain, freshly-drawn Rectangle (never a Lasso, a Wand
    // selection, or any combined result) is rotatable - see
    // canRotateSelection()'s own docs.
    if (effectivelyReplacing && currentShape_ == SelectionShape::Rectangle) {
        committedRotationRadians_ = 0.0;
        committedUnrotatedRect_ = *committedBounds_;
    } else {
        committedRotationRadians_.reset();
    }
    emit boundsChanged();
    emit selectionChanged();
}

void SelectionController::combineIntoCommittedSelection(
    const sound_mind::core::TimeFrequencyRect& newBounds,
    const std::optional<sound_mind::core::SelectionRegion>& newBoundary) {
    if (project_ == nullptr) {
        return;
    }
    paintController_->rebuildLayerContent(selectionLayer_);
    const sound_mind::core::Layer* layer = project_->layerById(selectionLayer_);
    if (layer == nullptr || !layer->content().has_value()) {
        return;
    }
    const auto& content = *layer->content();

    const sound_mind::core::FrameBinRange oldRange = rangeFor(*committedBounds_, content.config, content.frameCount);
    const sound_mind::core::FrameBinRange newRange = rangeFor(newBounds, content.config, content.frameCount);
    const sound_mind::core::SelectionRegion::BooleanOp op =
        (currentCombineMode_ == SelectionCombineMode::Add)   ? sound_mind::core::SelectionRegion::BooleanOp::Add
        : (currentCombineMode_ == SelectionCombineMode::Subtract) ? sound_mind::core::SelectionRegion::BooleanOp::Subtract
                                                                    : sound_mind::core::SelectionRegion::BooleanOp::Intersect;
    // Add's own result has to cover everything either operand selects;
    // Subtract/Intersect never exceed the *existing* selection's own
    // extent - see SelectionRegion::combine()'s own docs.
    const sound_mind::core::FrameBinRange resultRange =
        (op == sound_mind::core::SelectionRegion::BooleanOp::Add) ? unionRange(oldRange, newRange) : oldRange;

    committedBoundary_ = sound_mind::core::SelectionRegion::combine(committedBoundary_, oldRange, newBoundary, newRange,
                                                                      resultRange, op, content.config);
    committedBounds_ = boundsOfRange(resultRange, content.config);
}

void SelectionController::cancelSelectionDrag() {
    if (!dragActive_) {
        return;
    }
    dragActive_ = false;
    dragMoved_ = false;
    lassoRawPoints_.clear();
    lassoPreviewPath_ = sound_mind::core::Path{};
    pendingWandRegion_.reset();
    pendingWandBounds_.reset();
    emit boundsChanged();  // reverts the display back to the committed selection (or none).
}

void SelectionController::refreshRotatedSelection() {
    if (project_ == nullptr) {
        return;
    }
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
    // Close enough to zero stays a plain rectangle (std::nullopt) rather
    // than a degenerate zero-rotation Path - see this method's own docs.
    if (std::abs(*committedRotationRadians_) < 1e-9) {
        committedBoundary_.reset();
        committedBounds_ = committedUnrotatedRect_;
        return;
    }
    sound_mind::core::Path rotated =
        sound_mind::core::rotatedRectangle(committedUnrotatedRect_, *committedRotationRadians_, scale);
    committedBounds_ = rotated.bounds();
    committedBoundary_ = sound_mind::core::SelectionRegion(std::move(rotated));
}

void SelectionController::beginRotateDrag(sound_mind::core::TimeFrequencyPoint point) {
    if (!canRotateSelection() || project_ == nullptr) {
        return;
    }
    rotateDragActive_ = true;
    rotateDragStartRotation_ = *committedRotationRadians_;
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
    rotateDragStartAngleRadians_ = angleOfPoint(point, rectCenter(committedUnrotatedRect_), scale);
}

void SelectionController::continueRotateDrag(sound_mind::core::TimeFrequencyPoint point) {
    if (!rotateDragActive_ || project_ == nullptr) {
        return;
    }
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_->settings());
    const double currentAngle = angleOfPoint(point, rectCenter(committedUnrotatedRect_), scale);
    const double sweep = currentAngle - rotateDragStartAngleRadians_;
    committedRotationRadians_ = rotateDragStartRotation_ + sweep;
    refreshRotatedSelection();
    emit boundsChanged();
    emit selectionChanged();
}

void SelectionController::endRotateDrag() {
    // The rotation is already fully committed live, via
    // continueRotateDrag() - nothing further to do here.
    rotateDragActive_ = false;
}

void SelectionController::cancelRotateDrag() {
    if (!rotateDragActive_) {
        return;
    }
    rotateDragActive_ = false;
    const bool changed = committedRotationRadians_.has_value() && *committedRotationRadians_ != rotateDragStartRotation_;
    committedRotationRadians_ = rotateDragStartRotation_;
    refreshRotatedSelection();
    if (changed) {
        emit boundsChanged();
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::TimeFrequencyPoint> SelectionController::displayRotationHandle() const {
    if (!canRotateSelection()) {
        return std::nullopt;
    }
    const double scale = sound_mind::core::frequencyToTimeScaleFor(project_ != nullptr ? project_->settings()
                                                                                          : sound_mind::core::ProjectSettings{});
    const double normalizedHeight =
        (committedUnrotatedRect_.highFrequencyHz - committedUnrotatedRect_.lowFrequencyHz) / scale;
    const double offsetNormalized =
        std::max(normalizedHeight * kRotationHandleHeightFraction, kRotationHandleMinNormalizedOffset);

    // The handle's own position before rotation: directly above the
    // rectangle's own top-center, offsetNormalized further up.
    const sound_mind::core::TimeFrequencyPoint unrotatedHandle{
        (committedUnrotatedRect_.startTimeSeconds + committedUnrotatedRect_.endTimeSeconds) / 2.0,
        committedUnrotatedRect_.highFrequencyHz + offsetNormalized * scale};

    const sound_mind::core::TimeFrequencyPoint center = rectCenter(committedUnrotatedRect_);
    const double angle = *committedRotationRadians_;
    const double dt = unrotatedHandle.timeSeconds - center.timeSeconds;
    const double df = (unrotatedHandle.frequencyHz - center.frequencyHz) / scale;
    const double rotatedDt = dt * std::cos(angle) - df * std::sin(angle);
    const double rotatedDf = dt * std::sin(angle) + df * std::cos(angle);
    return sound_mind::core::TimeFrequencyPoint{center.timeSeconds + rotatedDt,
                                                 center.frequencyHz + rotatedDf * scale};
}

void SelectionController::clearSelection() {
    const bool hadSelection = committedBounds_.has_value();
    committedBounds_.reset();
    committedBoundary_.reset();
    committedRotationRadians_.reset();
    if (hadSelection) {
        emit boundsChanged();
        emit selectionChanged();
    }
}

std::optional<sound_mind::core::TimeFrequencyRect> SelectionController::displayBounds() const {
    if (dragActive_) {
        if (currentShape_ == SelectionShape::Lasso) {
            return lassoPreviewPath_.nodes().empty() ? std::nullopt
                                                       : std::optional(lassoPreviewPath_.bounds());
        }
        if (currentShape_ == SelectionShape::Wand) {
            return pendingWandBounds_;
        }
        return dragPreviewBounds_;
    }
    return committedBounds_;
}

std::optional<sound_mind::core::Path> SelectionController::displayBoundary() const {
    if (dragActive_) {
        if (currentShape_ == SelectionShape::Lasso && !lassoPreviewPath_.nodes().empty()) {
            return lassoPreviewPath_;
        }
        return std::nullopt;
    }
    if (committedBoundary_ && committedBoundary_->kind() == sound_mind::core::SelectionRegionKind::Path) {
        return committedBoundary_->path();
    }
    return std::nullopt;
}

bool SelectionController::hasMaskShapedSelection() const noexcept {
    if (dragActive_ && currentShape_ == SelectionShape::Wand) {
        // A live Wand preview is always Mask-shaped too - without this,
        // the dashed-vs-solid indicator would flicker briefly solid during
        // the (short, but real) window between press and release.
        return pendingWandRegion_.has_value();
    }
    return committedBoundary_.has_value() && committedBoundary_->kind() == sound_mind::core::SelectionRegionKind::Mask;
}

void SelectionController::fill(const sound_mind::core::Gradient& gradient) {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return;
    }
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::FillOperation>(id, selectionLayer_, *committedBounds_, gradient,
                                                                    std::nullopt, committedBoundary_));
    paintController_->notifyOperationCommitted();
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
    clipboardBoundary_ = committedBoundary_;
}

void SelectionController::cutSelection() {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return;
    }
    copySelection();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::FillOperation>(id, selectionLayer_, *committedBounds_,
                                                                    sound_mind::core::silenceGradient(), std::nullopt,
                                                                    committedBoundary_));
    paintController_->notifyOperationCommitted();
    paintController_->rebuildLayerContent(selectionLayer_);
    emit contentChanged(selectionLayer_);
}

std::optional<sound_mind::core::MindShotId> SelectionController::captureMindShot(const std::string& name) {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return std::nullopt;
    }
    // Same reasoning as copySelection()'s own identical call.
    paintController_->rebuildLayerContent(selectionLayer_);
    const sound_mind::core::Layer* layer = project_->layerById(selectionLayer_);
    if (layer == nullptr || !layer->content().has_value()) {
        return std::nullopt;
    }

    sound_mind::core::Clip clip = sound_mind::core::captureClip(*layer->content(), *committedBounds_);
    const sound_mind::core::MindShotId id = project_->addMindShot(name, std::move(clip));
    emit mindShotCaptured(id);
    return id;
}

std::optional<sound_mind::core::MindGrainId> SelectionController::captureMindGrain(const std::string& name) {
    if (!committedBounds_.has_value() || project_ == nullptr) {
        return std::nullopt;
    }
    // Unlike captureMindShot(), no content capture at all - a Mind Grain
    // stores only the reference {selectionLayer_, bounds}; see this
    // method's own docs.
    const sound_mind::core::MindGrainId id = project_->addMindGrain(name, selectionLayer_, *committedBounds_);
    emit mindGrainCaptured(id);
    return id;
}

std::optional<sound_mind::core::OperationId> SelectionController::pasteInto(sound_mind::core::LayerId targetLayer,
                                                                              sound_mind::core::BlendMode blendMode) {
    if (!clipboard_.has_value() || !clipboardBounds_.has_value() || project_ == nullptr) {
        return std::nullopt;
    }
    // Ensure targetLayer has real content to paste onto - same reasoning
    // as copySelection()'s own call.
    paintController_->rebuildLayerContent(targetLayer);

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::PasteOperation>(
        id, targetLayer, *clipboardBounds_, *clipboard_, std::nullopt, clipboardBoundary_, blendMode));
    paintController_->notifyOperationCommitted();
    paintController_->rebuildLayerContent(targetLayer);
    emit contentChanged(targetLayer);

    // The pasted region becomes the new committed selection, on the layer
    // it was actually pasted onto - visual confirmation of both where it
    // landed and (via selectionLayer_) which layer that was, the same way
    // a freshly drawn selection would be. Carries the same Lasso shape
    // forward too, if that's what was copied.
    selectionLayer_ = targetLayer;
    committedBounds_ = *clipboardBounds_;
    committedBoundary_ = clipboardBoundary_;
    // Never a rotatable Rectangle - a pasted selection was never drawn
    // fresh, regardless of what shape it was originally copied from.
    committedRotationRadians_.reset();
    emit boundsChanged();
    emit selectionChanged();

    return id;
}

}  // namespace sound_mind::studio
