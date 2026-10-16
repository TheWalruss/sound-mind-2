#include "sound_mind/studio/generator_dialog.h"

#include <array>
#include <limits>
#include <random>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QVariant>

#include "sound_mind/studio/gradient_editor_widget.h"
#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::GeneratorFamily;
using sound_mind::core::NoiseColor;

constexpr std::array<std::pair<GeneratorFamily, const char*>, 2> kFamilies{{
    {GeneratorFamily::Lattice, "Lattice"},
    {GeneratorFamily::Noise, "Noise"},
}};

constexpr std::array<std::pair<NoiseColor, const char*>, 6> kNoiseColors{{
    {NoiseColor::White, "White"},
    {NoiseColor::Pink, "Pink"},
    {NoiseColor::Red, "Red / Brownian"},
    {NoiseColor::Purple, "Purple / Violet"},
    {NoiseColor::Gray, "Gray (equal loudness)"},
    {NoiseColor::Custom, "Custom spectrum"},
}};

QComboBox* makeMindWaveCombo(QWidget* parent, const QString& objectName) {
    auto* combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->addItem(QObject::tr("None"));
    combo->setToolTip(QObject::tr("Vary this across the canvas with a MindWave"));
    return combo;
}

QHBoxLayout* boundRow(QWidget* field, QWidget* combo) {
    auto* row = new QHBoxLayout();
    row->addWidget(field, 1);
    row->addWidget(combo);
    return row;
}

}  // namespace

GeneratorDialog::GeneratorDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Generate Layer"));

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    mainForm_ = form;

    familyCombo_ = new QComboBox(this);
    familyCombo_->setObjectName(QStringLiteral("generatorFamilyCombo"));
    for (const auto& [family, name] : kFamilies) {
        familyCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(family)));
    }
    form->addRow(tr("Generator"), familyCombo_);

    orderChaosSlider_ = new QSlider(Qt::Horizontal);
    orderChaosSlider_->setObjectName(QStringLiteral("orderChaosSlider"));
    orderChaosSlider_->setRange(-100, 100);
    orderChaosSlider_->setValue(0);
    orderChaosSlider_->setToolTip(
        tr("Where this generator sits on the order/chaos continuum - rigid and repetitive toward Order, broadband "
           "noise toward Chaos, rich and organic near the middle"));
    form->addRow(tr("Chaos ↔ Order"), orderChaosSlider_);

    auto* seedRow = new QHBoxLayout();
    seedSpinBox_ = new IntegerField();
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

    // --- Noise (Pre-Beta Plan v0.Y.79.1) ---------------------------------
    const sound_mind::core::NoiseConfiguration defaults;
    noiseGroup_ = new QGroupBox(tr("Noise"), this);
    noiseGroup_->setObjectName(QStringLiteral("noiseGroup"));
    auto* noiseForm = new QFormLayout(noiseGroup_);

    noiseColorCombo_ = new QComboBox(noiseGroup_);
    noiseColorCombo_->setObjectName(QStringLiteral("noiseColorCombo"));
    for (const auto& [color, name] : kNoiseColors) {
        noiseColorCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(color)));
    }
    noiseColorCombo_->setCurrentIndex(noiseColorCombo_->findData(static_cast<int>(defaults.color)));
    noiseForm->addRow(tr("Color"), noiseColorCombo_);

    auto* unitSelector = new IntensityUnitSelector(noiseGroup_);
    noiseForm->addRow(tr("Intensity unit"), unitSelector);
    noiseLevelField_ = new NumericField(noiseGroup_);
    noiseLevelField_->setObjectName(QStringLiteral("noiseLevelField"));
    noiseLevelField_->setRange(-96.0, 0.0);
    noiseLevelField_->setDecimals(1);
    noiseLevelField_->setValue(defaults.levelDb);
    noiseLevelField_->setToolTip(tr("How loud the noise reads at 1 kHz (for Gray, everywhere)"));
    unitSelector->addField(noiseLevelField_);
    noiseForm->addRow(tr("Level"), noiseLevelField_);

    horizontalResolutionField_ = new NumericField(noiseGroup_);
    horizontalResolutionField_->setObjectName(QStringLiteral("noiseHorizontalResolutionField"));
    horizontalResolutionField_->setRange(1.0, 256.0);
    horizontalResolutionField_->setDecimals(1);
    horizontalResolutionField_->setValue(defaults.horizontalResolution);
    horizontalResolutionField_->setToolTip(tr("How many time columns one noise cell spans - larger is blockier"));
    horizontalResolutionMindWaveCombo_ =
        makeMindWaveCombo(noiseGroup_, QStringLiteral("noiseHorizontalResolutionMindWaveCombo"));
    noiseForm->addRow(tr("Horizontal resolution"), boundRow(horizontalResolutionField_, horizontalResolutionMindWaveCombo_));

    verticalResolutionField_ = new NumericField(noiseGroup_);
    verticalResolutionField_->setObjectName(QStringLiteral("noiseVerticalResolutionField"));
    verticalResolutionField_->setRange(1.0, 256.0);
    verticalResolutionField_->setDecimals(1);
    verticalResolutionField_->setValue(defaults.verticalResolution);
    verticalResolutionField_->setToolTip(tr("How many frequency rows one noise cell spans - larger is blockier"));
    verticalResolutionMindWaveCombo_ =
        makeMindWaveCombo(noiseGroup_, QStringLiteral("noiseVerticalResolutionMindWaveCombo"));
    noiseForm->addRow(tr("Vertical resolution"), boundRow(verticalResolutionField_, verticalResolutionMindWaveCombo_));

    customSpectrumSection_ = new QWidget(noiseGroup_);
    auto* spectrumLayout = new QVBoxLayout(customSpectrumSection_);
    spectrumLayout->setContentsMargins(0, 0, 0, 0);
    spectrumLayout->addWidget(new QLabel(tr("Spectrum (lowest frequency left, highest right):"), customSpectrumSection_));
    customSpectrumEditor_ = new GradientEditorWidget(customSpectrumSection_);
    customSpectrumEditor_->setObjectName(QStringLiteral("noiseCustomSpectrumEditor"));
    sound_mind::core::Gradient flat;
    auto stop = flat.stops().front();
    stop.leftIntensity = -30.0f;
    stop.rightIntensity = -30.0f;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    flat.setStopValues(0, stop);
    flat.setStopValues(1, stop);
    customSpectrumEditor_->setGradient(flat);
    spectrumLayout->addWidget(customSpectrumEditor_);
    customSpectrumMindWaveCombo_ =
        makeMindWaveCombo(customSpectrumSection_, QStringLiteral("noiseCustomSpectrumMindWaveCombo"));
    auto* spectrumBindingRow = new QHBoxLayout();
    spectrumBindingRow->addWidget(new QLabel(tr("Level varied by:"), customSpectrumSection_));
    spectrumBindingRow->addWidget(customSpectrumMindWaveCombo_, 1);
    spectrumLayout->addLayout(spectrumBindingRow);
    noiseForm->addRow(customSpectrumSection_);
    root->addWidget(noiseGroup_);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);

    connect(familyCombo_, &QComboBox::currentIndexChanged, this, &GeneratorDialog::updateVisibleControls);
    connect(noiseColorCombo_, &QComboBox::currentIndexChanged, this, &GeneratorDialog::updateVisibleControls);
    randomizeSeed();
    updateVisibleControls();
}

