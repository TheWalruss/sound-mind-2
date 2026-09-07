#pragma once

#include <QObject>

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void startsWithAFreshProject();
    void newProjectReplacesTheCurrentOne();
    void importAudioFileAddsANewLayer();
    void importImageFileAddsANewLayer();
    void importAudioFileFailsGracefullyForAMissingFile();
};
