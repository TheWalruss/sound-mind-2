#pragma once

#include <QObject>

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void hasARealWindowIconNotTheDefaultOne();
    void startsWithNoProjectOpen();
    void newProjectShowsTheCanvasInsteadOfTheLandingPage();
    void openProjectAtOpensAndRecordsARecentProject();
    void openProjectAtFailsGracefullyForAMissingFile();
    void landingPageRecentProjectRequestedOpensThatPath();
    void newProjectReplacesTheCurrentOne();
    void importAudioFileAddsANewLayer();
    void importImageFileAddsANewLayer();
    void importAudioFileFailsGracefullyForAMissingFile();
    void startPlaybackDoesNothingWithNoContent();
    void startPlaybackPlaysAnImportedLayer();
    void pauseAndResumePlayback();
    void stopPlaybackStopsIt();
    void poolTopmostLayerNowFailsGracefullyWithNoContent();
    void poolTopmostLayerNowPoolsAnImportedLayer();
    void exportTopmostLayerAudioNowFailsGracefullyWithNoContent();
    void exportTopmostLayerAudioNowExportsAnImportedLayer();
    void exportTopmostLayerAudioNowFailsForAnUnrecognizedExtension();
    void exportTopmostLayerVideoNowFailsGracefullyWithNoContent();
    void exportTopmostLayerVideoNowExportsAnImportedLayer();
    void importAudioFileShowsProgressThenCompletionInTheStatusBar();
    void poolTopmostLayerNowShowsProgressThenCompletionInTheStatusBar();
    void exportTopmostLayerAudioNowShowsProgressThenCompletionInTheStatusBar();
    void aFailedOperationClearsTheStatusBarRatherThanLeavingAStaleMessage();
    void toggleLoopModeAddsALayerAndStartsTheEngine();
    void toggleLoopModeStopsARunningCapture();
    void startPlaybackDoesNothingWhileLoopModeIsRunning();
    void toggleRecordingStartsAndStopsWithoutAddingALayerWhenNothingWasCaptured();
    void startPlaybackDoesNothingWhileRecordingIsRunning();
    void toggleLoopModeDoesNothingWhileRecordingIsRunning();
    void toggleRecordingDoesNothingWhileLoopModeIsRunning();

    // Project Lifecycle (v0.Y.10.1)
    void newProjectStartsWithNoUnsavedChanges();
    void importAudioFileMarksUnsavedChanges();
    void poolTopmostLayerNowMarksUnsavedChanges();
    void toggleLoopModeMarksUnsavedChangesWhenItStarts();
    void savingProjectClearsUnsavedChanges();
    void openProjectAtClearsUnsavedChangesFromThePreviousProject();
    void closeAcceptsWhenThereAreNoUnsavedChanges();
    void closeRefusesWhileLoopModeIsRunning();
    void newProjectRefusesWhileLoopModeIsRunning();
    void openProjectRefusesWhileRecordingIsRunning();
    void openProjectAtRefusesWhileLoopModeIsRunning();

    // Create Project Wizard (v0.Y.11.1)
    void createProjectAtSavesImmediatelyAndBecomesCurrent();
    void createProjectAtAppliesGivenSettings();
    void createProjectAtFailsGracefullyForAnUnwritableLocation();
    void importAudioFileUsesTheProjectsConfiguredCodecSettings();

    // Layers Panel (v0.Y.13.1)
    void layersPanelIsHiddenUntilAProjectExists();
    void refreshLayersPanelReflectsTheCurrentLayers();
    void toggleLayerVisibilityHidesALayerFromTopmostLookup();
    void toggleLayerVisibilityMarksUnsavedChanges();
    void setLayerOpacityChangesTheLayersOpacity();
    void renameLayerToRenamesTheLayer();
    void renameLayerToFailsForAnEmptyName();
    void renameLayerToFailsForAnUnknownId();
    void deleteLayerRemovesANormalLayer();
    void deleteLayerRefusesToDeleteTheBackgroundLayer();
    void reorderLayersAppliesAValidPermutation();
    void reorderLayersRejectsAnInvalidPermutation();
    void changingARealRowsOpacitySliderDoesNotCrash();

    // Loop Mode (v0.Y.12.1)
    void toggleLoopModeDoesNothingWithNoProjectOpen();
    void settingProjectReconfiguresTheLoopEngineForItsOwnSettings();
    void setKeepLoopingForwardsToTheLoopEngine();
    void setKeepLoopingDoesNothingWithNoProjectOpen();
    void toggleLoopModeReusesAnExistingLoopInputLayerInsteadOfCreatingANewOne();
    void toggleLoopModeGivesANewLoopInputLayerAPlaceholderContentImmediately();
};
