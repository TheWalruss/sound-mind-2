#include "sound_mind/studio/playback_preparer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QPushButton>
#include <QStatusBar>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/loudness_meter.h"

namespace sound_mind::studio {

namespace {

/// Composites and decodes `project`; the audio, its loudness, and whether
/// the composite was cancelled or failed (with the reason).
struct Prepared {
    std::optional<sound_mind::codec::AudioBuffer> audio;
    double loudnessLufs = -70.0;
    bool cancelled = false;
    QString error;
};

Prepared prepare(const sound_mind::core::Project& project, sound_mind::core::CancellationToken& token,
                 bool measureLoudness) {
    Prepared prepared;
    try {
        const auto composite = sound_mind::core::compositeProject(
            project, [&token]() { return token.cancellationRequested(); }, /*respectMute=*/true);
        if (composite.has_value() && !token.cancellationRequested()) {
            prepared.audio = sound_mind::codec::decode(*composite);  // decoded off the UI thread.
            if (measureLoudness) {
                prepared.loudnessLufs = sound_mind::core::integratedLoudnessLufs(*prepared.audio);
            }
        }
    } catch (const sound_mind::core::CompositeCancelled&) {
        prepared.cancelled = true;
    } catch (const std::exception& e) {
        prepared.error = QString::fromStdString(e.what());
    }
    return prepared;
}

}  // namespace

PlaybackPreparer::PlaybackPreparer(Context context, PlaybackController* playback, QStatusBar* statusBar,
                                   QObject* parent)
    : QObject(parent), context_(std::move(context)), playback_(playback), statusBar_(statusBar) {
    cancelButton_ = new QPushButton(tr("✕"));
    cancelButton_->setObjectName(QStringLiteral("compositeCancelButton"));
    cancelButton_->setToolTip(tr("Cancel"));
    cancelButton_->setFixedWidth(24);
    cancelButton_->hide();
    if (statusBar_ != nullptr) {
        statusBar_->addPermanentWidget(cancelButton_);
    }
    compositeJob_.setCancelButton(cancelButton_);
}

PlaybackPreparer::~PlaybackPreparer() {
    // The jobs' own destructors wait and drop the completions.
    compositeJob_.requestCancel();
    refreshJob_.requestCancel();
    editJob_.requestCancel();
}

void PlaybackPreparer::prepareAndPlay() {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return;
    }
    if (isCompositing()) {
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(tr("Already preparing playback - wait for it to finish, or cancel it, first."),
                                    5000);
        }
        return;
    }
    // Cheap synchronous pre-check: is there anything to composite at all?
    // The no-content case stays an instant no-op.
    const auto& layers = project->layers();
    const bool anyContributor = std::any_of(layers.begin(), layers.end(), [](const sound_mind::core::Layer& layer) {
        return layer.visible() && !layer.muted() && layer.content().has_value() &&
               !sound_mind::core::isFilterLayerType(layer.type());
    });
    if (!anyContributor) {
        return;
    }
    compositeJob_.start([this, snapshot = project->renderSnapshot()](
                            sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto prepared = std::make_shared<Prepared>(prepare(snapshot, token, /*measureLoudness=*/true));
        return [this, prepared](bool cancelled) {
            finishComposite(cancelled || prepared->cancelled, prepared->error, std::move(prepared->audio),
                            prepared->loudnessLufs);
        };
    });
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Preparing playback..."));
    }
}

void PlaybackPreparer::finishComposite(bool cancelled, const QString& error,
                                       std::optional<sound_mind::codec::AudioBuffer> audio, double loudnessLufs) {
    if (cancelled) {
        // Nothing to roll back - the playback was never touched.
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(tr("Playback preparation cancelled."), 5000);
        }
        return;
    }
    if (!error.isEmpty()) {
        if (statusBar_ != nullptr) {
            statusBar_->clearMessage();
        }
        context_.reportError(tr("Playback Failed"), error);
        return;
    }
    if (!audio.has_value()) {
        // The pre-check found a contributor; kept as a safe fallback.
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(tr("Nothing to play."), 5000);
        }
        return;
    }
    context_.loadedLoudness(loudnessLufs);
    playback_->load(std::move(*audio));
    // A fresh load plays the whole track, whatever the scope - narrowed only
    // once an edit arrives (contentChanged()).
    wholeTrackRange();
    syncRange();
    playback_->play();
    context_.started();
    if (statusBar_ != nullptr) {
        statusBar_->clearMessage();
    }
}

void PlaybackPreparer::scheduleRepeatRefresh() {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr || !repeat_ || !playback_->isPlaying()) {
        return;
    }
    if (refreshJob_.isRunning()) {
        refreshDirty_ = true;
        return;
    }
    refreshDirty_ = false;
    refreshJob_.start([this, snapshot = project->renderSnapshot()](
                          sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto prepared = std::make_shared<Prepared>(prepare(snapshot, token, /*measureLoudness=*/true));
        return [this, prepared](bool) { finishRefresh(std::move(prepared->audio), prepared->loudnessLufs); };
    });
}

