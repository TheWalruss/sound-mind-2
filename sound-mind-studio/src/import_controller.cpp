#include "sound_mind/studio/import_controller.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QPushButton>
#include <QStatusBar>

#include "sound_mind/core/project_sources.h"

namespace sound_mind::studio {

ImportController::ImportController(Context context, QStatusBar* statusBar, QObject* parent)
    : QObject(parent), context_(std::move(context)), statusBar_(statusBar) {
    cancelButton_ = new QPushButton(tr("✕"));
    cancelButton_->setObjectName(QStringLiteral("importCancelButton"));
    cancelButton_->setToolTip(tr("Cancel"));
    cancelButton_->setFixedWidth(24);
    cancelButton_->hide();
    if (statusBar_ != nullptr) {
        statusBar_->addPermanentWidget(cancelButton_);
    }
    job_.setCancelButton(cancelButton_);
}

ImportController::~ImportController() {
    // job_'s own destructor waits for the work and drops its completion.
    job_.requestCancel();
}

void ImportController::busy(const QString& message) {
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(message);
        QCoreApplication::processEvents();  // shown before the blocking import starts.
    }
}

void ImportController::say(const QString& message) {
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(message, 5000);
    }
}

bool ImportController::noProject(QString* errorMessage) const {
    if (context_.project() != nullptr) {
        return false;
    }
    if (errorMessage != nullptr) {
        *errorMessage = tr("No project is open.");
    }
    return true;
}

std::vector<AudioSnippetPickerDialog::RowData> ImportController::audioSnippetsForFile(const std::filesystem::path& path,
                                                                                       QString* errorMessage,
                                                                                       double offsetSeconds) const {
    if (noProject(errorMessage)) {
        return {};
    }
    return sound_mind::studio::audioSnippetsForFile(*context_.project(), path, errorMessage, offsetSeconds);
}

bool ImportController::importAudioSnippets(const std::filesystem::path& path,
                                           const std::vector<std::size_t>& snippetIndices, QString* errorMessage,
                                           double offsetSeconds) {
    if (noProject(errorMessage)) {
        return false;
    }
    busy(tr("Importing audio..."));
    const int importedCount = importAudioSnippetsInto(*context_.project(), path, snippetIndices, errorMessage,
                                                      offsetSeconds);
    if (importedCount == 0) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    context_.applyChange(ProjectChange::layersAdded());
    say(tr("Imported %1 layer(s) from \"%2\".").arg(importedCount).arg(QString::fromStdString(path.filename().string())));
    return true;
}

void ImportController::importAudioSnippetsAsync(const std::filesystem::path& path,
                                                const std::vector<std::size_t>& snippetIndices, double offsetSeconds) {
    sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        say(tr("No project is open."));
        return;
    }
    if (isRunning()) {
        say(tr("Already importing - wait for it to finish, or cancel it, first."));
        return;
    }
    // Settings copied by value - the encode never touches the project, so
    // the background thread has nothing shared to synchronize against.
    job_.start([this, path, snippetIndices, offsetSeconds, settings = project->settings(),
                sourcesFolder = project->sourcesFolder()](
                   sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto result = std::make_shared<Result>();
        QString encodeError;
        try {
            result->layers = encodeAudioSnippets(
                settings, path, snippetIndices, [&token]() { return token.cancellationRequested(); }, &encodeError,
                offsetSeconds);
            if (result->layers.empty()) {
                result->outcome = Outcome::Failed;
                result->error = encodeError;
            } else if (sourcesFolder.has_value()) {
                // The file itself is kept in the project (v0.1.43.2), copied
                // here off the UI thread; registered on completion.
                try {
                    result->source = sound_mind::core::storeSourceFile(*sourcesFolder, path, "audio");
                } catch (const std::exception&) {
                    result->source.reset();  // the import stands, just not replayable.
                }
            }
        } catch (const ImportCancelled&) {
            result->outcome = Outcome::Cancelled;
        } catch (const std::exception& e) {
            result->outcome = Outcome::Failed;
            result->error = QString::fromStdString(e.what());
        }
        return [this, path, result](bool) { finish(path, std::move(*result)); };
    });
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Importing audio..."));
    }
}

