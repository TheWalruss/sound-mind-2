#include "sound_mind/core/history.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "sound_mind/core/project.h"

namespace sound_mind::core {

namespace {

constexpr const char* kMindWaves = "mindWaves";
constexpr const char* kMindShots = "mindShots";
constexpr const char* kMindGrains = "mindGrains";
constexpr const char* kResonanceProfiles = "resonanceProfiles";
constexpr const char* kConvolutionKernels = "convolutionKernels";
constexpr const char* kToolPresets = "toolPresets";
constexpr const char* kFilterPresets = "filterPresets";
constexpr const char* kGridPresets = "gridPresets";
constexpr const char* kMidiProgramMappings = "midiProgramMappings";

nlohmann::json resourceJson(const Project& project, const std::string& collection) {
    if (collection == kMindWaves) {
        return project.mindWaves();
    }
    if (collection == kMindShots) {
        return project.mindShots();
    }
    if (collection == kMindGrains) {
        return project.mindGrains();
    }
    if (collection == kResonanceProfiles) {
        return project.resonanceProfiles();
    }
    if (collection == kConvolutionKernels) {
        return project.convolutionKernels();
    }
    if (collection == kToolPresets) {
        return project.toolPresets();
    }
    if (collection == kFilterPresets) {
        return project.filterPresets();
    }
    if (collection == kGridPresets) {
        return project.gridPresets();
    }
    return project.midiProgramMappings();
}

void setResourceJson(Project& project, const std::string& collection, const nlohmann::json& json) {
    if (collection == kMindWaves) {
        project.mindWaves() = json.get<std::vector<NamedMindWave>>();
    } else if (collection == kMindShots) {
        project.mindShots() = json.get<std::vector<NamedMindShot>>();
    } else if (collection == kMindGrains) {
        project.mindGrains() = json.get<std::vector<NamedMindGrain>>();
    } else if (collection == kResonanceProfiles) {
        project.resonanceProfiles() = json.get<std::vector<NamedResonanceProfile>>();
    } else if (collection == kConvolutionKernels) {
        project.convolutionKernels() = json.get<std::vector<NamedConvolutionKernel>>();
    } else if (collection == kToolPresets) {
        project.toolPresets() = json.get<std::vector<NamedToolPreset>>();
    } else if (collection == kFilterPresets) {
        project.filterPresets() = json.get<std::vector<NamedFilterPreset>>();
    } else if (collection == kGridPresets) {
        project.gridPresets() = json.get<std::vector<NamedGridPreset>>();
    } else if (collection == kMidiProgramMappings) {
        project.setMidiProgramMappings(json.get<std::vector<MidiProgramMapping>>());
    }
}

std::vector<LayerId> layerOrder(const ProjectState& state) {
    std::vector<LayerId> order;
    order.reserve(state.layers.size());
    for (const auto& layer : state.layers) {
        order.push_back(layer.id);
    }
    return order;
}

bool statesEqual(const ProjectState& a, const ProjectState& b) {
    if (a.layers.size() != b.layers.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.layers.size(); ++i) {
        if (a.layers[i].id != b.layers[i].id || a.layers[i].baseRevision != b.layers[i].baseRevision ||
            a.layers[i].json != b.layers[i].json) {
            return false;
        }
    }
    // The next-id counters alone don't make a change - reserving an id
    // for an operation that's then not appended isn't anything to undo.
    // An entry that records a real change still restores them.
    return a.operationIds == b.operationIds && a.stackOrder == b.stackOrder && a.baked == b.baked &&
           a.resources == b.resources;
}

double nowSeconds() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Puts the project's layers in `order`, leaving any it doesn't name on top
/// in their current order.
void reorderLayersTo(Project& project, const std::vector<LayerId>& order) {
    std::vector<Layer>& layers = project.layers();
    std::vector<Layer> reordered;
    reordered.reserve(layers.size());
    std::vector<bool> taken(layers.size(), false);
    for (const LayerId id : order) {
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (!taken[i] && layers[i].id() == id) {
                reordered.push_back(std::move(layers[i]));
                taken[i] = true;
                break;
            }
        }
    }
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (!taken[i]) {
            reordered.push_back(std::move(layers[i]));
        }
    }
    layers = std::move(reordered);
}

