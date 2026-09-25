#include "sound_mind/core/operation_log.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

void OperationLog::append(std::unique_ptr<Operation> operation) {
    // A fresh append past an undo() discards the redo tail - the vector
    // itself is truncated (not just the high-water mark moved), since
    // nothing can ever point at those discarded operations again (a
    // brand-new operation can't have been constructed with a supersedes()
    // referencing an id that's about to not exist).
    operations_.resize(activeCount_);
    operations_.push_back(std::move(operation));
    activeCount_ = operations_.size();

    // stackOrder_ placement - see the class's own docs. Any id
    // stackOrder_ still carries for an operation this truncation just
    // discarded simply stops being findable in operations_ - harmless,
    // since activeOperationsTargeting() only ever looks an id up after
    // already confirming it's currently active.
    const Operation& appended = *operations_.back();
    if (const auto supersedes = appended.supersedes(); supersedes.has_value()) {
        const auto it = std::find(stackOrder_.begin(), stackOrder_.end(), *supersedes);
        if (it != stackOrder_.end()) {
            stackOrder_.insert(std::next(it), appended.id());
        } else {
            // Defensive: no real call site constructs a supersedes()
            // reference stackOrder_ doesn't already know about - falls
            // back to the same "new object, goes on top" placement below.
            stackOrder_.push_back(appended.id());
        }
    } else {
        stackOrder_.push_back(appended.id());
    }
}

void OperationLog::undo() noexcept {
    if (canUndo()) {
        --activeCount_;
    }
}

void OperationLog::redo() noexcept {
    if (canRedo()) {
        ++activeCount_;
    }
}

