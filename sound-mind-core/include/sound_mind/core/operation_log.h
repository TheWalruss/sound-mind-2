#pragma once

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief A Project's single, project-wide, ordered sequence of operations.
 *
 * See `docs/sound-mind-architecture.md`'s Core Data Model and "Composer
 * Mode Fit": this belongs to the Project, not to any one Layer, since a
 * structural operation (reordering the layer stack) doesn't target a
 * single layer's content.
 *
 * @note Genuinely empty for now. No concrete `Operation` subtype exists
 *       yet (the first one arrives with the `Basic Painting` milestone -
 *       see `docs/sound-mind-roadmap.md`), so there is nothing to log or
 *       replay yet. This class exists so the project file has a stable
 *       place for the log to live once operations do.
 */
class OperationLog {
public:
    /**
     * @brief How many operations are currently logged.
     * @return Always 0 for now - see the class-level @note.
     */
    [[nodiscard]] std::size_t size() const noexcept { return 0; }
};

/// @brief Serializes the log to its JSON representation (an empty array, for now).
void to_json(nlohmann::json& json, const OperationLog& log);

/// @brief Parses the log from its JSON representation.
/// @throws nlohmann::json::exception if the JSON value isn't an array.
void from_json(const nlohmann::json& json, OperationLog& log);

}  // namespace sound_mind::core