std::optional<LayerId> targetLayerOfOperation(const OperationLog& log, OperationId id) {
    for (std::size_t i = 0; i < log.size(); ++i) {
        if (log.at(i).id() == id) {
            return log.at(i).targetLayer();
        }
    }
    return std::nullopt;
}

/// Applies one side of `entry` - see History::applyEntry(). `targetBases`
/// holds the base of every layer whose base the entry changed and which
/// exists on that side.
HistoryChange applySide(Project& project, HistoryEntry& entry, bool forward,
                        const std::map<LayerId, LayerBase>& targetBases) {
    HistoryChange change;
    OperationLog& log = project.operationLog();

    // Operations first when undoing (so superseded operations' targets can
    // still be looked up below), last when redoing.
    const auto noteOperationTargets = [&]() {
        for (const nlohmann::json& operation : entry.appendedOperations) {
            change.contentLayers.insert(operation.at("targetLayer").get<LayerId>());
            if (operation.contains("supersedes")) {
                if (const auto target = targetLayerOfOperation(log, operation.at("supersedes").get<OperationId>())) {
                    change.contentLayers.insert(*target);
                }
            }
        }
        if (entry.stackOrderBefore && entry.stackOrderAfter) {
            const auto& before = *entry.stackOrderBefore;
            const auto& after = *entry.stackOrderAfter;
            for (std::size_t i = 0; i < std::max(before.size(), after.size()); ++i) {
                const bool same = i < before.size() && i < after.size() && before[i] == after[i];
                if (same) {
                    continue;
                }
                for (const auto* order : {&before, &after}) {
                    if (i < order->size()) {
                        if (const auto target = targetLayerOfOperation(log, (*order)[i])) {
                            change.contentLayers.insert(*target);
                        }
                    }
                }
            }
        }
        if (entry.bakedBefore && entry.bakedAfter) {
            for (const auto* thresholds : {&*entry.bakedBefore, &*entry.bakedAfter}) {
                for (const auto& [layer, threshold] : *thresholds) {
                    const auto& other = thresholds == &*entry.bakedBefore ? *entry.bakedAfter : *entry.bakedBefore;
                    const auto it = other.find(layer);
                    if (it == other.end() || it->second != threshold) {
                        change.contentLayers.insert(layer);
                    }
                }
            }
        }
    };
    const bool operationsChanged = !entry.appendedOperations.empty() || entry.stackOrderAfter.has_value() ||
                                   entry.bakedAfter.has_value() || entry.nextOperationIdBefore != entry.nextOperationIdAfter;
    if (!forward && operationsChanged) {
        noteOperationTargets();
        log.truncate(entry.operationCountBefore);
        log.setNextId(entry.nextOperationIdBefore);
        if (entry.stackOrderBefore) {
            log.setStackOrder(*entry.stackOrderBefore);
        }
        if (entry.bakedBefore) {
            log.setBakedThresholds(*entry.bakedBefore);
        }
        change.operationsChanged = true;
    }

    std::vector<Layer>& layers = project.layers();
    for (HistoryLayerChange& layerChange : entry.layers) {
        const nlohmann::json& target = forward ? layerChange.after : layerChange.before;
        std::optional<LayerBase>& otherSide = forward ? layerChange.baseBefore : layerChange.baseAfter;
        Layer* existing = project.layerById(layerChange.id);
        change.layers.insert(layerChange.id);

        if (target.is_null()) {
            if (existing != nullptr) {
                if (!otherSide.has_value()) {
                    otherSide = existing->releaseBase();
                }
                std::erase_if(layers, [&](const Layer& layer) { return layer.id() == layerChange.id; });
                change.layerListChanged = true;
            }
            continue;
        }

        Layer properties = target.get<Layer>();
        if (existing == nullptr) {
            if (const auto it = targetBases.find(layerChange.id); it != targetBases.end()) {
                properties.restoreBase(it->second);
            }
            layers.push_back(std::move(properties));
            change.layerListChanged = true;
            change.contentLayers.insert(layerChange.id);
            continue;
        }

        if (layerChange.baseChanged) {
            LayerBase released = existing->releaseBase();
            if (!otherSide.has_value()) {
                otherSide = std::move(released);
            }
            if (const auto it = targetBases.find(layerChange.id); it != targetBases.end()) {
                existing->restoreBase(it->second);
            }
            change.contentLayers.insert(layerChange.id);
        }
        existing->assignProperties(properties);
    }

    if (const auto& order = forward ? entry.layerOrderAfter : entry.layerOrderBefore; order.has_value()) {
        reorderLayersTo(project, *order);
        change.layerListChanged = true;
    }
    project.setNextLayerId(forward ? entry.nextLayerIdAfter : entry.nextLayerIdBefore);

    if (forward && operationsChanged) {
        log.truncate(entry.operationCountBefore);
        for (const nlohmann::json& operation : entry.appendedOperations) {
            log.append(OperationLog::operationFromJson(operation));
        }
        log.setNextId(entry.nextOperationIdAfter);
        if (entry.stackOrderAfter) {
            log.setStackOrder(*entry.stackOrderAfter);
        }
        if (entry.bakedAfter) {
            log.setBakedThresholds(*entry.bakedAfter);
        }
        noteOperationTargets();
        change.operationsChanged = true;
    }

    for (const HistoryResourceChange& resource : entry.resources) {
        setResourceJson(project, resource.collection, forward ? resource.after : resource.before);
        change.resources.insert(resource.collection);
    }

    std::erase_if(change.contentLayers, [&](LayerId id) { return project.layerById(id) == nullptr; });
    return change;
}

