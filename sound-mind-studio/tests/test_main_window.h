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
    void importMidiFileAddsANewLayerWithRealPaintedContent();
    void importMidiFileFailsGracefullyForAMissingFile();
    void importMidiFileWithChannelNumbersOnlyImportsSelectedChannels();
    void midiImportPreviewForFileReturnsChannelsAndSnippets();
    void importMidiSelectionImportsOnlyTheSelectedSnippet();
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
    void poolLayerNowPoolsASpecificNonTopmostLayer();
    void layersPanelsPoolButtonPoolsTheSelectedLayerViaMainWindow();
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
    void cycleLayerVisibilityStateEventuallyHidesALayerFromTopmostLookup();
    void cycleLayerVisibilityStateMarksUnsavedChanges();
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
    void inputOutputMenuContainsPlaybackRecordAndLoopToggles();
    void showingOnePlaybackRecordOrLoopPanelHidesTheOtherTwo();
    void configureDevicesButtonInEachOfTheThreePanelsShowsAndRaisesTheSharedPanel();
    void configureMenuContainsEveryConfigurationPanelsOwnToggle();
    void macroButtonSitsNextToInputOutputAtTheTopRightAndListsAllThreeMacroActions();
    void composerPanelsToggleIsInTheViewMenuNotOnTheToolbar();

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
    void handleDroppedFilesAppliesGivenAudioSnippetOffsets();
    void handleDroppedFilesAppliesGivenMidiChoice();
    void handleDroppedFilesWithNoMidiChoiceImportsEveryChannel();

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
    void toolDropdownDefaultsToPanAndItsLabelSaysSo();
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
    void editFilterLayerSelectsTheLayerAndShowsTheFilterConfigurationPanel();
    void selectingANormalLayerShowsThePendingFilterConfigurationInsteadOfDisablingThePanel();
    void addFilterLayerSeedsTheNewLayerFromThePendingFilterConfiguration();
    void pendingFilterConfigurationPersistsAcrossMultipleAddedFilterLayers();
    void pendingFilterConfigurationResetsForAFreshProject();
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
    void toolDropdownLabelTracksTheActiveToolAndFallsBackToPanWhenTurnedOff();
    void setPanModeEnabledSelectsTheNeutralToolModeAndCanBeReachedDirectly();
    void drawingASelectionAndFillingItChangesTheLayersContent();
    void selectionPersistsAfterSwitchingAwayFromSelectMode();
    void deselectClearsTheCurrentSelectionSoFillBecomesANoOp();
    void fillSelectionWithIsANoOpWithNoSelection();
    void fillSelectionWithGradientAppliesARealMultiStopGradientAcrossTheSelection();
    void fillSelectionWithGradientIsANoOpWithNoSelection();
    void applyFilterToSelectionAppliesTheCurrentlyConfiguredFilter();
    void applyFilterToSelectionIsANoOpWithNoSelection();
    void settingANewProjectResetsSelectModeToOff();
    void copyThenPasteOnTheSameLayerReproducesTheSelection();
    void pasteUsesTheSelectionConfigurationPanelsOwnBlendMode();
    void cutClearsTheSourceRegionButPasteStillReproducesIt();
    void pasteCanTargetADifferentLayerThanItWasCopiedFrom();
    void pasteIsANoOpWithNoClipboard();
    void copySelectionIsANoOpWithNoSelection();
    void captureMindShotAddsANamedEntryToTheProjectsMindShotLibrary();
    void captureMindShotWithDetailsStoresFundamentalFrequencyAndStartTimeOffset();
    void captureMindShotIsANoOpWithNoSelection();

    // Resonant Instruments - Installment C: Studio capture workflow (v0.Y.59.1).
    void createResonanceFromPickedPathNamedAddsANamedEntryToTheLibrary();
    void createResonanceFromPickedPathNamedIsANoOpWithNothingPicked();
    void createResonanceFromPickedPathIsANoOpWithNothingPicked();

    // Resonant Instruments - Branching Curve editor (v0.Y.59.1, item 1's deferred half).
    void startBranchingCurveFromPickedPathBeginsASessionWithTheTrunk();
    void startBranchingCurveFromPickedPathIsANoOpWithNothingPicked();
    void pickingAPointOnTheTrunkThenAddingABranchGraftsItOn();
    void addPickedPathAsBranchIsANoOpWithNoPendingGraft();
    void cancelBranchingCurveDiscardsTheSession();
    void createResonanceFromPickedGraphNamedBuildsFromEveryBranchAndEndsTheSession();
    void createResonanceFromPickedGraphNamedIsANoOpWithNoSessionActive();
    void clickingInPathModePlacesNodesAndFinishPathCommitsANewPaintObject();
    void cancelPathDiscardsInProgressPlacementWithoutCommittingAnything();
    void smoothNodesToggleAffectsSubsequentlyPlacedNodes();
    void smoothNodesActionIsHiddenWithNoProjectOpen();
    void smoothNodesActionBecomesVisibleWhilePlacingANewPathAndHiddenAfterFinishing();
    void smoothNodesActionBecomesVisibleWhileAStrokeIsPickedAndHiddenAfterDeselecting();
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

    // saveConvolutionKernel() - confirmed with the user: saving a
    // convolution kernel now prompts for a name instead of silently
    // auto-naming it.
    void saveConvolutionKernelIsANoOpWithNoProjectOpen();

    void saveGridPresetIsANoOpWithNoProjectOpen();
    void deleteGridPresetIsANoOpWithNoProjectOpen();
    void deleteGridPresetRemovesTheEntryAndRefreshesTheCombo();
    void savingANewGridPresetNameAddsItToTheProjectAndRefreshesThePanel();
    void savingAGridPresetUnderAnExistingNameAndChoosingReplaceOverwritesInPlace();
    void savingAGridPresetUnderAnExistingNameAndChoosingCancelLeavesTheProjectUnchanged();

    // Resource Browser toggle promoted to a top-level menu-bar entry,
    // between View and Help (previously inside the Configure dropdown).
    void resourceBrowserToggleIsATopLevelMenuBarEntryBetweenViewAndHelp();

    // Default dock layout: every Right-dock-area panel tabs together
    // instead of stacking vertically.
    void rightDockAreaPanelsAreTabifiedTogetherByDefault();

    // Showing a tabified panel switches to its tab immediately, rather
    // than adding it in the background behind whichever tab was active.
    void showingATabifiedPanelSwitchesToItImmediately();

    // Edit menu redesign: the former flat, 29-action list is now grouped
    // into named submenus (Undo/Redo/Delete and Image Mode stay direct).
    void editMenuActionsAreGroupedIntoNamedSubmenus();

    // Canvas right-click context menu (direct user feedback).
    void rightClickingAPickedStrokeShowsTheObjectContextMenu();
    void rightClickingEmptyCanvasShowsTheCanvasContextMenu();

    // Real, user-reported hang (CPU pegged, UI locked) - a re-entrancy
    // regression in Decision #216's own raise()-on-show wiring, fixed by
    // Decision #221.
    void showingSeveralTabifiedPanelsInSequenceWithAProjectOpenDoesNotHang();

    // Tool Configuration's own live audio preview (direct user
    // feedback: "practically wherever there is a visual preview of
    // something, give the user the ability to play an audio preview of
    // whatever it is").
    void previewingTheCurrentToolConfigurationPlaysAudio();
    void previewToolConfigurationIsANoOpWithNoProjectOpen();
};