void GeneratorDialog::setAvailableMindWaves(
    const std::vector<std::pair<sound_mind::core::MindWaveId, QString>>& mindWaves) {
    for (QComboBox* combo :
         {horizontalResolutionMindWaveCombo_, verticalResolutionMindWaveCombo_, customSpectrumMindWaveCombo_}) {
        while (combo->count() > 1) {
            combo->removeItem(1);
        }
        for (const auto& [id, name] : mindWaves) {
            combo->addItem(name, QVariant::fromValue(static_cast<qulonglong>(id)));
        }
    }
}

void GeneratorDialog::updateVisibleControls() {
    const auto family = static_cast<GeneratorFamily>(familyCombo_->currentData().toInt());
    const bool noise = family == GeneratorFamily::Noise;
    const auto color = static_cast<NoiseColor>(noiseColorCombo_->currentData().toInt());
    // Noise isn't on the order/chaos axis - hide the whole row, label too.
    mainForm_->setRowVisible(orderChaosSlider_, !noise);
    noiseGroup_->setVisible(noise);
    noiseLevelField_->setEnabled(color != NoiseColor::Custom);
    customSpectrumSection_->setVisible(color == NoiseColor::Custom);
    adjustSize();
}

std::optional<sound_mind::core::MindWaveId> GeneratorDialog::boundMindWave(const QComboBox* combo) {
    if (combo->currentIndex() <= 0) {
        return std::nullopt;
    }
    return static_cast<sound_mind::core::MindWaveId>(combo->currentData().toULongLong());
}

void GeneratorDialog::randomizeSeed() {
    std::random_device randomSource;
    std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max());
    seedSpinBox_->setValue(distribution(randomSource));
}

sound_mind::core::GeneratorConfiguration GeneratorDialog::configuration() const {
    sound_mind::core::GeneratorConfiguration config;
    config.family = static_cast<GeneratorFamily>(familyCombo_->currentData().toInt());
    config.orderChaos = static_cast<double>(orderChaosSlider_->value()) / 100.0;
    config.seed = static_cast<std::uint64_t>(seedSpinBox_->value());
    config.noise.color = static_cast<NoiseColor>(noiseColorCombo_->currentData().toInt());
    config.noise.levelDb = static_cast<float>(noiseLevelField_->value());
    config.noise.horizontalResolution = horizontalResolutionField_->value();
    config.noise.horizontalResolutionMindWave = boundMindWave(horizontalResolutionMindWaveCombo_);
    config.noise.verticalResolution = verticalResolutionField_->value();
    config.noise.verticalResolutionMindWave = boundMindWave(verticalResolutionMindWaveCombo_);
    config.noise.customSpectrum = customSpectrumEditor_->gradient();
    config.noise.customSpectrumMindWave = boundMindWave(customSpectrumMindWaveCombo_);
    return config;
}

}  // namespace sound_mind::studio
