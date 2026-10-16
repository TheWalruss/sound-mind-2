#include "test_text_tool_panel.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include "sound_mind/studio/text_tool_panel.h"

using sound_mind::studio::TextSettings;
using sound_mind::studio::TextToolPanel;

void TextToolPanelTest::editingAControlEmitsTheCompleteSettings() {
    TextToolPanel panel;
    std::optional<TextSettings> received;
    QObject::connect(&panel, &TextToolPanel::settingsChanged,
                     [&received](const TextSettings& settings) { received = settings; });

    panel.findChild<QLineEdit*>(QStringLiteral("textEdit"))->setText(QStringLiteral("Hello"));
    QVERIFY(received.has_value());
    QCOMPARE(received->text, QStringLiteral("Hello"));

    panel.findChild<QCheckBox*>(QStringLiteral("textBoldCheckBox"))->setChecked(true);
    QVERIFY(received->bold);
    QCOMPARE(received->text, QStringLiteral("Hello"));  // complete, not just the changed field.

    auto* blend = panel.findChild<QComboBox*>(QStringLiteral("textBlendModeCombo"));
    blend->setCurrentIndex(blend->findText(QStringLiteral("Screen")));
    QCOMPARE(received->blendMode, sound_mind::core::BlendMode::Screen);
}

void TextToolPanelTest::setSettingsShowsThemWithoutEmitting() {
    TextToolPanel panel;
    int emitted = 0;
    QObject::connect(&panel, &TextToolPanel::settingsChanged, [&emitted](const TextSettings&) { ++emitted; });
    TextSettings settings;
    settings.text = QStringLiteral("Loaded");
    settings.fontSize = 72;
    settings.italic = true;
    settings.leftIntensityDb = -30.0f;
    settings.rotationDegrees = 45.0;

    panel.setSettings(settings);

    QCOMPARE(emitted, 0);
    const TextSettings shown = panel.settings();
    QCOMPARE(shown.text, QStringLiteral("Loaded"));
    QCOMPARE(shown.fontSize, 72);
    QVERIFY(shown.italic);
    QCOMPARE(shown.leftIntensityDb, -30.0f);
    QCOMPARE(shown.rotationDegrees, 45.0);
}

void TextToolPanelTest::applyAndResetAreEnabledOnlyWithABox() {
    TextToolPanel panel;
    auto* apply = panel.findChild<QPushButton*>(QStringLiteral("textApplyButton"));
    auto* reset = panel.findChild<QPushButton*>(QStringLiteral("textResetButton"));
    QVERIFY(!apply->isEnabled());
    QVERIFY(!reset->isEnabled());

    panel.setBoxPlaced(true);
    QVERIFY(apply->isEnabled());
    QVERIFY(reset->isEnabled());
    QSignalSpy applySpy(&panel, &TextToolPanel::applyRequested);
    apply->click();
    QCOMPARE(applySpy.count(), 1);
}
