#include "sound_mind/core/project.h"

#include <fstream>
#include <ios>

namespace sound_mind::core {

namespace {
// The Background layer is always the first (and, for now, only) layer a
// new project has - id allocation for more than one layer is a concern
// for whichever milestone first adds a second layer (Import).
constexpr LayerId kBackgroundLayerId = 1;
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
    return json.get<Project>();
}

void Project::save(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open project file for writing: " + path.string());
    }
    const nlohmann::json json = *this;
    file << json.dump(2);
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
