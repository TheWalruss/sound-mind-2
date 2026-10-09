#include "sound_mind/studio/resource_browser_panel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVariant>

namespace sound_mind::studio {

namespace {

QString categoryLabel(ResourceCategory category) {
    switch (category) {
        case ResourceCategory::MindWave:
            return QObject::tr("MindWave");
        case ResourceCategory::ToolPreset:
            return QObject::tr("Tool Preset");
        case ResourceCategory::MindShot:
            return QObject::tr("Mind Shot");
        case ResourceCategory::ResonanceProfile:
            return QObject::tr("Resonance Profile");
        case ResourceCategory::ConvolutionKernel:
            return QObject::tr("Filter");
        case ResourceCategory::MindGrain:
            return QObject::tr("Mind Grain");
        case ResourceCategory::Layer:
            return QObject::tr("Layer");
    }
    return QString();
}

}  // namespace

ResourceBrowserPanel::ResourceBrowserPanel(QWidget* parent) : QDockWidget(tr("Resource Browser"), parent) {
    auto* content = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(content);

    browsingLabel_ = new QLabel(tr("Browsing: this project"), content);
    browsingLabel_->setObjectName(QStringLiteral("browsingLabel"));
    rootLayout->addWidget(browsingLabel_);

    auto* browseRow = new QHBoxLayout();
    browseOtherProjectButton_ = new QPushButton(tr("Browse Other Project..."), content);
    browseOtherProjectButton_->setObjectName(QStringLiteral("browseOtherProjectButton"));
    returnToCurrentProjectButton_ = new QPushButton(tr("Return to This Project"), content);
    returnToCurrentProjectButton_->setObjectName(QStringLiteral("returnToCurrentProjectButton"));
    returnToCurrentProjectButton_->setVisible(false);
    browseRow->addWidget(browseOtherProjectButton_);
    browseRow->addWidget(returnToCurrentProjectButton_);
    rootLayout->addLayout(browseRow);
    connect(browseOtherProjectButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::browseOtherProjectRequested);
    connect(returnToCurrentProjectButton_, &QPushButton::clicked, this,
            &ResourceBrowserPanel::returnToCurrentProjectRequested);

    categoryCombo_ = new QComboBox(content);
    categoryCombo_->setObjectName(QStringLiteral("categoryCombo"));
    populateCategoryCombo();
    rootLayout->addWidget(categoryCombo_);
    connect(categoryCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        selectedCategory_ = static_cast<ResourceCategory>(categoryCombo_->itemData(index).toInt());
        emit categoryChanged(selectedCategory_);
    });

    auto* splitter = new QSplitter(Qt::Horizontal, content);

    auto* listPane = new QWidget(splitter);
    auto* listLayout = new QVBoxLayout(listPane);
    entriesList_ = new QListWidget(listPane);
    entriesList_->setObjectName(QStringLiteral("entriesList"));
    listLayout->addWidget(entriesList_);
    importFromFileButton_ = new QPushButton(tr("Import from File..."), listPane);
    importFromFileButton_->setObjectName(QStringLiteral("importFromFileButton"));
    listLayout->addWidget(importFromFileButton_);
    connect(importFromFileButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::importFromFileRequested);
    connect(entriesList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (current == nullptr) {
            selectedEntryId_ = std::nullopt;
            emit entrySelected(std::nullopt);
            return;
        }
        selectedEntryId_ = current->data(Qt::UserRole).toULongLong();
        emit entrySelected(selectedEntryId_);
    });
    splitter->addWidget(listPane);

    auto* inspectorPane = new QWidget(splitter);
    auto* inspectorLayout = new QVBoxLayout(inspectorPane);
    inspectorNameLabel_ = new QLabel(inspectorPane);
    inspectorNameLabel_->setObjectName(QStringLiteral("inspectorNameLabel"));
    inspectorLayout->addWidget(inspectorNameLabel_);
    inspectorRasterLabel_ = new QLabel(inspectorPane);
    inspectorRasterLabel_->setObjectName(QStringLiteral("inspectorRasterLabel"));
    inspectorRasterLabel_->setMinimumHeight(80);
    inspectorRasterLabel_->setAlignment(Qt::AlignCenter);
    inspectorLayout->addWidget(inspectorRasterLabel_);
    inspectorParametersEdit_ = new QPlainTextEdit(inspectorPane);
    inspectorParametersEdit_->setObjectName(QStringLiteral("inspectorParametersEdit"));
    inspectorParametersEdit_->setReadOnly(true);
    inspectorLayout->addWidget(inspectorParametersEdit_);

    auto* actionsRow = new QHBoxLayout();
    playButton_ = new QPushButton(tr("Play"), inspectorPane);
    playButton_->setObjectName(QStringLiteral("playButton"));
    stopButton_ = new QPushButton(tr("Stop"), inspectorPane);
    stopButton_->setObjectName(QStringLiteral("stopButton"));
    stopButton_->setVisible(false);
    exportButton_ = new QPushButton(tr("Export..."), inspectorPane);
    exportButton_->setObjectName(QStringLiteral("exportButton"));
    importEntryButton_ = new QPushButton(tr("Import"), inspectorPane);
    importEntryButton_->setObjectName(QStringLiteral("importEntryButton"));
    actionsRow->addWidget(playButton_);
    actionsRow->addWidget(stopButton_);
    actionsRow->addWidget(exportButton_);
    actionsRow->addWidget(importEntryButton_);
    inspectorLayout->addLayout(actionsRow);
    connect(playButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::playRequested);
    connect(stopButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::stopRequested);
    connect(exportButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::exportRequested);
    connect(importEntryButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::importEntryRequested);
    splitter->addWidget(inspectorPane);

    rootLayout->addWidget(splitter);

    auto* toolkitLabel = new QLabel(tr("Toolkit draft"), content);
    rootLayout->addWidget(toolkitLabel);
    toolkitEntriesList_ = new QListWidget(content);
    toolkitEntriesList_->setObjectName(QStringLiteral("toolkitEntriesList"));
    rootLayout->addWidget(toolkitEntriesList_);

    auto* toolkitRow = new QHBoxLayout();
    addToToolkitButton_ = new QPushButton(tr("Add to Toolkit"), content);
    addToToolkitButton_->setObjectName(QStringLiteral("addToToolkitButton"));
    removeFromToolkitButton_ = new QPushButton(tr("Remove"), content);
    removeFromToolkitButton_->setObjectName(QStringLiteral("removeFromToolkitButton"));
    exportToolkitButton_ = new QPushButton(tr("Export Toolkit..."), content);
    exportToolkitButton_->setObjectName(QStringLiteral("exportToolkitButton"));
    importToolkitButton_ = new QPushButton(tr("Import Toolkit..."), content);
    importToolkitButton_->setObjectName(QStringLiteral("importToolkitButton"));
    toolkitRow->addWidget(addToToolkitButton_);
    toolkitRow->addWidget(removeFromToolkitButton_);
    toolkitRow->addWidget(exportToolkitButton_);
    toolkitRow->addWidget(importToolkitButton_);
    rootLayout->addLayout(toolkitRow);

    connect(addToToolkitButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::addToToolkitRequested);
    connect(removeFromToolkitButton_, &QPushButton::clicked, this, [this]() {
        if (const auto index = selectedToolkitEntryIndex()) {
            emit removeFromToolkitRequested(*index);
        }
    });
    connect(exportToolkitButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::exportToolkitRequested);
    connect(importToolkitButton_, &QPushButton::clicked, this, &ResourceBrowserPanel::importToolkitRequested);

    clearInspector();
    setWidget(content);
}

