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
     *
     * Any layer whose media/pool file (see save()'s docs for the paths)
     * exists alongside the project is loaded back into that layer's
     * `Layer::content()`/`poolContent()`; a layer with no such file yet
     * (never rendered, or never Pooled) simply has no content there, same
     * as a freshly-created one.
     *
     * @param path Path to the project file.
     * @return The loaded project.
     * @throws std::ios_base::failure if the file can't be read.
     * @throws nlohmann::json::exception on malformed or missing required data.
     */
    [[nodiscard]] static Project load(const std::filesystem::path& path);

    /**
     * @brief Saves this project to a `.smproj` JSON file on disk.
     *
     * Per `docs/sound-mind-architecture.md`'s Project File & Folder layout,
     * any layer with cached content (see `Layer::content()`) also gets that
     * content written as its own Stream file, to `<path's folder>/<path's
     * stem>/media/layer_<id>.smstream`; any layer with Pool content (see
     * `Layer::poolContent()`) likewise gets `.../pool/layer_<id>.smpool` -
     * both created if they don't exist yet.
     *
     * @param path Destination path.
     * @throws std::ios_base::failure if the file (or a layer's media/pool
     *         file) can't be written.
     */
    void save(const std::filesystem::path& path) const;

    /**
     * @brief Appends a new layer to the top of the layer stack.
     *
     * @param layer The layer to add. Its own id is ignored - a fresh,
     *        unique id is assigned to the appended copy instead, since a
     *        caller building a layer to import has no way to know what ids
     *        are already taken.
     * @return The id actually assigned to the appended layer.
     */
    LayerId addLayer(Layer layer);

    /**
     * @brief Removes the layer with the given id, if one exists.
     *
     * No restriction here on removing a `Background`/`Equalizer` layer -
     * that's a UI-level rule (`sound-mind-studio`'s `LayersPanel` doesn't
     * even show a delete button for them), not a `Project`-level
     * invariant, the same division `Layer::setVisible()`'s docs draw for
     * the Background-stays-visible rule.
     *
     * @param id The layer to remove.
     * @return `true` if a layer with this id was found and removed;
     *         `false` (no change) if none was.
     */
    bool removeLayer(LayerId id);

    /**
     * @brief Reorders the layer stack.
     *
     * @param newOrderBottomToTop Every current layer's id, exactly once
     *        each, in the desired new bottom-to-top order.
     * @return `true` and applies the reorder if `newOrderBottomToTop` is
     *         a valid permutation of the current layers' ids (same size,
     *         same set, no duplicates); `false` (no change) otherwise -
     *         e.g. a missing id, an unknown id, or a duplicate.
     */
    bool reorderLayers(const std::vector<LayerId>& newOrderBottomToTop);

    /// @brief This project's settings (sample rate, canvas size, etc).
    /// @return The settings this project currently holds.
    [[nodiscard]] const ProjectSettings& settings() const noexcept { return settings_; }

    /// @brief This project's layer stack, in bottom-to-top order.
    /// @return The layers this project currently holds.
    [[nodiscard]] const std::vector<Layer>& layers() const noexcept { return layers_; }

    /// @brief This project's layer stack, in bottom-to-top order - mutable
    ///        access, for in-place changes (renaming, opacity, Pooling,
    ///        and eventually painting) that don't change the stack's
    ///        membership or order (addLayer() is still how a new layer
    ///        gets appended).
    /// @return The layers this project currently holds.
    [[nodiscard]] std::vector<Layer>& layers() noexcept { return layers_; }

    /**
     * @brief Finds the layer with the given id, if one exists.
     *
     * A small, project-level lookup shared by every caller that needs
     * "the layer this operation/selection/clipboard targets" from just an
     * id - `sound-mind-studio`'s `MainWindow`, `PaintController`, and
     * `SelectionController` each needed the identical few-line linear
     * search independently before this existed here instead.
     *
     * @param id The layer to find.
     * @return A pointer to that layer, or `nullptr` if no layer with this
     *         id exists.
     */
    [[nodiscard]] const Layer* layerById(LayerId id) const noexcept;

    /// @brief Mutable overload of layerById() - for in-place changes
    ///        (setting content, renaming, opacity) that don't change the
    ///        stack's own membership or order.
    /// @param id The layer to find.
    /// @return A mutable pointer to that layer, or `nullptr` if no layer
    ///         with this id exists.
    [[nodiscard]] Layer* layerById(LayerId id) noexcept;

    /// @brief This project's single, project-wide operation log.
    /// @return The operation log this project currently holds.
    [[nodiscard]] const OperationLog& operationLog() const noexcept { return operationLog_; }

    /// @brief Mutable access to this project's operation log, for
    ///        appending new operations (painting and, eventually, every
    ///        other loggable action) and undo()/redo().
    /// @return The operation log this project currently holds.
    [[nodiscard]] OperationLog& operationLog() noexcept { return operationLog_; }

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
