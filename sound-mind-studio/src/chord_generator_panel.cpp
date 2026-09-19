#include "sound_mind/studio/chord_generator_panel.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include "sound_mind/core/music_theory.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::ArpeggioOrder;
using sound_mind::core::ChordCategory;
using sound_mind::core::ChordPlaybackMode;
using sound_mind::core::buildChordNotes;
using sound_mind::core::chordsInCategory;
using sound_mind::core::frequencyForMidiNote;
using sound_mind::core::noteNameForFrequency;

// Matches music_theory.cpp's own kNoteNames exactly - see rootMidiNote()'s
// own docs below.
constexpr std::array<const char*, 12> kRootNames = {"C", "C#", "D",  "D#", "E",  "F",
                                                      "F#", "G",  "G#", "A",  "A#", "B"};

constexpr std::array<std::pair<ChordCategory, const char*>, 5> kCategories = {{
    {ChordCategory::Triads, "Triads"},
    {ChordCategory::Sixths, "6th"},
    {ChordCategory::Sevenths, "7th"},
    {ChordCategory::Ninths, "9th"},
    {ChordCategory::ExtendedAdded, "Extended/Added"},
}};

// Matches arpeggiator.py's own PRESET_NAMES exactly, for continuity with
// anyone already familiar with the legacy Studio.
constexpr std::array<std::pair<ArpeggioOrder, const char*>, 9> kOrders = {{
    {ArpeggioOrder::Ascending, "Ascending"},
    {ArpeggioOrder::Descending, "Descending"},
    {ArpeggioOrder::UpDown, "Up-Down"},
    {ArpeggioOrder::DownUp, "Down-Up"},
    {ArpeggioOrder::Alternating, "Alternating"},
    {ArpeggioOrder::OutsideIn, "Outside-In"},
    {ArpeggioOrder::InsideOut, "Inside-Out"},
    {ArpeggioOrder::Random, "Random"},
    {ArpeggioOrder::Custom, "Custom"},
}};

// Matches arpeggiator.py's own _BEATS_PER_STEP table exactly.
constexpr std::array<std::pair<const char*, double>, 6> kSubdivisions = {{
    {"1/1", 4.0},
    {"1/2", 2.0},
    {"1/4", 1.0},
    {"1/8", 0.5},
    {"1/16", 0.25},
    {"1/32", 0.125},
}};

/// @brief The root note's own absolute MIDI note number, from a root-name
///        combo index and an octave - see `ChordGeneratorParams::rootMidiNote()`'s
///        own docs for the `(octave+1)*12 + semitone` convention.
int rootMidiNote(int rootIndex, int octave) { return (octave + 1) * 12 + rootIndex; }

/// @brief Parses a comma-separated list of indices - `customIndicesLineEdit_`'s
///        own text into `ChordGeneratorParams::customOrderIndices`. Any
///        token that isn't a valid non-negative integer is silently
///        skipped - out-of-range indices are left for
///        `arpeggioIndexSequence()`'s own documented skip-at-use behavior,
///        not caught here.
std::vector<int> parseCustomIndices(const QString& text) {
    std::vector<int> indices;
    for (const QString& token : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int value = token.trimmed().toInt(&ok);
        if (ok && value >= 0) {
            indices.push_back(value);
        }
    }
    return indices;
}

}  // namespace

