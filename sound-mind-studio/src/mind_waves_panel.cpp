#include "sound_mind/studio/mind_waves_panel.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVariant>

#include "sound_mind/studio/mind_wave_editor.h"

namespace sound_mind::studio {

namespace {

using sound_mind::core::GeneratorType;
using sound_mind::core::MindWave;
using sound_mind::core::MindWaveId;
using sound_mind::core::SuperpositionBlendMode;

/// @brief A short display name for `type` - used as a row's own type
/// badge, and to label each stack member. A small, independent copy of
/// the same mapping `mind_wave_editor.cpp`'s own `kGeneratorTypes` table
/// carries - not shared across these two translation units, the same
/// "duplicated, not shared" precedent this codebase's own Decision #59
/// already established for something this small.
QString generatorTypeBadge(GeneratorType type) {
    switch (type) {
        case GeneratorType::Periodic:
            return QObject::tr("Periodic");
        case GeneratorType::Envelope:
            return QObject::tr("Envelope");
        case GeneratorType::SteppedNoise:
            return QObject::tr("Stepped/Noise");
        case GeneratorType::Spatial:
            return QObject::tr("Spatial");
        case GeneratorType::Fractal:
            return QObject::tr("Fractal");
    }
    return QString();
}

/// @brief A QLabel that emits clicked()/doubleClicked() - the same
/// technique `LayersPanel`'s own `ClickableNameLabel` uses for row
/// selection/rename, duplicated here rather than shared across modules
/// for a class this small and file-local.
class ClickableNameLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableNameLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {
        setCursor(Qt::PointingHandCursor);
    }

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QLabel::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        emit doubleClicked();
        QLabel::mouseDoubleClickEvent(event);
    }
};

/// @brief One library row's own widget - a name (click to select, double-
/// click to rename), a type badge, and a delete button. Deliberately
/// simpler than `LayersPanel`'s own `LayerRowWidget` - no drag handle, no
/// visibility toggle, no opacity slider: a MindWave library entry has
/// none of those concepts.
class MindWaveRowWidget : public QWidget {
    Q_OBJECT

public:
    MindWaveRowWidget(const MindWavesPanel::RowData& data, QWidget* parent = nullptr) : QWidget(parent), id_(data.id) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(2, 1, 2, 1);
        layout->setSpacing(4);

        auto* nameLabel = new ClickableNameLabel(data.name);
        nameLabel->setObjectName(QStringLiteral("nameLabel"));
        nameLabel->setMinimumWidth(60);
        connect(nameLabel, &ClickableNameLabel::clicked, this, [this]() { emit selected(id_); });
        connect(nameLabel, &ClickableNameLabel::doubleClicked, this, [this]() { emit renameRequested(id_); });
        layout->addWidget(nameLabel, 1);

        auto* typeTag = new QLabel(generatorTypeBadge(data.wave.type()));
        typeTag->setObjectName(QStringLiteral("typeTagLabel"));
        typeTag->setStyleSheet(QStringLiteral("color: #7b9fd4; font-size: 9px;"));
        layout->addWidget(typeTag);

        auto* deleteButton = new QPushButton(QStringLiteral("×"));
        deleteButton->setObjectName(QStringLiteral("deleteButton"));
        deleteButton->setFlat(true);
        deleteButton->setFixedWidth(22);
        deleteButton->setStyleSheet(QStringLiteral("color: #c04040;"));
        deleteButton->setToolTip(tr("Delete MindWave"));
        connect(deleteButton, &QPushButton::clicked, this, [this]() { emit deleteRequested(id_); });
        layout->addWidget(deleteButton);
    }

signals:
    void selected(sound_mind::core::MindWaveId id);
    void renameRequested(sound_mind::core::MindWaveId id);
    void deleteRequested(sound_mind::core::MindWaveId id);

private:
    sound_mind::core::MindWaveId id_;
};

}  // namespace

