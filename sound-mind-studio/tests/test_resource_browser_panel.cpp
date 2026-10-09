#include "test_resource_browser_panel.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/resource_browser_panel.h"

using sound_mind::studio::ResourceBrowserPanel;
using sound_mind::studio::ResourceCategory;

namespace {

std::vector<ResourceBrowserPanel::RowData> twoRows() {
    ResourceBrowserPanel::RowData first;
    first.id = 1;
    first.name = QStringLiteral("First");
    ResourceBrowserPanel::RowData second;
    second.id = 2;
    second.name = QStringLiteral("Second");
    return {first, second};
}

}  // namespace

void ResourceBrowserPanelTest::freshPanelHasNoRowsAndNoSelectionAndNoBrowsingLabel() {
    const ResourceBrowserPanel panel;
    QVERIFY(!panel.selectedEntryId().has_value());
    QCOMPARE(panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->count(), 0);
    QCOMPARE(panel.findChild<QLabel*>(QStringLiteral("browsingLabel"))->text(), QStringLiteral("Browsing: this project"));
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("returnToCurrentProjectButton"))->isHidden());
}

void ResourceBrowserPanelTest::setEntriesPopulatesTheListAndClickingARowSelectsIt() {
    ResourceBrowserPanel panel;
    QSignalSpy spy(&panel, &ResourceBrowserPanel::entrySelected);

    panel.setEntries(twoRows());
    auto* list = panel.findChild<QListWidget*>(QStringLiteral("entriesList"));
    QCOMPARE(list->count(), 2);

    list->setCurrentRow(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.selectedEntryId(), std::optional<std::uint64_t>(2));
}

void ResourceBrowserPanelTest::setEntriesClearsSelectionAndInspector() {
    ResourceBrowserPanel panel;
    panel.setEntries(twoRows());
    panel.findChild<QListWidget*>(QStringLiteral("entriesList"))->setCurrentRow(0);
    panel.setInspector(QStringLiteral("Name"), QStringLiteral("Params"), QImage(), /*canPlay=*/false,
                       /*canExport=*/true, /*canImportEntry=*/false);

    panel.setEntries(twoRows());

    QVERIFY(!panel.selectedEntryId().has_value());
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("inspectorNameLabel"))->text().isEmpty());
}

void ResourceBrowserPanelTest::setInspectorShowsOrHidesRasterPlayAndImportControlsPerFlag() {
    ResourceBrowserPanel panel;

    panel.setInspector(QStringLiteral("My Entry"), QStringLiteral("{}"), QImage(10, 10, QImage::Format_RGB888),
                       /*canPlay=*/true, /*canExport=*/true, /*canImportEntry=*/true);

    QCOMPARE(panel.findChild<QLabel*>(QStringLiteral("inspectorNameLabel"))->text(), QStringLiteral("My Entry"));
    QCOMPARE(panel.findChild<QPlainTextEdit*>(QStringLiteral("inspectorParametersEdit"))->toPlainText(),
             QStringLiteral("{}"));
    QVERIFY(!panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("exportButton"))->isHidden());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("importEntryButton"))->isHidden());

    panel.setInspector(QStringLiteral("Other"), QStringLiteral("{}"), QImage(), /*canPlay=*/false,
                       /*canExport=*/false, /*canImportEntry=*/false);

    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("inspectorRasterLabel"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("exportButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("importEntryButton"))->isHidden());
}

void ResourceBrowserPanelTest::setPlayingTogglesPlayAndStopVisibilityOnlyWhenCanPlay() {
    ResourceBrowserPanel panel;
    panel.setInspector(QStringLiteral("Shot"), QString(), QImage(), /*canPlay=*/true, /*canExport=*/true,
                       /*canImportEntry=*/false);

    panel.setPlaying(true);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("stopButton"))->isHidden());

    panel.setPlaying(false);
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("stopButton"))->isHidden());

    // A category with no playable preview: setPlaying(true) must not make
    // either button appear, even though it's the "turn Play into Stop"
    // request - there's nothing to play, so neither should ever show.
    panel.setInspector(QStringLiteral("Wave"), QString(), QImage(), /*canPlay=*/false, /*canExport=*/true,
                       /*canImportEntry=*/false);
    panel.setPlaying(true);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("playButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("stopButton"))->isHidden());
}

