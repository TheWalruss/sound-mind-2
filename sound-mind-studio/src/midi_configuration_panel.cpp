#include "sound_mind/studio/midi_configuration_panel.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

constexpr int kProgramCount = 128;
constexpr int kProgramColumn = 0;
constexpr int kToolPresetColumn = 1;
constexpr int kDurationScaleColumn = 2;
constexpr int kPitchOffsetColumn = 3;
constexpr int kDeleteColumn = 4;

QString programLabel(int programNumber) {
    return QStringLiteral("%1: %2").arg(programNumber).arg(juce::MidiMessage::getGMInstrumentName(programNumber));
}

}  // namespace

MidiConfigurationPanel::MidiConfigurationPanel(QWidget* parent) : QDockWidget(tr("MIDI Configuration"), parent) {
    setObjectName(QStringLiteral("midiConfigurationPanel"));

    auto* container = new QWidget(this);
    auto* root = new QVBoxLayout(container);

    auto* addRow = new QHBoxLayout();
    addProgramCombo_ = new QComboBox(container);
    addProgramCombo_->setObjectName(QStringLiteral("addProgramCombo"));
    addRow->addWidget(addProgramCombo_, 1);

    addMappingButton_ = new QPushButton(tr("Add Mapping"), container);
    addMappingButton_->setObjectName(QStringLiteral("addMappingButton"));
    connect(addMappingButton_, &QPushButton::clicked, this, &MidiConfigurationPanel::addMapping);
    addRow->addWidget(addMappingButton_);
    root->addLayout(addRow);

    table_ = new QTableWidget(0, 5, container);
    table_->setObjectName(QStringLiteral("midiMappingTable"));
    table_->setHorizontalHeaderLabels(
        {tr("Program"), tr("Tool Preset"), tr("Duration Scale"), tr("Pitch Offset (st)"), QString()});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(kToolPresetColumn, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_);

    setWidget(container);
    rebuildAddProgramCombo();
}

void MidiConfigurationPanel::setProject(sound_mind::core::Project* project) {
    project_ = project;
    refreshMappings();
}

void MidiConfigurationPanel::refreshMappings() {
    table_->setRowCount(0);
    if (project_ == nullptr) {
        rebuildAddProgramCombo();
        return;
    }
    for (const auto& mapping : project_->midiProgramMappings()) {
        appendRow(mapping);
    }
    rebuildAddProgramCombo();
}