void clearDisplacedBases(Project& project) {
    for (Layer& layer : project.layers()) {
        (void)layer.takeDisplacedBase();
    }
    (void)project.takeDisplacedLayerBases();
}

void mergeChange(HistoryChange& into, const HistoryChange& from) {
    into.layers.insert(from.layers.begin(), from.layers.end());
    into.contentLayers.insert(from.contentLayers.begin(), from.contentLayers.end());
    into.layerListChanged = into.layerListChanged || from.layerListChanged;
    into.operationsChanged = into.operationsChanged || from.operationsChanged;
    into.resources.insert(from.resources.begin(), from.resources.end());
}

}  // namespace

const std::vector<std::string>& historyResourceCollections() {
    static const std::vector<std::string> collections = {
        kMindWaves,    kMindShots,     kMindGrains,  kResonanceProfiles,  kConvolutionKernels,
        kToolPresets, kFilterPresets, kGridPresets, kMidiProgramMappings,
    };
    return collections;
}

ProjectState captureProjectState(const Project& project) {
    ProjectState state;
    state.layers.reserve(project.layers().size());
    for (const Layer& layer : project.layers()) {
        state.layers.push_back({layer.id(), nlohmann::json(layer), layer.baseRevision(), layer.baseContent() != nullptr});
    }
    state.nextLayerId = project.nextLayerId();
    const OperationLog& log = project.operationLog();
    state.operationIds.reserve(log.size());
    for (std::size_t i = 0; i < log.size(); ++i) {
        state.operationIds.push_back(log.at(i).id());
    }
    state.nextOperationId = log.nextId();
    state.stackOrder = log.stackOrder();
    state.baked = log.bakedThresholds();
    for (const std::string& collection : historyResourceCollections()) {
        state.resources[collection] = resourceJson(project, collection);
    }
    return state;
}

void History::setBaseline(Project& project) {
    committed_ = captureProjectState(project);
    clearDisplacedBases(project);
    hasBaseline_ = true;
}

bool History::hasUncommittedChanges(const Project& project) const {
    return hasBaseline_ && !statesEqual(committed_, captureProjectState(project));
}

