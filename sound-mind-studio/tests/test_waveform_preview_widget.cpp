#include "test_waveform_preview_widget.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/waveform_preview_widget.h"

using sound_mind::studio::WaveformPreviewWidget;

void WaveformPreviewWidgetTest::freshWidgetHasNoSamples() {
    const WaveformPreviewWidget widget;
    QVERIFY(widget.samples().empty());
}

void WaveformPreviewWidgetTest::sizeHintReturnsAReasonableDefault() {
    const WaveformPreviewWidget widget;
    QCOMPARE(widget.sizeHint(), QSize(240, 56));
}

void WaveformPreviewWidgetTest::setSamplesStoresThemExactly() {
    WaveformPreviewWidget widget;

    widget.setSamples({0.0f, 0.5f, 1.0f, 0.25f});

    QCOMPARE(widget.samples(), std::vector<float>({0.0f, 0.5f, 1.0f, 0.25f}));
}

void WaveformPreviewWidgetTest::paintingWithFewerThanTwoSamplesDoesNotCrash() {
    WaveformPreviewWidget widget;
    widget.resize(240, 56);

    widget.setSamples({});
    widget.grab();
    widget.setSamples({0.5f});
    widget.grab();

    QCOMPARE(widget.samples().size(), std::size_t{1});
}

void WaveformPreviewWidgetTest::setSamplesDrawsAVisibleDifference() {
    WaveformPreviewWidget widget;
    widget.resize(240, 56);
    const QImage before = widget.grab().toImage();

    widget.setSamples({0.0f, 1.0f, 0.0f, 1.0f, 0.0f});
    const QImage after = widget.grab().toImage();

    QVERIFY(before != after);
}
