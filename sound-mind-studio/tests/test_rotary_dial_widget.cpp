#include "test_rotary_dial_widget.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/rotary_dial_widget.h"

using sound_mind::studio::RotaryDialWidget;

void RotaryDialWidgetTest::freshDialHasTheDefaultZeroToOneRangeAndZeroValue() {
    const RotaryDialWidget dial;
    QCOMPARE(dial.minimum(), 0.0);
    QCOMPARE(dial.maximum(), 1.0);
    QCOMPARE(dial.value(), 0.0);
}

void RotaryDialWidgetTest::sizeHintReturnsAReasonableDefault() {
    const RotaryDialWidget dial;
    QCOMPARE(dial.sizeHint(), QSize(48, 48));
}

void RotaryDialWidgetTest::setValueClampsIntoRangeAndDoesNotEmit() {
    RotaryDialWidget dial;
    QSignalSpy spy(&dial, &RotaryDialWidget::valueChanged);

    dial.setValue(5.0);

    QCOMPARE(dial.value(), 1.0);  // Clamped to the default range's own maximum.
    QCOMPARE(spy.count(), 0);
}

void RotaryDialWidgetTest::setRangeClampsTheCurrentValueAndDoesNotEmit() {
    RotaryDialWidget dial;
    dial.setValue(0.8);
    QSignalSpy spy(&dial, &RotaryDialWidget::valueChanged);

    dial.setRange(0.0, 0.5);

    QCOMPARE(dial.minimum(), 0.0);
    QCOMPARE(dial.maximum(), 0.5);
    QCOMPARE(dial.value(), 0.5);
    QCOMPARE(spy.count(), 0);
}

void RotaryDialWidgetTest::draggingUpIncreasesValueAndEmits() {
    RotaryDialWidget dial;
    dial.resize(48, 48);
    QSignalSpy spy(&dial, &RotaryDialWidget::valueChanged);

    // See rotary_dial_widget.cpp's own kPixelsPerFullSweep (150px sweeps
    // the entire [0, 1] range) - 75px up is half that, so 0.5.
    const QPoint start(24, 24);
    QTest::mousePress(&dial, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&dial, start - QPoint(0, 75));
    QTest::mouseRelease(&dial, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 75));

    QVERIFY(spy.count() >= 1);
    QVERIFY(qAbs(dial.value() - 0.5) < 0.01);
}

void RotaryDialWidgetTest::draggingDownDecreasesValueAndEmits() {
    RotaryDialWidget dial;
    dial.resize(48, 48);
    dial.setValue(0.5);
    QSignalSpy spy(&dial, &RotaryDialWidget::valueChanged);

    const QPoint start(24, 24);
    QTest::mousePress(&dial, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&dial, start + QPoint(0, 75));
    QTest::mouseRelease(&dial, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 75));

    QVERIFY(spy.count() >= 1);
    QVERIFY(qAbs(dial.value() - 0.0) < 0.01);
}

void RotaryDialWidgetTest::draggingPastTheTopClampsToTheMaximum() {
    RotaryDialWidget dial;
    dial.resize(48, 48);

    const QPoint start(24, 24);
    QTest::mousePress(&dial, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&dial, start - QPoint(0, 1000));  // Far more than a full sweep's own 150px.
    QTest::mouseRelease(&dial, Qt::LeftButton, Qt::NoModifier, start - QPoint(0, 1000));

    QCOMPARE(dial.value(), 1.0);
}

void RotaryDialWidgetTest::pressingAndReleasingWithNoMovementDoesNotEmit() {
    RotaryDialWidget dial;
    dial.resize(48, 48);
    QSignalSpy spy(&dial, &RotaryDialWidget::valueChanged);

    const QPoint pos(24, 24);
    QTest::mousePress(&dial, Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseMove(&dial, pos);
    QTest::mouseRelease(&dial, Qt::LeftButton, Qt::NoModifier, pos);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(dial.value(), 0.0);
}
