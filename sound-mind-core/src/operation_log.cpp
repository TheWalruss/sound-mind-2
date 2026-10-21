#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/operation_log.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/filter_operation.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

void OperationLog::append(std::unique_ptr<Operation> operation) {
    operations_.push_back(std::move(operation));
    const Operation& appended = *operations_.back();
    insertIntoStackOrder(stackOrder_, appended.id(), appended.supersedes());
}

void OperationLog::insertIntoStackOrder(std::vector<OperationId>& order, OperationId id,
                                        std::optional<OperationId> supersedes) {
    if (supersedes.has_value()) {
        const auto it = std::find(order.begin(), order.end(), *supersedes);
        if (it != order.end()) {
            order.insert(std::next(it), id);
            return;
        }
    }
    order.push_back(id);
}

void OperationLog::truncate(std::size_t count) {
    if (count >= operations_.size()) {
        return;
    }
    std::unordered_set<OperationId> dropped;
    for (std::size_t i = count; i < operations_.size(); ++i) {
        dropped.insert(operations_[i]->id());
    }
    operations_.resize(count);
    std::erase_if(stackOrder_, [&dropped](OperationId id) { return dropped.contains(id); });
}

OperationId OperationLog::bakedThrough(LayerId layer) const noexcept {
    const auto it = bakedThrough_.find(layer);
    return it == bakedThrough_.end() ? OperationId{0} : it->second;
}

