#include "sound_mind/studio/recent_projects.h"

#include <algorithm>

#include <QSettings>
#include <QString>
#include <QStringList>

namespace sound_mind::studio {

namespace {
const char* kSettingsKey = "recentProjects";
}  // namespace

RecentProjects::RecentProjects(QSettings& settings) : settings_(settings) {}

void RecentProjects::add(const std::filesystem::path& path) {
    const QString entry = QString::fromStdString(path.string());

    QStringList entries = settings_.value(kSettingsKey).toStringList();
    entries.removeAll(entry);
    entries.prepend(entry);
    while (entries.size() > kMaxEntries) {
        entries.removeLast();
    }

    settings_.setValue(kSettingsKey, entries);
}

std::vector<std::filesystem::path> RecentProjects::list() const {
    const QStringList entries = settings_.value(kSettingsKey).toStringList();

    std::vector<std::filesystem::path> result;
    result.reserve(static_cast<std::size_t>(entries.size()));
    for (const QString& entry : entries) {
        std::filesystem::path path(entry.toStdString());
        if (std::filesystem::exists(path)) {
            result.push_back(std::move(path));
        }
    }
    return result;
}

}  // namespace sound_mind::studio
