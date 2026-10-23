#include "sound_mind/studio/create_project_wizard.h"

#include <cmath>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

CreateProjectWizard::CreateProjectWizard(QWidget* parent, double outputDeviceSampleRateHz) : QDialog(parent) {
    setWindowTitle(tr("Create Project"));

    const sound_mind::core::ProjectSettings defaults;

    auto* root = new QVBoxLayout(this);

    auto* mainForm = new QFormLayout();
    nameEdit_ = new QLineEdit();
    nameEdit_->setObjectName(QStringLiteral("nameEdit"));
    mainForm->addRow(tr("Name:"), nameEdit_);

    auto* locationRow = new QVBoxLayout();
    locationEdit_ = new QLineEdit();
    locationEdit_->setObjectName(QStringLiteral("locationEdit"));
    auto* browseButton = new QPushButton(tr("Browse..."));
    browseButton->setObjectName(QStringLiteral("browseButton"));
    connect(browseButton, &QPushButton::clicked, this, &CreateProjectWizard::browseForLocation);
    auto* locationHBox = new QHBoxLayout();
    locationHBox->addWidget(locationEdit_, 1);
    locationHBox->addWidget(browseButton);
    locationRow->addLayout(locationHBox);
    mainForm->addRow(tr("Save Folder:"), locationRow);

    durationSecondsSpin_ = new NumericField();
    durationSecondsSpin_->setObjectName(QStringLiteral("durationSpin"));
    durationSecondsSpin_->setRange(0.1, 36000.0);
    durationSecondsSpin_->setDecimals(2);
    durationSecondsSpin_->setSuffix(tr(" s"));
    // Matches ProjectSettings{}'s own implied duration (canvasWidth * timestepMs) exactly,
    // so leaving every field untouched reproduces today's default project.
    durationSecondsSpin_->setValue(static_cast<double>(defaults.canvasWidth) * defaults.timestepMs / 1000.0);
    mainForm->addRow(tr("Duration:"), durationSecondsSpin_);

    root->addLayout(mainForm);

    auto* advancedToggle = new QPushButton(tr("Advanced"));
    advancedToggle->setObjectName(QStringLiteral("advancedToggleButton"));
    advancedToggle->setCheckable(true);
    root->addWidget(advancedToggle);

    advancedContainer_ = new QWidget();
    advancedContainer_->setObjectName(QStringLiteral("advancedContainer"));
    advancedContainer_->setVisible(false);
    connect(advancedToggle, &QPushButton::toggled, advancedContainer_, &QWidget::setVisible);

    auto* advancedForm = new QFormLayout(advancedContainer_);

    sampleRateSpin_ = new IntegerField();
    sampleRateSpin_->setObjectName(QStringLiteral("sampleRateSpin"));
    sampleRateSpin_->setRange(static_cast<int>(sound_mind::core::kMinimumSampleRateHz),
                              static_cast<int>(sound_mind::core::kMaximumSampleRateHz));
    sampleRateSpin_->setSuffix(tr(" Hz"));
    // The output device's own rate, so playback runs at the right speed
    // without changing system settings - else 48 kHz (v0.1.49.1).
    const std::uint32_t sampleRateHz = sound_mind::core::newProjectSampleRateHz(outputDeviceSampleRateHz);
    const bool matchesDevice =
        outputDeviceSampleRateHz > 0.0 && static_cast<double>(sampleRateHz) == std::round(outputDeviceSampleRateHz);
    sampleRateSpin_->setValue(static_cast<int>(sampleRateHz));
    advancedForm->addRow(tr("Sample Rate:"), sampleRateSpin_);
    auto* sampleRateHint = new QLabel(
        matchesDevice ? tr("Matches the output device, so playback runs at the right speed.")
                      : tr("The output device's rate isn't known (or a project can't use it), so the most common "
                           "rate is suggested. "
                           "Playback runs at the right speed only if it matches your output device."));
    sampleRateHint->setObjectName(QStringLiteral("sampleRateHintLabel"));
    sampleRateHint->setWordWrap(true);
    advancedForm->addRow(QString(), sampleRateHint);

    minFrequencySpin_ = new IntegerField();
    minFrequencySpin_->setObjectName(QStringLiteral("minFrequencySpin"));
    minFrequencySpin_->setRange(1, 96000);
    minFrequencySpin_->setSuffix(tr(" Hz"));
    minFrequencySpin_->setValue(static_cast<int>(defaults.minFrequencyHz));
    advancedForm->addRow(tr("Min Frequency:"), minFrequencySpin_);

    maxFrequencySpin_ = new IntegerField();
    maxFrequencySpin_->setObjectName(QStringLiteral("maxFrequencySpin"));
    maxFrequencySpin_->setRange(1, 192000);
    maxFrequencySpin_->setSuffix(tr(" Hz"));
    maxFrequencySpin_->setValue(static_cast<int>(defaults.maxFrequencyHz));
    advancedForm->addRow(tr("Max Frequency:"), maxFrequencySpin_);

    binCountSpin_ = new IntegerField();
    binCountSpin_->setObjectName(QStringLiteral("binCountSpin"));
    binCountSpin_->setRange(32, 8192);
    binCountSpin_->setValue(static_cast<int>(defaults.binCount));
    advancedForm->addRow(tr("Bin Count:"), binCountSpin_);

    timestepMsSpin_ = new NumericField();
    timestepMsSpin_->setObjectName(QStringLiteral("timestepSpin"));
    timestepMsSpin_->setRange(0.1, 1000.0);
    timestepMsSpin_->setDecimals(2);
    timestepMsSpin_->setSuffix(tr(" ms"));
    timestepMsSpin_->setValue(defaults.timestepMs);
    advancedForm->addRow(tr("Timestep:"), timestepMsSpin_);

    root->addWidget(advancedContainer_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox_->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox_);

    // Without this, the dialog only ever grows: hiding advancedContainer_
    // frees up its layout space, but a QDialog doesn't shrink back down to
    // reclaim it on its own. SetFixedSize makes the top-level layout keep
    // the dialog's actual size in sync with its current size hint on every
    // change - including a child's visibility toggling - growing *and*
    // shrinking, at the cost of the dialog no longer being manually
    // resizable (an acceptable trade for a small settings dialog like
    // this one).
    root->setSizeConstraint(QLayout::SetFixedSize);

    connect(nameEdit_, &QLineEdit::textChanged, this, &CreateProjectWizard::updateOkEnabled);
    connect(locationEdit_, &QLineEdit::textChanged, this, &CreateProjectWizard::updateOkEnabled);
    updateOkEnabled();
}

