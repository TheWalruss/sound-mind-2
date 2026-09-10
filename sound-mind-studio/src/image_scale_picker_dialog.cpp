#include "sound_mind/studio/image_scale_picker_dialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

ImageScalePickerDialog::ImageScalePickerDialog(QWidget* parent, bool allowSequential) : QDialog(parent) {
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

    // Image Sequence Import (v0.Y.22.1): only offered when importing more
    // than one file at once (allowSequential) - see this constructor's own
    // docs. Checking it overrides/disables the five mode radios above
    // rather than coexisting with them, since a sequence import always
    // applies ScaleVerticalProportional to every file regardless of
    // whatever mode was selected before.
    if (allowSequential) {
        auto* sequentialCheckBox = new QCheckBox(tr("Import as sequence"), this);
        sequentialCheckBox->setObjectName(QStringLiteral("sequentialCheckBox"));
        sequentialCheckBox->setToolTip(
            tr("Lay the selected files out end-to-end in time instead of importing each independently"));
        connect(sequentialCheckBox, &QCheckBox::toggled, this, [this, group](bool checked) {
            importAsSequence_ = checked;
            const auto buttons = group->buttons();
            for (auto* button : buttons) {
                button->setEnabled(!checked);
            }
        });
        root->addWidget(sequentialCheckBox);
    }

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

ImageScalePickerDialog::Mode ImageScalePickerDialog::selectedMode() const {
    return selectedMode_;
}

bool ImageScalePickerDialog::importAsSequence() const {
    return importAsSequence_;
}

}  // namespace sound_mind::studio