std::vector<const Operation*> OperationLog::activeOperationsTargeting(LayerId layer) const {
    std::unordered_set<OperationId> supersededIds;
    for (const auto& operation : operations_) {
        if (const auto supersedes = operation->supersedes(); supersedes.has_value()) {
            supersededIds.insert(*supersedes);
        }
    }

    // The active, non-superseded, this-layer subset - same filter as
    // before, just no longer also deciding the result's own order (that's
    // stackOrder_'s job below) - see this method's own docs.
    const OperationId baked = bakedThrough(layer);
    std::unordered_map<OperationId, const Operation*> eligible;
    for (const auto& entry : operations_) {
        const Operation& operation = *entry;
        if (supersededIds.contains(operation.id())) {
            continue;  // a later active operation replaces this one - see this method's own docs.
        }
        if (operation.targetLayer() == layer && operation.id() > baked) {
            eligible.emplace(operation.id(), &operation);
        }
    }

    std::vector<const Operation*> result;
    result.reserve(eligible.size());
    for (const OperationId id : stackOrder_) {
        if (const auto it = eligible.find(id); it != eligible.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

bool OperationLog::bringToFront(OperationId id) { return reorderActiveOperation(id, ReorderDirection::ToFront); }

bool OperationLog::sendToBack(OperationId id) { return reorderActiveOperation(id, ReorderDirection::ToBack); }

bool OperationLog::bringForward(OperationId id) { return reorderActiveOperation(id, ReorderDirection::Forward); }

bool OperationLog::sendBackward(OperationId id) { return reorderActiveOperation(id, ReorderDirection::Backward); }

bool OperationLog::reorderActiveOperation(OperationId id, ReorderDirection direction) {
    const Operation* target = nullptr;
    for (const auto& operation : operations_) {
        if (operation->id() == id) {
            target = operation.get();
            break;
        }
    }
    if (target == nullptr || !target->targetLayer().has_value()) {
        return false;
    }

    // The current stack order among just this id's own layer's currently
    // active operations - exactly what a reorder gesture needs to permute
    // (other layers, and any of this layer's own inactive entries, are
    // left entirely alone).
    std::vector<OperationId> ids;
    for (const Operation* operation : activeOperationsTargeting(*target->targetLayer())) {
        ids.push_back(operation->id());
    }
    const std::unordered_set<OperationId> members(ids.begin(), ids.end());

    const auto it = std::find(ids.begin(), ids.end(), id);
    if (it == ids.end()) {
        return false;  // not currently active - nothing to reorder.
    }
    const auto index = static_cast<std::size_t>(std::distance(ids.begin(), it));

    bool changed = false;
    switch (direction) {
        case ReorderDirection::ToFront:
            if (index + 1 != ids.size()) {
                std::rotate(ids.begin() + static_cast<std::ptrdiff_t>(index),
                            ids.begin() + static_cast<std::ptrdiff_t>(index) + 1, ids.end());
                changed = true;
            }
            break;
        case ReorderDirection::ToBack:
            if (index != 0) {
                std::rotate(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(index),
                            ids.begin() + static_cast<std::ptrdiff_t>(index) + 1);
                changed = true;
            }
            break;
        case ReorderDirection::Forward:
            if (index + 1 != ids.size()) {
                std::swap(ids[index], ids[index + 1]);
                changed = true;
            }
            break;
        case ReorderDirection::Backward:
            if (index != 0) {
                std::swap(ids[index], ids[index - 1]);
                changed = true;
            }
            break;
    }
    if (!changed) {
        return false;
    }

    // Reassigns the same absolute stackOrder_ slots this layer's active
    // ids already occupied, now carrying the newly-permuted order -
    // every other entry (a different layer, or one no longer active)
    // stays exactly where it was.
    std::size_t nextIndex = 0;
    for (OperationId& slot : stackOrder_) {
        if (members.contains(slot)) {
            slot = ids[nextIndex++];
        }
    }
    return true;
}

namespace {

/// @brief The discriminator each logged operation's own JSON carries,
/// under `"kind"` - the only way to know which concrete subtype to
/// reconstruct on load, since `Operation` itself is abstract.
constexpr const char* kPaintOperationKind = "paint";
constexpr const char* kFillOperationKind = "fill";
constexpr const char* kPasteOperationKind = "paste";
constexpr const char* kSequenceOperationKind = "sequence";
constexpr const char* kFilterOperationKind = "filter";
constexpr const char* kGuidePathOperationKind = "guidePath";

/// @brief Writes `operation`'s own `id`/`supersedes`/`targetLayer` fields
/// into `entry` - the three keys every concrete `LayerContentOperation`
/// subtype's own `to_json()` branch needs, identically. Shared here
/// rather than repeated per branch (Refactor & Clean Up, `v0.Y.29.1`).
void writeCommonOperationFields(nlohmann::json& entry, const LayerContentOperation& operation) {
    entry["id"] = operation.id();
    if (const auto supersedes = operation.supersedes(); supersedes.has_value()) {
        entry["supersedes"] = *supersedes;
    }
    entry["targetLayer"] = *operation.targetLayer();
}

/// @brief The three fields writeCommonOperationFields() writes, read back.
struct CommonOperationFields {
    OperationId id;
    std::optional<OperationId> supersedes;
    LayerId targetLayer;
};

/// @brief The inverse of writeCommonOperationFields() - reads `entry`'s
/// own `id`/`supersedes`/`targetLayer` fields back out. Shared here
/// rather than repeated per `from_json()` branch (Refactor & Clean Up,
/// `v0.Y.29.1`).
CommonOperationFields readCommonOperationFields(const nlohmann::json& entry) {
    CommonOperationFields fields;
    fields.id = entry.at("id").get<OperationId>();
    fields.supersedes = entry.contains("supersedes") ? std::optional(entry.at("supersedes").get<OperationId>())
                                                       : std::nullopt;
    fields.targetLayer = entry.at("targetLayer").get<LayerId>();
    return fields;
}

}  // namespace

nlohmann::json OperationLog::operationToJson(const Operation& operation) {
    // A plain if/else-if dispatch, not a visitor - six concrete subtypes is
    // still few enough that a real dispatch mechanism would be speculative
    // machinery for a problem this doesn't have yet.
    if (const auto* paint = dynamic_cast<const PaintOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kPaintOperationKind;
        writeCommonOperationFields(entry, *paint);
        entry["path"] = paint->path();
        entry["config"] = paint->config();
        return entry;
    } else if (const auto* fill = dynamic_cast<const FillOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kFillOperationKind;
        writeCommonOperationFields(entry, *fill);
        entry["bounds"] = fill->bounds();
        entry["gradient"] = fill->gradient();
        if (fill->boundary()) {
            entry["boundary"] = *fill->boundary();
        }
        return entry;
    } else if (const auto* paste = dynamic_cast<const PasteOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kPasteOperationKind;
        writeCommonOperationFields(entry, *paste);
        entry["placement"] = paste->bounds();
        entry["clip"] = paste->clip();
        if (paste->boundary()) {
            entry["boundary"] = *paste->boundary();
        }
        entry["blendMode"] = paste->blendMode();
        if (paste->textSource()) {
            entry["textSource"] = *paste->textSource();
        }
        return entry;
    } else if (const auto* sequence = dynamic_cast<const SequenceOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kSequenceOperationKind;
        writeCommonOperationFields(entry, *sequence);
        entry["notes"] = sequence->notes();
        entry["config"] = sequence->config();
        return entry;
    } else if (const auto* filter = dynamic_cast<const FilterOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kFilterOperationKind;
        writeCommonOperationFields(entry, *filter);
        entry["bounds"] = filter->bounds();
        entry["config"] = filter->config();
        if (filter->boundary()) {
            entry["boundary"] = *filter->boundary();
        }
        return entry;
    } else if (const auto* guide = dynamic_cast<const GuidePathOperation*>(&operation)) {
        nlohmann::json entry;
        entry["kind"] = kGuidePathOperationKind;
        writeCommonOperationFields(entry, *guide);
        entry["branches"] = guide->branches();
        return entry;
    }
    throw std::invalid_argument("OperationLog: an operation of an unserializable type");
}

std::unique_ptr<Operation> OperationLog::operationFromJson(const nlohmann::json& json) {
    const std::string kind = json.at("kind").get<std::string>();
    if (kind == kPaintOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        Path path = json.at("path").get<Path>();
        std::unique_ptr<ToolConfiguration> config = toolConfigurationFromJson(json.at("config"));
        return std::make_unique<PaintOperation>(
            fields.id, fields.targetLayer, std::move(path), std::move(config), fields.supersedes);
    } else if (kind == kFillOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        TimeFrequencyRect bounds = json.at("bounds").get<TimeFrequencyRect>();
        Gradient gradient = json.at("gradient").get<Gradient>();
        std::optional<SelectionRegion> boundary =
            json.contains("boundary") ? std::optional(json.at("boundary").get<SelectionRegion>()) : std::nullopt;
        return std::make_unique<FillOperation>(
            fields.id, fields.targetLayer, bounds, std::move(gradient), fields.supersedes,
            std::move(boundary));
    } else if (kind == kPasteOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        TimeFrequencyRect placement = json.at("placement").get<TimeFrequencyRect>();
        Clip clip = json.at("clip").get<Clip>();
        std::optional<SelectionRegion> boundary =
            json.contains("boundary") ? std::optional(json.at("boundary").get<SelectionRegion>()) : std::nullopt;
        // Lenient (defaults to Overwrite if absent) - didn't exist
        // before v0.Y.37.1 (Deferred Blend Modes); a paste saved
        // before this milestone was implicitly always a hard
        // overwrite anyway.
        const BlendMode blendMode = json.value("blendMode", BlendMode::Overwrite);
        std::optional<TextSource> textSource =
            json.contains("textSource") ? std::optional(json.at("textSource").get<TextSource>()) : std::nullopt;
        return std::make_unique<PasteOperation>(
            fields.id, fields.targetLayer, placement, std::move(clip), fields.supersedes,
            std::move(boundary), blendMode, std::move(textSource));
    } else if (kind == kSequenceOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        std::vector<NoteEvent> notes = json.at("notes").get<std::vector<NoteEvent>>();
        std::unique_ptr<ToolConfiguration> config = toolConfigurationFromJson(json.at("config"));
        return std::make_unique<SequenceOperation>(
            fields.id, fields.targetLayer, std::move(notes), std::move(config), fields.supersedes);
    } else if (kind == kFilterOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        TimeFrequencyRect bounds = json.at("bounds").get<TimeFrequencyRect>();
        FilterConfiguration config = json.at("config").get<FilterConfiguration>();
        std::optional<SelectionRegion> boundary =
            json.contains("boundary") ? std::optional(json.at("boundary").get<SelectionRegion>()) : std::nullopt;
        return std::make_unique<FilterOperation>(
            fields.id, fields.targetLayer, bounds, std::move(config), fields.supersedes,
            std::move(boundary));
    } else if (kind == kGuidePathOperationKind) {
        const CommonOperationFields fields = readCommonOperationFields(json);
        std::vector<BranchGraft> branches = json.at("branches").get<std::vector<BranchGraft>>();
        return std::make_unique<GuidePathOperation>(fields.id, fields.targetLayer,
                                                                       std::move(branches), fields.supersedes);
    } else {
        throw std::invalid_argument("OperationLog: unrecognized operation kind \"" + kind + "\"");
    }
}

void to_json(nlohmann::json& json, const OperationLog& log) {
    nlohmann::json operations = nlohmann::json::array();
    for (const auto& operation : log.operations_) {
        operations.push_back(OperationLog::operationToJson(*operation));
    }
    json = nlohmann::json{{"operations", operations}, {"nextId", log.nextId_}, {"stackOrder", log.stackOrder_}};
    if (!log.bakedThrough_.empty()) {
        nlohmann::json baked = nlohmann::json::array();
        for (const auto& [layer, threshold] : log.bakedThrough_) {
            baked.push_back({{"layer", layer}, {"through", threshold}});
        }
        json["bakedThrough"] = std::move(baked);
    }
}

void from_json(const nlohmann::json& json, OperationLog& log) {
    log.operations_.clear();
    log.stackOrder_.clear();
    log.bakedThrough_.clear();

    if (json.contains("operations")) {
        for (const auto& entry : json.at("operations")) {
            log.operations_.push_back(OperationLog::operationFromJson(entry));
        }
        log.nextId_ = json.at("nextId").get<OperationId>();
        if (json.contains("stackOrder")) {
            log.stackOrder_ = json.at("stackOrder").get<std::vector<OperationId>>();
        } else {
            // A project file saved before stackOrder_ existed (`v0.0.26.2`)
            // - synthesize the same default it always implicitly had:
            // append order, i.e. every operation's own id in the order it
            // appears in "operations" above.
            log.stackOrder_.reserve(log.operations_.size());
            for (const auto& operation : log.operations_) {
                log.stackOrder_.push_back(operation->id());
            }
        }
        if (json.contains("bakedThrough")) {
            for (const auto& baked : json.at("bakedThrough")) {
                log.bakedThrough_[baked.at("layer").get<LayerId>()] = baked.at("through").get<OperationId>();
            }
        }
    } else {
        // The pre-`v0.0.24.1` empty-array shape, before any concrete
        // Operation subtype existed - an empty log either way.
        log.nextId_ = 1;
    }
}

}  // namespace sound_mind::core
