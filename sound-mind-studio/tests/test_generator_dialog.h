#pragma once

#include <QObject>

class GeneratorDialogTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsToLatticeFamilyAndCenteredOrderChaos();
    void movingTheSliderChangesOrderChaos();
    void randomizeButtonChangesTheSeed();
    void choosingNoiseShowsTheNoiseControlsAndHidesOrderChaos();
    void noiseSettingsAndBindingsReachTheConfiguration();
    void theCustomSpectrumShowsOnlyForCustomNoise();
};
