#include "test_create_project_wizard.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QtTest/QtTest>

#include "sound_mind/studio/create_project_wizard.h"

using sound_mind::core::ProjectSettings;
using sound_mind::studio::CreateProjectWizard;

namespace {

QPushButton* okButton(const CreateProjectWizard& wizard) {
    auto* box = wizard.findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
    return box == nullptr ? nullptr : box->button(QDialogButtonBox::Ok);
}

}  // namespace

void CreateProjectWizardTest::settingsMatchProjectSettingsDefaultsInitially() {
    const CreateProjectWizard wizard;
    const ProjectSettings settings = wizard.settings();
    const ProjectSettings defaults;

    QCOMPARE(settings.sampleRateHz, defaults.sampleRateHz);
    QCOMPARE(settings.binCount, defaults.binCount);
    QCOMPARE(settings.minFrequencyHz, defaults.minFrequencyHz);
    QCOMPARE(settings.maxFrequencyHz, defaults.maxFrequencyHz);
    QCOMPARE(settings.timestepMs, defaults.timestepMs);
    QCOMPARE(settings.canvasWidth, defaults.canvasWidth);
    QCOMPARE(settings.canvasHeight, defaults.canvasHeight);
}

void CreateProjectWizardTest::okIsDisabledUntilNameAndLocationAreBothSet() {
    CreateProjectWizard wizard;
    QPushButton* ok = okButton(wizard);
    QVERIFY(ok != nullptr);
    QVERIFY(!ok->isEnabled());

    auto* nameEdit = wizard.findChild<QLineEdit*>(QStringLiteral("nameEdit"));
    auto* locationEdit = wizard.findChild<QLineEdit*>(QStringLiteral("locationEdit"));
    QVERIFY(nameEdit != nullptr);
    QVERIFY(locationEdit != nullptr);

    nameEdit->setText(QStringLiteral("My Project"));
    QVERIFY(!ok->isEnabled());  // location still empty.

    locationEdit->setText(QStringLiteral("C:/projects/my_project.smproj"));
    QVERIFY(ok->isEnabled());

    nameEdit->clear();
    QVERIFY(!ok->isEnabled());  // name empty again.
}

void CreateProjectWizardTest::pathAppendsSmprojExtensionWhenMissing() {
    CreateProjectWizard wizard;
    auto* locationEdit = wizard.findChild<QLineEdit*>(QStringLiteral("locationEdit"));
    QVERIFY(locationEdit != nullptr);
    locationEdit->setText(QStringLiteral("C:/projects/my_project"));

    QCOMPARE(QString::fromStdString(wizard.path().string()), QStringLiteral("C:/projects/my_project.smproj"));
}

void CreateProjectWizardTest::pathKeepsAnAlreadyPresentSmprojExtension() {
    CreateProjectWizard wizard;
    auto* locationEdit = wizard.findChild<QLineEdit*>(QStringLiteral("locationEdit"));
    QVERIFY(locationEdit != nullptr);
    locationEdit->setText(QStringLiteral("C:/projects/my_project.smproj"));

    QCOMPARE(QString::fromStdString(wizard.path().string()), QStringLiteral("C:/projects/my_project.smproj"));
}

void CreateProjectWizardTest::advancedFieldsAreHiddenUntilToggled() {
    // isHidden(), not isVisible() - isVisible() also requires the whole
    // ancestor chain (the dialog itself) to be shown, which it never is
    // in this headless test, regardless of the container's own state.
    CreateProjectWizard wizard;
    auto* container = wizard.findChild<QWidget*>(QStringLiteral("advancedContainer"));
    QVERIFY(container != nullptr);
    QVERIFY(container->isHidden());

    auto* toggle = wizard.findChild<QPushButton*>(QStringLiteral("advancedToggleButton"));
    QVERIFY(toggle != nullptr);
    toggle->click();

    QVERIFY(!container->isHidden());
}

void CreateProjectWizardTest::changingAdvancedFieldsChangesSettings() {
    CreateProjectWizard wizard;
    auto* sampleRateSpin = wizard.findChild<QSpinBox*>(QStringLiteral("sampleRateSpin"));
    auto* binCountSpin = wizard.findChild<QSpinBox*>(QStringLiteral("binCountSpin"));
    auto* minFrequencySpin = wizard.findChild<QSpinBox*>(QStringLiteral("minFrequencySpin"));
    auto* maxFrequencySpin = wizard.findChild<QSpinBox*>(QStringLiteral("maxFrequencySpin"));
    QVERIFY(sampleRateSpin != nullptr);
    QVERIFY(binCountSpin != nullptr);
    QVERIFY(minFrequencySpin != nullptr);
    QVERIFY(maxFrequencySpin != nullptr);

    sampleRateSpin->setValue(48000);
    binCountSpin->setValue(256);
    minFrequencySpin->setValue(30);
    maxFrequencySpin->setValue(18000);

    const ProjectSettings settings = wizard.settings();
    QCOMPARE(settings.sampleRateHz, static_cast<std::uint32_t>(48000));
    QCOMPARE(settings.binCount, static_cast<std::uint32_t>(256));
    QCOMPARE(settings.minFrequencyHz, 30.0f);
    QCOMPARE(settings.maxFrequencyHz, 18000.0f);
}

void CreateProjectWizardTest::durationDrivesCanvasWidthAtTheCurrentTimestep() {
    CreateProjectWizard wizard;
    auto* durationSpin = wizard.findChild<QDoubleSpinBox*>(QStringLiteral("durationSpin"));
    auto* timestepSpin = wizard.findChild<QDoubleSpinBox*>(QStringLiteral("timestepSpin"));
    QVERIFY(durationSpin != nullptr);
    QVERIFY(timestepSpin != nullptr);

    timestepSpin->setValue(10.0);
    durationSpin->setValue(5.0);  // 5 seconds at 10ms/px = 500px.

    QCOMPARE(wizard.settings().canvasWidth, static_cast<std::uint32_t>(500));
}

void CreateProjectWizardTest::canvasHeightStaysEqualToBinCount() {
    CreateProjectWizard wizard;
    auto* binCountSpin = wizard.findChild<QSpinBox*>(QStringLiteral("binCountSpin"));
    QVERIFY(binCountSpin != nullptr);
    binCountSpin->setValue(256);

    const ProjectSettings settings = wizard.settings();
    QCOMPARE(settings.canvasHeight, settings.binCount);
}
