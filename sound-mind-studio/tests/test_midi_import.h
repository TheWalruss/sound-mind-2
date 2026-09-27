#pragma once

#include <QObject>

class MidiImportTest : public QObject {
    Q_OBJECT

private slots:
    void importMidiChannelsIntoCreatesOneLayerPerChannelByDefault();
    void importMidiChannelsIntoMergesEveryChannelIntoOneLayerWhenNotSeparating();
    void importMidiChannelsIntoFailsGracefullyForAnUnreadableFile();
    void importMidiChannelsIntoUsesAPlainProceduralConfigurationForEachChannel();
    void importMidiChannelsIntoOnlyIncludesSelectedChannelsWhenGiven();
    void rebuildingAMidiImportedLayerProducesRealNonSilentPaintedContent();

    void midiImportPreviewForFileReturnsChannelsAndComputedSnippets();
    void midiImportPreviewForFileFailsGracefullyForAnUnreadableFile();
    void importMidiSelectionIntoOnlyIncludesSelectedChannels();
    void importMidiSelectionIntoClipsAndRebasesNotesToTheSelectedSnippetWindow();
    void importMidiSelectionIntoMergesChannelsIntoOneLayerPerSnippetWhenNotSeparating();
    void importMidiSelectionIntoAppendsSnippetSuffixToLayerNamesWhenMoreThanOneSnippet();
    void importMidiSelectionIntoFailsGracefullyForAnUnreadableFile();
};