std::optional<HistoryEntryId> History::commit(Project& project, std::string description, nlohmann::json metadata) {
    if (!hasBaseline_) {
        setBaseline(project);
        return std::nullopt;
    }
    ProjectState current = captureProjectState(project);
    if (statesEqual(committed_, current)) {
        clearDisplacedBases(project);
        return std::nullopt;
    }

    HistoryEntry entry;
    entry.description = std::move(description);
    entry.metadata = std::move(metadata);
    entry.wallClockSeconds = nowSeconds();

    // The bases replaced or removed since the last commit - see
    // Layer::takeDisplacedBase() and Project::takeDisplacedLayerBases().
    std::map<LayerId, LayerBase> displaced = project.takeDisplacedLayerBases();
    for (Layer& layer : project.layers()) {
        if (auto base = layer.takeDisplacedBase()) {
            displaced.insert_or_assign(layer.id(), std::move(*base));
        }
    }

    std::unordered_map<LayerId, const ProjectState::LayerState*> before;
    for (const auto& layer : committed_.layers) {
        before.emplace(layer.id, &layer);
    }
    std::unordered_map<LayerId, const ProjectState::LayerState*> after;
    for (const auto& layer : current.layers) {
        after.emplace(layer.id, &layer);
    }
    std::vector<LayerId> ids = layerOrder(committed_);
    for (const auto& layer : current.layers) {
        if (!before.contains(layer.id)) {
            ids.push_back(layer.id);
        }
    }
    for (const LayerId id : ids) {
        const auto* was = before.contains(id) ? before.at(id) : nullptr;
        const auto* now = after.contains(id) ? after.at(id) : nullptr;
        if (was != nullptr && now != nullptr && was->json == now->json && was->baseRevision == now->baseRevision) {
            continue;
        }
        HistoryLayerChange layerChange;
        layerChange.id = id;
        layerChange.before = was != nullptr ? was->json : nlohmann::json();
        layerChange.after = now != nullptr ? now->json : nlohmann::json();
        layerChange.baseChanged = was == nullptr || now == nullptr || was->baseRevision != now->baseRevision;
        layerChange.beforeHasContent = was != nullptr && was->hasContent;
        layerChange.afterHasContent = now != nullptr && now->hasContent;
        if (was != nullptr && layerChange.baseChanged) {
            if (const auto it = displaced.find(id); it != displaced.end() && it->second.revision == was->baseRevision) {
                layerChange.baseBefore = it->second;
            }
        }
        entry.layers.push_back(std::move(layerChange));
    }
    if (layerOrder(committed_) != layerOrder(current)) {
        entry.layerOrderBefore = layerOrder(committed_);
        entry.layerOrderAfter = layerOrder(current);
    }
    entry.nextLayerIdBefore = committed_.nextLayerId;
    entry.nextLayerIdAfter = current.nextLayerId;

    // Operations: appended past the common prefix (they're never removed
    // other than by undoing).
    std::size_t common = 0;
    while (common < committed_.operationIds.size() && common < current.operationIds.size() &&
           committed_.operationIds[common] == current.operationIds[common]) {
        ++common;
    }
    entry.operationCountBefore = common;
    const OperationLog& log = project.operationLog();
    std::vector<OperationId> expectedOrder = committed_.stackOrder;
    if (common < committed_.operationIds.size()) {
        const std::unordered_set<OperationId> dropped(committed_.operationIds.begin() + static_cast<std::ptrdiff_t>(common),
                                                      committed_.operationIds.end());
        std::erase_if(expectedOrder, [&](OperationId id) { return dropped.contains(id); });
    }
    for (std::size_t i = common; i < log.size(); ++i) {
        entry.appendedOperations.push_back(OperationLog::operationToJson(log.at(i)));
        OperationLog::insertIntoStackOrder(expectedOrder, log.at(i).id(), log.at(i).supersedes());
    }
    entry.nextOperationIdBefore = committed_.nextOperationId;
    entry.nextOperationIdAfter = current.nextOperationId;
    if (expectedOrder != current.stackOrder) {
        entry.stackOrderBefore = committed_.stackOrder;
        entry.stackOrderAfter = current.stackOrder;
    }
    if (committed_.baked != current.baked) {
        entry.bakedBefore = committed_.baked;
        entry.bakedAfter = current.baked;
    }

    for (const std::string& collection : historyResourceCollections()) {
        const nlohmann::json& was = committed_.resources[collection];
        const nlohmann::json& now = current.resources[collection];
        if (was != now) {
            entry.resources.push_back({collection, was, now});
        }
    }

    entries_.resize(position_);
    entry.id = nextEntryId_++;
    const HistoryEntryId id = entry.id;
    entries_.push_back(std::move(entry));
    position_ = entries_.size();
    committed_ = std::move(current);
    enforceBaseBudget();
    return id;
}

