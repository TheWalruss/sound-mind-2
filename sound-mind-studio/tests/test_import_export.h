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
    void importImageFilesIntoImportsEachFileIndependentlyWhenNotSequential();
    void importImageFilesIntoAppliesCumulativeTranslationWhenSequential();
    void importImageFilesIntoReturnsZeroWhenNothingWasImported();
    void exportLayerAudioNowWritesARealFile();
    void exportLayerAudioNowFailsForAnUnrecognizedExtension();
    void exportLayerVideoNowWritesARealFile();
};