void PlaybackPreparer::finishRefresh(std::optional<sound_mind::codec::AudioBuffer> audio, double loudnessLufs) {
    if (refreshDirty_) {
        refreshDirty_ = false;
        scheduleRepeatRefresh();
        return;
    }
    if (!audio.has_value() || !repeat_ || !playback_->isPlaying()) {
        return;  // failed, or no longer wanted: playback keeps the older audio.
    }
    context_.loadedLoudness(loudnessLufs);
    if (playback_->stageAudio(std::move(*audio))) {
        return;  // swapped in seamlessly at the loop-back.
    }
    // Length or sample rate changed: reload in place at the same position
    // (stageAudio() left `audio` intact).
    playback_->load(std::move(*audio));
    if (scope_ == PlaybackScope::Track) {
        wholeTrackRange();
    }
    syncRange();
    playback_->seek(context_.positionSeconds());
    playback_->play();
}

void PlaybackPreparer::contentChanged(sound_mind::core::LayerId layer) {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return;
    }
    // Repeat on only reacts while a session is playing; Track then refreshes
    // in place. With Repeat off, Delta/Review start a one-shot preview of the
    // edit by themselves (docs/sound-mind-design.md's "Repeat Playback");
    // Track does nothing.
    const bool deltaOrReview = scope_ == PlaybackScope::Delta || scope_ == PlaybackScope::Review;
    if (repeat_) {
        if (!playback_->isPlaying()) {
            return;
        }
        if (scope_ == PlaybackScope::Track) {
            scheduleRepeatRefresh();
            return;
        }
    } else if (!deltaOrReview) {
        return;
    }
    // The edited region: the layer's newest active operation - exact for a
    // fresh stroke, fill, paste or stamp, and the closest stand-in for an
    // undo/redo.
    const auto operations = project->operationLog().activeOperationsTargeting(layer);
    pendingEditBounds_ = operations.empty() ? std::nullopt
                                            : std::optional<sound_mind::core::TimeFrequencyRect>(operations.back()->bounds());
    startEditPreparation();  // in the background (v0.1.47.1).
}

void PlaybackPreparer::startEditPreparation() {
    const sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return;
    }
    if (editJob_.isRunning()) {
        editDirty_ = true;
        editJob_.requestCancel();
        return;
    }
    editDirty_ = false;
    editJob_.start([this, snapshot = project->renderSnapshot()](
                       sound_mind::core::CancellationToken& token) -> BackgroundJob::Completion {
        auto prepared = std::make_shared<Prepared>(prepare(snapshot, token, /*measureLoudness=*/false));
        return [this, prepared](bool cancelled) {
            if (editDirty_) {
                startEditPreparation();  // a newer edit arrived: it wins.
                return;
            }
            if (cancelled || !prepared->audio.has_value() || context_.project() == nullptr) {
                return;
            }
            playEdit(std::move(*prepared->audio));
        };
    });
}

void PlaybackPreparer::playEdit(sound_mind::codec::AudioBuffer audio) {
    // load() stops playback first - the confirmed "audible restart on edit".
    playback_->load(std::move(audio));
    const double totalSeconds = playback_->totalSeconds();
    switch (scope_) {
        case PlaybackScope::Track:
            wholeTrackRange();
            // The range reaches the engine before the seek: a fresh load()
            // reset it to "no restriction".
            syncRange();
            playback_->seek(context_.positionSeconds());
            break;
        case PlaybackScope::Delta:
            rangeStartSeconds_ = pendingEditBounds_ ? pendingEditBounds_->startTimeSeconds : 0.0;
            rangeEndSeconds_ = pendingEditBounds_ ? pendingEditBounds_->endTimeSeconds : totalSeconds;
            loopBackSeconds_ = rangeStartSeconds_;
            syncRange();
            playback_->seek(rangeStartSeconds_);
            break;
        case PlaybackScope::Review:
            rangeStartSeconds_ = pendingEditBounds_ ? pendingEditBounds_->startTimeSeconds : 0.0;
            rangeEndSeconds_ = totalSeconds;
            loopBackSeconds_ = 0.0;  // Review loops the whole track, back to its start.
            syncRange();
            playback_->seek(rangeStartSeconds_);
            break;
    }
    playback_->play();
}

void PlaybackPreparer::setRepeat(bool enabled) {
    repeat_ = enabled;
    if (!enabled && playback_->isLoaded()) {
        wholeTrackRange();  // nothing of a Delta/Review range lingers once Repeat is off.
    }
    syncRange();  // also what carries Repeat itself down to the engine.
}

void PlaybackPreparer::widenToWholeTrack() {
    wholeTrackRange();
    syncRange();
}

void PlaybackPreparer::projectChanged() {
    editDirty_ = false;
    editJob_.requestCancel();  // its completion drops the old project's audio.
    pendingEditBounds_.reset();
}

void PlaybackPreparer::wholeTrackRange() {
    rangeStartSeconds_ = 0.0;
    rangeEndSeconds_ = playback_->totalSeconds();
    loopBackSeconds_ = 0.0;
}

void PlaybackPreparer::syncRange() {
    // An empty range (a single-click stroke's zero-width bounds) means
    // nothing to loop over: no restriction at all for that edit.
    if (rangeEndSeconds_ <= rangeStartSeconds_) {
        playback_->setPlaybackRange(/*loopEnabled=*/false, playback_->totalSeconds(), /*loopBackSeconds=*/0.0);
        return;
    }
    playback_->setPlaybackRange(repeat_, rangeEndSeconds_, loopBackSeconds_);
}

}  // namespace sound_mind::studio
