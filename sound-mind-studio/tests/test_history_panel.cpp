#include "test_history_panel.h"

#include <QListWidget>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/history_panel.h"

using sound_mind::studio::HistoryPanel;

void HistoryPanelTest::freshPanelShowsOnlyTheStartRow() {
    HistoryPanel panel;

    auto* list = panel.findChild<QListWidget*>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->text(), QStringLiteral("(Start)"));
    QCOMPARE(list->currentRow(), 0);
}

void HistoryPanelTest::setHistoryShowsEachDescriptionAndHighlightsTheCurrentIndex() {
    HistoryPanel panel;
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("historyList"));

    panel.setHistory({QStringLiteral("Painted stroke"), QStringLiteral("Changed layer opacity")}, 1);

    QCOMPARE(list->count(), 3);  // "(Start)" plus the two real entries.
    QCOMPARE(list->item(0)->text(), QStringLiteral("(Start)"));
    QCOMPARE(list->item(1)->text(), QStringLiteral("Painted stroke"));
    QCOMPARE(list->item(2)->text(), QStringLiteral("Changed layer opacity"));
    QCOMPARE(list->currentRow(), 1);  // currentIndex 1 -> row 1 (right after "Painted stroke").
}

void HistoryPanelTest::setHistoryUsesAPlaceholderForAnEmptyDescription() {
    // A command pushed before descriptions existed at all (see
    // UndoCommand's own docs) has an empty description - shown as a
    // placeholder rather than a blank, unclickable-looking row.
    HistoryPanel panel;
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("historyList"));

    panel.setHistory({QString()}, 1);

    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(1)->text(), QStringLiteral("(Unnamed action)"));
}

void HistoryPanelTest::doubleClickingARowEmitsJumpRequested() {
    HistoryPanel panel;
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("historyList"));
    panel.setHistory({QStringLiteral("Painted stroke"), QStringLiteral("Changed layer opacity")}, 2);
    QSignalSpy spy(&panel, &HistoryPanel::jumpRequested);

    // Emitting QListWidget's own itemDoubleClicked() directly, rather than
    // simulating a real mouse double-click - the latter needs the view to
    // have real, laid-out geometry (a shown, sized top-level window),
    // which this headless test deliberately doesn't set up. This still
    // exercises HistoryPanel's own connected handler exactly the same way
    // a genuine double-click would trigger it.
    emit list->itemDoubleClicked(list->item(1));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<std::size_t>(), std::size_t{1});
}
