#include "sound_mind/studio/export_controller.h"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <utility>

#include <QDesktopServices>
#include <QPushButton>
#include <QStatusBar>
#include <QUrl>

#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/layer.h"
#include "sound_mind/core/project_export.h"
#include "sound_mind/studio/import_export.h"

namespace sound_mind::studio {

ExportController::ExportController(Context context, QStatusBar* statusBar, QObject* parent)
    : QObject(parent), context_(std::move(context)), statusBar_(statusBar) {
    cancelButton_ = new QPushButton(tr("✕"));
    cancelButton_->setObjectName(QStringLiteral("exportCancelButton"));
    cancelButton_->setToolTip(tr("Cancel"));
    cancelButton_->setFixedWidth(24);
    cancelButton_->hide();
    if (statusBar_ != nullptr) {
        statusBar_->addPermanentWidget(cancelButton_);
    }
    job_.setCancelButton(cancelButton_);
    connect(&job_, &BackgroundJob::stillRunning, this, &ExportController::showProgress);
}

ExportController::~ExportController() {
    // Its completion (reporting to the status bar) is dropped: job_'s own
    // destructor waits for the work without running it.
    job_.requestCancel();
}

bool ExportController::hasExportableContent(const sound_mind::core::Project* project) {
    if (project == nullptr) {
        return false;
    }
    const auto& layers = project->layers();
    return std::any_of(layers.begin(), layers.end(), [](const sound_mind::core::Layer& layer) {
        return layer.visible() && !layer.muted() && layer.content().has_value() &&
               !sound_mind::core::isFilterLayerType(layer.type());
    });
}

bool ExportController::refuseWhileRunning() {
    if (!isRunning()) {
        return false;
    }
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Already exporting - wait for it to finish, or cancel it, first."), 5000);
    }
    return true;
}

void ExportController::exportVideoAsync(const std::filesystem::path& path,
                                        const sound_mind::codec::VideoExportOptions& options, bool openWhenDone) {
    if (refuseWhileRunning()) {
        return;
    }
    const sound_mind::core::Project* project = context_.project();
    if (!hasExportableContent(project)) {
        context_.reportError(tr("Export Video Failed"), tr("Nothing to export - no visible, unmuted layer has content."));
        return;
    }
    // A copy of the project - later edits on the UI thread can't race it.
    start(
        Kind::Video, path, openWhenDone,
        [path, options, projectCopy = *project, limiterCeiling = context_.limiterCeilingDb(),
         progress = progress_](sound_mind::core::CancellationToken& token) -> Result {
            const auto shouldCancel = [&token]() { return token.cancellationRequested(); };
            if (!sound_mind::core::exportProjectVideo(projectCopy, path, options, shouldCancel, limiterCeiling,
                                                      [progress](double done) { *progress = done; })) {
                return {Outcome::Failed, tr("Nothing to export - no visible, unmuted layer has content.")};
            }
            return {};
        },
        tr("Exporting video..."));
}

void ExportController::exportAudioAsync(const std::filesystem::path& path,
                                        sound_mind::codec::CompressedAudioFormat format) {
    if (refuseWhileRunning()) {
        return;
    }
    const sound_mind::core::Project* project = context_.project();
    if (!hasExportableContent(project)) {
        context_.reportError(tr("Export Audio Failed"), tr("Nothing to export - no visible, unmuted layer has content."));
        return;
    }
    start(
        Kind::Audio, path, /*openWhenDone=*/false,
        [path, format, projectCopy = *project,
         limiterCeiling = context_.limiterCeilingDb()](sound_mind::core::CancellationToken& token) -> Result {
            const auto shouldCancel = [&token]() { return token.cancellationRequested(); };
            if (!sound_mind::core::exportProjectAudio(projectCopy, path, format, shouldCancel, limiterCeiling)) {
                return {Outcome::Failed, tr("Nothing to export - no visible, unmuted layer has content.")};
            }
            return {};
        },
        tr("Exporting audio..."));
}

