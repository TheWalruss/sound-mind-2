#include "test_tone_curve_editor.h"

#include <array>
#include <vector>

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/tone_curve_editor.h"

using sound_mind::studio::ToneCurveEditor;

namespace {

/// @brief `plotToWidget()`'s own margin/flip formula, mirrored here so
/// tests can compute exactly which pixel a given normalized `[0,1]`
/// point lands on for a `240x160` editor (this file's own fixed test
/// size, matching `sizeHint()`) - must stay in sync with
/// `tone_curve_editor.cpp`'s own `kMargin`.
QPoint widgetPosFor(float x, float y) {
    constexpr double margin = 8.0;
    constexpr double width = 240.0 - 2 * margin;
    constexpr double height = 160.0 - 2 * margin;
    return QPoint(static_cast<int>(margin + x * width), static_cast<int>(margin + height - y * height));
}

}  // namespace

void ToneCurveEditorTest::freshEditorHasTheDefaultTwoPointIdentityCurve() {
    const ToneCurveEditor editor;
    QCOMPARE(editor.points().size(), std::size_t{2});
    QCOMPARE(editor.points().front()[0], 0.0f);
    QCOMPARE(editor.points().front()[1], 0.0f);
    QCOMPARE(editor.points().back()[0], 1.0f);
    QCOMPARE(editor.points().back()[1], 1.0f);
}

void ToneCurveEditorTest::sizeHintReturnsAReasonableDefault() {
    const ToneCurveEditor editor;
    QCOMPARE(editor.sizeHint(), QSize(240, 160));
}

void ToneCurveEditorTest::clickingNearAnExistingPointBeginsDraggingItInstead() {
    ToneCurveEditor editor;
    editor.resize(240, 160);
    QSignalSpy spy(&editor, &ToneCurveEditor::pointsChanged);

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.0f, 0.0f));
    QTest::mouseMove(&editor, widgetPosFor(0.0f, 0.5f));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.0f, 0.5f));

    QVERIFY(spy.count() >= 1);
    QCOMPARE(editor.points().size(), std::size_t{2});
    QVERIFY(qAbs(editor.points().front()[0] - 0.0f) < 0.01);
    QVERIFY(qAbs(editor.points().front()[1] - 0.5f) < 0.02);
}

void ToneCurveEditorTest::clickingEmptyAreaInsertsANewSortedPoint() {
    ToneCurveEditor editor;
    editor.resize(240, 160);
    QSignalSpy spy(&editor, &ToneCurveEditor::pointsChanged);

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.5f, 0.5f));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.5f, 0.5f));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(editor.points().size(), std::size_t{3});
    QVERIFY(qAbs(editor.points()[1][0] - 0.5f) < 0.01);
    QVERIFY(qAbs(editor.points()[1][1] - 0.5f) < 0.02);
}

void ToneCurveEditorTest::draggingAnInteriorPointClampsXBetweenItsNeighbors() {
    ToneCurveEditor editor;
    editor.resize(240, 160);
    editor.setPoints({{0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}});

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.5f, 0.5f));
    // Drag far past the last point's own x - widgetToPlot() alone clamps
    // to 1.0, and the interior-point neighbor clamp then pulls it back
    // just short of that.
    QTest::mouseMove(&editor, QPoint(1000, widgetPosFor(0.5f, 0.5f).y()));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(1000, widgetPosFor(0.5f, 0.5f).y()));

    QCOMPARE(editor.points().size(), std::size_t{3});
    QVERIFY(editor.points()[1][0] < 1.0f);
    QVERIFY(editor.points()[1][0] > 0.99f);
}

void ToneCurveEditorTest::theFirstAndLastPointsKeepXPinnedWhileDragging() {
    ToneCurveEditor editor;
    editor.resize(240, 160);

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.0f, 0.0f));
    QTest::mouseMove(&editor, QPoint(300, 40));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(300, 40));

    QCOMPARE(editor.points().front()[0], 0.0f);
    QVERIFY(editor.points().front()[1] > 0.5f);

    QTest::mousePress(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(1.0f, 1.0f));
    QTest::mouseMove(&editor, QPoint(-50, 130));
    QTest::mouseRelease(&editor, Qt::LeftButton, Qt::NoModifier, QPoint(-50, 130));

    QCOMPARE(editor.points().back()[0], 1.0f);
    QVERIFY(editor.points().back()[1] < 0.5f);
}

void ToneCurveEditorTest::doubleClickingAnInteriorPointRemovesIt() {
    ToneCurveEditor editor;
    editor.resize(240, 160);
    editor.setPoints({{0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}});
    QSignalSpy spy(&editor, &ToneCurveEditor::pointsChanged);

    QTest::mouseDClick(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.5f, 0.5f));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(editor.points().size(), std::size_t{2});
    QCOMPARE(editor.points().front()[0], 0.0f);
    QCOMPARE(editor.points().back()[0], 1.0f);
}

void ToneCurveEditorTest::doubleClickingAnEndpointDoesNothing() {
    ToneCurveEditor editor;
    editor.resize(240, 160);
    QSignalSpy spy(&editor, &ToneCurveEditor::pointsChanged);

    QTest::mouseDClick(&editor, Qt::LeftButton, Qt::NoModifier, widgetPosFor(0.0f, 0.0f));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(editor.points().size(), std::size_t{2});
}

void ToneCurveEditorTest::setPointsSyncsWithoutEmitting() {
    ToneCurveEditor editor;
    QSignalSpy spy(&editor, &ToneCurveEditor::pointsChanged);
    const std::vector<std::array<float, 2>> points{{0.0f, 0.0f}, {0.3f, 0.1f}, {0.7f, 0.9f}, {1.0f, 1.0f}};

    editor.setPoints(points);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(editor.points(), points);
}
