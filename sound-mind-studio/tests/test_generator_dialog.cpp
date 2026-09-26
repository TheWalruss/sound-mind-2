#include "test_generator_dialog.h"

#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/studio/generator_dialog.h"

using sound_mind::core::GeneratorFamily;
using sound_mind::studio::GeneratorDialog;

void GeneratorDialogTest::defaultsToLatticeFamilyAndCenteredOrderChaos() {
    GeneratorDialog dialog;

    const auto config = dialog.configuration();

    QCOMPARE(config.family, GeneratorFamily::Lattice);
    QCOMPARE(config.orderChaos, 0.0);
}

void GeneratorDialogTest::movingTheSliderChangesOrderChaos() {
    GeneratorDialog dialog;
    auto* slider = dialog.findChild<QSlider*>(QStringLiteral("orderChaosSlider"));
    QVERIFY(slider != nullptr);

    slider->setValue(50);

    QCOMPARE(dialog.configuration().orderChaos, 0.5);
}

void GeneratorDialogTest::randomizeButtonChangesTheSeed() {
    GeneratorDialog dialog;
    auto* seedSpinBox = dialog.findChild<QSpinBox*>(QStringLiteral("seedSpinBox"));
    auto* randomizeButton = dialog.findChild<QPushButton*>(QStringLiteral("randomizeSeedButton"));
    QVERIFY(seedSpinBox != nullptr);
    QVERIFY(randomizeButton != nullptr);
    const int initialSeed = seedSpinBox->value();

    // Clicking repeatedly until a different value actually appears -
    // a real (if astronomically unlikely) chance of picking the exact
    // same seed twice in a row shouldn't make this test flaky.
    bool changed = false;
    for (int attempt = 0; attempt < 5 && !changed; ++attempt) {
        randomizeButton->click();
        changed = seedSpinBox->value() != initialSeed;
    }

    QVERIFY(changed);
}
