#include "test_equalizer_curve_widget.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/equalizer_curve_widget.h"

using sound_mind::core::Gradient;
using sound_mind::core::ProjectSettings;
using sound_mind::studio::EqualizerCurveWidget;

namespace {

/// @brief `tToY()`'s own margin formula, mirrored here so tests can
/// compute exactly which pixel a given normalized `t` (frequency) lands
/// on for the widget's own fixed `sizeHint()` (`160x240`) - must stay in
/// sync with `equalizer_curve_widget.cpp`'s own `kMarginTop`/
/// `kMarginBottom`. `t=1` is at the top (smallest y), `t=0` at the bottom
/// - the opposite of `GradientBarWidget`'s own left-to-right `t` mapping.
int yPosFor(float t) {
    constexpr double marginTop = 8.0;
    constexpr double marginBottom = 8.0;
    constexpr double height = 240.0 - marginTop - marginBottom;
    const double bottom = 240.0 - marginBottom;
    return static_cast<int>(bottom - static_cast<double>(t) * height);
}

/// @brief `cutToX()`'s own margin formula, mirrored here so tests can
/// compute exactly which pixel a given Cut amount lands on - must stay in
/// sync with `equalizer_curve_widget.cpp`'s own `kMarginLeft`/
/// `kMarginRight`.
int xPosFor(float cut) {
    constexpr double marginLeft = 48.0;
    constexpr double marginRight = 8.0;
    constexpr double width = 160.0 - marginLeft - marginRight;
    return static_cast<int>(marginLeft + static_cast<double>(cut) * width);
}

}  // namespace

void EqualizerCurveWidgetTest::freshWidgetHasTheDefaultTwoStopTransparentGradientAndSelectsTheFirstStop() {
    const EqualizerCurveWidget widget;
    QCOMPARE(widget.gradient().stops().size(), std::size_t{2});
    QCOMPARE(widget.gradient().stops().front().t, 0.0f);
    QCOMPARE(widget.gradient().stops().back().t, 1.0f);
    QCOMPARE(widget.selectedIndex(), std::size_t{0});
}

void EqualizerCurveWidgetTest::sizeHintReturnsAReasonableDefault() {
    const EqualizerCurveWidget widget;
    QCOMPARE(widget.sizeHint(), QSize(160, 240));
}

void EqualizerCurveWidgetTest::clickingNearAnExistingStopSelectsItWithoutInserting() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);
    QSignalSpy selectionSpy(&widget, &EqualizerCurveWidget::selectionChanged);

    // The default gradient's last stop (t=1, Cut=0 - fully transparent).
    const QPoint handle(xPosFor(0.0f), yPosFor(1.0f));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, handle);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, handle);

    QCOMPARE(gradientSpy.count(), 0);  // A plain select, no drag - nothing about the gradient itself changed.
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(widget.gradient().stops().size(), std::size_t{2});  // Still just the two endpoints - no insert.
    QCOMPARE(widget.selectedIndex(), std::size_t{1});
}

void EqualizerCurveWidgetTest::clickingEmptyAreaInsertsANewSortedStopAndSelectsIt() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);
    QSignalSpy selectionSpy(&widget, &EqualizerCurveWidget::selectionChanged);

    // Mid-frequency, well away from either endpoint handle (both sit at
    // Cut=0, i.e. xPosFor(0.0f)) so this can't be mistaken for a hit.
    const QPoint empty(xPosFor(0.5f), yPosFor(0.5f));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, empty);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, empty);

    QCOMPARE(gradientSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(widget.gradient().stops().size(), std::size_t{3});
    QVERIFY(qAbs(widget.gradient().stops()[1].t - 0.5f) < 0.01f);
    QCOMPARE(widget.selectedIndex(), std::size_t{1});
}

void EqualizerCurveWidgetTest::draggingAnInteriorStopClampsTBetweenItsNeighborsAndPreservesCutAmount() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    Gradient gradient;
    gradient.insertStop(0.5f);
    widget.setGradient(gradient);

    const QPoint handle(xPosFor(0.0f), yPosFor(0.5f));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, handle);
    // Drag far past the last stop's own t (t=1 sits at the TOP of this
    // widget - the opposite of GradientBarWidget's own horizontal t axis)
    // - yToT() alone clamps to 1.0, and the interior-stop neighbor clamp
    // then pulls it back just short of that.
    QTest::mouseMove(&widget, QPoint(xPosFor(0.0f), -1000));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), -1000));

    QCOMPARE(widget.gradient().stops().size(), std::size_t{3});
    QVERIFY(widget.gradient().stops()[1].t < 1.0f);
    QVERIFY(widget.gradient().stops()[1].t > 0.99f);
    // Only frequency (t) moves on drag - Cut amount stays exactly what it
    // was (interpolated to 0 from the fully-transparent default) - see
    // mouseMoveEvent()'s own docs.
    QCOMPARE(widget.gradient().stops()[1].leftOpacity, 0.0f);
    QCOMPARE(widget.gradient().stops()[1].rightOpacity, 0.0f);
}

void EqualizerCurveWidgetTest::theFirstAndLastStopsStayPutWhileDragging() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);

    const QPoint handle(xPosFor(0.0f), yPosFor(0.0f));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, handle);
    QTest::mouseMove(&widget, QPoint(xPosFor(0.0f), 120));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), 120));

    QCOMPARE(gradientSpy.count(), 0);  // Endpoints are select-only - see mouseMoveEvent()'s own docs.
    QCOMPARE(widget.gradient().stops().front().t, 0.0f);
    QCOMPARE(widget.gradient().stops().back().t, 1.0f);
}

void EqualizerCurveWidgetTest::doubleClickingAnInteriorStopRemovesIt() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    Gradient gradient;
    gradient.insertStop(0.5f);
    widget.setGradient(gradient);
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);

    QTest::mouseDClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), yPosFor(0.5f)));

    QCOMPARE(gradientSpy.count(), 1);
    QCOMPARE(widget.gradient().stops().size(), std::size_t{2});
}

void EqualizerCurveWidgetTest::doubleClickingAnEndpointDoesNothing() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);

    QTest::mouseDClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), yPosFor(0.0f)));

    QCOMPARE(gradientSpy.count(), 0);
    QCOMPARE(widget.gradient().stops().size(), std::size_t{2});
}

void EqualizerCurveWidgetTest::setGradientSyncsWithoutEmittingAndSelectsTheFirstStop() {
    EqualizerCurveWidget widget;
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);
    Gradient gradient;
    gradient.insertStop(0.3f);
    gradient.insertStop(0.7f);

    widget.setGradient(gradient);

    QCOMPARE(gradientSpy.count(), 0);
    QCOMPARE(widget.gradient().stops().size(), std::size_t{4});
    QCOMPARE(widget.selectedIndex(), std::size_t{0});
}

void EqualizerCurveWidgetTest::setProjectSettingsAcceptsAValueAndNulloptWithoutCrashing() {
    EqualizerCurveWidget widget;
    widget.resize(160, 240);

    widget.setProjectSettings(ProjectSettings{});
    widget.setProjectSettings(std::nullopt);

    // Still fully interactive afterwards - settings only affect the
    // frequency-axis guides drawn in paintEvent(), never the gradient
    // editing behavior itself.
    QSignalSpy gradientSpy(&widget, &EqualizerCurveWidget::gradientChanged);
    const QPoint empty(xPosFor(0.5f), yPosFor(0.5f));
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, empty);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, empty);
    QCOMPARE(gradientSpy.count(), 1);
}
