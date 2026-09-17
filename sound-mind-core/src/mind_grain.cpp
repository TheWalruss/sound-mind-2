#include "sound_mind/core/mind_grain.h"

#include <algorithm>
#include <optional>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

namespace {

/// @brief The index of `id` within `order`, if present.
std::optional<std::size_t> indexOf(const std::vector<LayerId>& order, LayerId id) {
    const auto it = std::find(order.begin(), order.end(), id);
    if (it == order.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(order.begin(), it));
}

/// @brief Every current layer id in `project`, bottom-to-top - the same
/// order `Project::layers()` itself already stores them in.
std::vector<LayerId> currentLayerOrder(const Project& project) {
    std::vector<LayerId> order;
    order.reserve(project.layers().size());
    for (const Layer& layer : project.layers()) {
        order.push_back(layer.id());
    }
    return order;
}

}  // namespace

void to_json(nlohmann::json& json, const NamedMindGrain& namedMindGrain) {
    json = nlohmann::json{{"id", namedMindGrain.id},
                           {"name", namedMindGrain.name},
                           {"sourceLayerId", namedMindGrain.sourceLayerId},
                           {"bounds", namedMindGrain.bounds}};
}

void from_json(const nlohmann::json& json, NamedMindGrain& namedMindGrain) {
    json.at("id").get_to(namedMindGrain.id);
    json.at("name").get_to(namedMindGrain.name);
    json.at("sourceLayerId").get_to(namedMindGrain.sourceLayerId);
    json.at("bounds").get_to(namedMindGrain.bounds);
}

bool isLayerAbove(const Project& project, LayerId layer, LayerId other) noexcept {
    const std::vector<LayerId> order = currentLayerOrder(project);
    const auto layerIndex = indexOf(order, layer);
    const auto otherIndex = indexOf(order, other);
    if (!layerIndex.has_value() || !otherIndex.has_value()) {
        return false;
    }
    return *layerIndex > *otherIndex;
}

std::vector<OperationId> mindGrainOperationsBrokenByRemovingLayer(const Project& project, LayerId layerToRemove) {
    std::vector<OperationId> broken;
    for (const Layer& layer : project.layers()) {
        for (const Operation* operation : project.operationLog().activeOperationsTargeting(layer.id())) {
            const auto* paint = dynamic_cast<const PaintOperation*>(operation);
            if (paint == nullptr) {
                continue;
            }
            const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&paint->config());
            if (mindGrain == nullptr) {
                continue;
            }
            if (mindGrain->sourceLayerId() == layerToRemove) {
                broken.push_back(paint->id());
            }
        }
    }
    return broken;
}

std::vector<LayerId> layersWithMindGrainOperationsSourcedFrom(const Project& project, LayerId sourceLayer) {
    std::vector<LayerId> dependents;
    for (const Layer& layer : project.layers()) {
        for (const Operation* operation : project.operationLog().activeOperationsTargeting(layer.id())) {
            const auto* paint = dynamic_cast<const PaintOperation*>(operation);
            if (paint == nullptr) {
                continue;
            }
            const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&paint->config());
            if (mindGrain == nullptr) {
                continue;
            }
            if (mindGrain->sourceLayerId() == sourceLayer) {
                dependents.push_back(layer.id());
                break;  // Already counted this layer - one match is enough.
            }
        }
    }
    return dependents;
}

std::vector<OperationId> mindGrainOperationsBrokenByReorder(const Project& project,
                                                              const std::vector<LayerId>& newOrderBottomToTop) {
    std::vector<OperationId> broken;
    for (const Layer& layer : project.layers()) {
        for (const Operation* operation : project.operationLog().activeOperationsTargeting(layer.id())) {
            const auto* paint = dynamic_cast<const PaintOperation*>(operation);
            if (paint == nullptr) {
                continue;
            }
            const auto* mindGrain = dynamic_cast<const MindGrainConfiguration*>(&paint->config());
            if (mindGrain == nullptr) {
                continue;
            }
            const auto targetIndex = indexOf(newOrderBottomToTop, layer.id());
            const auto sourceIndex = indexOf(newOrderBottomToTop, mindGrain->sourceLayerId());
            if (!targetIndex.has_value() || !sourceIndex.has_value() || *targetIndex <= *sourceIndex) {
                broken.push_back(paint->id());
            }
        }
    }
    return broken;
}

}  // namespace sound_mind::core
