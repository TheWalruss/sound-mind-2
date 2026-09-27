#include "test_polar_origin_dialog.h"

#include <optional>

#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtMath>
#include <QtTest/QtTest>

#include "sound_mind/studio/polar_origin_dialog.h"

using sound_mind::studio::PolarImportParams;
using sound_mind::studio::PolarOriginDialog;
using sound_mind::studio::PolarOriginPickerWidget;

namespace {

constexpr double kTwoPi = 6.283185307179586;

QImage makeTestImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGB888);
    image.fill(Qt::gray);
    return image;
}

}  // namespace

// ---- PolarOriginPickerWidget ------------------------------------------

void PolarOriginDialogTest::freshWidgetHasPlaceholderDefaults() {
    PolarOriginPickerWidget widget;

    QCOMPARE(widget.originX(), 0.0);
    QCOMPARE(widget.originY(), 0.0);
    QCOMPARE(widget.radius(), 1.0);
    QCOMPARE(widget.arcStartRadians(), 0.0);
    QCOMPARE(widget.arcEndRadians(), 0.0);
}

void PolarOriginDialogTest::setSourceImageResetsToTheImagesOwnCentreRadiusAndFullCircle() {
    PolarOriginPickerWidget widget;

    widget.setSourceImage(makeTestImage(100, 60));

    QCOMPARE(widget.originX(), 50.0);
    QCOMPARE(widget.originY(), 30.0);
    QVERIFY(qAbs(widget.radius() - 27.0) < 0.01);  // min(100,60)/2 * 0.9 = 27.
    QCOMPARE(widget.arcStartRadians(), 0.0);
    QCOMPARE(widget.arcEndRadians(), 0.0);
}

void PolarOriginDialogTest::settersClampToTheSourceImagesOwnBounds() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));

    widget.setOriginX(500.0);
    QCOMPARE(widget.originX(), 100.0);
    widget.setOriginX(-10.0);
    QCOMPARE(widget.originX(), 0.0);

    widget.setOriginY(500.0);
    QCOMPARE(widget.originY(), 60.0);
    widget.setOriginY(-10.0);
    QCOMPARE(widget.originY(), 0.0);

    widget.setRadius(-5.0);
    QCOMPARE(widget.radius(), 1.0);  // Never below 1 - see this setter's own docs.

    // Wraps into [0, 2*pi) rather than storing a raw out-of-range angle.
    widget.setArcStartRadians(-1.0);
    QVERIFY(widget.arcStartRadians() > 0.0 && widget.arcStartRadians() < 6.29);
    widget.setArcEndRadians(10.0);  // > 2*pi.
    QVERIFY(widget.arcEndRadians() >= 0.0 && widget.arcEndRadians() < 6.29);
}

void PolarOriginDialogTest::draggingTheOriginHandleMovesItAndEmitsParamsChangedByDrag() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));
    widget.resize(200, 200);
    // scale = min(200/100, 200/60) = 2.0; image (0,0) -> widget (0, 40).
    // Origin defaults to image (50, 30) -> widget (100, 100).

    QSignalSpy spy(&widget, &PolarOriginPickerWidget::paramsChangedByDrag);
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseMove(&widget, QPoint(140, 120));  // -> image (70, 40).
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(140, 120));

    QVERIFY(spy.count() >= 1);
    QVERIFY(qAbs(widget.originX() - 70.0) < 0.5);
    QVERIFY(qAbs(widget.originY() - 40.0) < 0.5);
}

void PolarOriginDialogTest::draggingTheRingHandleChangesOnlyTheRadiusByVerticalDistance() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));
    widget.resize(200, 200);
    // Moves both arc handles off twelve o'clock first - otherwise they'd
    // shadow the ring handle's own hit area there (both start off exactly
    // coincident with it at the widget's own fresh-image default - see
    // this class's own hit-test priority, which checks the arc handles
    // before the ring one).
    widget.setArcStartRadians(qDegreesToRadians(90.0));
    widget.setArcEndRadians(qDegreesToRadians(180.0));

    // Ring handle sits at image (originX, originY - radius) = (50, 3) ->
    // widget (0 + 50*2, 40 + 3*2) = (100, 46).
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 46));
    // Drag to image y=10 (20 above the origin's own y=30) -> radius=20.
    // Widget y for image y=10: 40 + 10*2 = 60.
    QTest::mouseMove(&widget, QPoint(100, 60));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 60));

    QVERIFY(qAbs(widget.radius() - 20.0) < 0.5);
    QVERIFY(qAbs(qRadiansToDegrees(widget.arcStartRadians()) - 90.0) < 0.5);
    QVERIFY(qAbs(qRadiansToDegrees(widget.arcEndRadians()) - 180.0) < 0.5);
}

