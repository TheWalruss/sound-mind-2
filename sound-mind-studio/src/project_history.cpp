#include "sound_mind/studio/project_history.h"

namespace sound_mind::studio {

ProjectHistory::ProjectHistory(QObject* parent) : QObject(parent) {}

void ProjectHistory::setProject(sound_mind::core::Project* project) {
    project_ = project;
    // Whatever was done to the project before it was handed over isn't
    // anybody's action to undo.
    if (project_ != nullptr && !project_->history().hasBaseline()) {
        project_->history().setBaseline(*project_);
    } else if (project_ != nullptr && project_->history().hasUncommittedChanges(*project_)) {
        // Changed outside the history: its entries no longer rebuild it.
        project_->history().setBaseline(*project_);
        project_->history().markIncomplete();
    }
    emit historyChanged();
}

std::optional<sound_mind::core::HistoryEntryId> ProjectHistory::commit(sound_mind::core::Project& project,
                                                                       const QString& description,
                                                                       nlohmann::json metadata) {
    project_ = &project;
    const auto id = project.history().commit(project, description.toStdString(), std::move(metadata));
    if (id.has_value()) {
        emit entryCommitted(*id);
        emit historyChanged();
    }
    return id;
}

bool ProjectHistory::canUndo() const noexcept {
    return project_ != nullptr && (project_->history().canUndo() || project_->history().hasUncommittedChanges(*project_));
}

bool ProjectHistory::canRedo() const noexcept { return project_ != nullptr && project_->history().canRedo(); }

bool ProjectHistory::undo() {
    if (project_ == nullptr) {
        return false;
    }
    return finish(project_->history().undo(*project_, resolver_));
}

bool ProjectHistory::redo() {
    if (project_ == nullptr) {
        return false;
    }
    return finish(project_->history().redo(*project_, resolver_));
}

bool ProjectHistory::jumpTo(std::size_t index) {
    if (project_ == nullptr) {
        return false;
    }
    return finish(project_->history().jumpTo(*project_, index, resolver_));
}

bool ProjectHistory::finish(const std::optional<sound_mind::core::HistoryChange>& change) {
    if (!change.has_value()) {
        emit historyChanged();
        return false;
    }
    for (const auto& rebuild : contentRebuilders_) {
        rebuild(*change);
    }
    emit changed(*change);
    emit historyChanged();
    return true;
}

std::size_t ProjectHistory::count() const noexcept { return project_ != nullptr ? project_->history().size() : 0; }

std::size_t ProjectHistory::currentIndex() const noexcept {
    return project_ != nullptr ? project_->history().position() : 0;
}

QString ProjectHistory::descriptionAt(std::size_t index) const {
    return project_ != nullptr ? QString::fromStdString(project_->history().entry(index).description) : QString();
}

void ProjectHistory::addContentRebuilder(std::function<void(const sound_mind::core::HistoryChange&)> rebuilder) {
    contentRebuilders_.push_back(std::move(rebuilder));
}

void ProjectHistory::setBaseResolver(sound_mind::core::HistoryBaseResolver resolver) { resolver_ = std::move(resolver); }

}  // namespace sound_mind::studio