std::vector<const Operation*> OperationLog::activeOperationsTargeting(LayerId layer) const {
    std::unordered_set<OperationId> supersededIds;
    for (std::size_t i = 0; i < activeCount_; ++i) {
        if (const auto supersedes = operations_[i]->supersedes(); supersedes.has_value()) {
            supersededIds.insert(*supersedes);
        }
    }

    // The active, non-superseded, this-layer subset - same filter as
    // before, just no longer also deciding the result's own order (that's
    // stackOrder_'s job below) - see this method's own docs.
    std::unordered_map<OperationId, const Operation*> eligible;
    for (std::size_t i = 0; i < activeCount_; ++i) {
        const Operation& operation = *operations_[i];
        if (supersededIds.contains(operation.id())) {
            continue;  // a later active operation replaces this one - see this method's own docs.
        }
        if (operation.targetLayer() == layer) {
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

void to_json(nlohmann::json& json, const OperationLog& log) {
    nlohmann::json operations = nlohmann::json::array();
    for (const auto& operation : log.operations_) {
        // A plain if/else-if dispatch, not a visitor - four concrete
        // subtypes (PaintOperation, FillOperation, PasteOperation,
        // SequenceOperation) is still few enough that a real dispatch
        // mechanism would be speculative machinery for a problem this
        // doesn't have yet; revisit if a fifth subtype makes the chain
        // unwieldy.
        if (const auto* paint = dynamic_cast<const PaintOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kPaintOperationKind;
            writeCommonOperationFields(entry, *paint);
            entry["path"] = paint->path();
            entry["config"] = paint->config();
            operations.push_back(std::move(entry));
        } else if (const auto* fill = dynamic_cast<const FillOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kFillOperationKind;
            writeCommonOperationFields(entry, *fill);
            entry["bounds"] = fill->bounds();
            entry["gradient"] = fill->gradient();
            if (fill->boundary()) {
                entry["boundary"] = *fill->boundary();
            }
            operations.push_back(std::move(entry));
        } else if (const auto* paste = dynamic_cast<const PasteOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kPasteOperationKind;
            writeCommonOperationFields(entry, *paste);
            entry["placement"] = paste->bounds();
            entry["clip"] = paste->clip();
            if (paste->boundary()) {
                entry["boundary"] = *paste->boundary();
            }
            entry["blendMode"] = paste->blendMode();
            operations.push_back(std::move(entry));
        } else if (const auto* sequence = dynamic_cast<const SequenceOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kSequenceOperationKind;
            writeCommonOperationFields(entry, *sequence);
            entry["notes"] = sequence->notes();
            entry["config"] = sequence->config();
            operations.push_back(std::move(entry));
        }
    }
    json = nlohmann::json{{"operations", operations},
                          {"activeCount", log.activeCount_},
                          {"nextId", log.nextId_},
                          {"stackOrder", log.stackOrder_}};
}

void from_json(const nlohmann::json& json, OperationLog& log) {
    log.operations_.clear();
    log.stackOrder_.clear();

    if (json.contains("operations")) {
        // Current, real shape.
        for (const auto& entry : json.at("operations")) {
            const std::string kind = entry.at("kind").get<std::string>();
            if (kind == kPaintOperationKind) {
                const CommonOperationFields fields = readCommonOperationFields(entry);
                Path path = entry.at("path").get<Path>();
                std::unique_ptr<ToolConfiguration> config = toolConfigurationFromJson(entry.at("config"));
                log.operations_.push_back(std::make_unique<PaintOperation>(
                    fields.id, fields.targetLayer, std::move(path), std::move(config), fields.supersedes));
            } else if (kind == kFillOperationKind) {
                const CommonOperationFields fields = readCommonOperationFields(entry);
                TimeFrequencyRect bounds = entry.at("bounds").get<TimeFrequencyRect>();
                Gradient gradient = entry.at("gradient").get<Gradient>();
                std::optional<SelectionRegion> boundary =
                    entry.contains("boundary") ? std::optional(entry.at("boundary").get<SelectionRegion>()) : std::nullopt;
                log.operations_.push_back(std::make_unique<FillOperation>(
                    fields.id, fields.targetLayer, bounds, std::move(gradient), fields.supersedes,
                    std::move(boundary)));
            } else if (kind == kPasteOperationKind) {
                const CommonOperationFields fields = readCommonOperationFields(entry);
                TimeFrequencyRect placement = entry.at("placement").get<TimeFrequencyRect>();
                Clip clip = entry.at("clip").get<Clip>();
                std::optional<SelectionRegion> boundary =
                    entry.contains("boundary") ? std::optional(entry.at("boundary").get<SelectionRegion>()) : std::nullopt;
                // Lenient (defaults to Overwrite if absent) - didn't exist
                // before v0.Y.37.1 (Deferred Blend Modes); a paste saved
                // before this milestone was implicitly always a hard
                // overwrite anyway.
                const BlendMode blendMode = entry.value("blendMode", BlendMode::Overwrite);
                log.operations_.push_back(std::make_unique<PasteOperation>(
                    fields.id, fields.targetLayer, placement, std::move(clip), fields.supersedes,
                    std::move(boundary), blendMode));
            } else if (kind == kSequenceOperationKind) {
                const CommonOperationFields fields = readCommonOperationFields(entry);
                std::vector<NoteEvent> notes = entry.at("notes").get<std::vector<NoteEvent>>();
                std::unique_ptr<ToolConfiguration> config = toolConfigurationFromJson(entry.at("config"));
                log.operations_.push_back(std::make_unique<SequenceOperation>(
                    fields.id, fields.targetLayer, std::move(notes), std::move(config), fields.supersedes));
            } else {
                throw std::invalid_argument("OperationLog: unrecognized operation kind \"" + kind + "\"");
            }
        }
        log.activeCount_ = json.at("activeCount").get<std::size_t>();
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
    } else {
        // The pre-`v0.0.24.1` empty-array shape, before any concrete
        // Operation subtype existed - an empty log either way.
        log.activeCount_ = 0;
        log.nextId_ = 1;
    }
}

}  // namespace sound_mind::core
