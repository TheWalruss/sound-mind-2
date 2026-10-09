#pragma once

#include <QObject>

class ResourceBrowserControllerTest : public QObject {
    Q_OBJECT

private slots:
    void refreshPanelWithNoProjectShowsNoEntries();
    void refreshPanelListsEachCategorysOwnLibrary();
    void selectingAnEntryPopulatesTheInspector();
    void selectingAMindShotRendersARasterAndEnablesPlay();
    void exportThenImportFromFileRoundTripsAMindWaveIntoTheProject();
    void browseOtherProjectShowsItsOwnLibraryWithoutMutatingTheCurrentProject();
    void importEntryCopiesFromTheBrowsedProjectIntoTheCurrentOne();
    void returnToCurrentProjectRestoresTheCurrentProjectsOwnView();
    void mindGrainCategoryNeverOffersImportEvenWhileBrowsing();
    void addToToolkitThenExportThenImportRoundTripsMixedResourcesAndClearsTheDraft();
    void exportToolkitFailsWhenTheDraftIsEmpty();
    void removingADraftEntryTakesItOutOfTheNextExport();
    void importToolkitIsAllOrNothingOnAMalformedEntry();
    void setProjectStopsBrowsingAnyOtherProject();
    void toolkitDraftEntryIsASnapshotUnaffectedByLaterEditsToItsSource();
};
