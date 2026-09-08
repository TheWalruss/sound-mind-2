#include "test_recent_projects.h"

#include <fstream>

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "sound_mind/studio/recent_projects.h"

using sound_mind::studio::RecentProjects;

namespace {

/// @brief Creates an empty file at `path` - RecentProjects::list() filters
/// out paths that don't exist on disk, so tests need real (if empty) files
/// to exercise that filtering meaningfully.
void touchFile(const std::filesystem::path& path) {
    std::ofstream stream(path);
}

}  // namespace

void RecentProjectsTest::listIsEmptyForFreshSettings() {
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    RecentProjects recentProjects(settings);

    QVERIFY(recentProjects.list().empty());
}

void RecentProjectsTest::addThenListReturnsThatPath() {
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    RecentProjects recentProjects(settings);

    const auto path = std::filesystem::path(dir.filePath("a.smproj").toStdString());
    touchFile(path);

    recentProjects.add(path);

    const auto list = recentProjects.list();
    QCOMPARE(list.size(), static_cast<std::size_t>(1));
    QCOMPARE(list.front(), path);
}

void RecentProjectsTest::listFiltersOutPathsThatNoLongerExist() {
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    RecentProjects recentProjects(settings);

    const auto path = std::filesystem::path(dir.filePath("gone.smproj").toStdString());
    touchFile(path);
    recentProjects.add(path);
    QCOMPARE(recentProjects.list().size(), static_cast<std::size_t>(1));

    std::filesystem::remove(path);

    QVERIFY(recentProjects.list().empty());
}

void RecentProjectsTest::addMovesAnAlreadyPresentPathToTheFront() {
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    RecentProjects recentProjects(settings);

    const auto pathA = std::filesystem::path(dir.filePath("a.smproj").toStdString());
    const auto pathB = std::filesystem::path(dir.filePath("b.smproj").toStdString());
    touchFile(pathA);
    touchFile(pathB);

    recentProjects.add(pathA);
    recentProjects.add(pathB);
    recentProjects.add(pathA);  // re-add A - should move back to the front, not duplicate.

    const auto list = recentProjects.list();
    QCOMPARE(list.size(), static_cast<std::size_t>(2));
    QCOMPARE(list.at(0), pathA);
    QCOMPARE(list.at(1), pathB);
}

void RecentProjectsTest::addTrimsToMaxEntries() {
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    RecentProjects recentProjects(settings);

    std::vector<std::filesystem::path> paths;
    for (int i = 0; i < RecentProjects::kMaxEntries + 2; ++i) {
        auto path = std::filesystem::path(dir.filePath(QStringLiteral("p%1.smproj").arg(i)).toStdString());
        touchFile(path);
        paths.push_back(path);
        recentProjects.add(path);
    }

    const auto list = recentProjects.list();
    QCOMPARE(list.size(), static_cast<std::size_t>(RecentProjects::kMaxEntries));
    // Most recently added first; the two oldest (paths[0], paths[1]) should
    // have been dropped.
    QCOMPARE(list.front(), paths.back());
    for (const auto& dropped : {paths[0], paths[1]}) {
        QVERIFY(std::find(list.begin(), list.end(), dropped) == list.end());
    }
}
