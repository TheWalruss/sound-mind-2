#pragma once

#include <QObject>

class HistoryPanelTest : public QObject {
    Q_OBJECT

private slots:
    void freshPanelShowsOnlyTheStartRow();
    void setHistoryShowsEachDescriptionAndHighlightsTheCurrentIndex();
    void setHistoryUsesAPlaceholderForAnEmptyDescription();
    void doubleClickingARowEmitsJumpRequested();
};