ChordGeneratorPanel::ChordGeneratorPanel(QWidget* parent) : QDockWidget(tr("Chord Generator"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* topForm = new QFormLayout();

    rootCombo_ = new QComboBox(container);
    rootCombo_->setObjectName(QStringLiteral("chordRootCombo"));
    for (int i = 0; i < static_cast<int>(kRootNames.size()); ++i) {
        rootCombo_->addItem(tr(kRootNames[static_cast<std::size_t>(i)]), i);
    }
    connect(rootCombo_, &QComboBox::currentIndexChanged, this, [this](int) { emitParamsChanged(); });
    topForm->addRow(tr("Root:"), rootCombo_);

    octaveSpinBox_ = new QSpinBox(container);
    octaveSpinBox_->setObjectName(QStringLiteral("chordOctaveSpinBox"));
    octaveSpinBox_->setRange(-1, 9);
    octaveSpinBox_->setValue(4);
    connect(octaveSpinBox_, &QSpinBox::valueChanged, this, [this](int) { emitParamsChanged(); });
    topForm->addRow(tr("Octave:"), octaveSpinBox_);

    categoryCombo_ = new QComboBox(container);
    categoryCombo_->setObjectName(QStringLiteral("chordCategoryCombo"));
    for (const auto& [category, name] : kCategories) {
        categoryCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(category)));
    }
    connect(categoryCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        params_.category = static_cast<ChordCategory>(categoryCombo_->itemData(index).toInt());
        rebuildChordCombo();
        emitParamsChanged();
    });
    topForm->addRow(tr("Category:"), categoryCombo_);

    chordCombo_ = new QComboBox(container);
    chordCombo_->setObjectName(QStringLiteral("chordNameCombo"));
    connect(chordCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) {
            params_.chordIndex = static_cast<std::size_t>(index);
            emitParamsChanged();
        }
    });
    topForm->addRow(tr("Chord:"), chordCombo_);

    root->addLayout(topForm);

    notesPreviewLabel_ = new QLabel(container);
    notesPreviewLabel_->setObjectName(QStringLiteral("chordNotesPreviewLabel"));
    notesPreviewLabel_->setWordWrap(true);
    root->addWidget(notesPreviewLabel_);

    auto* modeForm = new QFormLayout();
    modeCombo_ = new QComboBox(container);
    modeCombo_->setObjectName(QStringLiteral("chordModeCombo"));
    modeCombo_->addItem(tr("Block Chord"), QVariant::fromValue(static_cast<int>(ChordPlaybackMode::Block)));
    modeCombo_->addItem(tr("Arpeggio"), QVariant::fromValue(static_cast<int>(ChordPlaybackMode::Arpeggio)));
    connect(modeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        params_.mode = static_cast<ChordPlaybackMode>(modeCombo_->itemData(index).toInt());
        updateGroupVisibility();
        emitParamsChanged();
    });
    modeForm->addRow(tr("Mode:"), modeCombo_);
    root->addLayout(modeForm);

    // --- Block mode's own group -------------------------------------------
    blockGroup_ = new QWidget(container);
    blockGroup_->setObjectName(QStringLiteral("chordBlockGroup"));
    auto* blockForm = new QFormLayout(blockGroup_);
    blockForm->setContentsMargins(0, 0, 0, 0);

    blockDurationSpinBox_ = new QDoubleSpinBox(blockGroup_);
    blockDurationSpinBox_->setObjectName(QStringLiteral("chordBlockDurationSpinBox"));
    blockDurationSpinBox_->setRange(0.01, 600.0);
    blockDurationSpinBox_->setSingleStep(0.1);
    blockDurationSpinBox_->setSuffix(tr(" s"));
    blockDurationSpinBox_->setValue(params_.blockDurationSeconds);
    connect(blockDurationSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        params_.blockDurationSeconds = value;
        emitParamsChanged();
    });
    blockForm->addRow(tr("Duration:"), blockDurationSpinBox_);
    root->addWidget(blockGroup_);

    // --- Arpeggio mode's own group ------------------------------------------
    arpeggioGroup_ = new QWidget(container);
    arpeggioGroup_->setObjectName(QStringLiteral("chordArpeggioGroup"));
    auto* arpeggioForm = new QFormLayout(arpeggioGroup_);
    arpeggioForm->setContentsMargins(0, 0, 0, 0);

    orderCombo_ = new QComboBox(arpeggioGroup_);
    orderCombo_->setObjectName(QStringLiteral("chordOrderCombo"));
    for (const auto& [order, name] : kOrders) {
        orderCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(order)));
    }
    connect(orderCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        params_.order = static_cast<ArpeggioOrder>(orderCombo_->itemData(index).toInt());
        updateGroupVisibility();
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Order:"), orderCombo_);

    customIndicesLineEdit_ = new QLineEdit(arpeggioGroup_);
    customIndicesLineEdit_->setObjectName(QStringLiteral("chordCustomIndicesLineEdit"));
    customIndicesLineEdit_->setPlaceholderText(tr("e.g. 0, 2, 1, 2"));
    connect(customIndicesLineEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        params_.customOrderIndices = parseCustomIndices(text);
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Custom Sequence:"), customIndicesLineEdit_);

    randomSeedSpinBox_ = new QSpinBox(arpeggioGroup_);
    randomSeedSpinBox_->setObjectName(QStringLiteral("chordRandomSeedSpinBox"));
    randomSeedSpinBox_->setRange(0, 999999);
    connect(randomSeedSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        params_.randomSeed = static_cast<std::uint64_t>(value);
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Random Seed:"), randomSeedSpinBox_);

    bpmSpinBox_ = new QDoubleSpinBox(arpeggioGroup_);
    bpmSpinBox_->setObjectName(QStringLiteral("chordBpmSpinBox"));
    bpmSpinBox_->setRange(1.0, 999.0);
    bpmSpinBox_->setValue(params_.bpm);
    connect(bpmSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        params_.bpm = value;
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("BPM:"), bpmSpinBox_);

    subdivisionCombo_ = new QComboBox(arpeggioGroup_);
    subdivisionCombo_->setObjectName(QStringLiteral("chordSubdivisionCombo"));
    for (const auto& [label, stepBeats] : kSubdivisions) {
        subdivisionCombo_->addItem(tr(label), stepBeats);
    }
    subdivisionCombo_->setCurrentIndex(4);  // 1/16, matching params_'s own default stepBeats (0.25).
    connect(subdivisionCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        params_.stepBeats = subdivisionCombo_->itemData(index).toDouble();
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Subdivision:"), subdivisionCombo_);

    noteDurationPercentSpinBox_ = new QSpinBox(arpeggioGroup_);
    noteDurationPercentSpinBox_->setObjectName(QStringLiteral("chordNoteDurationPercentSpinBox"));
    noteDurationPercentSpinBox_->setRange(1, 100);
    noteDurationPercentSpinBox_->setValue(static_cast<int>(params_.noteDurationFraction * 100.0));
    noteDurationPercentSpinBox_->setSuffix(tr("%"));
    connect(noteDurationPercentSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        params_.noteDurationFraction = static_cast<double>(value) / 100.0;
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Note Duration:"), noteDurationPercentSpinBox_);

    repeatsSpinBox_ = new QSpinBox(arpeggioGroup_);
    repeatsSpinBox_->setObjectName(QStringLiteral("chordRepeatsSpinBox"));
    repeatsSpinBox_->setRange(1, 64);
    repeatsSpinBox_->setValue(params_.repeats);
    connect(repeatsSpinBox_, &QSpinBox::valueChanged, this, [this](int value) {
        params_.repeats = value;
        emitParamsChanged();
    });
    arpeggioForm->addRow(tr("Repeats:"), repeatsSpinBox_);

    root->addWidget(arpeggioGroup_);
    root->addStretch();

    setWidget(container);

    rebuildChordCombo();
    updateGroupVisibility();
    emitParamsChanged();
}

void ChordGeneratorPanel::rebuildChordCombo() {
    const QSignalBlocker blocker(chordCombo_);
    const int previousIndex = chordCombo_->currentIndex();
    chordCombo_->clear();
    for (const auto& chord : chordsInCategory(params_.category)) {
        const QString label =
            chord.symbol.empty() ? QString::fromStdString(chord.name)
                                  : QStringLiteral("%1 (%2)").arg(QString::fromStdString(chord.name),
                                                                    QString::fromStdString(chord.symbol));
        chordCombo_->addItem(label);
    }
    const int clampedIndex = std::max(0, std::min(previousIndex, chordCombo_->count() - 1));
    chordCombo_->setCurrentIndex(clampedIndex);
    params_.chordIndex = static_cast<std::size_t>(clampedIndex);
}

void ChordGeneratorPanel::updateGroupVisibility() {
    blockGroup_->setVisible(params_.mode == ChordPlaybackMode::Block);
    arpeggioGroup_->setVisible(params_.mode == ChordPlaybackMode::Arpeggio);
    if (arpeggioGroup_->isVisible()) {
        customIndicesLineEdit_->setVisible(params_.order == ArpeggioOrder::Custom);
        randomSeedSpinBox_->setVisible(params_.order == ArpeggioOrder::Random);
    }
}

void ChordGeneratorPanel::emitParamsChanged() {
    params_.rootMidiNote = rootMidiNote(rootCombo_->currentData().toInt(), octaveSpinBox_->value());
    params_.startTimeSeconds = 0.0;

    sound_mind::core::ChordGeneratorParams previewParams = params_;
    const auto notes = buildChordNotes(previewParams);
    QStringList noteNames;
    for (const auto& note : notes) {
        noteNames << QString::fromStdString(noteNameForFrequency(note.frequencyHz, params_.referenceHz));
    }
    notesPreviewLabel_->setText(noteNames.isEmpty() ? tr("(no notes)") : noteNames.join(QStringLiteral("  ")));

    emit paramsChanged(params_);
}

}  // namespace sound_mind::studio