sound_mind::core::ProjectSettings CreateProjectWizard::settings() const {
    sound_mind::core::ProjectSettings result;
    result.sampleRateHz = static_cast<std::uint32_t>(sampleRateSpin_->value());
    result.timestepMs = timestepMsSpin_->value();
    result.binCount = static_cast<std::uint32_t>(binCountSpin_->value());
    result.canvasHeight = result.binCount;
    result.minFrequencyHz = static_cast<float>(minFrequencySpin_->value());
    result.maxFrequencyHz = static_cast<float>(maxFrequencySpin_->value());
    result.canvasWidth =
        static_cast<std::uint32_t>(std::lround(durationSecondsSpin_->value() * 1000.0 / result.timestepMs));
    return result;
}

std::filesystem::path CreateProjectWizard::path() const {
    QString name = nameEdit_->text();
    if (name.endsWith(QStringLiteral(".smproj"), Qt::CaseInsensitive)) {
        name.chop(7);  // strlen(".smproj") - avoid doubling it up below.
    }

    const std::filesystem::path directory(locationEdit_->text().toStdString());
    return directory / (name.toStdString() + ".smproj");
}

void CreateProjectWizard::browseForLocation() {
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Save Folder"), locationEdit_->text());
    if (!directory.isEmpty()) {
        locationEdit_->setText(directory);
    }
}

void CreateProjectWizard::updateOkEnabled() {
    QPushButton* ok = buttonBox_->button(QDialogButtonBox::Ok);
    if (ok != nullptr) {
        ok->setEnabled(!nameEdit_->text().isEmpty() && !locationEdit_->text().isEmpty());
    }
}

}  // namespace sound_mind::studio
