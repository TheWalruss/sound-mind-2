#include "sound_mind/core/project.h"

#include <algorithm>
#include <fstream>
#include <ios>

#include "sound_mind/codec/pool_file.h"
#include "sound_mind/codec/stream_file.h"

namespace sound_mind::core {

namespace {
// The Background layer is always the first (and, for now, only) layer a
// new project has - id allocation for more than one layer is a concern
// for whichever milestone first adds a second layer (Import).
constexpr LayerId kBackgroundLayerId = 1;

/// @brief The directory a project's media/sources/pool/mindshots folders
/// live under, per docs/sound-mind-architecture.md's Project File & Folder
/// layout: a same-named folder next to the .smproj file.
[[nodiscard]] std::filesystem::path projectFolder(const std::filesystem::path& projectFilePath) {
    return projectFilePath.parent_path() / projectFilePath.stem();
}

/// @brief The path a given layer's cached Stream render would live at,
/// within a project's folder.
[[nodiscard]] std::filesystem::path mediaPathFor(const std::filesystem::path& projectFilePath, LayerId layerId) {
    return projectFolder(projectFilePath) / "media" / ("layer_" + std::to_string(layerId) + ".smstream");
}

/// @brief The path a given layer's cached Pool render would live at,
/// within a project's folder.
[[nodiscard]] std::filesystem::path poolPathFor(const std::filesystem::path& projectFilePath, LayerId layerId) {
    return projectFolder(projectFilePath) / "pool" / ("layer_" + std::to_string(layerId) + ".smpool");
}
}  // namespace

Project Project::createNew(ProjectSettings settings) {
    Project project;
    project.settings_ = settings;
    project.layers_.emplace_back(kBackgroundLayerId, "Background", LayerType::Background);
    return project;
}

Project Project::load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open project file for reading: " + path.string());
    }
    nlohmann::json json;
    file >> json;
    Project project = json.get<Project>();

    for (Layer& layer : project.layers_) {
        const std::filesystem::path mediaPath = mediaPathFor(path, layer.id());
        if (std::filesystem::exists(mediaPath)) {
            layer.setContent(sound_mind::codec::readStreamFile(mediaPath));
        }
        const std::filesystem::path poolPath = poolPathFor(path, layer.id());
        if (std::filesystem::exists(poolPath)) {
            layer.setPoolContent(sound_mind::codec::readPoolFile(poolPath));
        }
    }

    return project;
}

void Project::save(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open project file for writing: " + path.string());
    }
    const nlohmann::json json = *this;
    file << json.dump(2);

    for (const Layer& layer : layers_) {
        if (layer.content().has_value()) {
            const std::filesystem::path mediaPath = mediaPathFor(path, layer.id());
            std::filesystem::create_directories(mediaPath.parent_path());
            sound_mind::codec::writeStreamFile(mediaPath, *layer.content());
        }
        if (layer.poolContent().has_value()) {
            const std::filesystem::path poolPath = poolPathFor(path, layer.id());
            std::filesystem::create_directories(poolPath.parent_path());
            sound_mind::codec::writePoolFile(poolPath, *layer.poolContent());
        }
    }
}

LayerId Project::addLayer(Layer layer) {
    const auto maxId = std::max_element(
        layers_.begin(), layers_.end(), [](const Layer& a, const Layer& b) { return a.id() < b.id(); });
    const LayerId newId = (maxId == layers_.end() ? kBackgroundLayerId : maxId->id()) + 1;

    layer.setId(newId);
    layers_.push_back(std::move(layer));
    return newId;
}

void to_json(nlohmann::json& json, const Project& project) {
    json = nlohmann::json{
        {"settings", project.settings_},
        {"layers", project.layers_},
        {"operationLog", project.operationLog_},
    };
}

void from_json(const nlohmann::json& json, Project& project) {
    json.at("settings").get_to(project.settings_);
    json.at("operationLog").get_to(project.operationLog_);

    project.layers_.clear();
    for (const auto& layerJson : json.at("layers")) {
        project.layers_.push_back(layerJson.get<Layer>());
    }
}

}  // namespace sound_mind::core
