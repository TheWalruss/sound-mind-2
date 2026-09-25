#include "test_gradient_editor_widget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/gradient_bar_widget.h"
#include "sound_mind/studio/gradient_editor_widget.h"

using sound_mind::core::Gradient;
using sound_mind::studio::GradientBarWidget;
using sound_mind::studio::GradientEditorWidget;

namespace {

/// @brief See test_gradient_bar_widget.cpp's own identical helper - kept
/// as its own local copy rather than shared, matching this codebase's own
/// "a little duplication is fine for one clearly-scoped formula" precedent
/// (`ToneCurveEditorTest`'s own comment on `FilterConfigurationPanelTest`'s
/// matching duplicate makes the same call).
int xPosFor(float t) {
    constexpr double margin = 8.0;
    constexpr double width = 240.0 - 2 * margin;
    return static_cast<int>(margin + static_cast<double>(t) * width);
}

}  // namespace

void GradientEditorWidgetTest::freshEditorHasTheDefaultTwoStopTransparentGradient() {
    const GradientEditorWidget editor;
    QCOMPARE(editor.gradient().stops().size(), std::size_t{2});
    QCOMPARE(editor.gradient().stops().front().leftOpacity, 0.0f);
}

void GradientEditorWidgetTest::spinBoxesReflectTheInitiallySelectedStop() {
    GradientEditorWidget editor;
    Gradient gradient;
    auto stop = gradient.stops().front();
    stop.leftIntensity = -20.0f;
    stop.rightIntensity = -30.0f;
    stop.leftOpacity = 0.4f;
    stop.rightOpacity = 0.6f;
    gradient.setStopValues(0, stop);
    editor.setGradient(gradient);

    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->value(), -20.0);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientRightIntensitySpinBox"))->value(), -30.0);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftOpacitySpinBox"))->value(), 0.4);
    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientRightOpacitySpinBox"))->value(), 0.6);
}

void GradientEditorWidgetTest::editingASpinBoxUpdatesTheSelectedStopAndEmits() {
    GradientEditorWidget editor;
    QSignalSpy spy(&editor, &GradientEditorWidget::gradientChanged);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->setValue(-15.0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(editor.gradient().stops().front().leftIntensity, -15.0f);
    QCOMPARE(editor.gradient().stops().front().rightIntensity, 0.0f);  // Not linked - untouched.
}

void GradientEditorWidgetTest::linkChannelsMirrorsLeftEditsToRight() {
    GradientEditorWidget editor;
    editor.findChild<QCheckBox*>(QStringLiteral("gradientLinkChannelsCheckBox"))->setChecked(true);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->setValue(-15.0);

    QCOMPARE(editor.gradient().stops().front().leftIntensity, -15.0f);
    QCOMPARE(editor.gradient().stops().front().rightIntensity, -15.0f);
}

void GradientEditorWidgetTest::linkChannelsMirrorsRightEditsToLeft() {
    GradientEditorWidget editor;
    editor.findChild<QCheckBox*>(QStringLiteral("gradientLinkChannelsCheckBox"))->setChecked(true);

    editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientRightOpacitySpinBox"))->setValue(0.7);

    QCOMPARE(editor.gradient().stops().front().rightOpacity, 0.7f);
    QCOMPARE(editor.gradient().stops().front().leftOpacity, 0.7f);
}

void GradientEditorWidgetTest::selectingADifferentStopOnTheBarUpdatesTheSpinBoxes() {
    GradientEditorWidget editor;
    Gradient gradient;
    gradient.insertStop(0.5f);
    auto stop = gradient.stops()[1];
    stop.leftIntensity = -10.0f;
    gradient.setStopValues(1, stop);
    editor.setGradient(gradient);
    auto* bar = editor.findChild<GradientBarWidget*>(QStringLiteral("gradientBar"));
    QVERIFY(bar != nullptr);
    bar->resize(240, 48);

    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));

    QCOMPARE(editor.findChild<QDoubleSpinBox*>(QStringLiteral("gradientLeftIntensitySpinBox"))->value(), -10.0);
}

void GradientEditorWidgetTest::deleteStopButtonIsDisabledForEndpointsAndEnabledForInterior() {
    GradientEditorWidget editor;
    Gradient gradient;
    gradient.insertStop(0.5f);
    editor.setGradient(gradient);
    auto* bar = editor.findChild<GradientBarWidget*>(QStringLiteral("gradientBar"));
    bar->resize(240, 48);
    auto* deleteButton = editor.findChild<QPushButton*>(QStringLiteral("gradientDeleteStopButton"));
    QVERIFY(deleteButton != nullptr);
    QVERIFY(!deleteButton->isEnabled());  // Selection defaults to the first endpoint.

    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));

    QVERIFY(deleteButton->isEnabled());
}

void GradientEditorWidgetTest::deleteStopButtonRemovesTheSelectedInteriorStop() {
    GradientEditorWidget editor;
    Gradient gradient;
    gradient.insertStop(0.5f);
    editor.setGradient(gradient);
    auto* bar = editor.findChild<GradientBarWidget*>(QStringLiteral("gradientBar"));
    bar->resize(240, 48);
    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, QPoint(xPosFor(0.5f), 24));
    QSignalSpy spy(&editor, &GradientEditorWidget::gradientChanged);

    editor.findChild<QPushButton*>(QStringLiteral("gradientDeleteStopButton"))->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(editor.gradient().stops().size(), std::size_t{2});
}

void GradientEditorWidgetTest::setGradientSyncsWithoutEmittingIncludingLinkChannels() {
    GradientEditorWidget editor;
    QSignalSpy spy(&editor, &GradientEditorWidget::gradientChanged);
    Gradient gradient;
    gradient.setLinkChannels(true);

    editor.setGradient(gradient);

    QCOMPARE(spy.count(), 0);
    QVERIFY(editor.findChild<QCheckBox*>(QStringLiteral("gradientLinkChannelsCheckBox"))->isChecked());
}