void PolarOriginDialogTest::draggingTheArcStartHandleChangesOnlyArcStart() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));
    widget.resize(200, 200);
    // At the widget's own fresh default (arcStart == arcEnd == 0), the
    // arc-start handle sits at exactly the same point the ring handle
    // does (twelve o'clock) - hit-tested first, per this class's own
    // priority, so a press there grabs arc-start, not the ring.
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 46));
    // Drag to three o'clock (image (77, 30), 27px right of the origin).
    QTest::mouseMove(&widget, QPoint(254, 100));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(254, 100));

    QVERIFY(qAbs(qRadiansToDegrees(widget.arcStartRadians()) - 90.0) < 2.0);
    QVERIFY(qAbs(widget.radius() - 27.0) < 0.5);  // Unchanged.
}

void PolarOriginDialogTest::draggingTheArcEndHandleChangesOnlyArcEnd() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));
    widget.resize(200, 200);
    // Moves arc-start away from twelve o'clock first, so it stops
    // shadowing arc-end's own (still-default) position there - see
    // draggingTheArcStartHandleChangesOnlyArcStart()'s own comment on
    // this same coincidence.
    widget.setArcStartRadians(qDegreesToRadians(90.0));

    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 46));
    // Drag to six o'clock (image (50, 57), 27px below the origin).
    QTest::mouseMove(&widget, QPoint(100, 154));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(100, 154));

    QVERIFY(qAbs(qRadiansToDegrees(widget.arcEndRadians()) - 180.0) < 2.0);
    QVERIFY(qAbs(qRadiansToDegrees(widget.arcStartRadians()) - 90.0) < 0.5);  // Unchanged.
}

void PolarOriginDialogTest::pressingAwayFromAnyHandleStartsNoDrag() {
    PolarOriginPickerWidget widget;
    widget.setSourceImage(makeTestImage(100, 60));
    widget.resize(200, 200);
    const double originXBefore = widget.originX();
    const double originYBefore = widget.originY();

    QSignalSpy spy(&widget, &PolarOriginPickerWidget::paramsChangedByDrag);
    // Top-left corner - far from every handle.
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    QTest::mouseMove(&widget, QPoint(50, 50));
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(widget.originX(), originXBefore);
    QCOMPARE(widget.originY(), originYBefore);
}

// ---- PolarOriginDialog --------------------------------------------------

void PolarOriginDialogTest::dialogSpinboxesStartFromThePickersOwnDefaults() {
    PolarOriginDialog dialog(makeTestImage(100, 60));

    auto* originXSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("originXSpinBox"));
    auto* originYSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("originYSpinBox"));
    auto* radiusSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("radiusSpinBox"));
    auto* arcStartSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("arcStartSpinBox"));
    auto* arcEndSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("arcEndSpinBox"));
    QVERIFY(originXSpinBox != nullptr);
    QVERIFY(originYSpinBox != nullptr);
    QVERIFY(radiusSpinBox != nullptr);
    QVERIFY(arcStartSpinBox != nullptr);
    QVERIFY(arcEndSpinBox != nullptr);

    QCOMPARE(originXSpinBox->value(), 50.0);
    QCOMPARE(originYSpinBox->value(), 30.0);
    QVERIFY(qAbs(radiusSpinBox->value() - 27.0) < 0.01);
    QCOMPARE(arcStartSpinBox->value(), 0.0);
    QCOMPARE(arcEndSpinBox->value(), 360.0);  // "Full circle" reads as 360, not 0 - see the class's own docs.
}

void PolarOriginDialogTest::editingASpinboxMovesThePickersOwnHandle() {
    PolarOriginDialog dialog(makeTestImage(100, 60));
    auto* originXSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("originXSpinBox"));
    auto* picker = dialog.findChild<PolarOriginPickerWidget*>(QStringLiteral("picker"));
    QVERIFY(originXSpinBox != nullptr);
    QVERIFY(picker != nullptr);

    originXSpinBox->setValue(10.0);

    QCOMPARE(picker->originX(), 10.0);
}

