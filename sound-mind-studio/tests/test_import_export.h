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
    void importImageFileIntoAddsANewLayer();
    void importImageFileIntoFailsForAnUnreadableFile();
    void importImageFilesIntoImportsEachFileIndependentlyWhenNotSequential();
    void importImageFilesIntoAppliesCumulativeTranslationWhenSequential();
    void importImageFilesIntoReturnsZeroWhenNothingWasImported();
    void exportLayerAudioNowWritesARealFile();
    void exportLayerAudioNowFailsForAnUnrecognizedExtension();
    void exportLayerVideoNowWritesARealFile();
};
