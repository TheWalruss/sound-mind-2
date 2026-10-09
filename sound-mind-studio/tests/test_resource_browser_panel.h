#pragma once

#include <QObject>

class ResourceBrowserPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelHasNoRowsAndNoSelectionAndNoBrowsingLabel();
    void setEntriesPopulatesTheListAndClickingARowSelectsIt();
    void setEntriesClearsSelectionAndInspector();
    void setInspectorShowsOrHidesRasterPlayAndImportControlsPerFlag();
    void setPlayingTogglesPlayAndStopVisibilityOnlyWhenCanPlay();
    void setBrowsingOtherProjectTogglesTheBrowseButtons();
    void setImportFromFileEnabledTogglesTheButton();
    void clickingEachButtonEmitsItsOwnSignal();
    void setToolkitEntriesPopulatesTheDraftList();
    void setAddToToolkitEnabledTogglesTheButton();
    void clickingRemoveEmitsRemoveFromToolkitRequestedWithTheSelectedIndex();
    void clickingExportOrImportToolkitEmitsItsOwnSignal();
};
