#pragma once

#include <filesystem>
#include <vector>

#include <QString>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"

namespace sound_mind::studio {

/**
 * @brief Imports every note-bearing channel of the Standard MIDI File at
 *        `path` into `project` as one or more new, non-destructive layers -
 *        `v0.Y.55.1`'s own MIDI-specific work, see
 *        `docs/sound-mind-design.md`'s "Import" ("each MIDI program/
 *        instrument in the file is mapped to a paint preset... every MIDI
 *        note becomes an individual paint event").
 *
 * Each channel's own notes (`sound_mind::core::parseMidiFile()`) become a
 * single `sound_mind::core::SequenceOperation` appended to `project`'s own
 * `OperationLog` - not baked pixels, matching the design doc's own "the
 * imported layer is therefore fully editable afterward" requirement (see
 * `SequenceOperation`'s own docs on why this makes a note re-timeable/re-
 * voiceable without repainting). **Deliberately does not itself render any
 * pixels** - a freshly added layer is left with no `content()` at all, the
 * same "content synthesized lazily on first rebuild" contract every other
 * content-less layer already has; the caller is responsible for triggering
 * that rebuild afterward (`ToolPaletteController::rebuildLayerContent()`,
 * in `MainWindow::importMidiFile()`'s own case) - this function has no
 * `PaintController` to call it with itself, matching this file's own
 * `import_export.cpp` counterpart's existing "pure `Project&` mutation, no
 * UI/rendering concerns" boundary.
 *
 * **Installment A scope, `v0.Y.55.1`** - two real simplifications, both
 * revisited by a later installment of this same milestone:
 * - **Every channel plays through a plain, default
 *   `ProceduralConfiguration`** (a fresh instance per channel, since each
 *   `SequenceOperation` owns its own `ToolConfiguration` - see that class's
 *   own docs) - mapping a channel's own `programNumber` to one of
 *   `project`'s own saved Tool Presets is the still-unbuilt "MIDI
 *   Configuration panel" installment's job.
 * - **No snippet chopping** - unlike `audioSnippetsForFile()`'s own
 *   project-length-window splitting, every note in the file is imported
 *   as-is, regardless of how far past `project`'s own duration it falls;
 *   a note starting beyond the canvas's own visible/paintable time range
 *   simply never renders anything (safe, not a crash - the same clipping
 *   behavior any other paint operation already has for an out-of-bounds
 *   stroke), but isn't lost from the operation log either. Letting the
 *   user pick which project-length window(s) to import - the same
 *   "snippets" concept audio import already has, per
 *   `docs/sound-mind-roadmap.md`'s own description of this milestone - is
 *   a real, separate, still-unbuilt installment.
 *
 * @param project The project to import into.
 * @param path Path to the `.mid`/`.midi` file to import.
 * @param separateLayerPerChannel When `true` (the default), each channel
 *        becomes its own new layer, named "`<stem>` - Ch`<n>` (`<GM
 *        instrument name>`)". When `false`, every channel's own
 *        `SequenceOperation` targets one single new layer instead, named
 *        plainly from the file's own stem - `OperationLog::
 *        activeOperationsTargeting()` already folds every operation
 *        targeting one layer together on rebuild, so this needs no special
 *        handling beyond appending each channel's operation to the same
 *        layer id.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @return The newly added layer ids, in the same order `parseMidiFile()`
 *         returned their own channels (ascending `channelNumber`) when
 *         `separateLayerPerChannel` is `true`; a single-entry vector with
 *         the one shared layer's id otherwise. Empty if the file couldn't
 *         be parsed, or had no notes on any channel.
 */
[[nodiscard]] std::vector<sound_mind::core::LayerId> importMidiChannelsInto(
    sound_mind::core::Project& project, const std::filesystem::path& path, bool separateLayerPerChannel = true,
    QString* errorMessage = nullptr);

}  // namespace sound_mind::studio