MindWavesPanel::MindWavesPanel(QWidget* parent) : QDockWidget(tr("MindWaves"), parent) {
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(320);

    auto* container = new QWidget();
    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);

    auto* topButtonRow = new QHBoxLayout();

    addButton_ = new QPushButton(tr("+ Add MindWave"));
    addButton_->setObjectName(QStringLiteral("addMindWaveButton"));
    connect(addButton_, &QPushButton::clicked, this, &MindWavesPanel::addRequested);
    topButtonRow->addWidget(addButton_, 1);

    // Preview (see the class's own docs) - a plain checkable toggle, the
    // same "checkbox-shaped QPushButton" style ToolConfigurationPanel's
    // own "Show bounding boxes"/"Show path geometry" checkboxes established
    // for a persistent on/off display aid, though those are QCheckBoxes;
    // a checkable QPushButton reads better alongside addButton_ here.
    previewButton_ = new QPushButton(tr("Preview"));
    previewButton_->setObjectName(QStringLiteral("previewButton"));
    previewButton_->setCheckable(true);
    previewButton_->setToolTip(
        tr("Show the selected MindWave's own field as a live grayscale overlay on the canvas"));
    connect(previewButton_, &QPushButton::toggled, this, &MindWavesPanel::previewToggled);
    topButtonRow->addWidget(previewButton_);

    root->addLayout(topButtonRow);

    list_ = new QListWidget();
    list_->setObjectName(QStringLiteral("mindWavesList"));
    list_->setSelectionMode(QListWidget::SingleSelection);
    list_->setMaximumHeight(150);
    root->addWidget(list_);

    mindWaveEditor_ = new MindWaveEditor();
    mindWaveEditor_->setObjectName(QStringLiteral("mindWaveEditor"));
    mindWaveEditor_->setEnabled(false);
    connect(mindWaveEditor_, &MindWaveEditor::mindWaveChanged, this,
            [this](const MindWave&) { emitCurrentMindWaveChanged(); });
    root->addWidget(mindWaveEditor_);

    auto* stackLabel = new QLabel(tr("Superposition"));
    root->addWidget(stackLabel);

    auto* stackButtonRow = new QHBoxLayout();
    addMemberButton_ = new QPushButton(tr("+ Add Member"));
    addMemberButton_->setObjectName(QStringLiteral("addMemberButton"));
    connect(addMemberButton_, &QPushButton::clicked, this, [this]() {
        if (!selectedMindWaveId_.has_value()) {
            return;
        }
        currentStack_.push_back(MindWave{});
        refreshStackList();
        stackList_->setCurrentRow(static_cast<int>(currentStack_.size()) - 1);
        emitCurrentMindWaveChanged();
    });
    stackButtonRow->addWidget(addMemberButton_);

    removeMemberButton_ = new QPushButton(tr("- Remove Member"));
    removeMemberButton_->setObjectName(QStringLiteral("removeMemberButton"));
    connect(removeMemberButton_, &QPushButton::clicked, this, [this]() {
        if (!selectedStackMemberIndex_.has_value()) {
            return;
        }
        currentStack_.erase(currentStack_.begin() + *selectedStackMemberIndex_);
        selectedStackMemberIndex_.reset();
        stackMemberEditor_->setEnabled(false);
        refreshStackList();
        emitCurrentMindWaveChanged();
    });
    stackButtonRow->addWidget(removeMemberButton_);
    root->addLayout(stackButtonRow);

    auto* blendForm = new QHBoxLayout();
    blendModeLabel_ = new QLabel(tr("Blend Mode:"));
    blendForm->addWidget(blendModeLabel_);
    blendModeCombo_ = new QComboBox();
    blendModeCombo_->setObjectName(QStringLiteral("blendModeCombo"));
    blendModeCombo_->addItem(tr("Multiply"), QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Multiply)));
    blendModeCombo_->addItem(tr("Add"), QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Add)));
    blendModeCombo_->addItem(tr("Min"), QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Min)));
    blendModeCombo_->addItem(tr("Max"), QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Max)));
    blendModeCombo_->addItem(tr("Average"), QVariant::fromValue(static_cast<int>(SuperpositionBlendMode::Average)));
    connect(blendModeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        currentBlendMode_ = static_cast<SuperpositionBlendMode>(blendModeCombo_->itemData(index).toInt());
        emitCurrentMindWaveChanged();
    });
    blendForm->addWidget(blendModeCombo_, 1);
    root->addLayout(blendForm);

    stackList_ = new QListWidget();
    stackList_->setObjectName(QStringLiteral("stackList"));
    stackList_->setSelectionMode(QListWidget::SingleSelection);
    stackList_->setMaximumHeight(100);
    connect(stackList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(currentStack_.size())) {
            selectedStackMemberIndex_.reset();
            stackMemberEditor_->setEnabled(false);
            return;
        }
        selectedStackMemberIndex_ = row;
        stackMemberEditor_->setEnabled(true);
        stackMemberEditor_->setMindWave(currentStack_[static_cast<std::size_t>(row)]);
    });
    root->addWidget(stackList_);

    stackMemberEditor_ = new MindWaveEditor();
    stackMemberEditor_->setObjectName(QStringLiteral("stackMemberEditor"));
    stackMemberEditor_->setEnabled(false);
    connect(stackMemberEditor_, &MindWaveEditor::mindWaveChanged, this, [this](const MindWave& wave) {
        if (!selectedStackMemberIndex_.has_value()) {
            return;
        }
        currentStack_[static_cast<std::size_t>(*selectedStackMemberIndex_)] = wave;
        emitCurrentMindWaveChanged();
    });
    root->addWidget(stackMemberEditor_, 1);

    // --- Warp (v0.Y.39.1 Installment A) -----------------------------------
    // One field distorts the coordinates another is sampled at - see
    // MindWave::hasWarpSource()'s own docs. A single 0-or-1 slot, not a
    // list (unlike Superposition above), so this section reuses the same
    // "checkbox enables a single nested editor" shape rather than
    // stackList_'s own add/remove list.
    auto* warpHeaderRow = new QHBoxLayout();
    warpEnabledCheckBox_ = new QCheckBox(tr("Enable Warp"));
    warpEnabledCheckBox_->setObjectName(QStringLiteral("warpEnabledCheckBox"));
    connect(warpEnabledCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        currentWarpEnabled_ = checked;
        warpSourceEditor_->setEnabled(checked);
        warpStrengthSpinBox_->setEnabled(checked);
        emitCurrentMindWaveChanged();
    });
    warpHeaderRow->addWidget(warpEnabledCheckBox_);

    warpHeaderRow->addWidget(new QLabel(tr("Strength:")));
    warpStrengthSpinBox_ = new QDoubleSpinBox();
    warpStrengthSpinBox_->setObjectName(QStringLiteral("warpStrengthSpinBox"));
    warpStrengthSpinBox_->setRange(-10.0, 10.0);
    warpStrengthSpinBox_->setSingleStep(0.1);
    warpStrengthSpinBox_->setEnabled(false);
    connect(warpStrengthSpinBox_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        currentWarpStrength_ = value;
        emitCurrentMindWaveChanged();
    });
    warpHeaderRow->addWidget(warpStrengthSpinBox_, 1);
    root->addLayout(warpHeaderRow);

    warpSourceEditor_ = new MindWaveEditor();
    warpSourceEditor_->setObjectName(QStringLiteral("warpSourceEditor"));
    warpSourceEditor_->setEnabled(false);
    connect(warpSourceEditor_, &MindWaveEditor::mindWaveChanged, this, [this](const MindWave& wave) {
        currentWarpSource_ = wave;
        emitCurrentMindWaveChanged();
    });
    root->addWidget(warpSourceEditor_, 1);

    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(currentRows_.size())) {
            return;
        }
        // list_ is populated in currentRows_'s own order (no reversal
        // needed - unlike LayersPanel, a MindWave library has no
        // "top/bottom of stack" concept to display in reverse).
        const RowData& selected = currentRows_[static_cast<std::size_t>(row)];
        selectedMindWaveId_ = selected.id;
        mindWaveEditor_->setEnabled(true);
        mindWaveEditor_->setMindWave(selected.wave);
        loadStackState(selected.wave);
        loadWarpState(selected.wave);
        emit selectionChanged(selected.id);
    });

    // A real QScrollArea, not just a plain wrapper widget - matching the
    // established convention every other multi-field dock panel already
    // uses (FilterConfigurationPanel, ToolConfigurationPanel, GridPanel,
    // PlaybackPanel, RecordPanel, LoopPanel). Without it, `container`'s own
    // QVBoxLayout forces this dock's minimum height up to fit everything
    // at once - two uncapped MindWaveEditors plus the library list plus
    // the superposition controls - which is genuinely too tall to fit a
    // typical dock area and, critically, can't be shrunk below that either.
    // `setWidgetResizable(true)` lets the dock resize freely regardless of
    // that content height, with a scrollbar picking up whatever doesn't fit.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);

    // currentStack_ starts empty (no selection yet) - this establishes the
    // correct initial hidden state for the Superposition section's own
    // controls (see refreshStackList()'s own docs) rather than leaving them
    // at Qt's default-visible state until the first real selection.
    refreshStackList();
}

