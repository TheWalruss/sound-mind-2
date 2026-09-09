#pragma once

#include <QObject>

class LandingPageTest : public QObject {
    Q_OBJECT

private slots:
    void showsTheEmbeddedLogo();
    void newProjectButtonEmitsNewProjectRequested();
    void openProjectButtonEmitsOpenProjectRequested();
    void setRecentProjectsWithNoPathsShowsNoClickableEntries();
    void setRecentProjectsCreatesEntriesThatEmitTheirPath();
    void setRecentProjectsReplacesThePreviousEntries();
};
