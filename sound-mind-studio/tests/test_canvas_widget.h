#pragma once

#include <QObject>

class CanvasWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void sizeHintFallsBackWithNoProject();
    void sizeHintMatchesProjectCanvasDimensions();
    void rendersALayersContentInsteadOfThePlaceholder();
};
