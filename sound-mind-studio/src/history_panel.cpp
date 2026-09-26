#include "sound_mind/studio/history_panel.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollArea>

namespace sound_mind::studio {

HistoryPanel::HistoryPanel(QWidget* parent) : QDockWidget(tr("History"), parent) {
    // Same feature set/minimum width every other dockable panel here
    // establishes (see LayersPanel's own docs on DockWidgetClosable's
    // specific importance for the toolbar toggle to actually work).
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(220);

    list_ = new QListWidget();
    list_->setObjectName(QStringLiteral("historyList"));
    list_->setSelectionMode(QListWidget::SingleSelection);
    list_->setToolTip(tr("Double-click an entry to jump to that point in history"));
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { emit jumpRequested(static_cast<std::size_t>(list_->row(item))); });

    // Wrapped in a real QScrollArea - matching LayersPanel's own
    // established convention for a dock whose content can grow past its
    // own current height.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(list_);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);

    setHistory({}, 0);
}

void HistoryPanel::setHistory(const QStringList& descriptions, std::size_t currentIndex) {
    list_->clear();
    list_->addItem(tr("(Start)"));
    for (const QString& description : descriptions) {
        list_->addItem(description.isEmpty() ? tr("(Unnamed action)") : description);
    }
    if (static_cast<int>(currentIndex) < list_->count()) {
        list_->setCurrentRow(static_cast<int>(currentIndex));
    }
}

}  // namespace sound_mind::studio
