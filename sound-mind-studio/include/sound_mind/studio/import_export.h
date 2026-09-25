#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <vector>

#include <QImage>
#include <QString>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"

namespace sound_mind::studio {

/**
 * @brief Thrown by encodeAudioSnippets() when its own `shouldCancel`
 *        callback starts returning `true` mid-encode -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (finding #12,
 *        Installment F).
 *
 * A distinct type, not a plain `std::runtime_error`, specifically so a
 * caller can tell "cancelled on request" apart from a genuine encode
 * failure. The Studio-level counterpart to
 * `sound_mind::codec::ExportCancelled` - a separate type, not the same one
 * reused, since this is a Studio-level (import orchestration), not
 * Codec-level, concern; `sound-mind-studio` already depends on
 * `sound-mind-codec`, but sharing that exception type here would mean an
 * "export" name on an import-side failure, which reads backwards.
 */
class ImportCancelled : public std::runtime_error {
public:
    ImportCancelled() : std::runtime_error("import cancelled") {}
};

/**
 * @brief The Import/Export operations extracted out of `MainWindow` as
 *        part of the Phase 2.5 Refactor & Clean Up milestone
 *        (`v0.Y.23.1`, part 2).
 *
 * Free functions, not a class - unlike `PlaybackController`, there's no
 * persistent engine or timer to own here, just project-mutating logic
 * that needs a `Project&`/`Layer` to work against, the same shape
 * `sound-mind-core`'s own `layer_export.h` already uses one level down.
 * Each function does the actual read/encode/decode/write work and mutates
 * `project` directly, but never touches UI: no status bar messages, no
 * `canvas_->update()`, no `hasUnsavedChanges_` - `MainWindow`'s own
 * wrapper methods (unchanged signatures) still own all of that, calling
 * these in between.
 */

/**
 * @brief Computes how the audio file at `path` would split into
 *        `project`'s own duration worth of snippets, without importing
 *        anything or showing any dialog.
 *
 * A snippet's length is the same `canvasWidth * hopLength` samples
 * `sound_mind::core::LoopEngine`'s own loop length is derived from -
 * snippet `0` is the source's first such stretch, snippet `1` the next,
 * and so on; the final snippet is shorter than the rest if the source's
 * length isn't an exact multiple. Audio no longer than one snippet's
 * worth always returns exactly one entry.
 *
 * @param project The project whose duration/codec settings to split
 *        against.
 * @param path Path to the WAV file to analyze.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @return One entry per snippet, in order; empty if the file couldn't be
 *         read.
 */
[[nodiscard]] std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(
    const sound_mind::core::Project& project, const std::filesystem::path& path, QString* errorMessage = nullptr);

/**
 * @brief Encodes specific snippets (see audioSnippetsForFile()) of an audio
 *        file into new, as-yet-unattached layers - importAudioSnippetsInto()'s
 *        own encode-only half, split out so the actual (potentially slow)
 *        encode work can run somewhere that isn't safe to mutate a live
 *        `Project` from directly, such as a background thread -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (finding #12,
 *        Installment F).
 *
 * Takes `settings` (a plain value, not a live `Project&`) rather than a
 * `Project` reference for exactly this reason: nothing here touches
 * anything shared or mutable, so a caller running this on a background
 * thread never needs to synchronize against the UI thread's own concurrent
 * project edits. Naming/content shape matches importAudioSnippetsInto()'s
 * own docs exactly - the two functions produce identical layers, this one
 * just doesn't add them to a project itself.
 *
 * @param settings The project settings (canvas width, codec config) to
 *        split/encode against - see audioSnippetsForFile()'s own docs for
 *        the same split math.
 * @param path Path to the WAV file to encode from.
 * @param snippetIndices Which of the source's snippets to encode, in any
 *        order and with any duplicates ignored; an index at or beyond the
 *        source's actual snippet count is silently skipped, not an error.
 * @param shouldCancel Consulted once per snippet, right before that
 *        snippet's own encode begins. Once it returns `true`, throws
 *        `ImportCancelled` immediately rather than encoding any further
 *        snippets - whatever's already been encoded into the return value
 *        so far is simply discarded along with the exception, never having
 *        touched any project. `nullptr` (the default) never cancels.
 * @param errorMessage If non-null and this returns empty, set to a
 *        human-readable description of what went wrong.
 * @return The encoded layers, in ascending snippet order - `id() == 0` on
 *         each (unattached; `Project::addLayer()` assigns a real one);
 *         empty if the file couldn't be read, or nothing was actually
 *         encoded (an empty `snippetIndices`, or every given index out of
 *         range).
 * @throws ImportCancelled if `shouldCancel` returns `true` - see its own
 *         docs.
 */
[[nodiscard]] std::vector<sound_mind::core::Layer> encodeAudioSnippets(
    const sound_mind::core::ProjectSettings& settings, const std::filesystem::path& path,
    const std::vector<std::size_t>& snippetIndices, const std::function<bool()>& shouldCancel = nullptr,
    QString* errorMessage = nullptr);

/**
 * @brief Imports specific snippets (see audioSnippetsForFile()) of an
 *        audio file as new layers into `project`.
 *
 * Each imported snippet becomes its own new Normal layer. With more than
 * one snippet in the source overall, a layer's name is `"<stem>_NNNN"`
 * (the source file's stem, an underscore, and its snippet index
 * zero-padded to four digits); with only one, the layer is named from the
 * file's own name directly.
 *
 * A thin wrapper around encodeAudioSnippets() (with no `shouldCancel`,
 * so it never throws `ImportCancelled`) plus `project.addLayer()` for each
 * result - kept as its own synchronous, non-cancellable entry point for
 * every existing caller (tests, `MainWindow`'s own drag-and-drop path)
 * that doesn't need cancellation.
 *
 * @param project The project to import into.
 * @param path Path to the WAV file to import from.
 * @param snippetIndices Which of the source's snippets to import, in any
 *        order and with any duplicates ignored; an index at or beyond the
 *        source's actual snippet count is silently skipped, not an error.
 * @param errorMessage If non-null and this returns `0`, set to a
 *        human-readable description of what went wrong.
 * @return How many snippets were actually imported - `0` if the file
 *         couldn't be read, or nothing was actually imported (an empty
 *         `snippetIndices`, or every given index out of range).
 */
[[nodiscard]] int importAudioSnippetsInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                                           const std::vector<std::size_t>& snippetIndices,
                                           QString* errorMessage = nullptr);

