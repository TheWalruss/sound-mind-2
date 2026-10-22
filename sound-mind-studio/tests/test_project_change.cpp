#include "test_project_change.h"

#include <QtTest/QtTest>

#include "sound_mind/studio/project_change.h"

using sound_mind::studio::ProjectChange;

// v0.1.48.1 - what each kind of edit tells MainWindow::applyProjectChange().
void ProjectChangeTest::settingsOnlyChangesNothingShown() {
    const ProjectChange change = ProjectChange::settingsOnly();
    QVERIFY(change.canvas == ProjectChange::Canvas::Unchanged);
    QVERIFY(!change.audible && !change.analysis && !change.layerList && !change.thumbnail.has_value());
    QVERIFY(!change.history && !change.composer && !change.recordTargets);
}

void ProjectChangeTest::addedLayersRefreshEverythingListingLayers() {
    const ProjectChange change = ProjectChange::layersAdded();
    QVERIFY(change.canvas == ProjectChange::Canvas::All);
    QVERIFY(change.audible && change.layerList && change.history && change.composer && change.recordTargets);
}

void ProjectChangeTest::layerContentRefoldsFromThatLayerAndItsThumbnail() {
    const ProjectChange change = ProjectChange::layerContent(3, 42);
    QVERIFY(change.canvas == ProjectChange::Canvas::FromLayer);
    QCOMPARE(change.fromLayerIndex, std::size_t{3});
    QCOMPARE(change.thumbnail, std::optional<sound_mind::core::LayerId>(42));
    QVERIFY(change.audible && change.history && change.composer);
    QVERIFY(!change.layerList);
}

void ProjectChangeTest::layerPropertiesLeaveCanvasAndPlaybackToTheLayerController() {
    const ProjectChange change = ProjectChange::layerProperties();
    QVERIFY(change.canvas == ProjectChange::Canvas::Unchanged);
    QVERIFY(!change.audible);
    QVERIFY(change.analysis && change.history && change.composer && change.recordTargets);
}
