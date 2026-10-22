#include "sound_mind/studio/paint_controller.h"

#include <algorithm>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/project_history.h"

namespace sound_mind::studio {

PaintController::PaintController(ProjectHistory* history, QObject* parent)
    : QObject(parent), history_(history != nullptr ? history : new ProjectHistory(this)), ownsHistory_(history == nullptr) {
    // Undo/redo restore the operations and bases; this re-renders every
    // layer whose content that changed, before anything refreshes.
    history_->addContentRebuilder([this](const sound_mind::core::HistoryChange& change) {
        for (const sound_mind::core::LayerId layer : change.layers) {
            if (project_ == nullptr || project_->layerById(layer) == nullptr) {
                forgetLayer(layer);
            }
        }
        for (const sound_mind::core::LayerId layer : change.contentLayers) {
            contentCache_[layer].invalidateAll();
            rebuildLayerContent(layer);
        }
    });
}

void PaintController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    strokeInProgress_ = false;
    strokePoints_.clear();
    previewPath_ = sound_mind::core::Path{};
    contentCache_.clear();
    cachedBaseRevision_.clear();
    history_->setProject(project);
}

void PaintController::invalidateContentCaches() {
    for (auto& [layerId, cache] : contentCache_) {
        cache.invalidateAll();
    }
}

void PaintController::invalidateContentCache(sound_mind::core::LayerId layer) {
    if (const auto it = contentCache_.find(layer); it != contentCache_.end()) {
        it->second.invalidateAll();
    }
}

void PaintController::beginStroke(sound_mind::core::LayerId targetLayer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr || strokeInProgress_ || !canPaintOn(targetLayer)) {
        return;
    }
    strokeInProgress_ = true;
    strokeTargetLayer_ = targetLayer;
    strokePoints_.clear();
    strokePoints_.push_back(point);
    previewPath_ = sound_mind::core::Path{};
}

bool PaintController::canPaintOn(sound_mind::core::LayerId targetLayer) const {
    // A Filter (or Equalizer) layer was never meant to hold its own
    // paintable amplitude content - it only composites-and-transforms the
    // layers beneath it (see docs/sound-mind-design.md's "Filter Layer").
    // Previously unguarded anywhere in the paint path (a real, pre-existing
    // gap - see docs/sound-mind-roadmap.md's Phase 4 "Known bug" note),
    // re-surfaced by a real-world testing pass; fixed here the same way the
    // Mind Grain guard just below silently refuses rather than erroring.
    if (const auto* targetLayerPtr = project_->layerById(targetLayer);
        targetLayerPtr != nullptr && sound_mind::core::isFilterLayerType(targetLayerPtr->type())) {
        return false;
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
            return false;
        }
    }
    return true;
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
    notifyOperationCommitted(tr("Painted stroke"));

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

bool PaintController::canUndo() const noexcept { return project_ != nullptr && history_->canUndo(); }

bool PaintController::canRedo() const noexcept { return project_ != nullptr && history_->canRedo(); }

void PaintController::undo() {
    if (project_ != nullptr) {
        history_->undo();
    }
}

void PaintController::redo() {
    if (project_ != nullptr) {
        history_->redo();
    }
}

void PaintController::notifyOperationCommitted(const QString& description) {
    if (project_ != nullptr) {
        history_->commit(*project_, description);
    }
}

void PaintController::notifyOperationsCommitted(std::size_t count, const QString& description) {
    if (count > 0) {
        notifyOperationCommitted(description);
    }
}

std::size_t PaintController::strokePaths(sound_mind::core::LayerId layer,
                                         const std::vector<sound_mind::core::Path>& paths,
                                         const QString& description) {
    if (project_ == nullptr || !canPaintOn(layer)) {
        return 0;
    }
    sound_mind::core::OperationLog& log = project_->operationLog();
    std::size_t count = 0;
    for (const sound_mind::core::Path& source : paths) {
        if (source.nodes().empty()) {
            continue;
        }
        sound_mind::core::Path path = source;
        path.gradient() = toolConfig_->defaultGradient();
        log.append(std::make_unique<sound_mind::core::PaintOperation>(log.reserveId(), layer, std::move(path),
                                                                        toolConfig_->clone()));
        ++count;
    }
    if (count == 0) {
        return 0;
    }
    notifyOperationsCommitted(count, description);
    rebuildLayerContent(layer);  // emits contentChanged().
    return count;
}