std::optional<HistoryChange> History::undo(Project& project, const HistoryBaseResolver& resolver) {
    if (hasUncommittedChanges(project)) {
        commit(project, "Edit");
    }
    if (!canUndo()) {
        return std::nullopt;
    }
    return step(project, /*forward=*/false, resolver);
}

std::optional<HistoryChange> History::redo(Project& project, const HistoryBaseResolver& resolver) {
    if (!canRedo()) {
        return std::nullopt;
    }
    return step(project, /*forward=*/true, resolver);
}

std::optional<HistoryChange> History::jumpTo(Project& project, std::size_t position,
                                             const HistoryBaseResolver& resolver) {
    if (hasUncommittedChanges(project)) {
        commit(project, "Edit");
    }
    position = std::min(position, entries_.size());
    HistoryChange merged;
    while (position_ != position) {
        const auto change = step(project, /*forward=*/position_ < position, resolver);
        if (!change) {
            return std::nullopt;
        }
        mergeChange(merged, *change);
    }
    std::erase_if(merged.contentLayers, [&](LayerId id) { return project.layerById(id) == nullptr; });
    return merged;
}

std::optional<HistoryChange> History::step(Project& project, bool forward, const HistoryBaseResolver& resolver) {
    HistoryEntry& entry = entries_[forward ? position_ : position_ - 1];
    const std::size_t targetPosition = forward ? position_ + 1 : position_ - 1;

    std::map<LayerId, LayerBase> targetBases;
    for (const HistoryLayerChange& layerChange : entry.layers) {
        const nlohmann::json& target = forward ? layerChange.after : layerChange.before;
        if (!layerChange.baseChanged || target.is_null()) {
            continue;
        }
        const std::optional<LayerBase>& kept = forward ? layerChange.baseAfter : layerChange.baseBefore;
        if (!(forward ? layerChange.afterHasContent : layerChange.beforeHasContent)) {
            targetBases.emplace(layerChange.id, LayerBase{nullptr, nullptr, target.value("baseRecipe", nlohmann::json()), 0});
        } else if (kept.has_value()) {
            targetBases.emplace(layerChange.id, *kept);
        } else if (auto recovered = resolver ? resolver(targetPosition, layerChange.id) : std::nullopt) {
            targetBases.emplace(layerChange.id, std::move(*recovered));
        } else {
            return std::nullopt;
        }
    }

    HistoryChange change = applySide(project, entry, forward, targetBases);
    // Keep what was recovered, so stepping back and forth doesn't replay
    // again.
    for (HistoryLayerChange& layerChange : entry.layers) {
        std::optional<LayerBase>& kept = forward ? layerChange.baseAfter : layerChange.baseBefore;
        if (!kept.has_value()) {
            if (const auto it = targetBases.find(layerChange.id); it != targetBases.end()) {
                kept = it->second;
            }
        }
    }
    position_ = targetPosition;
    committed_ = captureProjectState(project);
    clearDisplacedBases(project);
    enforceBaseBudget();
    return change;
}