void MindWavesPanel::loadStackState(const MindWave& wave) {
    currentStack_ = wave.superpositionStack();
    currentBlendMode_ = wave.superpositionBlendMode();
    selectedStackMemberIndex_.reset();
    stackMemberEditor_->setEnabled(false);

    const QSignalBlocker blendBlocker(blendModeCombo_);
    const int blendIndex = blendModeCombo_->findData(QVariant::fromValue(static_cast<int>(currentBlendMode_)));
    blendModeCombo_->setCurrentIndex(blendIndex >= 0 ? blendIndex : 0);

    refreshStackList();
}

void MindWavesPanel::loadWarpState(const MindWave& wave) {
    currentWarpEnabled_ = wave.hasWarpSource();
    currentWarpSource_ = wave.hasWarpSource() ? wave.warpSource() : MindWave{};
    currentWarpStrength_ = wave.warpStrength();

    const QSignalBlocker checkBlocker(warpEnabledCheckBox_);
    warpEnabledCheckBox_->setChecked(currentWarpEnabled_);
    const QSignalBlocker strengthBlocker(warpStrengthSpinBox_);
    warpStrengthSpinBox_->setValue(currentWarpStrength_);
    warpStrengthSpinBox_->setEnabled(currentWarpEnabled_);
    warpSourceEditor_->setMindWave(currentWarpSource_);
    warpSourceEditor_->setEnabled(currentWarpEnabled_);
}

