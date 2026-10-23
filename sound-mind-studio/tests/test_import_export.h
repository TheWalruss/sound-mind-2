#pragma once

#include <QObject>

class ImportExportTest : public QObject {
    Q_OBJECT

private slots:
    void audioSnippetsForFileReturnsOneSnippetForAudioNoLongerThanTheProject();
    void audioSnippetsForFileSplitsLongerAudioIntoProjectLengthSegments();
    void audioSnippetsForFileFailsGracefullyForAnUnreadableFile();
    void importAudioSnippetsIntoImportsOnlyTheRequestedSubset();
    void importAudioSnippetsIntoReturnsZeroWhenNothingWasImported();
    void encodeAudioSnippetsReturnsUnattachedLayersForTheRequestedSubset();
    void encodeAudioSnippetsThrowsImportCancelledOnceShouldCancelStartsReturningTrue();

    // Snippet offsets, real-world testing pass finding #23.
    void audioSnippetsForFileShiftsTheSplitByTheGivenOffset();
    void audioSnippetsForFileReturnsEmptyWhenTheOffsetExceedsTheFilesDuration();
    void encodeAudioSnippetsAppliesTheGivenOffset();
    void importAudioSnippetsIntoAppliesTheGivenOffset();
    void importImageFileIntoAddsANewLayer();
    void importImageFileIntoFailsForAnUnreadableFile();
    void importImageFileIntoWithModePolarUnwarpsAndAddsANewLayer();
    void importImageFileIntoWithModePolarAndNoParamsFails();
    void importImageFilesIntoImportsEachFileIndependentlyWhenNotSequential();
    void importImageFilesIntoAppliesCumulativeTranslationWhenSequential();
    void importImageFilesIntoReturnsZeroWhenNothingWasImported();
    void exportProjectAudioNowWritesARealFile();
    void exportProjectAudioNowFailsForAnUnrecognizedExtension();
    void exportProjectAudioNowFailsWithNothingToExport();
    void exportProjectVideoNowWritesARealFile();

    // v0.1.49.3 - audio at another rate than the project's.
    void audioAtAnotherRateSplitsByTheProjectsRealDuration();
    void audioAtAnotherRateFillsTheSameColumnsAsAtTheProjectsRate();

    // Image export (v0.1.52.1).
    void exportProjectImageNowWritesTheCanvasAtFullResolutionByDefault();
    void exportProjectImageNowScalesToTheGivenSize();
    void exportProjectImageNowAsASoundFlowerIsSquare();
    void exportProjectImageNowPicksTheFormatFromTheExtension();
    void exportProjectImageNowFailsForAnUnknownFormatOrNothingToExport();
};
