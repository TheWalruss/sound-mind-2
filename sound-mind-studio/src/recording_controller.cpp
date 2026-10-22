#include "sound_mind/studio/recording_controller.h"

#include <algorithm>
#include <exception>
#include <utility>
#include <vector>

#include <QStatusBar>

#include <nlohmann/json.hpp>

#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/project_sources.h"
#include "sound_mind/studio/record_panel.h"

namespace sound_mind::studio {

RecordingController::RecordingController(Context context, sound_mind::core::RecordEngine& engine, RecordPanel* panel,
                                         QStatusBar* statusBar, QObject* parent)
    : QObject(parent), context_(std::move(context)), engine_(engine), panel_(panel), statusBar_(statusBar) {
    drainTimer_.setInterval(33);  // also moves the playback position bar.
    connect(&drainTimer_, &QTimer::timeout, this, &RecordingController::drain);
    connect(panel_, &RecordPanel::toggleRequested, this, &RecordingController::toggle);
    connect(panel_, &RecordPanel::liveViewToggled, this, [this](bool enabled) {
        liveView_ = enabled;
        if (!enabled && engine_.isRecording()) {
            context_.setLiveOverlay(std::nullopt);
        }
    });
}

RecordingController::~RecordingController() {
    // saveJob_'s own destructor waits for the work and drops its completion.
    saveJob_.requestCancel();
}

void RecordingController::toggle() {
    if (engine_.isRecording()) {
        stopCapture();
        commitAsync(engine_.capturedAudio());
        return;
    }
    if (context_.project() == nullptr || !context_.mayStart()) {
        // Refuse rather than surprise-stop a running Loop Mode session -
        // both would otherwise want the same input device at once.
        return;
    }
    context_.aboutToStart();
    engine_.start();
    panel_->setRecording(true);
    context_.recordingChanged();
    if (statusBar_ != nullptr) {
        if (!engine_.isDeviceAvailable()) {
            statusBar_->showMessage(
                tr("Recording started, but no input device could be opened - nothing will be captured. %1")
                    .arg(QString::fromStdString(engine_.deviceProblem())),
                15000);
        } else {
            statusBar_->showMessage(tr("Recording..."));
        }
    }
    drainTimer_.start();
}

void RecordingController::abandon() {
    drainTimer_.stop();
    engine_.stop();
    panel_->setRecording(false);
    liveEncoder_.reset();
    liveFed_ = 0;
}

void RecordingController::stopCapture() {
    drainTimer_.stop();
    engine_.stop();
    panel_->setRecording(false);
    context_.recordingChanged();
    context_.setPlayheadFraction(std::nullopt);
    context_.setLiveOverlay(std::nullopt);
    liveEncoder_.reset();
    liveFed_ = 0;
}

void RecordingController::drain() {
    engine_.drainAvailable();
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return;
    }
    const auto config = sound_mind::core::streamCodecConfigFor(project->settings());
    const sound_mind::codec::AudioBuffer& captured = engine_.capturedAudio();
    if (liveView_) {
        if (!liveEncoder_) {
            liveEncoder_ = std::make_unique<sound_mind::codec::StreamIncrementalEncoder>(config);
            liveFed_ = 0;
        }
        if (captured.frameCount() > liveFed_) {
            liveEncoder_->pushSamples(captured.left.data() + liveFed_, captured.right.data() + liveFed_,
                                      captured.frameCount() - liveFed_);
            liveFed_ = captured.frameCount();
        }
        context_.setLiveOverlay(liveEncoder_->snapshot());
    }
    // The playback position bar follows the recording, from the canvas's
    // own start, until it reaches the end.
    const double canvasSamples =
        static_cast<double>(project->settings().canvasWidth) * static_cast<double>(config.hopLength);
    const double recorded = static_cast<double>(captured.frameCount());
    context_.setPlayheadFraction(canvasSamples > 0.0 ? std::optional<double>(std::min(1.0, recorded / canvasSamples))
                                                      : std::nullopt);
}

