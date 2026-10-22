#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include <nlohmann/json.hpp>

#include "sound_mind/core/project.h"
#include "sound_mind/studio/audio_snippet_picker_dialog.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/image_scale_picker_dialog.h"
#include "sound_mind/studio/import_export.h"
#include "sound_mind/studio/midi_import.h"
#include "sound_mind/studio/project_change.h"

class QPushButton;
class QStatusBar;

namespace sound_mind::studio {

/**
 * @brief The Studio's imports - audio (in the background, or at once),
 *        images and MIDI - in one place (`v0.1.48.3`, split out of
 *        `MainWindow`): the import job and its cancel button, adding the
 *        results to the project with one History entry, and reporting.
 *
 * The heavy lifting is `import_export.h`/`midi_import.h`'s; this adds the
 * status bar, the background job, the project's source registration and
 * the change notification (`Context::applyChange`). The file pickers and
 * option dialogs stay with the window's menus.
 *
 * A background audio import encodes from a copy of the project settings
 * and only touches the project when it's done, on the UI thread - so a
 * cancel has nothing to undo.
 *
 * @note UI thread only; the background import runs on a BackgroundJob.
 */
class ImportController : public QObject {
    Q_OBJECT

public:
    /// @brief What the controller needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<sound_mind::core::Project*()> project;
        /// @brief Commits a History entry (description, metadata).
        std::function<void(const QString&, nlohmann::json)> commitHistory;
        /// @brief Replays a layer's operations onto its content (MIDI's
        ///        notes are operations).
        std::function<void(sound_mind::core::LayerId)> rebuildLayerContent;
        /// @brief Brings the views up to date - `MainWindow::applyProjectChange()`.
        std::function<void(const ProjectChange&)> applyChange;
        /// @brief Shows a failure (title, message); a dialog in the Studio.
        std::function<void(const QString&, const QString&)> reportError;
    };

    /**
     * @brief Makes the controller and adds its cancel button to `statusBar`.
     * @param context The window's callbacks.
     * @param statusBar Where progress, results and the cancel button go.
     * @param parent The owning QObject.
     */
    ImportController(Context context, QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Cancels a running import and waits; its result is dropped.
    ~ImportController() override;

    /// @brief The snippets `path` splits into, for the snippet picker.
    /// @param path The audio file.
    /// @param errorMessage Receives the reason on failure; may be null.
    /// @param offsetSeconds Where the first snippet starts.
    /// @return The rows; empty on failure or with no project.
    [[nodiscard]] std::vector<AudioSnippetPickerDialog::RowData> audioSnippetsForFile(
        const std::filesystem::path& path, QString* errorMessage = nullptr, double offsetSeconds = 0.0) const;

    /**
     * @brief Imports the chosen snippets of an audio file at once, each as
     *        a new layer.
     * @param path The audio file.
     * @param snippetIndices Which snippets.
     * @param errorMessage Receives the reason on failure; may be null.
     * @param offsetSeconds Where the first snippet starts.
     * @return Whether anything was imported.
     */
    bool importAudioSnippets(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                             QString* errorMessage = nullptr, double offsetSeconds = 0.0);

    /**
     * @brief The background version of importAudioSnippets() (finding #12):
     *        encodes on a BackgroundJob, copies the file into the project's
     *        sources, then adds the layers with one History entry. Cancel
     *        adds nothing. A no-op (status message) with no project or while
     *        another import runs.
     * @param path The audio file.
     * @param snippetIndices Which snippets.
     * @param offsetSeconds Where the first snippet starts.
     */
    void importAudioSnippetsAsync(const std::filesystem::path& path, const std::vector<std::size_t>& snippetIndices,
                                  double offsetSeconds = 0.0);

    /// @brief Whether a background audio import is running.
    /// @return `true` until its result has been applied.
    [[nodiscard]] bool isRunning() const noexcept { return job_.isRunning(); }

    /// @brief Cancels the background import; nothing is added.
    void cancel() { job_.requestCancel(); }

    /**
     * @brief Imports one image file as a layer, scaled as chosen.
     * @param path The image.
     * @param mode How to scale it onto the canvas.
     * @param errorMessage Receives the reason on failure; may be null.
     * @return Whether it was imported.
     */
    bool importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                         QString* errorMessage = nullptr);

    /**
     * @brief Imports image files as layers, scaled as chosen.
     * @param paths The images.
     * @param mode How to scale them onto the canvas.
     * @param importAsSequence One layer, side by side, instead of one each.
     * @param errorMessage Receives the reason on failure; may be null.
     * @param polarParams Unwrap a Sound Flower picture with these, if set.
     * @return Whether anything was imported.
     */
    bool importImageFiles(const std::vector<std::filesystem::path>& paths, ImageScalePickerDialog::Mode mode,
                          bool importAsSequence = false, QString* errorMessage = nullptr,
                          std::optional<PolarImportParams> polarParams = std::nullopt);

    /// @brief The channels and notes a MIDI file holds, for its dialog.
    /// @param path The MIDI file.
    /// @param errorMessage Receives the reason on failure; may be null.
    /// @return The preview, or `std::nullopt`.
    [[nodiscard]] std::optional<MidiImportPreview> midiPreviewForFile(const std::filesystem::path& path,
                                                                      QString* errorMessage = nullptr) const;

    /**
     * @brief Imports a MIDI file's channels as painted notes.
     * @param path The MIDI file.
     * @param separateLayerPerChannel One layer per channel, or one for all.
     * @param errorMessage Receives the reason on failure; may be null.
     * @param channelNumbers Which channels (empty: all).
     * @return Whether anything was imported.
     */
    bool importMidiFile(const std::filesystem::path& path, bool separateLayerPerChannel,
                        QString* errorMessage = nullptr, const std::vector<int>& channelNumbers = {});

    /**
     * @brief Imports chosen channels and snippets of a MIDI file.
     * @param path The MIDI file.
     * @param channelNumbers Which channels.
     * @param snippetIndices Which time snippets.
     * @param separateLayerPerChannel One layer per channel, or one for all.
     * @param errorMessage Receives the reason on failure; may be null.
     * @return Whether anything was imported.
     */
    bool importMidiSelection(const std::filesystem::path& path, const std::vector<int>& channelNumbers,
                             const std::vector<std::size_t>& snippetIndices, bool separateLayerPerChannel,
                             QString* errorMessage = nullptr);

private:
    /// @brief How a background import ended.
    enum class Outcome { Success, Cancelled, Failed };
    /// @brief What a background audio import produced.
    struct Result {
        Outcome outcome = Outcome::Success;                     ///< How it ended.
        QString error;                                           ///< Why it failed, if it did.
        std::vector<sound_mind::core::Layer> layers;             ///< The encoded layers, not yet added.
        std::optional<sound_mind::core::ProjectSource> source;  ///< The copied file, not yet registered.
    };

    /// @brief Applies a finished background audio import.
    void finish(const std::filesystem::path& path, Result result);
    /// @brief Adds MIDI layers' notes, the source and the History entry.
    bool finishMidi(const std::vector<sound_mind::core::LayerId>& newLayerIds, const std::filesystem::path& path);
    /// @brief Shows `message` and repaints at once (the import blocks).
    void busy(const QString& message);
    /// @brief Shows `message` for a few seconds.
    void say(const QString& message);
    /// @brief `true` (and the reason) with no project open.
    bool noProject(QString* errorMessage) const;

    Context context_;
    QStatusBar* statusBar_;
    QPushButton* cancelButton_ = nullptr;
    /// @brief Declared last, so it's cancelled and waited for first.
    BackgroundJob job_;
};

}  // namespace sound_mind::studio
