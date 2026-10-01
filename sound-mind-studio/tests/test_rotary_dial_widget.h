#pragma once

#include <QObject>

class RotaryDialWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void freshDialHasTheDefaultZeroToOneRangeAndZeroValue();
    void sizeHintReturnsAReasonableDefault();
    void setValueClampsIntoRangeAndDoesNotEmit();
    void setRangeClampsTheCurrentValueAndDoesNotEmit();
    void draggingUpIncreasesValueAndEmits();
    void draggingDownDecreasesValueAndEmits();
    void draggingPastTheTopClampsToTheMaximum();
    void pressingAndReleasingWithNoMovementDoesNotEmit();
};
