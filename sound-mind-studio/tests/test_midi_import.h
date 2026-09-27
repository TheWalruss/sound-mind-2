#pragma once

#include <QObject>

class MidiImportTest : public QObject {
    Q_OBJECT

private slots:
    void importMidiChannelsIntoCreatesOneLayerPerChannelByDefault();
    void importMidiChannelsIntoMergesEveryChannelIntoOneLayerWhenNotSeparating();
    void importMidiChannelsIntoFailsGracefullyForAnUnreadableFile();
    void importMidiChannelsIntoUsesAPlainProceduralConfigurationForEachChannel();
};
