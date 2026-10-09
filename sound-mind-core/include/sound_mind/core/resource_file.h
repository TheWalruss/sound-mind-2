#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/convolution_kernel.h"
#include "sound_mind/core/filter_preset.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/resonance_profile.h"
#include "sound_mind/core/tool_preset.h"

namespace sound_mind::core {

/**
 * @brief Which kind of named resource a portable resource file holds - the
 *        `resourceType` tag in its JSON envelope (see this header's own
 *        docs below).
 *
 * `docs/sound-mind-architecture.md`'s "Portable Resource Files" table lists
 * the on-disk extension each one conventionally uses
 * (`portableResourceFileExtension()` below) - `.smwave`, `.sminst`,
 * `.smshot`, `.smresonance`, `.smfilter`, `.smkernel`. Layer and Mind Grain
 * deliberately have no entry here - see that same table's own notes on why
 * neither is a standalone-file resource (a layer travels between projects
 * directly, via `Project::addLayer()`; a Mind Grain embeds no pixel
 * content to export in the first place).
 *
 * **`FilterPreset` added after `ConvolutionKernel`, confirmed with the
 * user** ("make filter configuration save-able to a filter preset, just
 * like tool configurations are save-able to tool preset") - a whole
 * `FilterConfiguration` (any `FilterType`), not just a saved convolution
 * kernel. Took over the `.smfilter` extension - the name a user actually
 * expects "Filter" to mean, mirroring `ToolPreset`'s own `.sminst` - while
 * `ConvolutionKernel` moved to `.smkernel`, a rename made freely since
 * neither has shipped to any real user yet (see `docs/sound-mind-
 * architecture.md`'s own Decision on this).
 */
enum class PortableResourceType {
    MindWave,
    ToolPreset,
    MindShot,
    ResonanceProfile,
    ConvolutionKernel,
    FilterPreset,
};

// clang-format off
NLOHMANN_JSON_SERIALIZE_ENUM(PortableResourceType, {
    {PortableResourceType::MindWave, "mindWave"},
    {PortableResourceType::ToolPreset, "toolPreset"},
    {PortableResourceType::MindShot, "mindShot"},
    {PortableResourceType::ResonanceProfile, "resonanceProfile"},
    {PortableResourceType::ConvolutionKernel, "convolutionKernel"},
    {PortableResourceType::FilterPreset, "filterPreset"},
})
// clang-format on

/**
 * @brief The conventional file extension (with leading `.`) for standalone
 *        files of the given resource type - `docs/sound-mind-
 *        architecture.md`'s "Portable Resource Files" table.
 * @param type The resource type to look up.
 * @return The extension, e.g. `".smwave"` for `PortableResourceType::MindWave`.
 */
[[nodiscard]] std::string_view portableResourceFileExtension(PortableResourceType type) noexcept;

/**
 * @brief Writes a `NamedMindWave` to a standalone `.smwave` file.
 *
 * Every export function in this header shares one on-disk shape - a small
 * JSON envelope (`soundMindResourceFile: true`, this entry's own
 * `resourceType` tag, a `formatVersion`, its `name`, and its payload via
 * the same `to_json()` the owning Project's own library already uses for
 * it) - rather than inventing a bespoke format per resource type. `entry`'s
 * own `id` is deliberately not written: it's only ever meaningful within
 * the Project that assigned it, and a fresh one is assigned on import
 * (`Project::addMindWave()` and its siblings already do this for every
 * caller, standalone-file import included).
 *
 * @param entry The entry to export - its `name` and payload are written;
 *        its `id` is not.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportMindWave(const NamedMindWave& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedMindWave` back from a standalone `.smwave` file -
 *        the inverse of `exportMindWave()`.
 *
 * The returned entry's own `id` is always `0` (see `exportMindWave()`'s own
 * docs on why an id is never written) - a caller importing this into a
 * Project should pass its `name`/`wave` to `Project::addMindWave()`, which
 * assigns a fresh, project-unique id itself, rather than using this
 * placeholder directly.
 *
 * @param path The file to read.
 * @return The entry this file holds.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, or names a `resourceType` other than
 *         `PortableResourceType::MindWave`.
 */
[[nodiscard]] NamedMindWave importMindWave(const std::filesystem::path& path);

/**
 * @brief Writes a `NamedToolPreset` to a standalone `.sminst` file - see
 *        `exportMindWave()`'s own docs for the shared envelope shape this
 *        follows.
 *
 * Covers every `ToolType`, not just `ToolType::Instrument` - a "Sound Mind
 * Instrument" is a `ToolType::Instrument` preset in the actual
 * implementation, never its own class (`docs/sound-mind-architecture.md`'s
 * *Decisions Needed* on the still-pending "Instrument"/"Harmonics" rename),
 * so one portable-file kind serves every tool preset, named `.sminst` for
 * the design doc's own original "Sound Mind Instrument" portable file.
 *
 * @param entry The entry to export - its own `id` is not written, the same
 *        reasoning as `exportMindWave()`. Must have a non-null `config`.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 * @throws std::invalid_argument if `entry.config` is `nullptr`.
 */
void exportToolPreset(const NamedToolPreset& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedToolPreset` back from a standalone `.sminst` file -
 *        the inverse of `exportToolPreset()`.
 * @param path The file to read.
 * @return The entry this file holds, with `id == 0` - see `importMindWave()`'s
 *         own docs on why.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, names a `resourceType` other than
 *         `PortableResourceType::ToolPreset`, or its payload's own `"type"`
 *         field names an unrecognized `ToolType`.
 */
[[nodiscard]] NamedToolPreset importToolPreset(const std::filesystem::path& path);

/**
 * @brief Writes a `NamedMindShot` to a standalone `.smshot` file - see
 *        `exportMindWave()`'s own docs for the shared envelope shape this
 *        follows.
 *
 * **Not "just a Stream file"** - `docs/sound-mind-architecture.md`'s
 * "Portable Resource Files" table originally said a Mind Shot needed no
 * standalone format of its own because it was a `codec::StreamImage`; that
 * was only ever true of a draft representation - `NamedMindShot::clip` has
 * been a plain JSON `Clip` struct (the same one Copy/Paste already use)
 * since Mind Shots were actually built, so it gets the same JSON envelope
 * every other resource here does.
 *
 * @param entry The entry to export - its own `id` is not written, the same
 *        reasoning as `exportMindWave()`.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportMindShot(const NamedMindShot& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedMindShot` back from a standalone `.smshot` file -
 *        the inverse of `exportMindShot()`.
 * @param path The file to read.
 * @return The entry this file holds, with `id == 0` - see `importMindWave()`'s
 *         own docs on why.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, or names a `resourceType` other than
 *         `PortableResourceType::MindShot`.
 */
[[nodiscard]] NamedMindShot importMindShot(const std::filesystem::path& path);

/**
 * @brief Writes a `NamedResonanceProfile` to a standalone `.smresonance`
 *        file - see `exportMindWave()`'s own docs for the shared envelope
 *        shape this follows.
 * @param entry The entry to export - its own `id` is not written, the same
 *        reasoning as `exportMindWave()`.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportResonanceProfile(const NamedResonanceProfile& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedResonanceProfile` back from a standalone
 *        `.smresonance` file - the inverse of `exportResonanceProfile()`.
 * @param path The file to read.
 * @return The entry this file holds, with `id == 0` - see `importMindWave()`'s
 *         own docs on why.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, or names a `resourceType` other than
 *         `PortableResourceType::ResonanceProfile`.
 */
[[nodiscard]] NamedResonanceProfile importResonanceProfile(const std::filesystem::path& path);

/**
 * @brief Writes a `NamedConvolutionKernel` to a standalone `.smkernel`
 *        file - see `exportMindWave()`'s own docs for the shared envelope
 *        shape this follows.
 * @param entry The entry to export - its own `id` is not written, the same
 *        reasoning as `exportMindWave()`.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportConvolutionKernel(const NamedConvolutionKernel& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedConvolutionKernel` back from a standalone
 *        `.smkernel` file - the inverse of `exportConvolutionKernel()`.
 * @param path The file to read.
 * @return The entry this file holds, with `id == 0` - see `importMindWave()`'s
 *         own docs on why.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, or names a `resourceType` other than
 *         `PortableResourceType::ConvolutionKernel`.
 */
[[nodiscard]] NamedConvolutionKernel importConvolutionKernel(const std::filesystem::path& path);

/**
 * @brief Writes a `NamedFilterPreset` to a standalone `.smfilter` file -
 *        see `exportMindWave()`'s own docs for the shared envelope shape
 *        this follows.
 * @param entry The entry to export - its own `id` is not written, the same
 *        reasoning as `exportMindWave()`.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportFilterPreset(const NamedFilterPreset& entry, const std::filesystem::path& path);

/**
 * @brief Reads a `NamedFilterPreset` back from a standalone `.smfilter`
 *        file - the inverse of `exportFilterPreset()`.
 * @param path The file to read.
 * @return The entry this file holds, with `id == 0` - see `importMindWave()`'s
 *         own docs on why.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind portable resource file, or names a `resourceType` other than
 *         `PortableResourceType::FilterPreset`.
 */
[[nodiscard]] NamedFilterPreset importFilterPreset(const std::filesystem::path& path);

/**
 * @brief One resource bundled inside a Toolkit collection - `docs/sound-
 *        mind-roadmap.md`'s `v0.Y.57.1` Installment D, the roadmap
 *        backlog's own "SoundMind Toolkit collections" entry ("bundling
 *        several named resources... into one packaged, exportable,
 *        shareable, importable... unit").
 *
 * Deliberately just a type tag plus that resource's own already-
 * serialized JSON (the same shape `exportMindWave()` and its four
 * siblings each write as their file's own `"resource"` field) - a
 * Toolkit is a flat bundle of entries any mix of the five portable types
 * above, not a new resource type of its own, so it reuses each one's own
 * existing `to_json()`/`from_json()` rather than inventing parallel
 * fields per type.
 */
struct ToolkitEntry {
    /// @brief Which of the five portable resource types `resource` is.
    PortableResourceType type = PortableResourceType::MindWave;

    /// @brief That resource's own serialized JSON - e.g. a `NamedMindWave`
    ///        for `PortableResourceType::MindWave`. Its own `id` field, if
    ///        present, is ignored on both export and import (zeroed the
    ///        same way every standalone portable resource file's own `id`
    ///        is) - see `exportMindWave()`'s own docs for why.
    nlohmann::json resource;
};

/**
 * @brief Writes a named collection of resources to a standalone
 *        `.smtoolkit` file.
 *
 * One JSON envelope, the same spirit as every standalone resource file
 * above: `{soundMindToolkit: true, formatVersion: 1, name: ..., entries:
 * [{resourceType: ..., resource: ...}, ...]}`. Each entry's own `resource`
 * has its `id` field zeroed before writing (if present) - the same
 * "never meaningful outside the project that assigned it" reasoning
 * every other export function here already follows.
 *
 * @param name The toolkit's own display name.
 * @param entries The resources to bundle - any mix of the five portable
 *        types, in any order; duplicates (by type/content) are written
 *        as given, not deduplicated.
 * @param path Destination path. Any existing file there is overwritten.
 * @throws std::ios_base::failure if `path` can't be opened for writing.
 */
void exportToolkit(const std::string& name, const std::vector<ToolkitEntry>& entries,
                    const std::filesystem::path& path);

/// @brief A Toolkit collection read back from a standalone `.smtoolkit`
/// file - the inverse of exportToolkit()'s own inputs.
struct ImportedToolkit {
    /// @brief The toolkit's own display name, as given to exportToolkit().
    std::string name;

    /// @brief The bundled resources - each entry's own `resource.id` is
    ///        `0`, the same "never written, always zeroed back" contract
    ///        every other `importXxx()` function here already follows.
    std::vector<ToolkitEntry> entries;
};

/**
 * @brief Reads a Toolkit collection back from a standalone `.smtoolkit`
 *        file - the inverse of exportToolkit().
 * @param path The file to read.
 * @return The toolkit's own name and bundled entries.
 * @throws std::ios_base::failure if `path` can't be opened for reading.
 * @throws nlohmann::json::exception on malformed or missing required data.
 * @throws std::invalid_argument if the file's own envelope isn't a Sound
 *         Mind Toolkit file.
 */
[[nodiscard]] ImportedToolkit importToolkit(const std::filesystem::path& path);

}  // namespace sound_mind::core
