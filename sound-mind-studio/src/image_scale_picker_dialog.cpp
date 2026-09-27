#include "sound_mind/studio/image_scale_picker_dialog.h"

#include <algorithm>
#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

namespace {
constexpr double kTwoPi = 6.283185307179586;
}  // namespace

ImageScalePickerDialog::ImageScalePickerDialog(QWidget* parent, bool allowSequential, QImage polarSourceImage,
                                                std::uint32_t polarDefaultOutputWidth, double polarTimestepMs)
    : QDialog(parent),
      polarSourceImage_(std::move(polarSourceImage)),
      polarDefaultOutputWidth_(polarDefaultOutputWidth),
      polarTimestepMs_(polarTimestepMs) {
    if (!polarSourceImage_.isNull()) {
        polarParams_ = defaultPolarParams();
    }
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

    // Polar-form image import (v0.Y.53.1 Installment B) - only offered
    // when a real source image was given to preview/pick against (see
    // this constructor's own docs on why a multi-file import never has
    // one).
    if (!polarSourceImage_.isNull()) {
        auto* polarRow = new QHBoxLayout();
        auto* polarRadio = new QRadioButton(tr("Polar - un-warp a polar/flower-shaped image to rectangular"), this);
        polarRadio->setObjectName(QStringLiteral("polarRadio"));
        group->addButton(polarRadio);
        polarRow->addWidget(polarRadio);

        polarOriginButton_ = new QPushButton(tr("Set origin..."), this);
        polarOriginButton_->setObjectName(QStringLiteral("polarOriginButton"));
        polarOriginButton_->setToolTip(
            tr("Open the graphical origin picker to set the flower centre, sampling radius, and arc range"));
        polarOriginButton_->setVisible(false);
        connect(polarOriginButton_, &QPushButton::clicked, this, &ImageScalePickerDialog::openPolarOriginDialog);
        polarRow->addWidget(polarOriginButton_);
        root->addLayout(polarRow);

        connect(polarRadio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                selectedMode_ = Mode::Polar;
            }
            polarOriginButton_->setVisible(checked);
        });
    }

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

std::optional<PolarImportParams> ImageScalePickerDialog::polarParams() const {
    return polarParams_;
}

PolarImportParams ImageScalePickerDialog::defaultPolarParams() const {
    PolarImportParams params;
    params.originX = polarSourceImage_.width() / 2.0;
    params.originY = polarSourceImage_.height() / 2.0;
    params.radius = std::min(polarSourceImage_.width(), polarSourceImage_.height()) / 2.0 * 0.9;
    params.arcStartRadians = 0.0;
    params.arcEndRadians = 0.0;
    params.outputWidth = polarDefaultOutputWidth_ > 0
                              ? polarDefaultOutputWidth_
                              : static_cast<std::uint32_t>(std::max(1.0, kTwoPi * params.radius));
    return params;
}

void ImageScalePickerDialog::openPolarOriginDialog() {
    // Re-opening the picker (having already picked once) resumes from
    // that same choice, rather than resetting back to the defaults - see
    // PolarOriginDialog::PolarOriginDialog()'s own `initialParams` docs.
    PolarOriginDialog dialog(polarSourceImage_, polarParams_, polarDefaultOutputWidth_, polarTimestepMs_, this);
    if (dialog.exec() == QDialog::Accepted) {
        polarParams_ = dialog.params();
    }
}

}  // namespace sound_mind::studio
