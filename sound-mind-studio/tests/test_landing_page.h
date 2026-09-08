#pragma once

#include <QObject>

class LandingPageTest : public QObject {
    Q_OBJECT

private slots:
    void newProjectButtonEmitsNewProjectRequested();
    void openProjectButtonEmitsOpenProjectRequested();
    void setRecentProjectsWithNoPathsShowsNoClickableEntries();
    void setRecentProjectsCreatesEntriesThatEmitTheirPath();
    void setRecentProjectsReplacesThePreviousEntries();
};
