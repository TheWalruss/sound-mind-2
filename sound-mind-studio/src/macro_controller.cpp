#include "sound_mind/studio/macro_controller.h"

#include <QStatusBar>

#include "sound_mind/studio/base_recipes.h"
#include "sound_mind/studio/macro_player.h"

namespace sound_mind::studio {

MacroController::MacroController(Context context, ProjectHistory* history, MacroPlayer* player, QStatusBar* statusBar,
                                 QObject* parent)
    : QObject(parent), context_(std::move(context)), player_(player), statusBar_(statusBar), recorder_(history) {
    playheadTimer_.setInterval(33);
    connect(&playheadTimer_, &QTimer::timeout, this, &MacroController::notePlayhead);
}

void MacroController::setRecordingEnabled(bool enabled) {
    sound_mind::core::Project* project = context_.project();
    if (project == nullptr) {
        return;
    }
    if (enabled) {
        if (recorder_.isRecording()) {
            return;
        }
        const QString name = tr("Macro %1").arg(project->macros().size() + 1);
        const auto [position, playing] = context_.playhead();
        recorder_.start(*project, name, position, playing);
        playheadTimer_.start();
        context_.applyChange(ProjectChange::settingsOnly());
        if (statusBar_ != nullptr) {
            statusBar_->showMessage(
                tr("Recording \"%1\" - start Playback or Loop; its clock runs while they do.").arg(name));
        }
    } else if (recorder_.isRecording()) {
        notePlayhead();
        const auto id = *recorder_.recordingMacro();
        recorder_.stop();
        playheadTimer_.stop();
        context_.applyChange(ProjectChange::settingsOnly());
        const sound_mind::core::Macro* macro = project->macroById(id);
        if (macro != nullptr && statusBar_ != nullptr) {
            statusBar_->showMessage(tr("Recorded \"%1\": %2 s, %3 event(s).")
                                        .arg(QString::fromStdString(macro->name))
                                        .arg(macro->durationSeconds, 0, 'f', 1)
                                        .arg(macro->events.size()),
                                    8000);
        }
    }
}

void MacroController::notePlayhead() {
    if (recorder_.isRecording()) {
        const auto [position, playing] = context_.playhead();
        recorder_.notePlayhead(position, playing);
    }
}

std::unique_ptr<sound_mind::core::MacroRenderer> MacroController::rendererFor(sound_mind::core::MacroId id,
                                                                              QString* errorMessage) const {
    sound_mind::core::Project* project = context_.project();  // macroById() has no const overload.
    const sound_mind::core::Macro* macro = project != nullptr ? project->macroById(id) : nullptr;
    if (macro == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("That macro no longer exists.");
        }
        return nullptr;
    }
    auto start = sound_mind::core::macroStartEntries(*project, *macro);
    if (!start.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("This macro was recorded in a project whose history doesn't go back to its creation "
                               "(made with a version before 0.1.43.1), so it can't be rebuilt.");
        }
        return nullptr;
    }
    sound_mind::core::MacroRenderOptions options;
    options.runRecipe = replayRecipeRunner(sourceLookupFor(*project));
    options.limiterCeilingDb = context_.limiterCeilingDb();
    return std::make_unique<sound_mind::core::MacroRenderer>(macro->settings, std::move(*start), *macro,
                                                             std::move(options));
}

bool MacroController::play(sound_mind::core::MacroId id, QString* errorMessage) {
    if (player_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Macros can't be played here.");
        }
        return false;
    }
    auto renderer = rendererFor(id, errorMessage);
    if (!renderer) {
        return false;
    }
    context_.aboutToPlay();
    player_->start(std::move(renderer));
    if (statusBar_ != nullptr) {
        statusBar_->showMessage(tr("Preparing the macro - it starts playing in a moment..."), 4000);
    }
    return true;
}

void MacroController::projectChanged() {
    recorder_.discard();
    playheadTimer_.stop();
    if (player_ != nullptr) {
        player_->stop();
    }
}

}  // namespace sound_mind::studio
