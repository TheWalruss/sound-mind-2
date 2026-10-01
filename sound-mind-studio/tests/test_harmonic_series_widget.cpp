#include "test_harmonic_series_widget.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/harmonic_series_widget.h"

using sound_mind::studio::HarmonicSeriesWidget;

namespace {

/// @brief `valueToY()`'s own margin/display-ceiling formula, mirrored
/// here so tests can compute exactly which pixel a given strength value
/// lands on for a `240x100`-wide/tall widget (this file's own fixed test
/// size, matching `sizeHint()`) - must stay in sync with
/// `harmonic_series_widget.cpp`'s own `kMargin`/`kDisplayMaxStrength`.
int yPosFor(double value) {
    constexpr double margin = 8.0;
    constexpr double height = 100.0 - 2 * margin;
    constexpr double bottom = 100.0 - margin;
    constexpr double displayMax = 2.0;
    return static_cast<int>(bottom - (value / displayMax) * height);
}

/// @brief `columnAt()`'s own margin/width formula, mirrored here so tests
/// can compute an x-coordinate landing inside a given bar's own column.
int xPosForColumn(int column, std::size_t count) {
    constexpr double margin = 8.0;
    const double width = 240.0 - 2 * margin;
    const double columnWidth = width / static_cast<double>(count);
    return static_cast<int>(margin + (static_cast<double>(column) + 0.5) * columnWidth);
}

}  // namespace

void HarmonicSeriesWidgetTest::freshWidgetHasNoBars() {
    const HarmonicSeriesWidget widget;
    QVERIFY(widget.harmonicStrengths().empty());
}

void HarmonicSeriesWidgetTest::sizeHintReturnsAReasonableDefault() {
    const HarmonicSeriesWidget widget;
    QCOMPARE(widget.sizeHint(), QSize(240, 100));
}

void HarmonicSeriesWidgetTest::setHarmonicStrengthsSyncsWithoutEmitting() {
    HarmonicSeriesWidget widget;
    QSignalSpy spy(&widget, &HarmonicSeriesWidget::harmonicStrengthsChanged);

    widget.setHarmonicStrengths({1.0, 0.5, 0.25, 0.125});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(widget.harmonicStrengths(), std::vector<double>({1.0, 0.5, 0.25, 0.125}));
}

void HarmonicSeriesWidgetTest::clickingABarSetsItsValueAndEmits() {
    HarmonicSeriesWidget widget;
    widget.resize(240, 100);
    widget.setHarmonicStrengths({0.0, 0.0, 0.0, 0.0});
    QSignalSpy spy(&widget, &HarmonicSeriesWidget::harmonicStrengthsChanged);

    const QPoint pos(xPosForColumn(0, 4), yPosFor(1.0));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(widget.harmonicStrengths()[0], 1.0);
    // The other bars are untouched.
    QCOMPARE(widget.harmonicStrengths()[1], 0.0);
}

void HarmonicSeriesWidgetTest::draggingKeepsUpdatingTheOriginalColumnEvenIfTheCursorLeavesIt() {
    HarmonicSeriesWidget widget;
    widget.resize(240, 100);
    widget.setHarmonicStrengths({0.0, 0.0, 0.0, 0.0});

    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosForColumn(0, 4), yPosFor(1.0)));
    // Drag over into column 3's own horizontal area - the value change
    // should still land on column 0, the one the drag actually started on.
    QTest::mouseMove(&widget, QPoint(xPosForColumn(3, 4), yPosFor(0.5)));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosForColumn(3, 4), yPosFor(0.5)));

    QCOMPARE(widget.harmonicStrengths()[0], 0.5);
    QCOMPARE(widget.harmonicStrengths()[3], 0.0);
}

void HarmonicSeriesWidgetTest::aClickAboveTheTopClampsToTheDisplayCeiling() {
    HarmonicSeriesWidget widget;
    widget.resize(240, 100);
    widget.setHarmonicStrengths({0.0});

    const QPoint pos(xPosForColumn(0, 1), -50);  // Well above the widget's own top edge.
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(widget.harmonicStrengths()[0], 2.0);  // This widget's own display ceiling - see its own docs.
}

void HarmonicSeriesWidgetTest::renderThumbnailProducesAnImageOfTheRequestedSizeWithVisibleContent() {
    const QImage image = HarmonicSeriesWidget::renderThumbnail({1.0, 0.5}, QSize(32, 16));

    QCOMPARE(image.size(), QSize(32, 16));
    bool foundVisiblePixel = false;
    for (int y = 0; y < image.height() && !foundVisiblePixel; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                foundVisiblePixel = true;
                break;
            }
        }
    }
    QVERIFY(foundVisiblePixel);
}

void HarmonicSeriesWidgetTest::renderThumbnailOfAnEmptySeriesIsFullyTransparent() {
    const QImage image = HarmonicSeriesWidget::renderThumbnail({}, QSize(32, 16));

    QCOMPARE(image.size(), QSize(32, 16));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QCOMPARE(qAlpha(image.pixel(x, y)), 0);
        }
    }
}
