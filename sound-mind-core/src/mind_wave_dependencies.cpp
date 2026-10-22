#include "sound_mind/core/mind_wave_dependencies.h"

#include <string_view>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/project.h"

namespace sound_mind::core {

namespace {

[[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

[[nodiscard]] bool isBindingKey(std::string_view key) { return endsWith(key, "MindWaveId") || endsWith(key, "MindWave"); }

}  // namespace

bool jsonRefersToMindWave(const nlohmann::json& json, MindWaveId id) {
    if (json.is_object()) {
        for (const auto& [key, value] : json.items()) {
            if (isBindingKey(key) && value.is_number_unsigned() && value.get<MindWaveId>() == id) {
                return true;
            }
            if (isBindingKey(key) && value.is_number_integer() && value.get<long long>() >= 0 &&
                static_cast<MindWaveId>(value.get<long long>()) == id) {
                return true;
            }
            if ((value.is_object() || value.is_array()) && jsonRefersToMindWave(value, id)) {
                return true;
            }
        }
        return false;
    }
    if (json.is_array()) {
        for (const auto& element : json) {
            if (jsonRefersToMindWave(element, id)) {
                return true;
            }
        }
    }
    return false;
}

MindWaveDependents mindWaveDependents(const Project& project, MindWaveId id) {
    MindWaveDependents dependents;
    const auto& layers = project.layers();
    for (std::size_t index = 0; index < layers.size(); ++index) {
        const Layer& layer = layers[index];
        nlohmann::json layerJson = layer;
        layerJson.erase("baseRecipe");  // a generated layer's content is baked, not live.
        bool usesIt = jsonRefersToMindWave(layerJson, id);

        bool painted = false;
        for (const Operation* operation : project.operationLog().activeOperationsTargeting(layer.id())) {
            if (dynamic_cast<const PasteOperation*>(operation) != nullptr) {
                continue;  // no bindings, and a clip's data is large.
            }
            if (jsonRefersToMindWave(OperationLog::operationToJson(*operation), id)) {
                painted = true;
                break;
            }
        }
        if (!usesIt && !painted) {
            continue;
        }
        dependents.layers.push_back(layer.id());
        if (painted) {
            dependents.paintedLayers.push_back(layer.id());
        }
        if (!dependents.lowestStackIndex.has_value()) {
            dependents.lowestStackIndex = index;
        }
    }
    return dependents;
}

}  // namespace sound_mind::core
