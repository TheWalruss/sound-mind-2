#include "test_numeric_field.h"

#include <cmath>

#include <QLineEdit>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest/QtTest>

#include "sound_mind/studio/numeric_field.h"

using sound_mind::core::DecimalSeparator;
using sound_mind::studio::formatNumber;
using sound_mind::studio::IntegerField;
using sound_mind::studio::NumericField;
using sound_mind::studio::parseLenientNumber;

namespace {

void typeAndCommit(NumericField& field, const QString& text) {
    field.lineEdit()->setText(text);
    emit field.lineEdit()->editingFinished();
}

}  // namespace

void NumericFieldTest::cleanup() { NumericField::setDecimalSeparator(DecimalSeparator::Period); }

void NumericFieldTest::parseLenientNumberAcceptsEitherDecimalMarkAndTrimsSurroundingText() {
    QCOMPARE(parseLenientNumber(QStringLiteral("1.5")).value(), 1.5);
    QCOMPARE(parseLenientNumber(QStringLiteral("1,5")).value(), 1.5);
    QCOMPARE(parseLenientNumber(QStringLiteral(" 007.50 dB")).value(), 7.5);
    QCOMPARE(parseLenientNumber(QStringLiteral("-0,25")).value(), -0.25);
    QCOMPARE(parseLenientNumber(QStringLiteral("+3")).value(), 3.0);
    QCOMPARE(parseLenientNumber(QStringLiteral("Hz 440")).value(), 440.0);
    QCOMPARE(parseLenientNumber(QStringLiteral(".5")).value(), 0.5);
    QCOMPARE(parseLenientNumber(QStringLiteral("12.")).value(), 12.0);
}

void NumericFieldTest::parseLenientNumberRejectsMalformedText() {
    QVERIFY(!parseLenientNumber(QStringLiteral("1.2.3")).has_value());
    QVERIFY(!parseLenientNumber(QStringLiteral("1,2.3")).has_value());
    QVERIFY(!parseLenientNumber(QStringLiteral("abc")).has_value());
    QVERIFY(!parseLenientNumber(QString()).has_value());
    QVERIFY(!parseLenientNumber(QStringLiteral("-")).has_value());
    QVERIFY(!parseLenientNumber(QStringLiteral("1-2")).has_value());
    QVERIFY(!parseLenientNumber(QStringLiteral("1 2")).has_value());
}

void NumericFieldTest::formatNumberUsesTheChosenSeparatorAndDropsTrailingZeros() {
    QCOMPARE(formatNumber(1.5, 2, DecimalSeparator::Period), QStringLiteral("1.5"));
    QCOMPARE(formatNumber(1.5, 2, DecimalSeparator::Comma), QStringLiteral("1,5"));
    QCOMPARE(formatNumber(2.0, 3, DecimalSeparator::Period), QStringLiteral("2"));
    QCOMPARE(formatNumber(-0.25, 2, DecimalSeparator::Comma), QStringLiteral("-0,25"));
    QCOMPARE(formatNumber(0.3333, 2, DecimalSeparator::Period), QStringLiteral("0.33"));
    QCOMPARE(formatNumber(-0.0001, 2, DecimalSeparator::Period), QStringLiteral("0"));
}

void NumericFieldTest::typingDoesNotEmitUntilCommitted() {
    NumericField field;
    field.setRange(0.0, 10.0);
    field.setDecimals(2);
    QSignalSpy spy(&field, &NumericField::valueChanged);

    field.lineEdit()->setText(QStringLiteral("4"));
    field.lineEdit()->setText(QStringLiteral("4.5"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(field.value(), 0.0);
}

void NumericFieldTest::committingNormalizesTheTextAndEmitsOnce() {
    NumericField field;
    field.setRange(0.0, 10.0);
    field.setDecimals(2);
    QSignalSpy spy(&field, &NumericField::valueChanged);

    typeAndCommit(field, QStringLiteral(" 007,50 "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 7.5);
    QCOMPARE(field.value(), 7.5);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("7.5"));

    // Committing the same value again rewrites the text but doesn't emit.
    typeAndCommit(field, QStringLiteral("7.500"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("7.5"));
}

void NumericFieldTest::invalidTextRevertsWithoutEmitting() {
    NumericField field;
    field.setRange(0.0, 10.0);
    field.setValue(2.0);
    QSignalSpy spy(&field, &NumericField::valueChanged);

    typeAndCommit(field, QStringLiteral("1.2.3"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(field.value(), 2.0);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("2"));
}

void NumericFieldTest::anOutOfRangeValueIsClampedOnCommit() {
    NumericField field;
    field.setRange(-96.0, 0.0);

    typeAndCommit(field, QStringLiteral("12"));
    QCOMPARE(field.value(), 0.0);
    typeAndCommit(field, QStringLiteral("-200"));
    QCOMPARE(field.value(), -96.0);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("-96"));
}

void NumericFieldTest::setValueEmitsLikeASpinBoxAndIsSilencedBySignalBlocker() {
    NumericField field;
    field.setRange(0.0, 10.0);
    QSignalSpy spy(&field, &NumericField::valueChanged);

    field.setValue(3.0);
    QCOMPARE(spy.count(), 1);
    field.setValue(3.0);  // unchanged - no emission.
    QCOMPARE(spy.count(), 1);
    {
        const QSignalBlocker blocker(&field);
        field.setValue(4.0);
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(field.value(), 4.0);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("4"));
}

void NumericFieldTest::draggingTheSliderPreviewsInTheTextButOnlyEmitsOnRelease() {
    NumericField field;
    field.setRange(0.0, 10.0);
    QSignalSpy spy(&field, &NumericField::valueChanged);
    QSlider* slider = field.slider();

    slider->setSliderDown(true);
    slider->setSliderPosition(slider->maximum() / 2);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(field.value(), 0.0);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("5"));

    slider->setSliderDown(false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(field.value(), 5.0);
}

void NumericFieldTest::aKeyboardSliderStepEmitsImmediately() {
    NumericField field;
    field.setRange(0.0, 10.0);
    QSignalSpy spy(&field, &NumericField::valueChanged);

    field.slider()->triggerAction(QAbstractSlider::SliderPageStepAdd);

    QCOMPARE(spy.count(), 1);
    QVERIFY(field.value() > 0.0);
}

void NumericFieldTest::changingTheDecimalSeparatorReformatsExistingFields() {
    NumericField field;
    field.setRange(0.0, 10.0);
    field.setDecimals(2);
    field.setValue(1.5);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("1.5"));

    NumericField::setDecimalSeparator(DecimalSeparator::Comma);

    QCOMPARE(field.lineEdit()->text(), QStringLiteral("1,5"));
    QCOMPARE(field.value(), 1.5);
}

void NumericFieldTest::aLogarithmicSliderPlacesTheGeometricMidpointInTheMiddle() {
    NumericField field;
    field.setRange(20.0, 20000.0);
    field.setLogarithmicSlider(true);

    field.setValue(std::sqrt(20.0 * 20000.0));

    QVERIFY(std::abs(field.slider()->value() - field.slider()->maximum() / 2) <= 1);
}

void NumericFieldTest::integerFieldRoundsAndEmitsAnInt() {
    IntegerField field;
    field.setRange(0, 16);
    QSignalSpy spy(&field, &IntegerField::valueChanged);

    field.lineEdit()->setText(QStringLiteral("3,7"));
    emit field.lineEdit()->editingFinished();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 4);
    QCOMPARE(field.value(), 4);
    QCOMPARE(field.lineEdit()->text(), QStringLiteral("4"));
}