void ImportController::finish(const std::filesystem::path& path, Result result) {
    sound_mind::core::Project* project = context_.project();
    switch (result.outcome) {
        case Outcome::Success: {
            if (project == nullptr) {
                return;  // the window refuses to switch projects while importing; defensive.
            }
            const auto importedCount = static_cast<int>(result.layers.size());
            if (result.source.has_value()) {
                const sound_mind::core::SourceId source = project->addSource(std::move(*result.source));
                for (auto& layer : result.layers) {
                    nlohmann::json recipe = layer.baseRecipe();
                    recipe["source"] = source;
                    layer.setBaseRecipe(std::move(recipe));
                }
            }
            for (auto& layer : result.layers) {
                project->addLayer(std::move(layer));
            }
            context_.commitHistory(tr("Imported \"%1\"").arg(QString::fromStdString(path.filename().string())), {});
            context_.applyChange(ProjectChange::layersAdded());
            say(tr("Imported %1 layer(s) from \"%2\".")
                    .arg(importedCount)
                    .arg(QString::fromStdString(path.filename().string())));
            break;
        }
        case Outcome::Cancelled:
            // Nothing to roll back - the encoded layers were never added
            // (finding #12's "cancel rolls back partial work").
            say(tr("Audio import cancelled."));
            break;
        case Outcome::Failed:
            if (statusBar_ != nullptr) {
                statusBar_->clearMessage();
            }
            context_.reportError(tr("Import Audio Failed"), result.error);
            break;
    }
}

bool ImportController::importImageFile(const std::filesystem::path& path, ImageScalePickerDialog::Mode mode,
                                       QString* errorMessage) {
    if (noProject(errorMessage)) {
        return false;
    }
    busy(tr("Importing image..."));
    if (!importImageFileInto(*context_.project(), path, mode, errorMessage)) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    context_.applyChange(ProjectChange::layersAdded());
    say(tr("Imported \"%1\".").arg(QString::fromStdString(path.filename().string())));
    return true;
}

bool ImportController::importImageFiles(const std::vector<std::filesystem::path>& paths,
                                        ImageScalePickerDialog::Mode mode, bool importAsSequence,
                                        QString* errorMessage, std::optional<PolarImportParams> polarParams) {
    if (noProject(errorMessage)) {
        return false;
    }
    busy(tr("Importing image..."));
    const int importedCount =
        importImageFilesInto(*context_.project(), paths, mode, importAsSequence, errorMessage, polarParams);
    if (importedCount == 0) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    context_.applyChange(ProjectChange::layersAdded());
    say(tr("Imported %1 file(s).").arg(importedCount));
    return true;
}

std::optional<MidiImportPreview> ImportController::midiPreviewForFile(const std::filesystem::path& path,
                                                                      QString* errorMessage) const {
    if (noProject(errorMessage)) {
        return std::nullopt;
    }
    return midiImportPreviewForFile(*context_.project(), path, errorMessage);
}

bool ImportController::importMidiFile(const std::filesystem::path& path, bool separateLayerPerChannel,
                                      QString* errorMessage, const std::vector<int>& channelNumbers) {
    if (noProject(errorMessage)) {
        return false;
    }
    busy(tr("Importing MIDI..."));
    const auto newLayerIds =
        importMidiChannelsInto(*context_.project(), path, separateLayerPerChannel, errorMessage, channelNumbers);
    return finishMidi(newLayerIds, path);
}

bool ImportController::importMidiSelection(const std::filesystem::path& path, const std::vector<int>& channelNumbers,
                                           const std::vector<std::size_t>& snippetIndices,
                                           bool separateLayerPerChannel, QString* errorMessage) {
    if (noProject(errorMessage)) {
        return false;
    }
    busy(tr("Importing MIDI..."));
    const auto newLayerIds = importMidiSelectionInto(*context_.project(), path, channelNumbers, snippetIndices,
                                                     separateLayerPerChannel, errorMessage);
    return finishMidi(newLayerIds, path);
}

bool ImportController::finishMidi(const std::vector<sound_mind::core::LayerId>& newLayerIds,
                                  const std::filesystem::path& path) {
    if (newLayerIds.empty()) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        return false;
    }
    for (const auto layerId : newLayerIds) {
        context_.rebuildLayerContent(layerId);
    }
    nlohmann::json metadata;
    if (const auto source = attachImportSource(*context_.project(), path, "midi", {})) {
        metadata = {{"source", *source}};
    }
    context_.commitHistory(tr("Imported MIDI \"%1\"").arg(QString::fromStdString(path.filename().string())),
                           std::move(metadata));
    context_.applyChange(ProjectChange::layersAdded());
    say(tr("Imported \"%1\".").arg(QString::fromStdString(path.filename().string())));
    return true;
}

}  // namespace sound_mind::studio
