#include "sound_mind/studio/grid_panel.h"

#include <array>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringList>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include "sound_mind/core/music_theory.h"
#include "sound_mind/studio/checklist_dialog.h"

namespace sound_mind::studio {

using sound_mind::core::PitchClass;
using sound_mind::core::pitchClassName;
using sound_mind::core::pitchClassesInScale;
using sound_mind::core::ScaleType;
using sound_mind::core::scaleTypeName;
using sound_mind::core::stepsPerOctave;
using sound_mind::core::stepWithinOctaveForPitchClass;
using sound_mind::core::supportsKeyAndScale;
using sound_mind::core::Temperament;
using sound_mind::core::temperamentName;

namespace {

/// @brief Every `Qt::PenStyle` this panel offers, paired with its
/// display name - just the three a hand-drawn reference line reasonably
/// needs (a continuous line, an evenly-dashed one, and a fine dotted
/// one), not every style `Qt::PenStyle` itself defines.
constexpr std::array<std::pair<Qt::PenStyle, const char*>, 3> kDashStyles{{
    {Qt::SolidLine, "Solid"},
    {Qt::DashLine, "Dash"},
    {Qt::DotLine, "Dot"},
}};

/// @brief Every `TimingGridMode` paired with its display name.
constexpr std::array<std::pair<TimingGridMode, const char*>, 3> kTimingGridModes{{
    {TimingGridMode::Off, "Off"},
    {TimingGridMode::Interval, "Fixed Interval"},
    {TimingGridMode::Tempo, "Tempo"},
}};

/// @brief Every note-value subdivision the Timing Grid's own Tempo mode
/// offers, paired with its own fraction of a quarter-note beat - down to
/// a thirty-second note, matching `docs/sound-mind-design.md`'s own
/// "subdivisions down to the finest rhythmic value in use" phrasing with
/// a fixed, musically-legible list rather than an open-ended spin box.
constexpr std::array<std::pair<double, const char*>, 6> kTempoSubdivisions{{
    {4.0, "Whole"},
    {2.0, "Half"},
    {1.0, "Quarter"},
    {0.5, "Eighth"},
    {0.25, "Sixteenth"},
    {0.125, "Thirty-second"},
}};

/// @brief Every `Temperament` this panel offers, in the order the
/// Temperament combo lists them - `Equal12` first, matching this
/// codebase's own long-standing default.
constexpr std::array<Temperament, 13> kTemperaments{{
    Temperament::Equal12,
    Temperament::Equal15,
    Temperament::Equal17,
    Temperament::Equal19,
    Temperament::Equal22,
    Temperament::Equal24,
    Temperament::Equal31,
    Temperament::Equal34,
    Temperament::Equal41,
    Temperament::Equal53,
    Temperament::Equal72,
    Temperament::QuarterCommaMeantone,
    Temperament::Pythagorean,
}};

/// @brief Every `ScaleType` this panel's own Scale combo offers, in
/// display order - `Chromatic` ("no restriction") first.
constexpr std::array<ScaleType, 15> kScaleTypes{{
    ScaleType::Chromatic,
    ScaleType::Major,
    ScaleType::Dorian,
    ScaleType::Phrygian,
    ScaleType::Lydian,
    ScaleType::Mixolydian,
    ScaleType::Minor,
    ScaleType::Locrian,
    ScaleType::HarmonicMinor,
    ScaleType::MelodicMinor,
    ScaleType::MajorPentatonic,
    ScaleType::MinorPentatonic,
    ScaleType::Blues,
    ScaleType::WholeTone,
    ScaleType::Octatonic,
}};

/// @brief Every `PitchClass`, `C` through `B`, in display order for the
/// Key combo.
constexpr std::array<PitchClass, 12> kPitchClasses{{
    PitchClass::C, PitchClass::CSharp, PitchClass::D, PitchClass::DSharp, PitchClass::E, PitchClass::F,
    PitchClass::FSharp, PitchClass::G, PitchClass::GSharp, PitchClass::A, PitchClass::ASharp, PitchClass::B,
}};

/// @brief The fixed octave range the "Octaves..." dialog lists -
/// `kMinEqual12Step`/`kMaxEqual12Step`'s own span (`grid_config.cpp`,
/// the widest range the note grid's own computation ever actually walks)
/// converted to scientific-pitch-notation octave numbers. `GridPanel`
/// has no `ProjectSettings` of its own to derive a narrower, project-
/// specific range from - see this class's own "purely presentational"
/// docs - so this is deliberately generous rather than exact.
constexpr int kMinOctave = -1;
constexpr int kMaxOctave = 10;

/// @brief `step`'s own display label for the Notes checklist dialog,
/// under `temperament` - a pitch class name (`"C"`, `"A#"`, ...) where
/// `supportsKeyAndScale(temperament)` makes one available (a quarter-tone
/// step under `Equal24` - one that isn't a multiple of
/// `stepsPerOctave/12` - is labeled relative to the nearest pitch class
/// below it instead, e.g. `"A (+1/4)"`, since it has no letter name of
/// its own), or a plain step number otherwise (see
/// `sound_mind::core::Temperament`'s own docs on why those have no
/// standard note-name mapping at all).
QString noteGridStepLabel(Temperament temperament, int step) {
    if (!supportsKeyAndScale(temperament)) {
        return QStringLiteral("Step %1").arg(step);
    }
    const int divisionsPerSemitone = stepsPerOctave(temperament) / 12;
    const int semitoneIndex = step / divisionsPerSemitone;
    const int remainder = step - semitoneIndex * divisionsPerSemitone;
    // stepWithinOctaveForPitchClass()'s own inverse: which PitchClass
    // lands on step `semitoneIndex * divisionsPerSemitone`, given pitch
    // class A itself lands on step 0.
    const auto pitchClass = static_cast<PitchClass>((semitoneIndex + 9) % 12);
    const QString name = QString::fromStdString(pitchClassName(pitchClass));
    return remainder == 0 ? name : QStringLiteral("%1 (+%2/%3)").arg(name).arg(remainder).arg(divisionsPerSemitone);
}

}  // namespace

GridPanel::GridPanel(QWidget* parent) : QDockWidget(tr("Grid"), parent) {
    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* axisForm = new QFormLayout();

    verticalAxisCombo_ = new QComboBox(container);
    verticalAxisCombo_->setObjectName(QStringLiteral("verticalAxisCombo"));
    verticalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Off)));
    verticalAxisCombo_->addItem(tr("Hz"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Hertz)));
    verticalAxisCombo_->addItem(tr("Notes"), QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::Notes)));
    verticalAxisCombo_->addItem(tr("Bin index"),
                                 QVariant::fromValue(static_cast<int>(VerticalAxisLabelMode::BinIndex)));
    connect(verticalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        verticalAxisLabelMode_ = static_cast<VerticalAxisLabelMode>(verticalAxisCombo_->itemData(index).toInt());
        emit verticalAxisLabelModeChanged(verticalAxisLabelMode_);
    });
    axisForm->addRow(tr("Vertical axis (frequency):"), verticalAxisCombo_);

    horizontalAxisCombo_ = new QComboBox(container);
    horizontalAxisCombo_->setObjectName(QStringLiteral("horizontalAxisCombo"));
    horizontalAxisCombo_->addItem(tr("Off"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Off)));
    horizontalAxisCombo_->addItem(tr("Seconds"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Seconds)));
    horizontalAxisCombo_->addItem(
        tr("Milliseconds"), QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::Milliseconds)));
    horizontalAxisCombo_->addItem(tr("Frame index"),
                                   QVariant::fromValue(static_cast<int>(HorizontalAxisLabelMode::FrameIndex)));
    connect(horizontalAxisCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        horizontalAxisLabelMode_ =
            static_cast<HorizontalAxisLabelMode>(horizontalAxisCombo_->itemData(index).toInt());
        emit horizontalAxisLabelModeChanged(horizontalAxisLabelMode_);
    });
    axisForm->addRow(tr("Horizontal axis (time):"), horizontalAxisCombo_);
    root->addLayout(axisForm);

    // --- Frequency Grid -----------------------------------------------
    auto* frequencyGroup = new QGroupBox(tr("Frequency Grid"), container);
    auto* frequencyForm = new QFormLayout(frequencyGroup);

    noteGridCheckBox_ = new QCheckBox(tr("Note grid"), frequencyGroup);
    noteGridCheckBox_->setObjectName(QStringLiteral("noteGridCheckBox"));
    connect(noteGridCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.noteGridEnabled = checked;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(noteGridCheckBox_);

    temperamentCombo_ = new QComboBox(frequencyGroup);
    temperamentCombo_->setObjectName(QStringLiteral("temperamentCombo"));
    for (const Temperament temperament : kTemperaments) {
        temperamentCombo_->addItem(QString::fromStdString(temperamentName(temperament)),
                                     QVariant::fromValue(static_cast<int>(temperament)));
    }
    connect(temperamentCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        frequencyGridConfig_.noteGridTemperament = static_cast<Temperament>(temperamentCombo_->itemData(index).toInt());
        // A different step count (or none at all) means the old excluded
        // steps either mean something different or nothing at all -
        // always starts fresh, then re-applies Key/Scale on top if the
        // new temperament still supports them (see
        // applyKeyAndScaleToExcludedSteps()'s own docs). Excluded octaves
        // are untouched - octave numbering is temperament-independent.
        frequencyGridConfig_.noteGridExcludedSteps.clear();
        applyKeyAndScaleToExcludedSteps();
        updateKeyAndScaleControlsEnabled();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Temperament:"), temperamentCombo_);

    keyCombo_ = new QComboBox(frequencyGroup);
    keyCombo_->setObjectName(QStringLiteral("keyCombo"));
    for (const PitchClass pitchClass : kPitchClasses) {
        keyCombo_->addItem(QString::fromStdString(pitchClassName(pitchClass)),
                             QVariant::fromValue(static_cast<int>(pitchClass)));
    }
    connect(keyCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        frequencyGridConfig_.noteGridKey = static_cast<PitchClass>(keyCombo_->itemData(index).toInt());
        applyKeyAndScaleToExcludedSteps();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Key:"), keyCombo_);

    scaleCombo_ = new QComboBox(frequencyGroup);
    scaleCombo_->setObjectName(QStringLiteral("scaleCombo"));
    for (const ScaleType scale : kScaleTypes) {
        scaleCombo_->addItem(QString::fromStdString(scaleTypeName(scale)), QVariant::fromValue(static_cast<int>(scale)));
    }
    connect(scaleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        frequencyGridConfig_.noteGridScale = static_cast<ScaleType>(scaleCombo_->itemData(index).toInt());
        applyKeyAndScaleToExcludedSteps();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Scale:"), scaleCombo_);

    auto* noteOctaveButtonsRow = new QHBoxLayout();
    notesButton_ = new QPushButton(tr("Notes..."), frequencyGroup);
    notesButton_->setObjectName(QStringLiteral("notesButton"));
    connect(notesButton_, &QPushButton::clicked, this, &GridPanel::showNotesDialog);
    noteOctaveButtonsRow->addWidget(notesButton_);

    octavesButton_ = new QPushButton(tr("Octaves..."), frequencyGroup);
    octavesButton_->setObjectName(QStringLiteral("octavesButton"));
    connect(octavesButton_, &QPushButton::clicked, this, &GridPanel::showOctavesDialog);
    noteOctaveButtonsRow->addWidget(octavesButton_);
    frequencyForm->addRow(noteOctaveButtonsRow);

    harmonicSeriesCheckBox_ = new QCheckBox(tr("Harmonic series"), frequencyGroup);
    harmonicSeriesCheckBox_->setObjectName(QStringLiteral("harmonicSeriesCheckBox"));
    connect(harmonicSeriesCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.harmonicSeriesEnabled = checked;
        updateFrequencyGridControlsEnabled();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(harmonicSeriesCheckBox_);

    harmonicFundamentalSpinBox_ = new QDoubleSpinBox(frequencyGroup);
    harmonicFundamentalSpinBox_->setObjectName(QStringLiteral("harmonicFundamentalSpinBox"));
    harmonicFundamentalSpinBox_->setRange(1.0, 20000.0);
    harmonicFundamentalSpinBox_->setSuffix(QStringLiteral(" Hz"));
    harmonicFundamentalSpinBox_->setValue(frequencyGridConfig_.harmonicFundamentalHz);
    connect(harmonicFundamentalSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        frequencyGridConfig_.harmonicFundamentalHz = value;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Fundamental:"), harmonicFundamentalSpinBox_);

    customFrequenciesCheckBox_ = new QCheckBox(tr("Custom frequencies"), frequencyGroup);
    customFrequenciesCheckBox_->setObjectName(QStringLiteral("customFrequenciesCheckBox"));
    connect(customFrequenciesCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        frequencyGridConfig_.customFrequenciesEnabled = checked;
        updateFrequencyGridControlsEnabled();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(customFrequenciesCheckBox_);

    customFrequenciesLineEdit_ = new QLineEdit(frequencyGroup);
    customFrequenciesLineEdit_->setObjectName(QStringLiteral("customFrequenciesLineEdit"));
    customFrequenciesLineEdit_->setPlaceholderText(tr("e.g. 220, 440, 880"));
    connect(customFrequenciesLineEdit_, &QLineEdit::textChanged, this, [this](const QString& /*text*/) {
        updateCustomFrequenciesFromText();
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Frequencies (Hz):"), customFrequenciesLineEdit_);

    frequencyGridColorButton_ = new QPushButton(frequencyGroup);
    frequencyGridColorButton_->setObjectName(QStringLiteral("frequencyGridColorButton"));
    frequencyGridColorButton_->setText(frequencyGridConfig_.lineColor.name());
    frequencyGridColorButton_->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(frequencyGridConfig_.lineColor.name()));
    connect(frequencyGridColorButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(frequencyGridConfig_.lineColor, this, tr("Choose Line Color"));
        if (!picked.isValid()) {
            return;
        }
        frequencyGridConfig_.lineColor = picked;
        frequencyGridColorButton_->setText(picked.name());
        frequencyGridColorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(picked.name()));
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line color:"), frequencyGridColorButton_);

    frequencyGridWidthSpinBox_ = new QDoubleSpinBox(frequencyGroup);
    frequencyGridWidthSpinBox_->setObjectName(QStringLiteral("frequencyGridWidthSpinBox"));
    frequencyGridWidthSpinBox_->setRange(1.0, 10.0);
    frequencyGridWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    frequencyGridWidthSpinBox_->setValue(frequencyGridConfig_.lineWidthPixels);
    connect(frequencyGridWidthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        frequencyGridConfig_.lineWidthPixels = value;
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line width:"), frequencyGridWidthSpinBox_);

    frequencyGridDashStyleCombo_ = new QComboBox(frequencyGroup);
    frequencyGridDashStyleCombo_->setObjectName(QStringLiteral("frequencyGridDashStyleCombo"));
    for (const auto& [style, name] : kDashStyles) {
        frequencyGridDashStyleCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(style)));
    }
    connect(frequencyGridDashStyleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        frequencyGridConfig_.lineStyle =
            static_cast<Qt::PenStyle>(frequencyGridDashStyleCombo_->itemData(index).toInt());
        emitFrequencyGridConfigChanged();
    });
    frequencyForm->addRow(tr("Line style:"), frequencyGridDashStyleCombo_);

    root->addWidget(frequencyGroup);

    // --- Timing Grid ---------------------------------------------------
    auto* timingGroup = new QGroupBox(tr("Timing Grid"), container);
    auto* timingForm = new QFormLayout(timingGroup);

    timingGridModeCombo_ = new QComboBox(timingGroup);
    timingGridModeCombo_->setObjectName(QStringLiteral("timingGridModeCombo"));
    for (const auto& [mode, name] : kTimingGridModes) {
        timingGridModeCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
    }
    connect(timingGridModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.mode = static_cast<TimingGridMode>(timingGridModeCombo_->itemData(index).toInt());
        updateTimingGridControlsEnabled();
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Mode:"), timingGridModeCombo_);

    timingGridIntervalSpinBox_ = new QDoubleSpinBox(timingGroup);
    timingGridIntervalSpinBox_->setObjectName(QStringLiteral("timingGridIntervalSpinBox"));
    timingGridIntervalSpinBox_->setRange(0.01, 3600.0);
    timingGridIntervalSpinBox_->setSuffix(QStringLiteral(" s"));
    timingGridIntervalSpinBox_->setValue(timingGridConfig_.intervalSeconds);
    connect(timingGridIntervalSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        timingGridConfig_.intervalSeconds = value;
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Interval:"), timingGridIntervalSpinBox_);

    timingGridSubdivisionCombo_ = new QComboBox(timingGroup);
    timingGridSubdivisionCombo_->setObjectName(QStringLiteral("timingGridSubdivisionCombo"));
    for (const auto& [fraction, name] : kTempoSubdivisions) {
        timingGridSubdivisionCombo_->addItem(tr(name), QVariant::fromValue(fraction));
    }
    timingGridSubdivisionCombo_->setCurrentIndex(2);  // Quarter - matches timingGridConfig_'s own default (1.0).
    connect(timingGridSubdivisionCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.tempoBeatFraction = timingGridSubdivisionCombo_->itemData(index).toDouble();
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Subdivision:"), timingGridSubdivisionCombo_);

    timingGridColorButton_ = new QPushButton(timingGroup);
    timingGridColorButton_->setObjectName(QStringLiteral("timingGridColorButton"));
    timingGridColorButton_->setText(timingGridConfig_.lineColor.name());
    timingGridColorButton_->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(timingGridConfig_.lineColor.name()));
    connect(timingGridColorButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(timingGridConfig_.lineColor, this, tr("Choose Line Color"));
        if (!picked.isValid()) {
            return;
        }
        timingGridConfig_.lineColor = picked;
        timingGridColorButton_->setText(picked.name());
        timingGridColorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(picked.name()));
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line color:"), timingGridColorButton_);

    timingGridWidthSpinBox_ = new QDoubleSpinBox(timingGroup);
    timingGridWidthSpinBox_->setObjectName(QStringLiteral("timingGridWidthSpinBox"));
    timingGridWidthSpinBox_->setRange(1.0, 10.0);
    timingGridWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    timingGridWidthSpinBox_->setValue(timingGridConfig_.lineWidthPixels);
    connect(timingGridWidthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        timingGridConfig_.lineWidthPixels = value;
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line width:"), timingGridWidthSpinBox_);

    timingGridDashStyleCombo_ = new QComboBox(timingGroup);
    timingGridDashStyleCombo_->setObjectName(QStringLiteral("timingGridDashStyleCombo"));
    for (const auto& [style, name] : kDashStyles) {
        timingGridDashStyleCombo_->addItem(tr(name), QVariant::fromValue(static_cast<int>(style)));
    }
    connect(timingGridDashStyleCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        timingGridConfig_.lineStyle = static_cast<Qt::PenStyle>(timingGridDashStyleCombo_->itemData(index).toInt());
        emitTimingGridConfigChanged();
    });
    timingForm->addRow(tr("Line style:"), timingGridDashStyleCombo_);

    root->addWidget(timingGroup);

    // --- Snap to Grid ----------------------------------------------------
    snapToGridCheckBox_ = new QCheckBox(tr("Snap to Grid"), container);
    snapToGridCheckBox_->setObjectName(QStringLiteral("snapToGridCheckBox"));
    connect(snapToGridCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        snapToGridEnabled_ = checked;
        emit snapToGridChanged(checked);
    });
    root->addWidget(snapToGridCheckBox_);

    root->addStretch();

    updateFrequencyGridControlsEnabled();
    updateKeyAndScaleControlsEnabled();
    updateTimingGridControlsEnabled();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void GridPanel::updateCustomFrequenciesFromText() {
    frequencyGridConfig_.customFrequenciesHz.clear();
    const QStringList tokens =
        customFrequenciesLineEdit_->text().split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        bool ok = false;
        const double frequencyHz = token.toDouble(&ok);
        if (ok && frequencyHz > 0.0) {
            frequencyGridConfig_.customFrequenciesHz.push_back(frequencyHz);
        }
    }
}

void GridPanel::updateFrequencyGridControlsEnabled() {
    harmonicFundamentalSpinBox_->setEnabled(harmonicSeriesCheckBox_->isChecked());
    customFrequenciesLineEdit_->setEnabled(customFrequenciesCheckBox_->isChecked());
}

void GridPanel::updateKeyAndScaleControlsEnabled() {
    const bool enabled = supportsKeyAndScale(frequencyGridConfig_.noteGridTemperament);
    keyCombo_->setEnabled(enabled);
    scaleCombo_->setEnabled(enabled);
}

void GridPanel::applyKeyAndScaleToExcludedSteps() {
    if (!supportsKeyAndScale(frequencyGridConfig_.noteGridTemperament)) {
        return;
    }
    const auto includedPitchClasses =
        pitchClassesInScale(frequencyGridConfig_.noteGridKey, frequencyGridConfig_.noteGridScale);
    std::set<int> includedSteps;
    for (const PitchClass pitchClass : includedPitchClasses) {
        includedSteps.insert(stepWithinOctaveForPitchClass(frequencyGridConfig_.noteGridTemperament, pitchClass));
    }
    frequencyGridConfig_.noteGridExcludedSteps.clear();
    const int divisions = stepsPerOctave(frequencyGridConfig_.noteGridTemperament);
    for (int step = 0; step < divisions; ++step) {
        if (includedSteps.count(step) == 0) {
            frequencyGridConfig_.noteGridExcludedSteps.insert(step);
        }
    }
}

void GridPanel::showNotesDialog() {
    const int divisions = stepsPerOctave(frequencyGridConfig_.noteGridTemperament);
    QStringList labels;
    std::vector<bool> initiallyChecked;
    labels.reserve(divisions);
    initiallyChecked.reserve(static_cast<std::size_t>(divisions));
    for (int step = 0; step < divisions; ++step) {
        labels.append(noteGridStepLabel(frequencyGridConfig_.noteGridTemperament, step));
        initiallyChecked.push_back(frequencyGridConfig_.noteGridExcludedSteps.count(step) == 0);
    }

    CheckListDialog dialog(tr("Notes"), labels, initiallyChecked, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    applyNoteSelection(dialog.checkedStates());
}

void GridPanel::showOctavesDialog() {
    QStringList labels;
    std::vector<bool> initiallyChecked;
    for (int octave = kMinOctave; octave <= kMaxOctave; ++octave) {
        labels.append(tr("Octave %1").arg(octave));
        initiallyChecked.push_back(frequencyGridConfig_.noteGridExcludedOctaves.count(octave) == 0);
    }

    CheckListDialog dialog(tr("Octaves"), labels, initiallyChecked, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    applyOctaveSelection(dialog.checkedStates());
}

void GridPanel::applyNoteSelection(const std::vector<bool>& checkedStates) {
    for (std::size_t step = 0; step < checkedStates.size(); ++step) {
        if (checkedStates[step]) {
            frequencyGridConfig_.noteGridExcludedSteps.erase(static_cast<int>(step));
        } else {
            frequencyGridConfig_.noteGridExcludedSteps.insert(static_cast<int>(step));
        }
    }
    emitFrequencyGridConfigChanged();
}

void GridPanel::applyOctaveSelection(const std::vector<bool>& checkedStates) {
    for (std::size_t i = 0; i < checkedStates.size(); ++i) {
        const int octave = kMinOctave + static_cast<int>(i);
        if (checkedStates[i]) {
            frequencyGridConfig_.noteGridExcludedOctaves.erase(octave);
        } else {
            frequencyGridConfig_.noteGridExcludedOctaves.insert(octave);
        }
    }
    emitFrequencyGridConfigChanged();
}

void GridPanel::updateTimingGridControlsEnabled() {
    timingGridIntervalSpinBox_->setEnabled(timingGridConfig_.mode == TimingGridMode::Interval);
    timingGridSubdivisionCombo_->setEnabled(timingGridConfig_.mode == TimingGridMode::Tempo);
}

void GridPanel::emitFrequencyGridConfigChanged() { emit frequencyGridConfigChanged(frequencyGridConfig_); }

void GridPanel::emitTimingGridConfigChanged() { emit timingGridConfigChanged(timingGridConfig_); }

}  // namespace sound_mind::studio