std::optional<HistoryChange> History::applyEntry(Project& project, HistoryEntry& entry,
                                                 const std::function<std::optional<LayerBase>(LayerId)>& base) {
    std::map<LayerId, LayerBase> targetBases;
    for (const HistoryLayerChange& layerChange : entry.layers) {
        if (!layerChange.baseChanged || layerChange.after.is_null()) {
            continue;
        }
        if (!layerChange.afterHasContent) {
            targetBases.emplace(layerChange.id,
                                LayerBase{nullptr, nullptr, layerChange.after.value("baseRecipe", nlohmann::json()), 0});
        } else if (layerChange.baseAfter.has_value()) {
            targetBases.emplace(layerChange.id, *layerChange.baseAfter);
        } else if (auto supplied = base ? base(layerChange.id) : std::nullopt) {
            targetBases.emplace(layerChange.id, std::move(*supplied));
        } else {
            return std::nullopt;
        }
    }
    HistoryChange change = applySide(project, entry, /*forward=*/true, targetBases);
    clearDisplacedBases(project);
    return change;
}

void History::setBaseBudgetBytes(std::size_t bytes) {
    baseBudgetBytes_ = bytes;
    enforceBaseBudget();
}

std::size_t History::keptBaseBytes() const {
    std::unordered_set<const void*> seen;
    std::size_t total = 0;
    const auto count = [&](const std::optional<LayerBase>& base) {
        if (!base.has_value()) {
            return;
        }
        const void* key = base->stream ? static_cast<const void*>(base->stream.get())
                                       : static_cast<const void*>(base->pool.get());
        if (key != nullptr && seen.insert(key).second) {
            total += base->byteSize();
        }
    };
    for (const HistoryEntry& entry : entries_) {
        for (const HistoryLayerChange& layerChange : entry.layers) {
            count(layerChange.baseBefore);
            count(layerChange.baseAfter);
        }
    }
    return total;
}

void History::enforceBaseBudget() {
    for (std::size_t index = 0; index < entries_.size() && keptBaseBytes() > baseBudgetBytes_; ++index) {
        for (HistoryLayerChange& layerChange : entries_[index].layers) {
            layerChange.baseBefore.reset();
            layerChange.baseAfter.reset();
        }
    }
}

void to_json(nlohmann::json& json, const HistoryEntry& entry) {
    nlohmann::json layers = nlohmann::json::array();
    for (const HistoryLayerChange& layer : entry.layers) {
        layers.push_back({{"id", layer.id},
                          {"before", layer.before},
                          {"after", layer.after},
                          {"baseChanged", layer.baseChanged},
                          {"beforeHasContent", layer.beforeHasContent},
                          {"afterHasContent", layer.afterHasContent}});
    }
    nlohmann::json resources = nlohmann::json::array();
    for (const HistoryResourceChange& resource : entry.resources) {
        resources.push_back({{"collection", resource.collection}, {"before", resource.before}, {"after", resource.after}});
    }
    json = nlohmann::json{
        {"id", entry.id},
        {"description", entry.description},
        {"wallClockSeconds", entry.wallClockSeconds},
        {"layers", std::move(layers)},
        {"nextLayerIdBefore", entry.nextLayerIdBefore},
        {"nextLayerIdAfter", entry.nextLayerIdAfter},
        {"operationCountBefore", entry.operationCountBefore},
        {"appendedOperations", entry.appendedOperations},
        {"nextOperationIdBefore", entry.nextOperationIdBefore},
        {"nextOperationIdAfter", entry.nextOperationIdAfter},
        {"resources", std::move(resources)},
    };
    if (!entry.metadata.is_null()) {
        json["metadata"] = entry.metadata;
    }
    if (entry.layerOrderBefore && entry.layerOrderAfter) {
        json["layerOrderBefore"] = *entry.layerOrderBefore;
        json["layerOrderAfter"] = *entry.layerOrderAfter;
    }
    if (entry.stackOrderBefore && entry.stackOrderAfter) {
        json["stackOrderBefore"] = *entry.stackOrderBefore;
        json["stackOrderAfter"] = *entry.stackOrderAfter;
    }
    const auto bakedJson = [](const std::map<LayerId, OperationId>& baked) {
        nlohmann::json array = nlohmann::json::array();
        for (const auto& [layer, through] : baked) {
            array.push_back({{"layer", layer}, {"through", through}});
        }
        return array;
    };
    if (entry.bakedBefore && entry.bakedAfter) {
        json["bakedBefore"] = bakedJson(*entry.bakedBefore);
        json["bakedAfter"] = bakedJson(*entry.bakedAfter);
    }
}