void ExportController::exportMacroVideoAsync(const std::filesystem::path& path, sound_mind::core::MacroId id,
                                             const sound_mind::codec::VideoExportOptions& options, bool openWhenDone) {
    if (refuseWhileRunning()) {
        return;
    }
    QString error;
    std::shared_ptr<sound_mind::core::MacroRenderer> renderer = context_.macroRenderer(id, &error);
    if (!renderer) {
        context_.reportError(tr("Export Macro Video Failed"), error);
        return;
    }
    const double macroSeconds = std::max(renderer->durationSeconds(), 1e-9);
    start(
        Kind::MacroVideo, path, openWhenDone,
        [path, renderer, options, macroSeconds, progress = progress_](sound_mind::core::CancellationToken& token) -> Result {
            // Rendered on a project of the renderer's own: the open one is
            // never touched (v0.1.44.2).
            renderer->setShouldCancel([&token]() { return token.cancellationRequested(); });
            std::string failure;
            const bool written =
                renderer->prepare(&failure) &&
                sound_mind::core::exportMacroVideo(
                    *renderer, path, options, [progress, macroSeconds](double seconds) { *progress = seconds / macroSeconds; },
                    &failure);
            if (token.cancellationRequested()) {
                return {Outcome::Cancelled, {}};
            }
            if (!written) {
                return {Outcome::Failed, QString::fromStdString(failure)};
            }
            return {};
        },
        tr("Exporting macro video..."));
}

bool ExportController::exportAudioNow(const std::filesystem::path& path, QString* errorMessage) {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Exporting audio..."));
    }
    if (!exportProjectAudioNow(*project, path, errorMessage)) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Exported audio to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
    }
    return true;
}

bool ExportController::exportVideoNow(const std::filesystem::path& path, QString* errorMessage) {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No project is open.");
        }
        return false;
    }
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Exporting video..."));
    }
    if (!exportProjectVideoNow(*project, path, errorMessage)) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Exported video to \"%1\".").arg(QString::fromStdString(path.string())), 5000);
    }
    return true;
}

void ExportController::start(Kind kind, const std::filesystem::path& path, bool openWhenDone,
                             std::function<Result(sound_mind::core::CancellationToken&)> work,
                             const QString& startMessage) {
    kind_ = kind;
    *progress_ = 0.0;
    job_.start([this, kind, path, openWhenDone,
                work = std::move(work)](sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        Result result;
        try {
            result = work(token);
        } catch (const sound_mind::codec::ExportCancelled&) {
            result = {Outcome::Cancelled, {}};
        } catch (const sound_mind::core::CompositeCancelled&) {
            result = {Outcome::Cancelled, {}};
        } catch (const std::exception& e) {
            result = {Outcome::Failed, QString::fromStdString(e.what())};
        }
        return [this, kind, path, openWhenDone, result](bool) { finish(kind, path, openWhenDone, result); };
    });
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(startMessage);
    }
}

void ExportController::showProgress() {
    if (kind_ != Kind::Audio && statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Exporting %1... %2%")
                                    .arg(kind_ == Kind::Video ? tr("video") : tr("macro video"))
                                    .arg(static_cast<int>(std::lround(progress_->load() * 100.0))));
    }
}

void ExportController::finish(Kind kind, const std::filesystem::path& path, bool openWhenDone, const Result& result) {
    const QString kindLabel = kind == Kind::Video    ? tr("video")
                              : kind == Kind::Audio ? tr("audio")
                                                    : tr("macro video");
    switch (result.outcome) {
        case Outcome::Success:
            if (statusBar_ != nullptr) {
                statusBar_->showMessage(
                    tr("Exported %1 to \"%2\".").arg(kindLabel, QString::fromStdWString(path.wstring())), 5000);
            }
            if (openWhenDone && kind != Kind::Audio) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(path.wstring())));
            }
            break;
        case Outcome::Cancelled: {
            // Rolls back the partial file ExportCancelled leaves behind,
            // rather than leaving a broken file at the path the user chose.
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            if (statusBar_ != nullptr) {
                statusBar_->showMessage(tr("%1 export cancelled.")
                                            .arg(kind == Kind::Video    ? tr("Video")
                                                 : kind == Kind::Audio ? tr("Audio")
                                                                       : tr("Macro video")),
                                        5000);
            }
            break;
        }
        case Outcome::Failed: {
            if (statusBar_ != nullptr) {
                statusBar_->clearMessage();
            }
            context_.reportError(kind == Kind::Video    ? tr("Export Video Failed")
                                 : kind == Kind::Audio ? tr("Export Audio Failed")
                                                       : tr("Export Macro Video Failed"),
                                 result.error);
            break;
        }
    }
}

}  // namespace sound_mind::studio
