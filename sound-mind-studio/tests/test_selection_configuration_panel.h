#pragma once

#include <QObject>

class SelectionConfigurationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelDefaultsToRectangle();
    void changingTheSelectionTypeComboEmitsSelectionShapeChangedAndUpdatesSelectionShape();
};
