#include "sound_mind/studio/warp_dialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace sound_mind::studio {

using sound_mind::core::WarpAxis;
using sound_mind::core::WarpMode;

WarpDialog::WarpDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Warp Selection"));

    auto* root = new QVBoxLayout(this);

    auto* axisBox = new QGroupBox(tr("Axis"), this);
    auto* axisLayout = new QVBoxLayout(axisBox);
    auto* axisGroup = new QButtonGroup(this);

    auto* frequencyRadio = new QRadioButton(tr("Frequency (vertical shift)"), axisBox);
    frequencyRadio->setObjectName(QStringLiteral("frequencyRadio"));
    frequencyRadio->setChecked(true);
    axisGroup->addButton(frequencyRadio);
    axisLayout->addWidget(frequencyRadio);
    connect(frequencyRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedAxis_ = WarpAxis::Frequency;
        }
    });

    auto* timeRadio = new QRadioButton(tr("Time (horizontal shift)"), axisBox);
    timeRadio->setObjectName(QStringLiteral("timeRadio"));
    axisGroup->addButton(timeRadio);
    axisLayout->addWidget(timeRadio);
    connect(timeRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedAxis_ = WarpAxis::Time;
        }
    });
    root->addWidget(axisBox);

    auto* modeBox = new QGroupBox(tr("Mode"), this);
    auto* modeLayout = new QVBoxLayout(modeBox);
    auto* modeGroup = new QButtonGroup(this);

    auto* displaceRadio = new QRadioButton(tr("Displace (every line shifts by the full deflection)"), modeBox);
    displaceRadio->setObjectName(QStringLiteral("displaceRadio"));
    displaceRadio->setChecked(true);
    modeGroup->addButton(displaceRadio);
    modeLayout->addWidget(displaceRadio);
    connect(displaceRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = WarpMode::Displace;
        }
    });

    auto* stretchRadio =
        new QRadioButton(tr("Stretch (ramps from unaffected to the full deflection across the selection)"), modeBox);
    stretchRadio->setObjectName(QStringLiteral("stretchRadio"));
    modeGroup->addButton(stretchRadio);
    modeLayout->addWidget(stretchRadio);
    connect(stretchRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            selectedMode_ = WarpMode::Stretch;
        }
    });
    root->addWidget(modeBox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

}  // namespace sound_mind::studio