void MidiConfigurationPanel::refreshToolPresets() {
    if (project_ == nullptr) {
        return;
    }
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* combo = qobject_cast<QComboBox*>(table_->cellWidget(row, kToolPresetColumn));
        if (combo == nullptr) {
            continue;
        }
        const QVariant previousData = combo->currentData();
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(tr("None"));
        for (const auto& named : project_->toolPresets()) {
            combo->addItem(QString::fromStdString(named.name), QVariant::fromValue(static_cast<qulonglong>(named.id)));
        }
        const int index = combo->findData(previousData);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

void MidiConfigurationPanel::addMapping() {
    if (project_ == nullptr || addProgramCombo_->count() == 0) {
        return;
    }
    const int programNumber = addProgramCombo_->currentData().toInt();

    sound_mind::core::MidiProgramMapping mapping;
    mapping.programNumber = programNumber;
    project_->setMidiProgramMapping(mapping);

    appendRow(mapping);
    rebuildAddProgramCombo();
}

void MidiConfigurationPanel::rebuildAddProgramCombo() {
    const QVariant previousData = addProgramCombo_->currentData();
    const QSignalBlocker blocker(addProgramCombo_);
    addProgramCombo_->clear();
    for (int programNumber = 0; programNumber < kProgramCount; ++programNumber) {
        if (project_ != nullptr && project_->midiProgramMappingForProgram(programNumber) != nullptr) {
            continue;  // Already mapped - see this class's own docs on why a program maps only once.
        }
        addProgramCombo_->addItem(programLabel(programNumber), programNumber);
    }
    const int index = addProgramCombo_->findData(previousData);
    addProgramCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void MidiConfigurationPanel::handleToolPresetChanged(int programNumber,
                                                      std::optional<sound_mind::core::ToolPresetId> toolPresetId) {
    if (project_ == nullptr) {
        return;
    }
    const auto* existing = project_->midiProgramMappingForProgram(programNumber);
    sound_mind::core::MidiProgramMapping mapping = existing != nullptr ? *existing : sound_mind::core::MidiProgramMapping{};
    mapping.programNumber = programNumber;
    mapping.toolPresetId = toolPresetId;
    project_->setMidiProgramMapping(mapping);
}

void MidiConfigurationPanel::handleDurationScaleChanged(int programNumber, double value) {
    if (project_ == nullptr) {
        return;
    }
    const auto* existing = project_->midiProgramMappingForProgram(programNumber);
    sound_mind::core::MidiProgramMapping mapping = existing != nullptr ? *existing : sound_mind::core::MidiProgramMapping{};
    mapping.programNumber = programNumber;
    mapping.durationScale = value;
    project_->setMidiProgramMapping(mapping);
}

void MidiConfigurationPanel::handlePitchOffsetChanged(int programNumber, double value) {
    if (project_ == nullptr) {
        return;
    }
    const auto* existing = project_->midiProgramMappingForProgram(programNumber);
    sound_mind::core::MidiProgramMapping mapping = existing != nullptr ? *existing : sound_mind::core::MidiProgramMapping{};
    mapping.programNumber = programNumber;
    mapping.pitchOffsetSemitones = value;
    project_->setMidiProgramMapping(mapping);
}

void MidiConfigurationPanel::deleteMapping(int programNumber) {
    if (project_ == nullptr) {
        return;
    }
    project_->removeMidiProgramMapping(programNumber);
    const int row = rowForProgram(programNumber);
    if (row >= 0) {
        table_->removeRow(row);
    }
    rebuildAddProgramCombo();
}

void MidiConfigurationPanel::appendRow(const sound_mind::core::MidiProgramMapping& mapping) {
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* programItem = new QTableWidgetItem(programLabel(mapping.programNumber));
    programItem->setData(Qt::UserRole, mapping.programNumber);
    table_->setItem(row, kProgramColumn, programItem);

    auto* presetCombo = new QComboBox(table_);
    presetCombo->addItem(tr("None"));
    if (project_ != nullptr) {
        for (const auto& named : project_->toolPresets()) {
            presetCombo->addItem(QString::fromStdString(named.name),
                                  QVariant::fromValue(static_cast<qulonglong>(named.id)));
        }
    }
    if (mapping.toolPresetId.has_value()) {
        const int index = presetCombo->findData(QVariant::fromValue(static_cast<qulonglong>(*mapping.toolPresetId)));
        presetCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    const int programNumber = mapping.programNumber;
    connect(presetCombo, &QComboBox::currentIndexChanged, this, [this, presetCombo, programNumber](int index) {
        const QVariant data = presetCombo->itemData(index);
        handleToolPresetChanged(programNumber,
                                 data.isValid() ? std::optional(static_cast<sound_mind::core::ToolPresetId>(
                                                       data.toULongLong()))
                                                 : std::nullopt);
    });
    table_->setCellWidget(row, kToolPresetColumn, presetCombo);

    auto* durationSpinBox = new QDoubleSpinBox(table_);
    durationSpinBox->setRange(0.1, 10.0);
    durationSpinBox->setSingleStep(0.1);
    durationSpinBox->setValue(mapping.durationScale);
    connect(durationSpinBox, &QDoubleSpinBox::valueChanged, this,
            [this, programNumber](double value) { handleDurationScaleChanged(programNumber, value); });
    table_->setCellWidget(row, kDurationScaleColumn, durationSpinBox);

    auto* pitchSpinBox = new QDoubleSpinBox(table_);
    pitchSpinBox->setRange(-48.0, 48.0);
    pitchSpinBox->setSingleStep(1.0);
    pitchSpinBox->setValue(mapping.pitchOffsetSemitones);
    connect(pitchSpinBox, &QDoubleSpinBox::valueChanged, this,
            [this, programNumber](double value) { handlePitchOffsetChanged(programNumber, value); });
    table_->setCellWidget(row, kPitchOffsetColumn, pitchSpinBox);

    auto* deleteButton = new QPushButton(tr("Delete"), table_);
    connect(deleteButton, &QPushButton::clicked, this, [this, programNumber]() { deleteMapping(programNumber); });
    table_->setCellWidget(row, kDeleteColumn, deleteButton);
}

int MidiConfigurationPanel::rowForProgram(int programNumber) const {
    for (int row = 0; row < table_->rowCount(); ++row) {
        const auto* item = table_->item(row, kProgramColumn);
        if (item != nullptr && item->data(Qt::UserRole).toInt() == programNumber) {
            return row;
        }
    }
    return -1;
}

}  // namespace sound_mind::studio
