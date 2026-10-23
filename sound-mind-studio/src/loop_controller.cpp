#include "sound_mind/studio/loop_controller.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

#include <QStatusBar>

#include <nlohmann/json.hpp>

#include "sound_mind/core/compositor.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/project_sources.h"
#include "sound_mind/studio/loop_panel.h"

namespace sound_mind::studio {

LoopController::LoopController(Context context, LoopPanel* panel, QStatusBar* statusBar, QObject* parent)
    : QObject(parent), context_(std::move(context)), panel_(panel), statusBar_(statusBar) {
    updateTimer_.setInterval(33);
    connect(&updateTimer_, &QTimer::timeout, this, &LoopController::update);
    connect(panel_, &LoopPanel::toggleRequested, this, &LoopController::toggle);
    connect(panel_, &LoopPanel::keepLoopingChanged, this, &LoopController::setKeepLooping);
    connect(panel_, &LoopPanel::liveViewToggled, this, [this](bool enabled) {
        liveView_ = enabled;
        if (!enabled) {
            context_.setLiveOverlay(std::nullopt);
        }
    });
}

LoopController::~LoopController() {
    // backingJob_'s own destructor waits for the work and drops its completion.
    backingJob_.requestCancel();
}

void LoopController::setEngine(std::unique_ptr<sound_mind::core::LoopEngine> engine) {
    abandon();
    engine_ = std::move(engine);
}

bool LoopController::isRunning() const noexcept { return startPending_ || (engine_ && engine_->isRunning()); }

void LoopController::showStatus(const QString& message, int timeoutMs) {
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(message, timeoutMs);
    }
}

void LoopController::stopSession() {
    startPending_ = false;
    backingJob_.requestCancel();  // its completion drops the result.
    updateTimer_.stop();
    if (engine_) {
        engine_->stop();
    }
    layerId_.reset();
    panel_->setRunning(false);
}

void LoopController::abandon() { stopSession(); }

void LoopController::toggle() {
    if (!engine_) {
        // No project has been opened yet - the engine is made per project,
        // since a loop is the project's own duration.
        return;
    }
    if (isRunning()) {
        stopSession();
        context_.setPlayheadFraction(std::nullopt);
        context_.setLiveOverlay(std::nullopt);
        context_.runningChanged();
        showStatus(tr("Loop stopped."), 5000);
        return;
    }
    sound_mind::core::Project* project = context_.project();
    if (project == nullptr || !context_.mayStart()) {
        // Refuse rather than surprise-stop an in-progress recording - both
        // would want the same input device at once.
        return;
    }
    context_.aboutToStart();

    // Reuse an existing "Loop Input" layer (from an earlier session, or
    // reloaded from disk), so its last take stays visible and audible.
    const std::string loopInputName = tr("Loop Input").toStdString();
    const sound_mind::core::Layer* existing = nullptr;
    for (const sound_mind::core::Layer& candidate : project->layers()) {
        if (candidate.type() == sound_mind::core::LayerType::Normal && candidate.name() == loopInputName) {
            existing = &candidate;
            break;
        }
    }
    if (existing != nullptr) {
        layerId_ = existing->id();
    } else {
        sound_mind::core::Layer layer(0, loopInputName, sound_mind::core::LayerType::Normal);
        layer.setBaseContent(engine_->emptyImage(), nlohmann::json{{"kind", "empty"}});
        layerId_ = project->addLayer(std::move(layer));
        context_.commitHistory(tr("Added Loop Input layer"));
    }
    ProjectChange loopLayerAdded = ProjectChange::layersAdded();
    loopLayerAdded.audible = false;  // the loop prepares its own audio (scheduleBacking()).
    context_.applyChange(loopLayerAdded);

    startPending_ = true;
    panel_->setRunning(true);
    context_.runningChanged();
    showStatus(tr("Preparing loop..."));
    scheduleBacking();
}

void LoopController::projectEdited() {
    if (isRunning()) {
        scheduleBacking();
    }
}