/**
 * @brief Imports an image file as a new layer into `project`, resized per
 *        `mode`.
 *
 * The image is first resized to `project`'s canvas dimensions according
 * to `mode` (see `ImageScalePickerDialog::Mode`'s own docs), then its RGB
 * pixels are converted into amplitude/phase data via
 * `sound_mind::codec::fromRgbImage()`.
 *
 * @param project The project to import into.
 * @param path Path to the image file to import.
 * @param mode How to resize the image before importing it.
 * @param errorMessage If non-null and this returns `false`, set to a
 *        human-readable description of what went wrong.
 * @param outLayerId If non-null and this returns `true`, set to the
 *        newly added layer's own id - `Project::addLayer()`'s own return
 *        value, exposed here since the new layer's own position in the
 *        stack isn't predictable from the outside alone (it lands just
 *        below an Equalizer layer, if one exists - see `addLayer()`'s
 *        own docs - so it's never simply `project.layers().back()`).
 * @return `true` on success; `false` if loading or converting it failed.
 */
[[nodiscard]] bool importImageFileInto(sound_mind::core::Project& project, const std::filesystem::path& path,
                                        ImageScalePickerDialog::Mode mode, QString* errorMessage = nullptr,
                                        sound_mind::core::LayerId* outLayerId = nullptr);

/**
 * @brief Imports several image files at once into `project`.
 *
 * When `importAsSequence` is `false`, every path is imported
 * independently via importImageFileInto(), each with `mode` and no
 * translation. When `true`, `mode` is ignored entirely: every file is
 * imported with `ImageScalePickerDialog::Mode::ScaleVerticalProportional`,
 * sorted by path first (deterministic), and given a cumulative
 * `translationColumns()` so each layer starts immediately after the
 * previous one's own (proportional) width ends - wrapping back to `0`
 * once the running total reaches `project`'s own `canvasWidth`.
 *
 * A failure importing one file doesn't stop the rest.
 *
 * @param project The project to import into.
 * @param paths The image files to import.
 * @param mode How to resize each image - ignored when `importAsSequence`
 *        is `true`.
 * @param importAsSequence Whether to lay the files out end-to-end in time
 *        instead of importing each independently.
 * @param errorMessage If non-null and this returns `0`, set to the first
 *        failure's own message.
 * @return How many files were actually imported - `0` if `paths` is
 *         empty, or every file failed.
 */
[[nodiscard]] int importImageFilesInto(sound_mind::core::Project& project,
                                        const std::vector<std::filesystem::path>& paths,
                                        ImageScalePickerDialog::Mode mode, bool importAsSequence,
                                        QString* errorMessage = nullptr);

/**
 * @brief Exports `layer`'s audio to `path` (see
 *        `sound_mind::core::exportLayerAudio()` - Pool content when
 *        available, else a Stream-mode bounce).
 *
 * The compressed format (Flac/Ogg/Mp3) is inferred from `path`'s own
 * extension.
 *
 * @param layer The layer to export.
 * @param path Destination path; its extension selects the format.
 * @param errorMessage If non-null and this returns `false`, set to a
 *        human-readable description of what went wrong.
 * @return `true` on success; `false` if the extension didn't match a
 *         supported format, or the underlying codec export failed.
 */
[[nodiscard]] bool exportLayerAudioNow(const sound_mind::core::Layer& layer, const std::filesystem::path& path,
                                        QString* errorMessage = nullptr);

/**
 * @brief Exports `layer` as an MP4 video at `path` (see
 *        `sound_mind::core::exportLayerVideo()`): its rendered canvas
 *        animated with a playhead synced to its audio.
 *
 * @param layer The layer to export.
 * @param path Destination path.
 * @param canvasWidth The project's canvas width - passed through to
 *        `sound_mind::core::renderLayer()` via `exportLayerVideo()`.
 * @param errorMessage If non-null and this returns `false`, set to a
 *        human-readable description of what went wrong.
 * @return `true` on success; `false` if the underlying codec export
 *         failed.
 */
[[nodiscard]] bool exportLayerVideoNow(const sound_mind::core::Layer& layer, const std::filesystem::path& path,
                                        std::uint32_t canvasWidth, QString* errorMessage = nullptr);

}  // namespace sound_mind::studio
