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
    void startPlaybackRunsCompositeInTheBackgroundAndShowsTheCancelButton();
    void cancelPlaybackCompositeDiscardsTheResultWithoutStartingPlayback();
    void startPlaybackDoesNothingWhileAlreadyCompositing();
    void closeRefusesWhileCompositingForPlayback();
    void newProjectRefusesWhileCompositingForPlayback();
    void openProjectRefusesWhileCompositingForPlayback();
    void openProjectAtRefusesWhileCompositingForPlayback();
    void poolTopmostLayerNowFailsGracefullyWithNoContent();
    void poolTopmostLayerNowPoolsAnImportedLayer();
    void poolTopmostLayerAsyncRunsInTheBackgroundAndShowsTheCancelButton();
    void poolTopmostLayerAsyncCompletesSuccessfullyAndAppliesTheResult();
    void cancelPoolDiscardsTheComputedResultWithoutApplyingIt();
    void poolTopmostLayerAsyncDoesNothingWhileAlreadyRunning();
    void closeRefusesWhileAPoolIsRunning();
    void newProjectRefusesWhileAPoolIsRunning();
    void exportTopmostLayerAudioNowFailsGracefullyWithNoContent();
    void exportTopmostLayerAudioNowExportsAnImportedLayer();
    void exportTopmostLayerAudioNowFailsForAnUnrecognizedExtension();
    void exportTopmostLayerVideoNowFailsGracefullyWithNoContent();
    void exportTopmostLayerVideoNowExportsAnImportedLayer();
    void exportTopmostLayerVideoAsyncRunsInTheBackgroundAndShowsTheCancelButton();
    void exportTopmostLayerVideoAsyncCompletesSuccessfullyAndHidesTheCancelButton();
    void cancelVideoExportStopsItAndDeletesThePartialFile();
    void exportTopmostLayerVideoAsyncDoesNothingWhileAlreadyRunning();
    void exportTopmostLayerAudioAsyncCompletesSuccessfullyAndHidesTheCancelButton();
    void cancelAudioExportStopsItAndDeletesThePartialFile();
    void exportTopmostLayerAudioAsyncDoesNothingWhileAVideoExportIsRunning();
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
    void configuredDeviceAndGainPersistAcrossANewProjectsFreshLoopEngine();
    void theLoopInputLayersNameIsActuallyVisibleInTheLayersPanel();

    // Transport Panels (v0.Y.16.1)
    void transportPanelsStayHiddenByDefaultEvenAfterAProjectExists();
    void panelVisibilityPersistsAcrossProjectSwitches();
    void layersToggleActionShowsAndHidesTheLayersPanel();
    void toggleLoopModeSyncsTheLoopPanelsRunningState();
    void toggleRecordingSyncsTheRecordPanelsRecordingState();
    void setKeepLoopingSyncsTheLoopPanelsCheckBox();
    void setConfiguredInputDeviceForwardsToTheLoopEngine();
    void setConfiguredOutputDeviceForwardsToTheLoopEngine();
    void setConfiguredInputDeviceForwardsToTheRecordEngine();
    void setPlaybackVolumeForwardsToThePlaybackEngine();
    void loopPanelToggleButtonStartsAndStopsTheRealEngine();
    void loopModeLocksAndUnlocksConfigureDevicesDeviceCombos();
    void recordingLocksTheInputComboButNotTheOutputCombo();
    void playbackPanelButtonsDriveRealPlayback();

    // Audio Import Snippets (v0.Y.19.1)
    void audioSnippetsForFileReturnsOneSnippetForAudioNoLongerThanTheProject();
    void audioSnippetsForFileSplitsLongerAudioIntoProjectLengthSegments();
    void audioSnippetsForFileFailsGracefullyWithNoProjectOpen();
    void importAudioFileImportsEveryComputedSnippetForLongAudio();
    void importAudioSnippetsImportsOnlyTheRequestedSubset();
    void importAudioSnippetsSkipsOutOfRangeIndicesGracefully();
    void importAudioSnippetsFailsWhenNothingWasImported();
    void importAudioSnippetsAsyncRunsInTheBackgroundAndShowsTheCancelButton();
    void importAudioSnippetsAsyncCompletesSuccessfullyAndAddsTheLayers();
    void cancelImportDiscardsTheEncodedLayersWithoutAddingThem();
    void importAudioSnippetsAsyncDoesNothingWhileAlreadyRunning();
    void closeRefusesWhileAnImportIsRunning();
    void newProjectRefusesWhileAnImportIsRunning();
    void openProjectRefusesWhileAnImportIsRunning();
    void openProjectAtRefusesWhileAnImportIsRunning();

    // Image Import Scaling (v0.Y.20.1)
    void importImageFileRescaleToFitProjectStretchesBothAxes();
    void importImageFileScaleVerticalKeepHorizontalKeepsNativeWidth();
    void importImageFileScaleHorizontalKeepVerticalKeepsNativeHeight();
    void importImageFileScaleVerticalProportionalPreservesAspectRatio();
    void importImageFileKeepNativeResolutionDoesNotRescale();

    // Drag & Drop Import (v0.Y.17.1)
    void handleDroppedFilesImportsAWavFile();
    void handleDroppedFilesImportsAnImageFile();
    void handleDroppedFilesOpensASmprojFile();
    void handleDroppedFilesIgnoresUnrecognizedExtensions();
    void handleDroppedFilesRoutesMultipleFilesInOrder();
    void handleDroppedFilesSmprojRefusesWhileLoopModeIsRunning();
    void handleDroppedFilesAppliesTheGivenImageMode();
    void handleDroppedFilesSequencesDroppedImagesWhenRequested();
    void handleDroppedFilesAppliesGivenAudioSnippetSelections();

    // Layer Time Alignment (v0.Y.21.1)
    void setLayerTranslationChangesTheLayersTranslation();
    void setLayerRescaleChangesTheLayersRescale();

    // Image Sequence Import (v0.Y.22.1)
    void importImageFilesImportsEachFileIndependentlyWhenNotSequential();
    void importImageFilesAppliesProportionalScalingAndCumulativeTranslationWhenSequential();
    void importImageFilesWrapsCumulativeOffsetPastCanvasWidth();
    void importImageFilesSortsFilesAlphabeticallyWhenSequential();
    void importImageFilesSucceedsIfAtLeastOneFileImports();
    void importImageFilesFailsWhenNothingWasImported();

    // UI Polish pass (v0.0.21.1)
    void windowTitleIncludesTheProjectNameOnceOneExists();
    void startPlaybackSetsThePlaybackPanelDuration();
    void seekPlaybackMovesThePlaybackPosition();
    void stopPlaybackResetsThePlaybackPanelPosition();

    // Workflow & Device Polish, Installment B: Repeat Playback (v0.0.42.2)
    void setPlaybackRepeatAndScopeDoNothingWithNoProjectOpen();
    void paintingWhileRepeatIsOffDoesNotInterruptPlayback();
    void paintingWhileRepeatIsOnWithDeltaScopeJumpsPlaybackToTheEditedRegion();
    void paintingWhileRepeatIsOnWithTrackScopeKeepsTheSamePosition();
    void paintingWhileRepeatIsOnWithReviewScopeWrapsToTheTrackStartNotTheEdit();
    void paintingWithRepeatOffAndDeltaScopeStartsAOneShotPlaybackAutomatically();
    void oneShotDeltaPlaybackHaltsAtTheEditsEndWithoutLooping();
    void paintingWithRepeatOffAndReviewScopeStartsAOneShotPlaybackAutomatically();

    // Basic Painting (v0.Y.24.1)
    void paintModeIsOffByDefault();
    void setPaintModeEnabledTogglesTheCanvasToolMode();
    void paintingOnTheCanvasAppendsAPaintOperationToTheProjectsLog();
    void undoAndRedoDelegateToThePaintController();
    void undoInterleavesPaintStrokesAndLayerPropertyChangesInChronologicalOrder();
    void settingANewProjectResetsPaintModeToOff();
    void paintingWithTheDefaultToolConfigurationActuallyPaintsSomethingVisible();
    void toggleToolConfigurationPanelShowsAndHidesIt();
    void paintingTargetsTheSelectedLayerNotNecessarilyTheTopmostOne();
    void addEmptyLayerAddsASilentLayerAndSelectsIt();
    void addEmptyLayerIsANoOpWithNoProjectOpen();
    void addFilterLayerAddsAFilterTypeLayerAndSelectsIt();
    void selectingAFilterLayerLoadsAndEnablesFilterConfigurationPanel();
    void selectingANormalLayerDisablesFilterConfigurationPanel();
    void selectingTheEqualizerLayerSwitchesTheFilterConfigurationPanelToCutMode();
    void editingFilterConfigurationPanelWritesBackToTheSelectedLayer();
    void movingTheMouseOverTheCanvasUpdatesTheCursorPositionLabel();
    void leavingTheCanvasClearsTheCursorPositionLabel();
    void paintingTheBackgroundLayerActuallyPaintsSomethingVisible();
    void pickAndPaintToolbarActionsAreMutuallyExclusive();
    void pickingAPaintedStrokeLoadsItsSettingsIntoThePanel();
    void movingAPickedStrokeCommitsATranslatedOperation();
    void deletingAPickedStrokeLeavesAnEmptyTombstone();
    void settingANewProjectResetsPickModeToOff();
    void pickingTheSameSpotTwiceSelectsTheOccludedStrokeUnderneath();
    void paintPickAndSelectToolbarActionsAreAllMutuallyExclusive();
    void drawingASelectionAndFillingItChangesTheLayersContent();
    void selectionPersistsAfterSwitchingAwayFromSelectMode();
    void deselectClearsTheCurrentSelectionSoFillBecomesANoOp();
    void fillSelectionWithIsANoOpWithNoSelection();
    void settingANewProjectResetsSelectModeToOff();
    void copyThenPasteOnTheSameLayerReproducesTheSelection();
    void pasteUsesTheSelectionConfigurationPanelsOwnBlendMode();
    void cutClearsTheSourceRegionButPasteStillReproducesIt();
    void pasteCanTargetADifferentLayerThanItWasCopiedFrom();
    void pasteIsANoOpWithNoClipboard();
    void copySelectionIsANoOpWithNoSelection();
    void captureMindShotAddsANamedEntryToTheProjectsMindShotLibrary();
    void captureMindShotIsANoOpWithNoSelection();
    void clickingInPathModePlacesNodesAndFinishPathCommitsANewPaintObject();
    void cancelPathDiscardsInProgressPlacementWithoutCommittingAnything();
    void smoothNodesToggleAffectsSubsequentlyPlacedNodes();
    void finishPathWithNoNodesPlacedIsANoOp();
    void settingANewProjectResetsPathModeToOff();
    void pastedContentIsPickableAndMovable();
    void movingAPastedRegionPreservesItsOwnVerticalShapeAndOrientation();
    void pasteSwitchesToPickModeAndSelectsThePastedRegionImmediately();
    void modifyingAPaintedStrokeAfterCuttingOverItKeepsTheCutRegionSilenced();
    void bringPickedObjectToFrontMovesItAboveLaterStrokesOnTheSameLayer();
    void editingAPickedStrokesPathMovesANodeAndCommitsOnApply();
    void cancelingAPickedStrokesPathEditDiscardsTheDragWithoutCommitting();
    void cutRegionIsPickableAndMovable();

    // Workflow & Device Polish, Installment D: Documentation links (v0.0.42.4)
    void openUserDocIfBundledOpensItAndReturnsTrueWhenPresent();
    void openUserDocIfBundledReturnsFalseWithoutOpeningAnythingWhenMissing();

    // Workflow & Device Polish, Installment E: Hardware Acceleration toggle (v0.0.42.5)
    void setHardwareAccelerationEnabledForwardsToCore();
};
