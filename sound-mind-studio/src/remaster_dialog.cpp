#include "sound_mind/studio/remaster_dialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

namespace {

constexpr int kSourceIdRole = Qt::UserRole + 1;

QString kindLabel(const std::string& kind) {
    if (kind == "audio") return QObject::tr("Audio file");
    if (kind == "image") return QObject::tr("Image file");
    if (kind == "midi") return QObject::tr("MIDI file");
    if (kind == "recording") return QObject::tr("Recording");
    if (kind == "loopTake") return QObject::tr("Loop take");
    if (kind == "layerImage") return QObject::tr("Imported layer");
    return QString::fromStdString(kind);
}

QString toQString(const std::filesystem::path& path) { return QString::fromStdWString(path.wstring()); }

}  // namespace

RemasterDialog::RemasterDialog(const sound_mind::core::Project& project, std::filesystem::path projectPath,
                               QWidget* parent)
    : QDialog(parent), project_(project), projectPath_(std::move(projectPath)) {
    setWindowTitle(tr("Remaster"));
    auto* root = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Rebuilds the project as a new project by replaying everything done to it, from its "
                                "original sources - at different settings, or with some sources replaced. The "
                                "project stays as it is."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    const sound_mind::core::ProjectSettings& settings = project.settings();
    auto* form = new QFormLayout();
    sampleRateField_ = new IntegerField();
    sampleRateField_->setObjectName(QStringLiteral("remasterSampleRate"));
    sampleRateField_->setRange(8000, 192000);
    sampleRateField_->setSuffix(tr(" Hz"));
    sampleRateField_->setValue(static_cast<int>(settings.sampleRateHz));
    form->addRow(tr("Sample Rate:"), sampleRateField_);

    timestepField_ = new NumericField();
    timestepField_->setObjectName(QStringLiteral("remasterTimestep"));
    timestepField_->setRange(0.1, 1000.0);
    timestepField_->setDecimals(2);
    timestepField_->setSuffix(tr(" ms"));
    timestepField_->setValue(settings.timestepMs);
    form->addRow(tr("Timestep:"), timestepField_);

    binCountField_ = new IntegerField();
    binCountField_->setObjectName(QStringLiteral("remasterBinCount"));
    binCountField_->setRange(32, 8192);
    binCountField_->setValue(static_cast<int>(settings.binCount));
    form->addRow(tr("Bin Count:"), binCountField_);

    minFrequencyField_ = new IntegerField();
    minFrequencyField_->setObjectName(QStringLiteral("remasterMinFrequency"));
    minFrequencyField_->setRange(1, 96000);
    minFrequencyField_->setSuffix(tr(" Hz"));
    minFrequencyField_->setValue(static_cast<int>(settings.minFrequencyHz));
    form->addRow(tr("Min Frequency:"), minFrequencyField_);

    maxFrequencyField_ = new IntegerField();
    maxFrequencyField_->setObjectName(QStringLiteral("remasterMaxFrequency"));
    maxFrequencyField_->setRange(1, 192000);
    maxFrequencyField_->setSuffix(tr(" Hz"));
    maxFrequencyField_->setValue(static_cast<int>(settings.maxFrequencyHz));
    form->addRow(tr("Max Frequency:"), maxFrequencyField_);
    root->addLayout(form);

    root->addWidget(new QLabel(tr("Sources (replace any with another file):")));
    sourcesTable_ = new QTableWidget(static_cast<int>(project.sources().size()), 4);
    sourcesTable_->setObjectName(QStringLiteral("remasterSources"));
    sourcesTable_->setHorizontalHeaderLabels({tr("Source"), tr("Kind"), tr("Replace with"), QString()});
    sourcesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    sourcesTable_->verticalHeader()->hide();
    sourcesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int row = 0; row < static_cast<int>(project.sources().size()); ++row) {
        const sound_mind::core::ProjectSource& source = project.sources()[static_cast<std::size_t>(row)];
        auto* name = new QTableWidgetItem(QString::fromStdString(source.originalName));
        name->setData(kSourceIdRole, static_cast<qulonglong>(source.id));
        sourcesTable_->setItem(row, 0, name);
        sourcesTable_->setItem(row, 1, new QTableWidgetItem(kindLabel(source.kind)));
        sourcesTable_->setItem(row, 2, new QTableWidgetItem());
        auto* replace = new QPushButton(tr("Replace..."));
        connect(replace, &QPushButton::clicked, this, [this, row]() { chooseReplacement(row); });
        sourcesTable_->setCellWidget(row, 3, replace);
    }
    sourcesTable_->setMinimumHeight(120);
    root->addWidget(sourcesTable_, 1);

    auto* outputRow = new QHBoxLayout();
    outputEdit_ = new QLineEdit();
    outputEdit_->setObjectName(QStringLiteral("remasterOutput"));
    auto* browse = new QPushButton(tr("Browse..."));
    connect(browse, &QPushButton::clicked, this, &RemasterDialog::browseForOutput);
    outputRow->addWidget(outputEdit_, 1);
    outputRow->addWidget(browse);
    auto* outputForm = new QFormLayout();
    outputForm->addRow(tr("Save as:"), outputRow);
    root->addLayout(outputForm);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox_->button(QDialogButtonBox::Ok)->setText(tr("Remaster"));
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox_);
    connect(outputEdit_, &QLineEdit::textChanged, this, &RemasterDialog::updateOkEnabled);
    resize(560, 520);
    updateOkEnabled();
}

