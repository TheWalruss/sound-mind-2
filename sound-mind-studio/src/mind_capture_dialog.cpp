#include "sound_mind/studio/mind_capture_dialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace sound_mind::studio {

MindCaptureDialog::MindCaptureDialog(const QString& windowTitle, const QString& defaultName, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(windowTitle);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    nameLineEdit_ = new QLineEdit(defaultName, this);
    nameLineEdit_->setObjectName(QStringLiteral("nameLineEdit"));
    form->addRow(tr("Name:"), nameLineEdit_);

    fundamentalFrequencySpinBox_ = new QDoubleSpinBox(this);
    fundamentalFrequencySpinBox_->setObjectName(QStringLiteral("fundamentalFrequencySpinBox"));
    fundamentalFrequencySpinBox_->setRange(0.0, 20000.0);
    fundamentalFrequencySpinBox_->setSingleStep(1.0);
    fundamentalFrequencySpinBox_->setSuffix(tr(" Hz"));
    fundamentalFrequencySpinBox_->setValue(0.0);
    fundamentalFrequencySpinBox_->setToolTip(
        tr("The real-world pitch this capture's own content was recorded/painted at - needed so a MIDI note "
           "played back through it can be pitch-shifted to its own correct target pitch. 0 (the default) means "
           "\"not set\" - this capture always paints back verbatim, at every pitch, with no shifting at all."));
    form->addRow(tr("Fundamental Frequency:"), fundamentalFrequencySpinBox_);

    startTimeOffsetSpinBox_ = new QDoubleSpinBox(this);
    startTimeOffsetSpinBox_->setObjectName(QStringLiteral("startTimeOffsetSpinBox"));
    startTimeOffsetSpinBox_->setRange(0.0, 60.0);
    startTimeOffsetSpinBox_->setSingleStep(0.01);
    startTimeOffsetSpinBox_->setDecimals(3);
    startTimeOffsetSpinBox_->setSuffix(tr(" s"));
    startTimeOffsetSpinBox_->setValue(0.0);
    startTimeOffsetSpinBox_->setToolTip(
        tr("How far into this capture's own span the \"true\" onset actually sits, in seconds - if the capture "
           "includes a little lead-in before the real attack, set this so that attack lands exactly on a MIDI "
           "note's own start instead of the capture's geometric middle. 0 (the default) means \"no offset\" - "
           "centered exactly as before this field existed."));
    form->addRow(tr("Start-Time Offset:"), startTimeOffsetSpinBox_);

    root->addLayout(form);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

QString MindCaptureDialog::name() const {
    return nameLineEdit_->text().trimmed();
}

double MindCaptureDialog::fundamentalFrequencyHz() const {
    return fundamentalFrequencySpinBox_->value();
}

double MindCaptureDialog::startTimeOffsetSeconds() const {
    return startTimeOffsetSpinBox_->value();
}

}  // namespace sound_mind::studio
