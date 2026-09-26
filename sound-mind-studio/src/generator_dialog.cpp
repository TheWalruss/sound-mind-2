#include "sound_mind/studio/generator_dialog.h"

#include <limits>
#include <random>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sound_mind::studio {

GeneratorDialog::GeneratorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Generate Layer"));

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    orderChaosSlider_ = new QSlider(Qt::Horizontal);
    orderChaosSlider_->setObjectName(QStringLiteral("orderChaosSlider"));
    orderChaosSlider_->setRange(-100, 100);
    orderChaosSlider_->setValue(0);
    orderChaosSlider_->setToolTip(
        tr("Where this generator sits on the order/chaos continuum - rigid and repetitive toward Order, broadband "
           "noise toward Chaos, rich and organic near the middle"));
    form->addRow(tr("Chaos ↔ Order"), orderChaosSlider_);

    auto* seedRow = new QHBoxLayout();
    seedSpinBox_ = new QSpinBox();
    seedSpinBox_->setObjectName(QStringLiteral("seedSpinBox"));
    seedSpinBox_->setRange(0, std::numeric_limits<int>::max());
    seedSpinBox_->setToolTip(
        tr("Generating again with the same seed reproduces the exact same result - change it for a different one"));
    seedRow->addWidget(seedSpinBox_, 1);
    auto* randomizeButton = new QPushButton(tr("Randomize"));
    randomizeButton->setObjectName(QStringLiteral("randomizeSeedButton"));
    connect(randomizeButton, &QPushButton::clicked, this, &GeneratorDialog::randomizeSeed);
    seedRow->addWidget(randomizeButton);
    form->addRow(tr("Seed"), seedRow);

    root->addLayout(form);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);

    randomizeSeed();
}

void GeneratorDialog::randomizeSeed() {
    std::random_device randomSource;
    std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max());
    seedSpinBox_->setValue(distribution(randomSource));
}

sound_mind::core::GeneratorConfiguration GeneratorDialog::configuration() const {
    sound_mind::core::GeneratorConfiguration config;
    config.family = sound_mind::core::GeneratorFamily::Lattice;
    config.orderChaos = static_cast<double>(orderChaosSlider_->value()) / 100.0;
    config.seed = static_cast<std::uint64_t>(seedSpinBox_->value());
    return config;
}

}  // namespace sound_mind::studio