RemasterRequest RemasterDialog::request() const {
    RemasterRequest request;
    request.settings = project_.settings();
    request.settings.sampleRateHz = static_cast<std::uint32_t>(sampleRateField_->value());
    request.settings.timestepMs = timestepField_->value();
    request.settings.binCount = static_cast<std::uint32_t>(binCountField_->value());
    request.settings.canvasHeight = request.settings.binCount;
    request.settings.minFrequencyHz = static_cast<float>(minFrequencyField_->value());
    request.settings.maxFrequencyHz = static_cast<float>(maxFrequencyField_->value());
    request.replacements = replacements_;
    request.outputPath = std::filesystem::path(outputEdit_->text().trimmed().toStdWString());
    return request;
}

void RemasterDialog::setOutputPath(const std::filesystem::path& path) { outputEdit_->setText(toQString(path)); }

void RemasterDialog::setReplacement(sound_mind::core::SourceId id, const std::filesystem::path& path) {
    replacements_[id] = path;
    for (int row = 0; row < sourcesTable_->rowCount(); ++row) {
        if (sourcesTable_->item(row, 0)->data(kSourceIdRole).toULongLong() == id) {
            sourcesTable_->item(row, 2)->setText(toQString(path.filename()));
        }
    }
}

void RemasterDialog::chooseReplacement(int row) {
    const QString file = QFileDialog::getOpenFileName(this, tr("Replace Source"));
    if (!file.isEmpty()) {
        setReplacement(sourcesTable_->item(row, 0)->data(kSourceIdRole).toULongLong(),
                       std::filesystem::path(file.toStdWString()));
    }
}

void RemasterDialog::browseForOutput() {
    QString file = QFileDialog::getSaveFileName(this, tr("Save Remastered Project As"), QString(),
                                                tr("Sound Mind Projects (*.smproj)"));
    if (file.isEmpty()) {
        return;
    }
    if (!file.endsWith(QStringLiteral(".smproj"), Qt::CaseInsensitive)) {
        file += QStringLiteral(".smproj");
    }
    outputEdit_->setText(file);
}

void RemasterDialog::updateOkEnabled() {
    const QString text = outputEdit_->text().trimmed();
    const bool valid = !text.isEmpty() && std::filesystem::path(text.toStdWString()) != projectPath_;
    buttonBox_->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

}  // namespace sound_mind::studio
