#pragma once

#include <QObject>

class NumericFieldTest : public QObject {
    Q_OBJECT

private slots:
    void cleanup();

    void parseLenientNumberAcceptsEitherDecimalMarkAndTrimsSurroundingText();
    void parseLenientNumberRejectsMalformedText();
    void formatNumberUsesTheChosenSeparatorAndDropsTrailingZeros();

    void typingDoesNotEmitUntilCommitted();
    void committingNormalizesTheTextAndEmitsOnce();
    void invalidTextRevertsWithoutEmitting();
    void anOutOfRangeValueIsClampedOnCommit();
    void setValueEmitsLikeASpinBoxAndIsSilencedBySignalBlocker();
    void draggingTheSliderPreviewsInTheTextButOnlyEmitsOnRelease();
    void aKeyboardSliderStepEmitsImmediately();
    void changingTheDecimalSeparatorReformatsExistingFields();
    void aLogarithmicSliderPlacesTheGeometricMidpointInTheMiddle();

    void integerFieldRoundsAndEmitsAnInt();
    void intensityValueUnitShowsAndAcceptsTheCanonicalValue();
    void intensityUnitSelectorSwitchesEveryFieldInItsGroup();
};
