#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>

#include "sound_mind/codec/audio_export.h"
#include "sound_mind/codec/video_options.h"
#include "sound_mind/core/macro.h"
#include "sound_mind/core/macro_renderer.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/background_job.h"
#include "sound_mind/studio/import_export.h"

class QPushButton;
class QStatusBar;

namespace sound_mind::studio {

/**
 * @brief The Studio's exports - audio, video and Macro video - in one place
 *        (`v0.1.48.2`, split out of `MainWindow`): the background job, its
 *        status-bar cancel button and progress, and what happens when it
 *        finishes.
 *
 * Each export renders a *copy* of the project (or, for a Macro, a project
 * the Macro's renderer rebuilds), so editing while it runs is safe. Only one
 * export runs at a time - the status bar has room for one message and one
 * cancel button. On success the status bar names the destination (and the
 * file is opened if asked); a cancel deletes the partial file; a failure is
 * reported through `Context::reportError` (a dialog in the Studio).
 *
 * Everything it needs from the window comes through a Context of callbacks,
 * so it can be driven - and tested - without a `MainWindow`.
 *
 * @note UI thread only; the exports themselves run on a BackgroundJob.
 */
class ExportController : public QObject {
    Q_OBJECT

public:
    /// @brief What the controller needs from its window.
    struct Context {
        /// @brief The open project, or `nullptr`.
        std::function<const sound_mind::core::Project*()> project;
        /// @brief The master limiter's ceiling, or `std::nullopt` when off.
        std::function<std::optional<float>()> limiterCeilingDb;
        /// @brief A renderer for a Macro, or `nullptr` with a reason.
        std::function<std::unique_ptr<sound_mind::core::MacroRenderer>(sound_mind::core::MacroId, QString*)>
            macroRenderer;
        /// @brief Shows a failure (title, message); a dialog in the Studio.
        std::function<void(const QString&, const QString&)> reportError;
    };

    /**
     * @brief Makes the controller and adds its cancel button to `statusBar`.
     * @param context The window's callbacks.
     * @param statusBar Where progress, results and the cancel button go.
     * @param parent The owning QObject.
     */
    ExportController(Context context, QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Cancels a running export and waits for it; its result is
    ///        dropped, not reported.
    ~ExportController() override;

    /**
     * @brief Starts exporting the whole project as a video (the composite
     *        canvas with a playhead, and the composite audio).
     *
     * Refuses (reportError) when no visible, unmuted, non-filter layer has
     * content; does nothing (a status message) while another export runs.
     * While it runs, the status bar shows the percentage done.
     * @param path Destination path.
     * @param options Format, size, frame rate, animation, playhead, polar.
     * @param openWhenDone Open the file in the system's player when done.
     */
    void exportVideoAsync(const std::filesystem::path& path, const sound_mind::codec::VideoExportOptions& options = {},
                          bool openWhenDone = false);

    /**
     * @brief Starts exporting the whole project's audio (what Playback
     *        hears, through the limiter if it's on), as exportVideoAsync().
     * @param path Destination path.
     * @param format FLAC, Ogg Vorbis or MP3.
     */
    void exportAudioAsync(const std::filesystem::path& path, sound_mind::codec::CompressedAudioFormat format);

    /**
     * @brief Starts exporting a recorded Macro as a video (`v0.1.44.2`): its
     *        renderer rebuilds the performance on a project of its own, so
     *        the open one is never touched.
     * @param path Destination path.
     * @param id The Macro.
     * @param options Format, size, frame rate, animation, playhead, polar.
     * @param openWhenDone Open the file when done.
     */
    void exportMacroVideoAsync(const std::filesystem::path& path, sound_mind::core::MacroId id,
                               const sound_mind::codec::VideoExportOptions& options = {}, bool openWhenDone = false);

    /**
     * @brief Starts exporting the canvas as an image (`v0.1.52.1`): every
     *        visible layer as the canvas shows it, at the chosen size and
     *        view, in the format the file's extension names - see
     *        exportProjectImageNow(). Refuses (reportError) when no visible
     *        layer has content; does nothing (a status message) while
     *        another export runs. Cancel stops it between layers.
     * @param path Destination path.
     * @param options Size, view and quality.
     * @param openWhenDone Open the image in the system's viewer when done.
     */
    void exportImageAsync(const std::filesystem::path& path, const ImageExportOptions& options,
                          bool openWhenDone = false);

    /// @brief Exports the project's audio synchronously - headless use and
    ///        tests.
    /// @param path Destination; its extension picks the format.
    /// @param errorMessage Receives the reason on failure; may be null.
    /// @return Whether it was written.
    bool exportAudioNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief Exports the project as a video synchronously - headless use
    ///        and tests.
    /// @param path Destination.
    /// @param errorMessage Receives the reason on failure; may be null.
    /// @return Whether it was written.
    bool exportVideoNow(const std::filesystem::path& path, QString* errorMessage = nullptr);

    /// @brief Whether an export of any kind is running.
    /// @return `true` from the start until its result has been reported.
    [[nodiscard]] bool isRunning() const noexcept { return job_.isRunning(); }

    /// @brief Cancels the running export; its partial file is deleted.
    void cancel() { job_.requestCancel(); }

    /// @brief Whether an export would produce anything: some visible,
    ///        unmuted, non-filter layer of `project` has content.
    /// @param project The project, or `nullptr`.
    /// @return `true` if so.
    [[nodiscard]] static bool hasExportableContent(const sound_mind::core::Project* project);

private:
    /// @brief Which kind of export is running - for its status messages.
    enum class Kind { Video, Audio, MacroVideo, Image };
    /// @brief How an export ended.
    enum class Outcome { Success, Cancelled, Failed };
    /// @brief What an export's background work reports.
    struct Result {
        Outcome outcome = Outcome::Success;  ///< How it ended.
        QString error;                       ///< Why it failed, if it did.
    };

    /// @brief Starts `work` on the job; exceptions become a result.
    void start(Kind kind, const std::filesystem::path& path, bool openWhenDone,
               std::function<Result(sound_mind::core::CancellationToken&)> work, const QString& startMessage);
    /// @brief The job's progress display (video kinds only).
    void showProgress();
    /// @brief Reports a finished export.
    void finish(Kind kind, const std::filesystem::path& path, bool openWhenDone, const Result& result);
    /// @brief The "already exporting" check; `true` (with a message) if busy.
    [[nodiscard]] bool refuseWhileRunning();

    Context context_;
    QStatusBar* statusBar_;
    QPushButton* cancelButton_ = nullptr;
    Kind kind_ = Kind::Video;
    /// @brief How far a running video export is, `0` to `1`; shared with
    ///        its worker so the worker never touches the controller.
    std::shared_ptr<std::atomic<double>> progress_ = std::make_shared<std::atomic<double>>(0.0);
    /// @brief Declared last, so it's cancelled and waited for first.
    BackgroundJob job_;
};

}  // namespace sound_mind::studio
