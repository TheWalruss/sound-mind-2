#include "test_theme.h"

#include <QIcon>
#include <QString>
#include <QtTest/QtTest>

#include "sound_mind/studio/theme.h"

using sound_mind::studio::studioStyleSheet;
using sound_mind::studio::studioWindowIcon;

void ThemeTest::studioStyleSheetIsNotEmpty() { QVERIFY(!studioStyleSheet().isEmpty()); }

void ThemeTest::studioStyleSheetContainsBothBrandColors() {
    // The legacy documentation's brand palette (docs/sound-mind-roadmap.md's
    // Visual Identity milestone, v0.Y.14.1): deep orange to amber gold.
    // Checking for the literal hex codes, not just "some styling exists",
    // is what actually pins this to the right palette rather than any QSS.
    const QString style = studioStyleSheet();
    QVERIFY(style.contains(QStringLiteral("#DD4B00"), Qt::CaseInsensitive));
    QVERIFY(style.contains(QStringLiteral("#FEC100"), Qt::CaseInsensitive));
}

void ThemeTest::studioWindowIconIsNotNull() { QVERIFY(!studioWindowIcon().isNull()); }
