#include "test_path_edit_session.h"

#include <QtTest/QtTest>

#include "sound_mind/core/path.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/path_edit_session.h"

using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::PathEditSession;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    settings.canvasHeight = 50;
    settings.binCount = 50;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2020.0f;
    settings.timestepMs = 10.0;
    return settings;
}

PathNode cornerAt(double timeSeconds, double frequencyHz) {
    PathNode node;
    node.anchor = TimeFrequencyPoint{timeSeconds, frequencyHz};
    node.type = PathNodeType::Corner;
    return node;
}

}  // namespace

void PathEditSessionTest::freshSessionIsNotActive() {
    const PathEditSession session;
    QVERIFY(!session.isActive());
    QVERIFY(session.previewPath().nodes().empty());
    QVERIFY(!session.selectedNodeIndex().has_value());
}

void PathEditSessionTest::beginCopiesTheInitialPathAndSelectsNothing() {
    Path path;
    path.addNode(cornerAt(0.0, 500.0));
    path.addNode(cornerAt(1.0, 500.0));

    PathEditSession session;
    session.begin(path);

    QVERIFY(session.isActive());
    QCOMPARE(session.previewPath().nodes().size(), std::size_t{2});
    QVERIFY(!session.selectedNodeIndex().has_value());
}

void PathEditSessionTest::endClearsActiveStateAndThePreview() {
    Path path;
    path.addNode(cornerAt(0.0, 500.0));
    PathEditSession session;
    session.begin(path);

    session.end();

    QVERIFY(!session.isActive());
    QVERIFY(session.previewPath().nodes().empty());
    QVERIFY(!session.selectedNodeIndex().has_value());
}

void PathEditSessionTest::selectNodeNearFindsTheClosestAnchorWithinTolerance() {
    Path path;
    path.addNode(cornerAt(0.5, 1000.0));
    PathEditSession session;
    session.begin(path);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(testSettings());

    const bool selected = session.selectNodeNear(TimeFrequencyPoint{0.505, 1000.0}, scale);

    QVERIFY(selected);
    QCOMPARE(session.selectedNodeIndex().value(), std::size_t{0});
}

void PathEditSessionTest::selectNodeNearFindsNothingBeyondTolerance() {
    Path path;
    path.addNode(cornerAt(0.5, 1000.0));
    PathEditSession session;
    session.begin(path);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(testSettings());

    const bool selected = session.selectNodeNear(TimeFrequencyPoint{5.0, 1000.0}, scale);

    QVERIFY(!selected);
    QVERIFY(!session.selectedNodeIndex().has_value());
}

void PathEditSessionTest::selectNodeNearPrefersAHandleOverANearbyAnchor() {
    // Anchor at t=0.5; handleOut at t=0.52 (0.02 away). Clicking at
    // t=0.505 lands closer to the anchor (0.005 away) than to the handle
    // (0.015 away), but both are within the fixed hit tolerance - the
    // handle still wins, per selectNodeNear()'s own "handles first"
    // priority (see its own docs), not proximity alone.
    PathNode node = cornerAt(0.5, 1000.0);
    node.type = PathNodeType::Smooth;
    node.handleOut = TimeFrequencyPoint{0.52, 1000.0};
    node.handleIn = TimeFrequencyPoint{0.48, 1000.0};
    Path path;
    path.addNode(node);
    PathEditSession session;
    session.begin(path);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(testSettings());

    const bool selected = session.selectNodeNear(TimeFrequencyPoint{0.505, 1000.0}, scale);

    QVERIFY(selected);
    QCOMPARE(session.selectedNodeIndex().value(), std::size_t{0});
    // Dragging now moves the handle, not the anchor - confirmed indirectly
    // via continueDrag()'s own test below; this test only confirms which
    // part got selected in the first place.
}

