#pragma once

#include <QObject>

class PolarOriginDialogTest : public QObject {
    Q_OBJECT

private slots:
    // PolarOriginPickerWidget.
    void freshWidgetHasPlaceholderDefaults();
    void setSourceImageResetsToTheImagesOwnCentreRadiusAndFullCircle();
    void settersClampToTheSourceImagesOwnBounds();
    void draggingTheOriginHandleMovesItAndEmitsParamsChangedByDrag();
    void draggingTheRingHandleChangesOnlyTheRadiusByVerticalDistance();
    void draggingTheArcStartHandleChangesOnlyArcStart();
    void draggingTheArcEndHandleChangesOnlyArcEnd();
    void pressingAwayFromAnyHandleStartsNoDrag();

    // PolarOriginDialog.
    void dialogSpinboxesStartFromThePickersOwnDefaults();
    void editingASpinboxMovesThePickersOwnHandle();
    void draggingThePickerUpdatesTheSpinboxes();
    void outputWidthTracksTheRadiusUntilEditedDirectly();
    void durationLabelShowsADashWithNoTimestepAndARealDurationWithOne();
    void initialParamsSeedsThePickerAndSpinboxesInsteadOfTheDefaults();
    void paramsReflectsTheCurrentPickerState();
};
