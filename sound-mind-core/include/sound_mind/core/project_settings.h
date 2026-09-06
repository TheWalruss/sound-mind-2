#pragma once

#include <cstdint>

#include <nlohmann/json.hpp>

namespace sound_mind::core {

/**
 * @brief Which frequency-axis layout a project's spectrogram data uses.
 *
 * Only `Log` exists so far - the mel/octave/variable-Q alternatives from
 * `docs/sound-mind-design.md`'s "Frequency Scale" aren't meaningful until a
 * codec exists to interpret them. Adding a new enum value later is an
 * additive, non-breaking change to the project file (old files simply
 * don't use it), so there's no need to pre-declare them now.
 */
enum class FrequencyScale {
    Log,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(FrequencyScale, {
    {FrequencyScale::Log, "log"},
})
// clang-format on

/**
 * @brief The codec defaults and project-wide preferences every new layer
 *        inherits, per `docs/sound-mind-architecture.md`'s "Project File &
 *        Folder".
 *
 * @note Deliberately minimal for now: only fields that don't depend on a
 *       not-yet-existing codec or feature are included. Frequency-scale
 *       parameters beyond the scale choice itself (e.g. variable-Q zone
 *       breakpoints), and anything Pool/Stream-format-specific, are added
 *       once the codec that needs them exists.
 */
struct ProjectSettings {
    /// @brief Audio sample rate in Hz new layers are encoded/decoded at.
    std::uint32_t sampleRateHz = 44100;

    /// @brief Frequency-axis layout. See FrequencyScale.
    FrequencyScale frequencyScale = FrequencyScale::Log;

    /// @brief Milliseconds per horizontal canvas pixel.
    double timestepMs = 10.0;

    /// @brief Canvas width in pixels for a newly created layer.
    std::uint32_t canvasWidth = 1024;

    /// @brief Canvas height in pixels for a newly created layer.
    std::uint32_t canvasHeight = 512;

    /// @brief Tuning reference for note names/grids, in Hz (A4).
    double referenceHz = 440.0;

    /// @brief Default project tempo in beats per minute.
    double defaultTempoBpm = 120.0;
};

/// @brief Serializes settings to their JSON representation.
void to_json(nlohmann::json& json, const ProjectSettings& settings);

/// @brief Parses settings from their JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, ProjectSettings& settings);

}  // namespace sound_mind::core