void ResourceBrowserPanel::populateCategoryCombo() {
    static constexpr ResourceCategory kCategories[] = {
        ResourceCategory::MindWave,       ResourceCategory::ToolPreset, ResourceCategory::MindShot,
        ResourceCategory::ResonanceProfile, ResourceCategory::ConvolutionKernel, ResourceCategory::MindGrain,
        ResourceCategory::Layer,
    };
    for (const ResourceCategory category : kCategories) {
        categoryCombo_->addItem(categoryLabel(category), static_cast<int>(category));
    }
}

void ResourceBrowserPanel::setEntries(const std::vector<RowData>& rows) {
    const QSignalBlocker blocker(entriesList_);
    entriesList_->clear();
    for (const RowData& row : rows) {
        auto* item = new QListWidgetItem(row.name, entriesList_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(row.id));
    }
    selectedEntryId_ = std::nullopt;
    clearInspector();
}

void ResourceBrowserPanel::setImportFromFileEnabled(bool enabled) { importFromFileButton_->setVisible(enabled); }

void ResourceBrowserPanel::clearInspector() {
    inspectorNameLabel_->clear();
    inspectorParametersEdit_->clear();
    inspectorRasterLabel_->clear();
    inspectorRasterLabel_->setVisible(false);
    inspectorCanPlay_ = false;
    playButton_->setVisible(false);
    stopButton_->setVisible(false);
    exportButton_->setVisible(false);
    importEntryButton_->setVisible(false);
}

void ResourceBrowserPanel::setInspector(const QString& name, const QString& parameterText, const QImage& raster,
                                         bool canPlay, bool canExport, bool canImportEntry) {
    inspectorNameLabel_->setText(name);
    inspectorParametersEdit_->setPlainText(parameterText);
    if (raster.isNull()) {
        inspectorRasterLabel_->clear();
        inspectorRasterLabel_->setVisible(false);
    } else {
        inspectorRasterLabel_->setPixmap(QPixmap::fromImage(raster));
        inspectorRasterLabel_->setVisible(true);
    }
    inspectorCanPlay_ = canPlay;
    playButton_->setVisible(canPlay);
    stopButton_->setVisible(false);
    exportButton_->setVisible(canExport);
    importEntryButton_->setVisible(canImportEntry);
}

void ResourceBrowserPanel::setPlaying(bool playing) {
    playButton_->setVisible(!playing && inspectorCanPlay_);
    stopButton_->setVisible(playing && inspectorCanPlay_);
}

void ResourceBrowserPanel::setBrowsingOtherProject(const QString& otherProjectPath) {
    const bool browsingOther = !otherProjectPath.isEmpty();
    browsingLabel_->setText(browsingOther ? tr("Browsing: %1").arg(otherProjectPath) : tr("Browsing: this project"));
    browseOtherProjectButton_->setVisible(!browsingOther);
    returnToCurrentProjectButton_->setVisible(browsingOther);
}

void ResourceBrowserPanel::setToolkitEntries(const QStringList& names) {
    const QSignalBlocker blocker(toolkitEntriesList_);
    toolkitEntriesList_->clear();
    toolkitEntriesList_->addItems(names);
}

void ResourceBrowserPanel::setAddToToolkitEnabled(bool enabled) { addToToolkitButton_->setEnabled(enabled); }

std::optional<int> ResourceBrowserPanel::selectedToolkitEntryIndex() const {
    const int row = toolkitEntriesList_->currentRow();
    return row >= 0 ? std::optional<int>(row) : std::nullopt;
}

}  // namespace sound_mind::studio