void from_json(const nlohmann::json& json, HistoryEntry& entry) {
    entry = HistoryEntry{};
    json.at("id").get_to(entry.id);
    json.at("description").get_to(entry.description);
    entry.wallClockSeconds = json.value("wallClockSeconds", 0.0);
    entry.metadata = json.contains("metadata") ? json.at("metadata") : nlohmann::json();
    for (const auto& layer : json.at("layers")) {
        HistoryLayerChange change;
        layer.at("id").get_to(change.id);
        change.before = layer.at("before");
        change.after = layer.at("after");
        change.baseChanged = layer.value("baseChanged", false);
        change.beforeHasContent = layer.value("beforeHasContent", !change.before.is_null());
        change.afterHasContent = layer.value("afterHasContent", !change.after.is_null());
        entry.layers.push_back(std::move(change));
    }
    json.at("nextLayerIdBefore").get_to(entry.nextLayerIdBefore);
    json.at("nextLayerIdAfter").get_to(entry.nextLayerIdAfter);
    json.at("operationCountBefore").get_to(entry.operationCountBefore);
    entry.appendedOperations = json.at("appendedOperations").get<std::vector<nlohmann::json>>();
    json.at("nextOperationIdBefore").get_to(entry.nextOperationIdBefore);
    json.at("nextOperationIdAfter").get_to(entry.nextOperationIdAfter);
    for (const auto& resource : json.at("resources")) {
        entry.resources.push_back(
            {resource.at("collection").get<std::string>(), resource.at("before"), resource.at("after")});
    }
    if (json.contains("layerOrderBefore")) {
        entry.layerOrderBefore = json.at("layerOrderBefore").get<std::vector<LayerId>>();
        entry.layerOrderAfter = json.at("layerOrderAfter").get<std::vector<LayerId>>();
    }
    if (json.contains("stackOrderBefore")) {
        entry.stackOrderBefore = json.at("stackOrderBefore").get<std::vector<OperationId>>();
        entry.stackOrderAfter = json.at("stackOrderAfter").get<std::vector<OperationId>>();
    }
    const auto bakedFrom = [](const nlohmann::json& array) {
        std::map<LayerId, OperationId> baked;
        for (const auto& item : array) {
            baked[item.at("layer").get<LayerId>()] = item.at("through").get<OperationId>();
        }
        return baked;
    };
    if (json.contains("bakedBefore")) {
        entry.bakedBefore = bakedFrom(json.at("bakedBefore"));
        entry.bakedAfter = bakedFrom(json.at("bakedAfter"));
    }
}

void to_json(nlohmann::json& json, const History& history) {
    nlohmann::json entries = nlohmann::json::array();
    for (std::size_t i = 0; i < history.position_; ++i) {
        entries.push_back(history.entries_[i]);
    }
    json = nlohmann::json{{"entries", std::move(entries)},
                          {"nextEntryId", history.nextEntryId_},
                          {"startsAtCreation", history.startsAtCreation_}};
}

void from_json(const nlohmann::json& json, History& history) {
    history.entries_ = json.at("entries").get<std::vector<HistoryEntry>>();
    history.position_ = history.entries_.size();
    history.nextEntryId_ = json.value("nextEntryId", HistoryEntryId{history.entries_.size() + 1});
    history.committed_ = ProjectState{};
    history.hasBaseline_ = false;
    history.startsAtCreation_ = json.value("startsAtCreation", false);
}

}  // namespace sound_mind::core
