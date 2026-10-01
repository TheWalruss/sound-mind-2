#pragma once

#include <QObject>

class WaveformPreviewWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshWidgetHasNoSamples();
    void sizeHintReturnsAReasonableDefault();
    void setSamplesStoresThemExactly();
    void paintingWithFewerThanTwoSamplesDoesNotCrash();
    void setSamplesDrawsAVisibleDifference();
};
