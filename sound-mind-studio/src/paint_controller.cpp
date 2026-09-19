#include "sound_mind/studio/paint_controller.h"

#include <algorithm>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/undo_stack.h"

namespace sound_mind::studio {

PaintController::PaintController(UndoStack* undoStack, QObject* parent) : QObject(parent), undoStack_(undoStack) {}

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
    // Silent refusal to even start a Mind Grain stroke on/below its own
    // source layer - the backstop layer of defense described in
    // docs/sound-mind-design.md's "Mind Grains"; every other guardrail
    // (Tool Configuration's own red highlight, the Layers Panel's red X,
    // the Paint button's own disabled state) exists so the user practically
    // never reaches this point in the first place, but this check is what
    // actually makes an invalid stroke impossible rather than just
    // discouraged.
    if (const auto* mindGrain = dynamic_cast<const sound_mind::core::MindGrainConfiguration*>(toolConfig_.get())) {
        if (!sound_mind::core::isLayerAbove(*project_, targetLayer, mindGrain->sourceLayerId())) {
            return;
        }
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
        previewPath_ = sound_mind::core::fitPathToPoints(
            strokePoints_, sound_mind::core::frequencyToTimeScaleFor(project_->settings()), /*simplifyToleranceSeconds=*/0.01);
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
        finalPath = sound_mind::core::fitPathToPoints(
            strokePoints_, sound_mind::core::frequencyToTimeScaleFor(project_->settings()), /*simplifyToleranceSeconds=*/0.01);
    }
    // Seed the new stroke's own gradient from the tool's default - see
    // ToolConfiguration::defaultGradient()'s own docs.
    finalPath.gradient() = toolConfig_->defaultGradient();

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    log.append(std::make_unique<sound_mind::core::PaintOperation>(id, strokeTargetLayer_, std::move(finalPath),
                                                                    toolConfig_->clone()));
    notifyOperationCommitted();

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

void PaintController::notifyOperationCommitted() {
    if (undoStack_ == nullptr) {
        return;
    }
    undoStack_->push({/*undo=*/[this]() { undo(); }, /*redo=*/[this]() { redo(); }});
}

void PaintController::rebuildLayerContent(sound_mind::core::LayerId layer) {
    std::unordered_set<sound_mind::core::LayerId> visited;
    rebuildLayerContentAndCascade(layer, visited);
}

void PaintController::rebuildLayerContentAndCascade(sound_mind::core::LayerId layer,
                                                     std::unordered_set<sound_mind::core::LayerId>& visited) {
    if (project_ == nullptr || visited.contains(layer)) {
        return;
    }
    visited.insert(layer);

    sound_mind::core::Layer* target = project_->layerById(layer);
    if (target == nullptr) {
        return;
    }

    // Capture this layer's pre-paint base exactly once - see this
    // method's own docs. A layer with no content yet (the Background
    // layer, in particular - Project::createNew() deliberately leaves it
    // content-less, see docs/sound-mind-architecture.md's Decisions Made)
    // gets a real, silent, project-sized base synthesized on the spot
    // instead of being unpaintable: every layer is a real canvas the
    // moment something is actually painted onto it, whether or not it
    // already had content from an import/recording.
    if (!baseContent_.contains(layer)) {
        const sound_mind::codec::StreamImage base = target->content().has_value()
                                                          ? *target->content()
                                                          : sound_mind::core::silentContentFor(project_->settings());
        baseContent_.emplace(layer, base);
    }

    // Resolves another layer's own *current* content for a Mind Grain
    // stamp (see paint_application.h's own LayerContentResolver docs) -
    // reading straight from the live Project, so a Mind Grain stroke
    // rebuilt here always sees its source layer as of *this* rebuild.
    const sound_mind::core::Project* project = project_;
    const auto resolveLayerContent =
        [project](sound_mind::core::LayerId id) -> const sound_mind::codec::StreamImage* {
        const sound_mind::core::Layer* layer = project->layerById(id);
        return (layer != nullptr && layer->content().has_value()) ? &*layer->content() : nullptr;
    };

    // Resolves an InstrumentConfiguration's own vibrato/tremolo MindWaveId
    // for a replayed PaintOperation (see paint_application.h's own
    // MindWaveResolver docs) - reading straight from the live Project, the
    // same "resolved fresh, not a snapshot" contract every other MindWave
    // binding in this codebase already keeps.
    const auto resolveMindWave = [project](sound_mind::core::MindWaveId id) -> const sound_mind::core::MindWave* {
        const sound_mind::core::NamedMindWave* named = project->mindWaveById(id);
        return named != nullptr ? &named->wave : nullptr;
    };

    const auto activeOperations = project_->operationLog().activeOperationsTargeting(layer);
    sound_mind::codec::StreamImage rebuilt = sound_mind::core::rebuildPaintedContent(
        baseContent_.at(layer), activeOperations, sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
        resolveLayerContent, resolveMindWave);
    target->setContent(std::move(rebuilt));

    emit contentChanged(layer);

    // Cascade immediately to every layer with a Mind Grain stroke sourced
    // from this one - see rebuildLayerContent()'s own docs. `visited`
    // already guards against re-rebuilding a layer reachable through more
    // than one dependency chain (a diamond).
    for (const sound_mind::core::LayerId dependent :
         sound_mind::core::layersWithMindGrainOperationsSourcedFrom(*project_, layer)) {
        rebuildLayerContentAndCascade(dependent, visited);
    }
}

}  // namespace sound_mind::studio
