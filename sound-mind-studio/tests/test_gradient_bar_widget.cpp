#include "test_gradient_bar_widget.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/gradient_bar_widget.h"

using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;
using sound_mind::studio::GradientBarWidget;

namespace {

/// @brief `tToX()`'s own margin formula, mirrored here so tests can
/// compute exactly which pixel a given normalized `t` lands on for a
/// `240`-wide bar (this file's own fixed test size, matching
/// `sizeHint()`) - must stay in sync with `gradient_bar_widget.cpp`'s own
/// `kMargin`.
int xPosFor(float t) {
    constexpr double margin = 8.0;
    constexpr double width = 240.0 - 2 * margin;
    return static_cast<int>(margin + static_cast<double>(t) * width);
}

}  // namespace

void GradientBarWidgetTest::freshBarHasTheDefaultTwoStopTransparentGradient() {
    const GradientBarWidget bar;
    QCOMPARE(bar.gradient().stops().size(), std::size_t{2});
    QCOMPARE(bar.gradient().stops().front().t, 0.0f);
    QCOMPARE(bar.gradient().stops().back().t, 1.0f);
    QCOMPARE(bar.gradient().stops().front().leftOpacity, 0.0f);  // fully transparent - see Gradient's own docs.
    QCOMPARE(bar.selectedIndex(), std::size_t{0});
}

void GradientBarWidgetTest::sizeHintReturnsAReasonableDefault() {
    const GradientBarWidget bar;
    QCOMPARE(bar.sizeHint(), QSize(240, 48));
}

void GradientBarWidgetTest::clickingNearAnExistingStopSelectsItWithoutInserting() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);
    QSignalSpy selectionSpy(&bar, &GradientBarWidget::selectionChanged);

    QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(1.0f), 24));
    QTest::mouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(1.0f), 24));

    QCOMPARE(gradientSpy.count(), 0);  // A plain select, no drag - nothing about the gradient itself changed.
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{2});  // Still just the two endpoints - no insert.
    QCOMPARE(bar.selectedIndex(), std::size_t{1});
}

void GradientBarWidgetTest::clickingEmptyAreaInsertsANewSortedStopAndSelectsIt() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);
    QSignalSpy selectionSpy(&bar, &GradientBarWidget::selectionChanged);

    QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    QTest::mouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));

    QCOMPARE(gradientSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{3});
    QVERIFY(qAbs(bar.gradient().stops()[1].t - 0.5f) < 0.01f);
    QCOMPARE(bar.selectedIndex(), std::size_t{1});
}

void GradientBarWidgetTest::draggingAnInteriorStopClampsTBetweenItsNeighbors() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    Gradient gradient;
    gradient.insertStop(0.5f);
    bar.setGradient(gradient);

    QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    // Drag far past the last stop's own t - xToT() alone clamps to 1.0,
    // and the interior-stop neighbor clamp then pulls it back just short
    // of that.
    QTest::mouseMove(&bar, QPoint(1000, 24));
    QTest::mouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(1000, 24));

    QCOMPARE(bar.gradient().stops().size(), std::size_t{3});
    QVERIFY(bar.gradient().stops()[1].t < 1.0f);
    QVERIFY(bar.gradient().stops()[1].t > 0.99f);
}

void GradientBarWidgetTest::theFirstAndLastStopsStayPutWhileDragging() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    QTest::mousePress(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), 24));
    QTest::mouseMove(&bar, QPoint(120, 24));
    QTest::mouseRelease(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(120, 24));

    QCOMPARE(gradientSpy.count(), 0);  // Endpoints are select-only - see mouseMoveEvent()'s own docs.
    QCOMPARE(bar.gradient().stops().front().t, 0.0f);
    QCOMPARE(bar.gradient().stops().back().t, 1.0f);
}

void GradientBarWidgetTest::doubleClickingAnInteriorStopRemovesIt() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    Gradient gradient;
    gradient.insertStop(0.5f);
    bar.setGradient(gradient);
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    QTest::mouseDClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));

    QCOMPARE(gradientSpy.count(), 1);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{2});
}

void GradientBarWidgetTest::doubleClickingAnEndpointDoesNothing() {
    GradientBarWidget bar;
    bar.resize(240, 48);
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    QTest::mouseDClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.0f), 24));

    QCOMPARE(gradientSpy.count(), 0);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{2});
}

void GradientBarWidgetTest::setGradientSyncsWithoutEmittingAndSelectsTheFirstStop() {
    GradientBarWidget bar;
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);
    Gradient gradient;
    gradient.insertStop(0.3f);
    gradient.insertStop(0.7f);

    bar.setGradient(gradient);

    QCOMPARE(gradientSpy.count(), 0);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{4});
    QCOMPARE(bar.selectedIndex(), std::size_t{0});
}

void GradientBarWidgetTest::removeSelectedStopRefusesAnEndpoint() {
    GradientBarWidget bar;
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    bar.removeSelectedStop();  // selectedIndex() defaults to 0, the first endpoint.

    QCOMPARE(gradientSpy.count(), 0);
    QCOMPARE(bar.gradient().stops().size(), std::size_t{2});
}

void GradientBarWidgetTest::setSelectedStopValuesAppliesWithoutMovingOrChangingSelection() {
    GradientBarWidget bar;
    Gradient gradient;
    gradient.insertStop(0.5f);
    bar.setGradient(gradient);  // selectedIndex() defaults to 0 - see its own docs.
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    GradientStop values;
    values.t = 0.9f;  // Deliberately different from the selected stop's own t - should be ignored.
    values.leftIntensity = -20.0f;
    values.rightIntensity = -30.0f;
    values.leftOpacity = 0.5f;
    values.rightOpacity = 0.6f;
    bar.setSelectedStopValues(values);

    QCOMPARE(gradientSpy.count(), 1);
    QCOMPARE(bar.selectedIndex(), std::size_t{0});
    QCOMPARE(bar.gradient().stops().front().t, 0.0f);  // Position unchanged.
    QCOMPARE(bar.gradient().stops().front().leftIntensity, -20.0f);
    QCOMPARE(bar.gradient().stops().front().rightIntensity, -30.0f);
    QCOMPARE(bar.gradient().stops().front().leftOpacity, 0.5f);
    QCOMPARE(bar.gradient().stops().front().rightOpacity, 0.6f);
}

void GradientBarWidgetTest::setLinkChannelsEmitsWithTheFlagSet() {
    GradientBarWidget bar;
    QSignalSpy gradientSpy(&bar, &GradientBarWidget::gradientChanged);

    bar.setLinkChannels(true);

    QCOMPARE(gradientSpy.count(), 1);
    QVERIFY(bar.gradient().linkChannels());
}