void LoopController::scheduleBacking() {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr || !engine_ || !layerId_) {
        return;
    }
    if (backingJob_.isRunning()) {
        backingDirty_ = true;
        return;
    }
    backingDirty_ = false;

    sound_mind::core::Project projectCopy = project->renderSnapshot();
    if (sound_mind::core::Layer* loopLayer = projectCopy.layerById(*layerId_)) {
        loopLayer->setMuted(true);  // the engine mixes the live take in itself.
    }
    const std::size_t loopLength = engine_->loopLengthSamples();
    const std::uint32_t sampleRateHz = sound_mind::core::streamCodecConfigFor(project->settings()).sampleRateHz;
    backingJob_.start([this, projectCopy = std::move(projectCopy), loopLength, sampleRateHz](
                          sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto audio = std::make_shared<std::optional<sound_mind::codec::AudioBuffer>>();
        sound_mind::codec::AudioBuffer mix;
        mix.sampleRateHz = sampleRateHz;
        try {
            const auto composite = sound_mind::core::compositeProject(
                projectCopy, [&token]() { return token.cancellationRequested(); }, /*respectMute=*/true);
            if (composite.has_value()) {
                mix = sound_mind::codec::decode(*composite);
            }
            mix.sampleRateHz = sampleRateHz;
            mix.left.resize(loopLength, 0.0f);
            mix.right.resize(loopLength, 0.0f);
            *audio = std::move(mix);
        } catch (const std::exception&) {
            audio->reset();  // cancelled or failed: keep whatever the engine already plays.
        }
        return [this, audio](bool cancelled) { finishBacking(std::move(*audio), cancelled); };
    });
}

void LoopController::finishBacking(std::optional<sound_mind::codec::AudioBuffer> result, bool cancelled) {
    if (backingDirty_) {
        scheduleBacking();
        return;
    }
    if (cancelled || !result.has_value() || !engine_) {
        return;
    }
    engine_->setBackingAudio(std::move(*result));

    if (!startPending_) {
        return;
    }
    startPending_ = false;
    takesShown_ = 0;
    engine_->start();
    if (!engine_->isDeviceAvailable()) {
        showStatus(tr("Loop started, but no audio device could be opened - nothing will be heard or recorded. %1")
                       .arg(QString::fromStdString(engine_->deviceProblem())),
                   15000);
    } else {
        showStatus(tr("Looping..."));
    }
    update();
    updateTimer_.start();
}

void LoopController::setKeepLooping(bool keepLooping) {
    panel_->setKeepLoopingChecked(keepLooping);
    if (engine_) {
        engine_->setFrozen(keepLooping);
    }
}

void LoopController::update() {
    sound_mind::core::Project* project = context_.project();
    if (!layerId_ || !engine_ || project == nullptr) {
        return;
    }
    sound_mind::core::Layer* layer = project->layerById(*layerId_);
    if (layer == nullptr) {
        return;
    }

    // The take plays at the Loop layer's own gain and balance - the same
    // pan law the compositor uses (Layer::balance()) - and not at all while
    // the layer is hidden or muted.
    const float gain = (layer->visible() && !layer->muted()) ? layer->opacity() : 0.0f;
    const float balance = layer->balance();
    engine_->setTakeGains(gain * std::min(1.0f, 2.0f * (1.0f - balance)), gain * std::min(1.0f, 2.0f * balance));

    // Each finished take becomes the Loop layer's content once the engine's
    // worker has encoded it.
    const std::uint64_t takes = engine_->takesCompleted();
    if (takes != takesShown_) {
        sound_mind::codec::StreamImage image = engine_->currentImage();
        if (image.frameCount > 0) {
            // Each take replaces what the Loop layer is built on.
            nlohmann::json recipe{{"kind", "loopTake"}, {"take", takes}};
            const auto folder = project->sourcesFolder();
            const auto& takeAudio = engine_->currentTakeAudio();
            if (folder.has_value() && takeAudio.frameCount() > 0) {
                try {
                    recipe["source"] = project->addSource(sound_mind::core::storeSourceAudio(
                        *folder, takeAudio, "loopTake", tr("Loop take %1").arg(takes).toStdString()));
                } catch (const std::exception&) {
                    // Kept without a source: not replayable from the original.
                }
            }
            layer->setBaseContent(std::move(image), std::move(recipe));
            project->operationLog().bakeLayer(layer->id());
            takesShown_ = takes;
            context_.commitHistory(tr("Loop take %1").arg(takes));
            context_.takeApplied(*layerId_);
        }
    }

    if (liveView_) {
        context_.setLiveOverlay(engine_->currentPreviewImage());
    }

    const std::size_t length = engine_->loopLengthSamples();
    context_.setPlayheadFraction(static_cast<double>(engine_->playbackPositionSamples()) /
                                 static_cast<double>(std::max<std::size_t>(length, 1)));
    showStatus(engine_->frozen() ? tr("Looping (frozen)...") : tr("Looping..."));
}

}  // namespace sound_mind::studio