void MindWavesPanel::refreshStackList() {
    const QSignalBlocker listBlocker(stackList_);
    stackList_->clear();
    for (std::size_t i = 0; i < currentStack_.size(); ++i) {
        auto* item = new QListWidgetItem(
            tr("%1: %2").arg(i + 1).arg(generatorTypeBadge(currentStack_[i].type())));
        stackList_->addItem(item);
    }

    // See this method's own docs - addMemberButton_ (and the "Superposition"
    // label) are the only controls still shown with an empty stack.
    const bool hasMembers = !currentStack_.empty();
    removeMemberButton_->setVisible(hasMembers);
    blendModeLabel_->setVisible(hasMembers);
    blendModeCombo_->setVisible(hasMembers);
    stackList_->setVisible(hasMembers);
    stackMemberEditor_->setVisible(hasMembers);
}

void MindWavesPanel::emitCurrentMindWaveChanged() {
    if (!selectedMindWaveId_.has_value()) {
        return;
    }
    MindWave composite = mindWaveEditor_->mindWave();
    composite.setSuperpositionStack(currentStack_);
    composite.setSuperpositionBlendMode(currentBlendMode_);
    if (currentWarpEnabled_) {
        composite.setWarpSource(currentWarpSource_);
    } else {
        composite.clearWarpSource();
    }
    composite.setWarpStrength(currentWarpStrength_);
    emit mindWaveChanged(*selectedMindWaveId_, composite);
}

