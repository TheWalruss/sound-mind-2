#include "sound_mind/studio/image_scale_picker_dialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

ImageScalePickerDialog::ImageScalePickerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Choose How to Scale This Image"));

    auto* root = new QVBoxLayout(this);
    auto* group = new QButtonGroup(this);

    auto* rescaleToFitRadio = new QRadioButton(tr("Rescale to fit project"), this);
    rescaleToFitRadio->setObjectName(QStringLiteral("rescaleToFitRadio"));
    rescaleToFitRadio->setChecked(true);  // the confirmed default - see the class docs.
    group->addButton(rescaleToFitRadio);
    root->addWidget(rescaleToFitRadio);
    connect(rescaleToFitRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = Mode::RescaleToFitProject;
        }
    });

    auto* scaleVerticalKeepHorizontalRadio =
        new QRadioButton(tr("Scale vertically to fit project, keep horizontal resolution"), this);
    scaleVerticalKeepHorizontalRadio->setObjectName(QStringLiteral("scaleVerticalKeepHorizontalRadio"));
    group->addButton(scaleVerticalKeepHorizontalRadio);
    root->addWidget(scaleVerticalKeepHorizontalRadio);
    connect(scaleVerticalKeepHorizontalRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = Mode::ScaleVerticalKeepHorizontal;
        }
    });

    auto* scaleHorizontalKeepVerticalRadio =
        new QRadioButton(tr("Scale horizontally to fit project, keep vertical resolution"), this);
    scaleHorizontalKeepVerticalRadio->setObjectName(QStringLiteral("scaleHorizontalKeepVerticalRadio"));
    group->addButton(scaleHorizontalKeepVerticalRadio);
    root->addWidget(scaleHorizontalKeepVerticalRadio);
    connect(scaleHorizontalKeepVerticalRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = Mode::ScaleHorizontalKeepVertical;
        }
    });

    auto* scaleVerticalProportionalRadio =
        new QRadioButton(tr("Scale vertically to fit project, rescale horizontal in proportion"), this);
    scaleVerticalProportionalRadio->setObjectName(QStringLiteral("scaleVerticalProportionalRadio"));
    group->addButton(scaleVerticalProportionalRadio);
    root->addWidget(scaleVerticalProportionalRadio);
    connect(scaleVerticalProportionalRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = Mode::ScaleVerticalProportional;
        }
    });

    auto* keepNativeResolutionRadio = new QRadioButton(tr("Keep native resolution"), this);
    keepNativeResolutionRadio->setObjectName(QStringLiteral("keepNativeResolutionRadio"));
    group->addButton(keepNativeResolutionRadio);
    root->addWidget(keepNativeResolutionRadio);
    connect(keepNativeResolutionRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = Mode::KeepNativeResolution;
        }
    });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

ImageScalePickerDialog::Mode ImageScalePickerDialog::selectedMode() const {
    return selectedMode_;
}

}  // namespace sound_mind::studio