bool RecordingController::commit(const sound_mind::codec::AudioBuffer& captured) {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr || captured.frameCount() == 0) {
        return false;
    }
    try {
        return apply(
            prepare(captured, sound_mind::core::streamCodecConfigFor(project->settings()), project->sourcesFolder()));
    } catch (const std::exception& e) {
        context_.reportError(tr("Record Failed"), QString::fromStdString(e.what()));
        return false;
    }
}

RecordingController::Prepared RecordingController::prepare(const sound_mind::codec::AudioBuffer& captured,
                                                           const sound_mind::codec::StreamCodecConfig& config,
                                                           const std::optional<std::filesystem::path>& sourcesFolder) {
    Prepared prepared;
    // Encoded exactly as an imported file would be.
    prepared.content = sound_mind::codec::encode(captured, config);
    if (sourcesFolder.has_value()) {
        try {
            prepared.source = sound_mind::core::storeSourceAudio(*sourcesFolder, captured, "recording",
                                                                 tr("Recording").toStdString());
        } catch (const std::exception&) {
            // Kept without a source: not replayable from the original.
        }
    }
    return prepared;
}

bool RecordingController::apply(Prepared prepared) {
    sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return false;
    }
    try {
        nlohmann::json recipe{{"kind", "record"}};
        if (prepared.source.has_value()) {
            recipe["source"] = project->addSource(std::move(*prepared.source));
        }
        const auto target = panel_->targetLayerId();
        sound_mind::core::Layer* existing = target.has_value() ? project->layerById(*target) : nullptr;
        QString message;
        if (existing != nullptr) {
            // Recording over a layer replaces it, painting included.
            existing->setBaseContent(std::move(prepared.content), std::move(recipe));
            project->operationLog().bakeLayer(existing->id());
            const QString name = QString::fromStdString(existing->name());
            context_.commitHistory(tr("Recorded over \"%1\"").arg(name));
            message = tr("Recording replaced \"%1\".").arg(name);
        } else {
            sound_mind::core::Layer layer(0, panel_->newLayerName().toStdString(), sound_mind::core::LayerType::Normal);
            layer.setBaseContent(std::move(prepared.content), std::move(recipe));
            layer.setRecorded(true);
            project->addLayer(std::move(layer));
            context_.commitHistory(tr("Recorded a new layer"));
            message = tr("Recording added as a new layer.");
        }
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(message, 5000);
        }
        context_.applyChange(ProjectChange::layersAdded());
        return true;
    } catch (const std::exception& e) {
        context_.reportError(tr("Record Failed"), QString::fromStdString(e.what()));
        return false;
    }
}

void RecordingController::commitAsync(sound_mind::codec::AudioBuffer captured) {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr || captured.frameCount() == 0) {
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(tr("Recording stopped - nothing captured."), 5000);
        }
        return;
    }
    flushSave();  // one at a time, in order.
    saveJob_.start(
        [audio = std::move(captured), config = sound_mind::core::streamCodecConfigFor(project->settings()),
         folder = project->sourcesFolder(), this](sound_mind::core::CancellationToken&) -> BackgroundJob::Completion {
            auto prepared = std::make_shared<Prepared>(prepare(audio, config, folder));
            return [this, prepared](bool) { apply(std::move(*prepared)); };
        },
        [this](const QString& error) { context_.reportError(tr("Record Failed"), error); });
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Saving the recording..."));
    }
}

void RecordingController::refreshTargets() {
    std::vector<std::pair<sound_mind::core::LayerId, QString>> targets;
    if (const sound_mind::core::Project* project = context_.project()) {
        for (const sound_mind::core::Layer& layer : project->layers()) {
            if (layer.recorded()) {
                targets.emplace_back(layer.id(), QString::fromStdString(layer.name()));
            }
        }
    }
    panel_->setRecordedLayers(targets);
}

}  // namespace sound_mind::studio