std::unique_ptr<sound_mind::core::Operation> PaintController::makeDeletionTombstone(
    const sound_mind::core::Operation& operation, sound_mind::core::OperationId newId) {
    const sound_mind::core::LayerId layer = *operation.targetLayer();
    if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(&operation)) {
        // An empty path paints nothing whatever the config. A Mind Grain
        // config would still name its source layer, though, and keep
        // counting as a dependent of it - so it's swapped for a plain one.
        auto config = dynamic_cast<const sound_mind::core::MindGrainConfiguration*>(&paint->config()) != nullptr
                          ? std::unique_ptr<sound_mind::core::ToolConfiguration>(
                                std::make_unique<sound_mind::core::ProceduralConfiguration>())
                          : paint->config().clone();
        return std::make_unique<sound_mind::core::PaintOperation>(newId, layer, sound_mind::core::Path{},
                                                                   std::move(config), operation.id());
    }
    if (dynamic_cast<const sound_mind::core::GuidePathOperation*>(&operation) != nullptr) {
        // Silent to begin with, so an empty one is enough - a silence Fill
        // would erase whatever is painted underneath it.
        return std::make_unique<sound_mind::core::GuidePathOperation>(
            newId, layer, std::vector<sound_mind::core::BranchGraft>{}, operation.id());
    }
    return std::make_unique<sound_mind::core::FillOperation>(newId, layer, operation.bounds(),
                                                             sound_mind::core::silenceGradient(), operation.id());
}

void PaintController::deleteOperations(const std::vector<sound_mind::core::OperationId>& ids,
                                       const QString& description) {
    if (project_ == nullptr) {
        return;
    }
    sound_mind::core::OperationLog& log = project_->operationLog();

    std::vector<const sound_mind::core::Operation*> toDelete;
    for (const sound_mind::core::OperationId id : ids) {
        for (std::size_t i = 0; i < log.size(); ++i) {
            const sound_mind::core::Operation& candidate = log.at(i);
            if (candidate.id() != id || !candidate.targetLayer().has_value()) {
                continue;
            }
            const auto active = log.activeOperationsTargeting(*candidate.targetLayer());
            if (std::find(active.begin(), active.end(), &candidate) != active.end()) {
                toDelete.push_back(&candidate);
            }
            break;
        }
    }
    if (toDelete.empty()) {
        return;
    }

    // Build every tombstone before appending any - append() may reallocate
    // the log's own storage, which would invalidate the pointers above.
    std::vector<std::unique_ptr<sound_mind::core::Operation>> tombstones;
    std::unordered_set<sound_mind::core::LayerId> touchedLayers;
    for (const sound_mind::core::Operation* operation : toDelete) {
        touchedLayers.insert(*operation->targetLayer());
        tombstones.push_back(makeDeletionTombstone(*operation, log.reserveId()));
    }
    const std::size_t count = tombstones.size();
    for (auto& tombstone : tombstones) {
        log.append(std::move(tombstone));
    }

    notifyOperationsCommitted(count, description);
    for (const sound_mind::core::LayerId layer : touchedLayers) {
        rebuildLayerContent(layer);
        emit contentChanged(layer);
    }
}

void PaintController::forgetLayer(sound_mind::core::LayerId layer) {
    contentCache_.erase(layer);
    cachedBaseRevision_.erase(layer);
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

    // The layer's own base (v0.1.43.1): what an import, recording,
    // generator or phase cleanup made it - or, for a layer with
    // no content yet (the Background layer, which Project::createNew()
    // leaves content-less), silence, so every layer is paintable. A new
    // base means the replay cache describes something else.
    if (const auto it = cachedBaseRevision_.find(layer);
        it == cachedBaseRevision_.end() || it->second != target->baseRevision()) {
        contentCache_[layer].invalidateAll();
        cachedBaseRevision_[layer] = target->baseRevision();
    }
    const sound_mind::codec::StreamImage silent =
        target->baseContent() == nullptr ? sound_mind::core::silentContentFor(project_->settings())
                                         : sound_mind::codec::StreamImage{};
    const sound_mind::codec::StreamImage& base = target->baseContent() != nullptr ? *target->baseContent() : silent;

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
    const sound_mind::core::ProjectSettings settings = project_->settings();
    sound_mind::codec::StreamImage rebuilt = sound_mind::core::rebuildPaintedContentCached(
        base, activeOperations, sound_mind::core::frequencyToTimeScaleFor(project_->settings()),
        contentCache_[layer], resolveLayerContent, resolveMindWave, &settings);
    target->setContent(std::move(rebuilt));

    emit contentChanged(layer);

    // Cascade immediately to every layer with a Mind Grain stroke sourced
    // from this one - see rebuildLayerContent()'s own docs. `visited`
    // already guards against re-rebuilding a layer reachable through more
    // than one dependency chain (a diamond). Each dependent's own
    // PaintContentCache is explicitly invalidated first (v0.1.8.1) - its
    // own active operations haven't necessarily changed (the Mind Grain
    // stroke itself is still the same pointer, same position), but what
    // it *resolves to* just did, since `layer`'s own content is what it
    // reads live - see sound_mind::core::PaintContentCache's own docs on
    // why this explicit call is needed here specifically.
    for (const sound_mind::core::LayerId dependent :
         sound_mind::core::layersWithMindGrainOperationsSourcedFrom(*project_, layer)) {
        contentCache_[dependent].invalidateAll();
        rebuildLayerContentAndCascade(dependent, visited);
    }
}

}  // namespace sound_mind::studio