void MindWavesPanel::setMindWaves(const std::vector<RowData>& entries) {
    currentRows_ = entries;

    bool stillPresent = false;
    if (selectedMindWaveId_.has_value()) {
        stillPresent = std::any_of(entries.begin(), entries.end(),
                                    [this](const RowData& row) { return row.id == *selectedMindWaveId_; });
        if (!stillPresent) {
            selectedMindWaveId_.reset();
            mindWaveEditor_->setEnabled(false);
            stackMemberEditor_->setEnabled(false);
            currentStack_.clear();
            selectedStackMemberIndex_.reset();
            currentWarpEnabled_ = false;
            currentWarpSource_ = MindWave{};
            warpEnabledCheckBox_->setChecked(false);
            warpSourceEditor_->setEnabled(false);
            warpStrengthSpinBox_->setEnabled(false);
            emit selectionChanged(std::nullopt);
        }
    }

    // Same deleteLater()-before-clear() gotcha LayersPanel::setLayers()'s
    // own docs explain in detail - an item widget set via
    // setItemWidget() isn't owned by the QListWidgetItem it's attached
    // to, and a plain delete here could destroy a row still executing one
    // of its own signal handlers further up this very call stack.
    const QSignalBlocker listBlocker(list_);
    for (int i = 0; i < list_->count(); ++i) {
        if (QWidget* rowWidget = list_->itemWidget(list_->item(i))) {
            rowWidget->deleteLater();
        }
    }
    list_->clear();
    int rowToReselect = -1;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const RowData& row = entries[i];
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(row.id)));
        item->setSizeHint(QSize(0, 28));
        list_->addItem(item);
        auto* rowWidget = new MindWaveRowWidget(row, list_);
        connect(rowWidget, &MindWaveRowWidget::selected, list_, [this, i]() { list_->setCurrentRow(static_cast<int>(i)); });
        connect(rowWidget, &MindWaveRowWidget::renameRequested, this, &MindWavesPanel::renameRequested);
        connect(rowWidget, &MindWaveRowWidget::deleteRequested, this, &MindWavesPanel::deleteRequested);
        list_->setItemWidget(item, rowWidget);
        if (stillPresent && row.id == *selectedMindWaveId_) {
            rowToReselect = static_cast<int>(i);
        }
    }

    if (rowToReselect >= 0) {
        // Redisplay from entries' own (possibly just-updated) data, rather
        // than leaving the editors showing whatever they had before this
        // refresh - see setMindWaves()'s own docs.
        const RowData& row = entries[static_cast<std::size_t>(rowToReselect)];
        list_->setCurrentRow(rowToReselect);
        mindWaveEditor_->setMindWave(row.wave);
        loadStackState(row.wave);
        loadWarpState(row.wave);
    }
}

void MindWavesPanel::clearSelection() {
    selectedMindWaveId_.reset();
    selectedStackMemberIndex_.reset();
    currentStack_.clear();
    currentWarpEnabled_ = false;
    currentWarpSource_ = MindWave{};
    mindWaveEditor_->setEnabled(false);
    stackMemberEditor_->setEnabled(false);
    warpEnabledCheckBox_->setChecked(false);
    warpSourceEditor_->setEnabled(false);
    warpStrengthSpinBox_->setEnabled(false);
    list_->clearSelection();
    list_->setCurrentRow(-1);
    emit selectionChanged(std::nullopt);
}

void MindWavesPanel::selectMindWave(MindWaveId id) {
    for (std::size_t i = 0; i < currentRows_.size(); ++i) {
        if (currentRows_[i].id == id) {
            list_->setCurrentRow(static_cast<int>(i));
            return;
        }
    }
}

bool MindWavesPanel::previewEnabled() const { return previewButton_->isChecked(); }

void MindWavesPanel::setPreviewEnabled(bool enabled) {
    const QSignalBlocker blocker(previewButton_);
    previewButton_->setChecked(enabled);
}

}  // namespace sound_mind::studio

#include "mind_waves_panel.moc"