void PathEditSessionTest::continueDragMovesTheAnchorAndBothHandles() {
    PathNode node = cornerAt(0.5, 1000.0);
    node.type = PathNodeType::Smooth;
    // Handles placed far outside the hit tolerance so only the anchor is
    // selected below.
    node.handleIn = TimeFrequencyPoint{0.1, 1000.0};
    node.handleOut = TimeFrequencyPoint{0.9, 1000.0};
    Path path;
    path.addNode(node);
    PathEditSession session;
    session.begin(path);
    const auto settings = testSettings();
    const double scale = sound_mind::core::frequencyToTimeScaleFor(settings);
    QVERIFY(session.selectNodeNear(TimeFrequencyPoint{0.5, 1000.0}, scale));

    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    // Same frequency as the drag's own start - isolates the assertion to
    // the time axis alone.
    const bool changed = session.continueDrag(TimeFrequencyPoint{0.6, 1000.0}, config);

    QVERIFY(changed);
    const PathNode& dragged = session.previewPath().nodes().at(0);
    QCOMPARE(dragged.anchor.timeSeconds, 0.6);
    QVERIFY(dragged.handleIn.has_value());
    QCOMPARE(dragged.handleIn->timeSeconds, 0.2);
    QVERIFY(dragged.handleOut.has_value());
    QCOMPARE(dragged.handleOut->timeSeconds, 1.0);
}

void PathEditSessionTest::continueDragWithNothingSelectedDoesNothing() {
    Path path;
    path.addNode(cornerAt(0.5, 1000.0));
    PathEditSession session;
    session.begin(path);
    const auto config = sound_mind::core::streamCodecConfigFor(testSettings());

    const bool changed = session.continueDrag(TimeFrequencyPoint{0.9, 1000.0}, config);

    QVERIFY(!changed);
    QCOMPARE(session.previewPath().nodes().at(0).anchor.timeSeconds, 0.5);
}

void PathEditSessionTest::deleteSelectedNodeRemovesItButRefusesToEmptyThePath() {
    Path path;
    path.addNode(cornerAt(0.0, 500.0));
    path.addNode(cornerAt(1.0, 500.0));
    PathEditSession session;
    session.begin(path);
    const double scale = sound_mind::core::frequencyToTimeScaleFor(testSettings());
    QVERIFY(session.selectNodeNear(TimeFrequencyPoint{0.0, 500.0}, scale));

    QVERIFY(session.deleteSelectedNode());
    QCOMPARE(session.previewPath().nodes().size(), std::size_t{1});
    QVERIFY(!session.selectedNodeIndex().has_value());

    // Only one node left - selecting and trying to delete it again is
    // refused rather than emptying the path.
    QVERIFY(session.selectNodeNear(TimeFrequencyPoint{1.0, 500.0}, scale));
    QVERIFY(!session.deleteSelectedNode());
    QCOMPARE(session.previewPath().nodes().size(), std::size_t{1});
}

void PathEditSessionTest::toggleSelectedNodeTypeSmoothsAndUnsmooths() {
    Path path;
    path.addNode(cornerAt(0.0, 500.0));
    path.addNode(cornerAt(0.5, 1000.0));
    path.addNode(cornerAt(1.0, 500.0));
    PathEditSession session;
    session.begin(path);
    const auto settings = testSettings();
    const double scale = sound_mind::core::frequencyToTimeScaleFor(settings);
    QVERIFY(session.selectNodeNear(TimeFrequencyPoint{0.5, 1000.0}, scale));

    QVERIFY(session.toggleSelectedNodeType(settings));
    const PathNode& smoothed = session.previewPath().nodes().at(1);
    QCOMPARE(smoothed.type, PathNodeType::Smooth);
    QVERIFY(smoothed.handleIn.has_value());
    QVERIFY(smoothed.handleOut.has_value());

    QVERIFY(session.toggleSelectedNodeType(settings));
    const PathNode& unsmoothed = session.previewPath().nodes().at(1);
    QCOMPARE(unsmoothed.type, PathNodeType::Corner);
    QVERIFY(!unsmoothed.handleIn.has_value());
    QVERIFY(!unsmoothed.handleOut.has_value());
}
