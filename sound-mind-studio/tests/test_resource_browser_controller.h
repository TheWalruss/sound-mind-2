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

    // --- Resource Browser enhancements (direct user feedback) ---
    void filterPresetCategoryListsExportsAndImports();
    void selectingAMindWaveRendersAPreviewRaster();
    void selectingAToolPresetRendersAStrokePreviewRaster();
    void selectingAResonanceProfileRendersItsSourceCurveAlongsideTheSpectrum();
    void addingAMindWaveBoundToolPresetToToolkitAlsoAddsTheMindWaveAndNotifies();
    void addingAnEntryWithNoDependenciesNeverNotifies();
    void addingTheSameDependencyTwiceDoesNotDuplicateIt();
    void toolPresetPreviewStillRendersWhenTheProjectsOwnMaxFrequencyIsBelowOneKilohertz();

    // Mind Shot preview decode deferred to the first Play click (direct
    // user feedback: "the Mind Shot preview in the Resource Browser is
    // very very slow").
    void playingAMindShotDecodesLazilyOnFirstPlayAndStaysPlayableOnASecondPlay();
};
