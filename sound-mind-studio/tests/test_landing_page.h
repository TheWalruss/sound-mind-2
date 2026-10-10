#pragma once

#include <QObject>

class LandingPageTest : public QObject {
    Q_OBJECT

private slots:
    void showsTheEmbeddedLogo();
    void newProjectButtonEmitsNewProjectRequested();
    void deviceConfigurationEmbedsAFunctioningDeviceConfigurationWidget();
    void openProjectButtonEmitsOpenProjectRequested();
    void setRecentProjectsWithNoPathsShowsNoClickableEntries();
    void setRecentProjectsCreatesEntriesThatEmitTheirPath();
    void setRecentProjectsReplacesThePreviousEntries();
    void quickStartButtonEmitsQuickStartRequested();
    void readmeButtonEmitsReadmeRequested();
    void userGuideButtonEmitsUserGuideRequested();
    void changelogButtonEmitsChangelogRequested();
    void aboutButtonEmitsAboutRequested();

    // Header + two-column reorganization.
    void headerIsNotHorizontallyCentered();
    void newOpenProjectAndRecentProjectsLiveInTheLeftColumn();
    void documentationAndDeviceConfigurationLiveInTheRightColumn();
    void leftAndRightColumnsAreEachTheirOwnResizableScrollArea();
};
