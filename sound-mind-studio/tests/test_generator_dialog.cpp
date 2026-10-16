#include "test_generator_dialog.h"

#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSlider>
#include <QtTest/QtTest>

#include "sound_mind/studio/generator_dialog.h"
#include "sound_mind/studio/numeric_field.h"

using sound_mind::core::GeneratorFamily;
using sound_mind::studio::GeneratorDialog;
using sound_mind::studio::IntegerField;
using sound_mind::studio::NumericField;

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
    auto* seedSpinBox = dialog.findChild<IntegerField*>(QStringLiteral("seedSpinBox"));
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

namespace {

void choose(QComboBox* combo, int data) { combo->setCurrentIndex(combo->findData(data)); }

}  // namespace

void GeneratorDialogTest::choosingNoiseShowsTheNoiseControlsAndHidesOrderChaos() {
    GeneratorDialog dialog;
    auto* family = dialog.findChild<QComboBox*>(QStringLiteral("generatorFamilyCombo"));
    auto* noiseGroup = dialog.findChild<QGroupBox*>(QStringLiteral("noiseGroup"));
    auto* slider = dialog.findChild<QSlider*>(QStringLiteral("orderChaosSlider"));
    QVERIFY(family != nullptr && noiseGroup != nullptr && slider != nullptr);
    QVERIFY(noiseGroup->isHidden());
    QVERIFY(!slider->isHidden());

    choose(family, static_cast<int>(GeneratorFamily::Noise));

    QVERIFY(!noiseGroup->isHidden());
    QVERIFY(slider->isHidden());
    QCOMPARE(dialog.configuration().family, GeneratorFamily::Noise);
}

void GeneratorDialogTest::noiseSettingsAndBindingsReachTheConfiguration() {
    GeneratorDialog dialog;
    dialog.setAvailableMindWaves({{sound_mind::core::MindWaveId{4}, QStringLiteral("Sweep")}});
    choose(dialog.findChild<QComboBox*>(QStringLiteral("generatorFamilyCombo")), static_cast<int>(GeneratorFamily::Noise));
    choose(dialog.findChild<QComboBox*>(QStringLiteral("noiseColorCombo")),
           static_cast<int>(sound_mind::core::NoiseColor::Red));
    dialog.findChild<NumericField*>(QStringLiteral("noiseLevelField"))->setValue(-40.0);
    dialog.findChild<NumericField*>(QStringLiteral("noiseHorizontalResolutionField"))->setValue(6.0);
    auto* binding = dialog.findChild<QComboBox*>(QStringLiteral("noiseVerticalResolutionMindWaveCombo"));
    QCOMPARE(binding->count(), 2);  // None + Sweep.
    binding->setCurrentIndex(1);

    const auto config = dialog.configuration();

    QCOMPARE(config.noise.color, sound_mind::core::NoiseColor::Red);
    QCOMPARE(config.noise.levelDb, -40.0f);
    QCOMPARE(config.noise.horizontalResolution, 6.0);
    QVERIFY(!config.noise.horizontalResolutionMindWave.has_value());
    QCOMPARE(*config.noise.verticalResolutionMindWave, sound_mind::core::MindWaveId{4});
}

void GeneratorDialogTest::theCustomSpectrumShowsOnlyForCustomNoise() {
    GeneratorDialog dialog;
    choose(dialog.findChild<QComboBox*>(QStringLiteral("generatorFamilyCombo")), static_cast<int>(GeneratorFamily::Noise));
    auto* editor = dialog.findChild<QWidget*>(QStringLiteral("noiseCustomSpectrumEditor"));
    QVERIFY(editor != nullptr);
    QVERIFY(editor->parentWidget()->isHidden());

    choose(dialog.findChild<QComboBox*>(QStringLiteral("noiseColorCombo")),
           static_cast<int>(sound_mind::core::NoiseColor::Custom));

    QVERIFY(!editor->parentWidget()->isHidden());
    QCOMPARE(dialog.configuration().noise.customSpectrum.stops().front().leftIntensity, -30.0f);
}

void GeneratorDialogTest::fractalIsOfferedOnTheOrderChaosAxis() {
    GeneratorDialog dialog;
    auto* family = dialog.findChild<QComboBox*>(QStringLiteral("generatorFamilyCombo"));
    QVERIFY(family->findData(static_cast<int>(GeneratorFamily::Fractal)) >= 0);
    QVERIFY(family->findData(static_cast<int>(GeneratorFamily::Streaming)) >= 0);

    choose(family, static_cast<int>(GeneratorFamily::Fractal));
    dialog.findChild<QSlider*>(QStringLiteral("orderChaosSlider"))->setValue(-40);

    QVERIFY(!dialog.findChild<QSlider*>(QStringLiteral("orderChaosSlider"))->isHidden());
    QVERIFY(dialog.findChild<QGroupBox*>(QStringLiteral("noiseGroup"))->isHidden());
    QCOMPARE(dialog.configuration().family, GeneratorFamily::Fractal);
    QCOMPARE(dialog.configuration().orderChaos, -0.4);
}
