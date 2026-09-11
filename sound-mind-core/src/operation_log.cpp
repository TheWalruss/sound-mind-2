#include "sound_mind/core/operation_log.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"

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

}  // namespace

void to_json(nlohmann::json& json, const OperationLog& log) {
    nlohmann::json operations = nlohmann::json::array();
    for (const auto& operation : log.operations_) {
        // A plain if/else-if dispatch, not a visitor - three concrete
        // subtypes (PaintOperation, FillOperation, PasteOperation) is still
        // few enough that a real dispatch mechanism would be speculative
        // machinery for a problem this doesn't have yet; revisit if a
        // fourth subtype makes the chain unwieldy.
        if (const auto* paint = dynamic_cast<const PaintOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kPaintOperationKind;
            entry["id"] = paint->id();
            if (const auto supersedes = paint->supersedes(); supersedes.has_value()) {
                entry["supersedes"] = *supersedes;
            }
            entry["targetLayer"] = *paint->targetLayer();
            entry["path"] = paint->path();
            entry["config"] = paint->config();
            operations.push_back(std::move(entry));
        } else if (const auto* fill = dynamic_cast<const FillOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kFillOperationKind;
            entry["id"] = fill->id();
            if (const auto supersedes = fill->supersedes(); supersedes.has_value()) {
                entry["supersedes"] = *supersedes;
            }
            entry["targetLayer"] = *fill->targetLayer();
            entry["bounds"] = fill->bounds();
            entry["gradient"] = fill->gradient();
            operations.push_back(std::move(entry));
        } else if (const auto* paste = dynamic_cast<const PasteOperation*>(operation.get())) {
            nlohmann::json entry;
            entry["kind"] = kPasteOperationKind;
            entry["id"] = paste->id();
            if (const auto supersedes = paste->supersedes(); supersedes.has_value()) {
                entry["supersedes"] = *supersedes;
            }
            entry["targetLayer"] = *paste->targetLayer();
            entry["placement"] = paste->bounds();
            entry["clip"] = paste->clip();
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
                const OperationId id = entry.at("id").get<OperationId>();
                const std::optional<OperationId> supersedes =
                    entry.contains("supersedes") ? std::optional(entry.at("supersedes").get<OperationId>())
                                                  : std::nullopt;
                const LayerId targetLayer = entry.at("targetLayer").get<LayerId>();
                Path path = entry.at("path").get<Path>();
                ToolConfiguration config = entry.at("config").get<ToolConfiguration>();
                log.operations_.push_back(std::make_unique<PaintOperation>(id, targetLayer, std::move(path),
                                                                            std::move(config), supersedes));
            } else if (kind == kFillOperationKind) {
                const OperationId id = entry.at("id").get<OperationId>();
                const std::optional<OperationId> supersedes =
                    entry.contains("supersedes") ? std::optional(entry.at("supersedes").get<OperationId>())
                                                  : std::nullopt;
                const LayerId targetLayer = entry.at("targetLayer").get<LayerId>();
                TimeFrequencyRect bounds = entry.at("bounds").get<TimeFrequencyRect>();
                Gradient gradient = entry.at("gradient").get<Gradient>();
                log.operations_.push_back(std::make_unique<FillOperation>(id, targetLayer, bounds,
                                                                            std::move(gradient), supersedes));
            } else if (kind == kPasteOperationKind) {
                const OperationId id = entry.at("id").get<OperationId>();
                const std::optional<OperationId> supersedes =
                    entry.contains("supersedes") ? std::optional(entry.at("supersedes").get<OperationId>())
                                                  : std::nullopt;
                const LayerId targetLayer = entry.at("targetLayer").get<LayerId>();
                TimeFrequencyRect placement = entry.at("placement").get<TimeFrequencyRect>();
                Clip clip = entry.at("clip").get<Clip>();
                log.operations_.push_back(std::make_unique<PasteOperation>(id, targetLayer, placement,
                                                                             std::move(clip), supersedes));
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