void PolarOriginDialogTest::draggingThePickerUpdatesTheSpinboxes() {
    PolarOriginDialog dialog(makeTestImage(100, 60));
    auto* picker = dialog.findChild<PolarOriginPickerWidget*>(QStringLiteral("picker"));
    auto* originXSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("originXSpinBox"));
    QVERIFY(picker != nullptr);
    QVERIFY(originXSpinBox != nullptr);
    picker->resize(200, 200);

    // Origin at widget (100, 100) - see draggingTheOriginHandleMovesItAndEmitsParamsChangedByDrag()'s own comment.
    QTest::mousePress(picker, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseMove(picker, QPoint(140, 100));  // -> image (70, 30).
    QTest::mouseRelease(picker, Qt::LeftButton, Qt::NoModifier, QPoint(140, 100));

    QVERIFY(qAbs(originXSpinBox->value() - 70.0) < 0.5);
}

void PolarOriginDialogTest::outputWidthTracksTheRadiusUntilEditedDirectly() {
    PolarOriginDialog dialog(makeTestImage(100, 60));  // defaultOutputWidth left at 0 - tracks the radius.
    auto* radiusSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("radiusSpinBox"));
    auto* outputWidthSpinBox = dialog.findChild<QSpinBox*>(QStringLiteral("outputWidthSpinBox"));
    QVERIFY(radiusSpinBox != nullptr);
    QVERIFY(outputWidthSpinBox != nullptr);

    QCOMPARE(outputWidthSpinBox->value(), static_cast<int>(kTwoPi * 27.0));  // ~169.

    radiusSpinBox->setValue(50.0);
    QCOMPARE(outputWidthSpinBox->value(), static_cast<int>(kTwoPi * 50.0));  // ~314 - still tracking.

    outputWidthSpinBox->setValue(500);  // A direct edit - stops tracking from here on.
    radiusSpinBox->setValue(10.0);
    QCOMPARE(outputWidthSpinBox->value(), 500);  // Unchanged by the radius edit above.
}

void PolarOriginDialogTest::durationLabelShowsADashWithNoTimestepAndARealDurationWithOne() {
    PolarOriginDialog noTimestepDialog(makeTestImage(100, 60));
    auto* noTimestepLabel = noTimestepDialog.findChild<QLabel*>(QStringLiteral("durationLabel"));
    QVERIFY(noTimestepLabel != nullptr);
    QCOMPARE(noTimestepLabel->text(), QStringLiteral("—"));

    PolarOriginDialog withTimestepDialog(makeTestImage(100, 60), std::nullopt, /*defaultOutputWidth=*/100,
                                         /*timestepMs=*/10.0);
    auto* label = withTimestepDialog.findChild<QLabel*>(QStringLiteral("durationLabel"));
    QVERIFY(label != nullptr);
    // 100 columns * 10ms = 1000ms = 1.0s.
    QCOMPARE(label->text(), QStringLiteral("0:01.0"));
}

void PolarOriginDialogTest::initialParamsSeedsThePickerAndSpinboxesInsteadOfTheDefaults() {
    PolarImportParams initial;
    initial.originX = 5.0;
    initial.originY = 6.0;
    initial.radius = 7.0;
    initial.arcStartRadians = 0.1;
    initial.arcEndRadians = 0.2;
    initial.outputWidth = 42;

    PolarOriginDialog dialog(makeTestImage(100, 60), initial);

    auto* picker = dialog.findChild<PolarOriginPickerWidget*>(QStringLiteral("picker"));
    auto* outputWidthSpinBox = dialog.findChild<QSpinBox*>(QStringLiteral("outputWidthSpinBox"));
    QVERIFY(picker != nullptr);
    QVERIFY(outputWidthSpinBox != nullptr);

    QCOMPARE(picker->originX(), 5.0);
    QCOMPARE(picker->originY(), 6.0);
    QCOMPARE(picker->radius(), 7.0);
    QVERIFY(qAbs(picker->arcStartRadians() - 0.1) < 1e-9);
    QVERIFY(qAbs(picker->arcEndRadians() - 0.2) < 1e-9);
    QCOMPARE(outputWidthSpinBox->value(), 42);
}

void PolarOriginDialogTest::paramsReflectsTheCurrentPickerState() {
    PolarOriginDialog dialog(makeTestImage(100, 60));
    auto* originXSpinBox = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("originXSpinBox"));
    auto* outputWidthSpinBox = dialog.findChild<QSpinBox*>(QStringLiteral("outputWidthSpinBox"));
    QVERIFY(originXSpinBox != nullptr);
    QVERIFY(outputWidthSpinBox != nullptr);

    originXSpinBox->setValue(12.0);
    outputWidthSpinBox->setValue(200);

    const PolarImportParams params = dialog.params();
    QCOMPARE(params.originX, 12.0);
    QCOMPARE(params.outputWidth, static_cast<std::uint32_t>(200));
}