void ResourceBrowserPanelTest::setBrowsingOtherProjectTogglesTheBrowseButtons() {
    ResourceBrowserPanel panel;

    panel.setBrowsingOtherProject(QStringLiteral("C:/other.smproj"));
    QCOMPARE(panel.findChild<QLabel*>(QStringLiteral("browsingLabel"))->text(),
             QStringLiteral("Browsing: C:/other.smproj"));
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("returnToCurrentProjectButton"))->isHidden());

    panel.setBrowsingOtherProject(QString());
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton"))->isHidden());
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("returnToCurrentProjectButton"))->isHidden());
}

void ResourceBrowserPanelTest::setImportFromFileEnabledTogglesTheButton() {
    ResourceBrowserPanel panel;
    panel.setImportFromFileEnabled(false);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("importFromFileButton"))->isHidden());
    panel.setImportFromFileEnabled(true);
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("importFromFileButton"))->isHidden());
}

void ResourceBrowserPanelTest::clickingEachButtonEmitsItsOwnSignal() {
    ResourceBrowserPanel panel;
    // QTest::mouseClick() needs real, laid-out geometry to hit-test
    // against - show() first, the same precedent test_chord_generator_
    // panel.cpp/test_main_window.cpp already establish for interaction-
    // heavy tests (a plain data-driven setInspector()/isHidden() check,
    // unlike an actual simulated click, doesn't need this).
    panel.show();
    panel.setInspector(QStringLiteral("Entry"), QString(), QImage(), /*canPlay=*/true, /*canExport=*/true,
                       /*canImportEntry=*/true);

    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::browseOtherProjectRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("browseOtherProjectButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::importFromFileRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("importFromFileButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::importEntryRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("importEntryButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::exportRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("exportButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::playRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("playButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    panel.setPlaying(true);
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::stopRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("stopButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::categoryChanged);
        auto* combo = panel.findChild<QComboBox*>(QStringLiteral("categoryCombo"));
        combo->setCurrentIndex(combo->currentIndex() == 0 ? 1 : 0);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::addToToolkitRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
}

void ResourceBrowserPanelTest::setToolkitEntriesPopulatesTheDraftList() {
    ResourceBrowserPanel panel;
    panel.setToolkitEntries({QStringLiteral("First (MindWave)"), QStringLiteral("Second (Filter)")});

    auto* list = panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"));
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->text(), QStringLiteral("First (MindWave)"));
    QCOMPARE(list->item(1)->text(), QStringLiteral("Second (Filter)"));

    panel.setToolkitEntries({});
    QCOMPARE(list->count(), 0);
}

void ResourceBrowserPanelTest::setAddToToolkitEnabledTogglesTheButton() {
    ResourceBrowserPanel panel;
    panel.setAddToToolkitEnabled(false);
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton"))->isEnabled());
    panel.setAddToToolkitEnabled(true);
    QVERIFY(panel.findChild<QPushButton*>(QStringLiteral("addToToolkitButton"))->isEnabled());
}

void ResourceBrowserPanelTest::clickingRemoveEmitsRemoveFromToolkitRequestedWithTheSelectedIndex() {
    ResourceBrowserPanel panel;
    panel.show();
    panel.setToolkitEntries({QStringLiteral("First"), QStringLiteral("Second")});
    panel.findChild<QListWidget*>(QStringLiteral("toolkitEntriesList"))->setCurrentRow(1);

    QSignalSpy spy(&panel, &ResourceBrowserPanel::removeFromToolkitRequested);
    QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("removeFromToolkitButton")), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);
}

void ResourceBrowserPanelTest::clickingExportOrImportToolkitEmitsItsOwnSignal() {
    ResourceBrowserPanel panel;
    panel.show();

    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::exportToolkitRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("exportToolkitButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    {
        QSignalSpy spy(&panel, &ResourceBrowserPanel::importToolkitRequested);
        QTest::mouseClick(panel.findChild<QPushButton*>(QStringLiteral("importToolkitButton")), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
}
