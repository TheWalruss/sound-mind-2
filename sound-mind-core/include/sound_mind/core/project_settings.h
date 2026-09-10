#pragma once

#include <cstdint>

#include <nlohmann/json.hpp>

#include "sound_mind/codec/stream_codec.h"

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
 *       breakpoints), and anything Pool-format-specific, are added once
 *       the codec that needs them exists.
 *
 * **As of `v0.Y.11.1` (Create Project Wizard):** `binCount`,
 * `minFrequencyHz`, and `maxFrequencyHz` were added specifically so
 * `streamCodecConfigFor()` below has a real project-level source for
 * every field `sound_mind::codec::StreamCodecConfig` needs - see that
 * function's own docs for what actually consumes it.
 */
struct ProjectSettings {
    /// @brief Audio sample rate in Hz new layers are encoded/decoded at.
    std::uint32_t sampleRateHz = 44100;

    /// @brief Frequency-axis layout. See FrequencyScale.
    FrequencyScale frequencyScale = FrequencyScale::Log;

    /// @brief Milliseconds per horizontal canvas pixel - also this
    /// project's Stream codec hop length, once converted to a sample
    /// count via streamCodecConfigFor().
    double timestepMs = 10.0;

    /// @brief Canvas width in pixels for a newly created layer.
    std::uint32_t canvasWidth = 1024;

    /// @brief Canvas height in pixels for a newly created layer - kept
    /// numerically equal to `binCount` (one canvas row per frequency
    /// bin), by whatever sets both together (the Create Project Wizard,
    /// in particular) rather than being derived automatically here.
    std::uint32_t canvasHeight = 512;

    /// @brief Tuning reference for note names/grids, in Hz (A4).
    double referenceHz = 440.0;

    /// @brief Default project tempo in beats per minute.
    double defaultTempoBpm = 120.0;

    /// @brief Number of log-spaced frequency bins new layers are encoded
    /// with - see `sound_mind::codec::StreamCodecConfig::binCount`.
    std::uint32_t binCount = 512;

    /// @brief Lower edge of the encoded frequency range, in Hz - see
    /// `sound_mind::codec::StreamCodecConfig::minFrequencyHz`.
    float minFrequencyHz = 20.0f;

    /// @brief Upper edge of the encoded frequency range, in Hz - see
    /// `sound_mind::codec::StreamCodecConfig::maxFrequencyHz`.
    float maxFrequencyHz = 16000.0f;
};

/// @brief Serializes settings to their JSON representation.
void to_json(nlohmann::json& json, const ProjectSettings& settings);

/// @brief Parses settings from their JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, ProjectSettings& settings);

/**
 * @brief Builds the Stream codec configuration a project's own settings
 *        imply, for encoding new content into one of its layers.
 *
 * Bridges `ProjectSettings` (a Core concept) to
 * `sound_mind::codec::StreamCodecConfig` (Codec's own, deliberately
 * Core-independent parameter struct - see its docs for why the two
 * aren't the same type) - this function is the bridge precisely because
 * it lives in Core, which is allowed to depend on Codec, rather than the
 * other way around.
 *
 * **As of `v0.Y.11.1`:** used by `sound-mind-studio`'s `MainWindow` for
 * `importAudioFile()`/`importImageFile()` and Recording's post-capture
 * encode - the direct, per-call encode sites. `LiveEngine`'s own
 * capture/encode config is deliberately *not* wired to this yet: it's
 * constructed once, before any project exists, and reconfiguring it per
 * project is exactly the kind of lifecycle change Loop Mode's own
 * reimplementation (`v0.Y.12.1`, immediately next) is already expected to
 * make - investing in it here would likely be thrown away there.
 *
 * @param settings The project settings to derive a config from.
 * @return A `StreamCodecConfig` with `sampleRateHz`, `binCount`,
 *         `minFrequencyHz`, and `maxFrequencyHz` carried over unchanged,
 *         and `hopLength` derived from `timestepMs` at `sampleRateHz`
 *         (rounded to the nearest whole sample).
 */
[[nodiscard]] sound_mind::codec::StreamCodecConfig streamCodecConfigFor(const ProjectSettings& settings);

/**
 * @brief A real, silent (all-zero) `StreamImage`, sized to exactly fill a
 *        project's own canvas - `canvasWidth` columns' worth of true
 *        digital silence, run through the same `sound_mind::codec::
 *        encode()` pipeline any other imported/recorded audio goes
 *        through (not a raw-zeroed buffer skipping encoding), so it
 *        behaves exactly like any other layer's own content under
 *        decode()/export/further painting.
 *
 * Gives a layer real content immediately rather than leaving it
 * `std::nullopt` - used wherever a layer needs to start as a real,
 * already-paintable/-playable canvas rather than an empty placeholder
 * (e.g. `PaintController`'s lazy fill-in for a content-less layer's first
 * stroke - see `docs/sound-mind-architecture.md`'s Decisions Made for
 * why the Background layer specifically needs this).
 *
 * @param settings The project settings to size/encode the silence with.
 * @return The encoded silent StreamImage.
 */
[[nodiscard]] sound_mind::codec::StreamImage silentContentFor(const ProjectSettings& settings);

/**
 * @brief The per-project normalization scale `sound_mind::core::
 *        applyPaintOperation()`/`fitPathToPoints()`/hit-testing a Picked
 *        object all need to compare a time (seconds) distance and a
 *        frequency (Hz) distance on equal footing - see those functions'
 *        own docs for why a Path's own geometry needs one shared unit
 *        rather than mixing seconds and Hz directly.
 *
 * Derived from the project's own canvas geometry: its total duration in
 * seconds against its total encoded frequency range in Hz. Falls back to
 * an arbitrary, always-positive `1000.0` for a degenerate (zero or
 * negative duration) project, so callers never have to guard against a
 * division by zero themselves.
 *
 * @param settings The project settings to derive the scale from.
 * @return The frequency-to-time scale, in Hz per second-equivalent.
 */
[[nodiscard]] double frequencyToTimeScaleFor(const ProjectSettings& settings) noexcept;

}  // namespace sound_mind::core
