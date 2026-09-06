#pragma once

#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief A Sound Mind Project: settings, an ordered layer stack, and the
 *        operation log, per `docs/sound-mind-architecture.md`'s Core Data
 *        Model and "Project File & Folder".
 *
 * @note Deliberately minimal for now: resource libraries (MindWaves, Sound
 *       Mind Instruments, Mind Shots, Mind Grains) and sequences aren't
 *       represented yet, since none of those features exist. Their absence
 *       from a saved file is meant to be forward-compatible - added as
 *       fields once each feature lands, not designed in speculatively now.
 */
class Project {
public:
    /// @brief Default-constructs a Project with default settings and no
    ///        layers.
    ///
    /// Exists so `from_json` (a free function, not a member) can populate
    /// an instance in place via nlohmann::json's default (de)serialization
    /// convention - the same reasoning as `Layer`'s default constructor.
    /// Prefer createNew() for normal use.
    Project() = default;

    /**
     * @brief Creates a new project with the given settings and a single
     *        Background layer, per `docs/sound-mind-design.md`'s "Special
     *        Layers".
     *
     * @note The Equalizer special layer isn't created yet - it needs a
     *       working Filter mechanism, which doesn't exist until the
     *       `Filter Layers` milestone (see `docs/sound-mind-roadmap.md`).
     *       A non-functional stand-in would just need redesigning then.
     *
     * @param settings The settings the new project should carry.
     * @return The new project, with its Background layer and an empty
     *         operation log.
     */
    [[nodiscard]] static Project createNew(ProjectSettings settings);

    /**
     * @brief Loads a project from its `.smproj` JSON file on disk.
     * @param path Path to the project file.
     * @return The loaded project.
     * @throws std::ios_base::failure if the file can't be read.
     * @throws nlohmann::json::exception on malformed or missing required data.
     */
    [[nodiscard]] static Project load(const std::filesystem::path& path);

    /**
     * @brief Saves this project to a `.smproj` JSON file on disk.
     * @param path Destination path.
     * @throws std::ios_base::failure if the file can't be written.
     */
    void save(const std::filesystem::path& path) const;

    /// @brief This project's settings (sample rate, canvas size, etc).
    /// @return The settings this project currently holds.
    [[nodiscard]] const ProjectSettings& settings() const noexcept { return settings_; }

    /// @brief This project's layer stack, in bottom-to-top order.
    /// @return The layers this project currently holds.
    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }

    /// @brief This project's single, project-wide operation log.
    /// @return The operation log this project currently holds.
    [[nodiscard]] const OperationLog& operationLog() const noexcept { return operationLog_; }

    friend void to_json(nlohmann::json& json, const Project& project);
    friend void from_json(const nlohmann::json& json, Project& project);

private:
    ProjectSettings settings_;
    std::vector<Layer> layers_;
    OperationLog operationLog_;
};

/// @brief Serializes a Project to its JSON representation.
void to_json(nlohmann::json& json, const Project& project);

/// @brief Parses a Project from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Project& project);

}  // namespace sound_mind::core
