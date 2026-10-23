#include "sound_mind/studio/image_export_dialog.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

namespace {

/// @brief Each format's label and the extension it writes.
struct Format {
    const char* label;
    const char* extension;
};
constexpr std::array<Format, 5> kFormats{{
    {"PNG", ".png"},
    {"JPEG", ".jpg"},
    {"BMP", ".bmp"},
    {"TGA", ".tga"},
    {"WebP", ".webp"},
}};

/// @brief The largest size either side can be typed as.
constexpr int kMaximumSide = 32768;

/// @brief The kFormats index for `path`'s extension, or -1.
int formatIndexFor(const std::filesystem::path& path) {
    const auto format = imageFormatForPath(path);
    if (!format.has_value()) {
        return -1;
    }
    for (std::size_t i = 0; i < kFormats.size(); ++i) {
        if (QString::fromLatin1(kFormats[i].label).compare(QString::fromLatin1(*format), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

ImageExportDialog::ImageExportDialog(const std::filesystem::path& suggestedPath, bool polar, QSize fullSize,
                                     QSize fullPolarSize, QWidget* parent)
    : QDialog(parent), fullSize_(fullSize), fullPolarSize_(fullPolarSize) {
    setWindowTitle(tr("Export Image"));
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    auto* pathRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(QString::fromStdWString(suggestedPath.wstring()), this);
    pathEdit_->setObjectName(QStringLiteral("imagePathEdit"));
    connect(pathEdit_, &QLineEdit::editingFinished, this, &ImageExportDialog::pathEdited);
    connect(pathEdit_, &QLineEdit::textChanged, this, &ImageExportDialog::updateState);
    auto* browseButton = new QPushButton(tr("Browse..."), this);
    browseButton->setObjectName(QStringLiteral("imageBrowseButton"));
    connect(browseButton, &QPushButton::clicked, this, &ImageExportDialog::browse);
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseButton);
    form->addRow(tr("File:"), pathRow);

    formatCombo_ = new QComboBox(this);
    formatCombo_->setObjectName(QStringLiteral("imageFormatCombo"));
    for (const Format& format : kFormats) {
        formatCombo_->addItem(QString::fromLatin1(format.label));
    }
    formatCombo_->setCurrentIndex(std::max(0, formatIndexFor(suggestedPath)));
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, &ImageExportDialog::formatChosen);
    form->addRow(tr("Format:"), formatCombo_);

    polarCheckBox_ = new QCheckBox(tr("As Sound Flower"), this);
    polarCheckBox_->setObjectName(QStringLiteral("imagePolarCheckBox"));
    polarCheckBox_->setToolTip(tr("The polar view: the canvas wrapped into a disk, in a square image"));
    polarCheckBox_->setChecked(polar);
    connect(polarCheckBox_, &QCheckBox::toggled, this, &ImageExportDialog::updateState);
    form->addRow(tr("View:"), polarCheckBox_);
    root->addLayout(form);

    auto* sizeGroup = new QButtonGroup(this);
    fullResolutionRadio_ = new QRadioButton(this);
    fullResolutionRadio_->setObjectName(QStringLiteral("imageFullResolutionRadio"));
    fullResolutionRadio_->setChecked(true);
    customSizeRadio_ = new QRadioButton(tr("Custom size:"), this);
    customSizeRadio_->setObjectName(QStringLiteral("imageCustomSizeRadio"));
    sizeGroup->addButton(fullResolutionRadio_);
    sizeGroup->addButton(customSizeRadio_);
    connect(customSizeRadio_, &QRadioButton::toggled, this, &ImageExportDialog::updateState);
    root->addWidget(fullResolutionRadio_);
    root->addWidget(customSizeRadio_);

    auto* customForm = new QFormLayout();
    customForm->setContentsMargins(24, 0, 0, 0);
    widthField_ = new IntegerField(this);
    widthField_->setObjectName(QStringLiteral("imageWidthField"));
    widthField_->setRange(1, kMaximumSide);
    widthField_->setSuffix(tr(" px"));
    heightField_ = new IntegerField(this);
    heightField_->setObjectName(QStringLiteral("imageHeightField"));
    heightField_->setRange(1, kMaximumSide);
    heightField_->setSuffix(tr(" px"));
    const QSize start = polar ? fullPolarSize_ : fullSize_;
    widthField_->setValue(std::max(1, start.width()));
    heightField_->setValue(std::max(1, start.height()));
    connect(widthField_, &IntegerField::valueChanged, this, &ImageExportDialog::widthChanged);
    connect(heightField_, &IntegerField::valueChanged, this, &ImageExportDialog::heightChanged);
    customForm->addRow(tr("Width:"), widthField_);
    customForm->addRow(tr("Height:"), heightField_);
    keepProportionsCheckBox_ = new QCheckBox(tr("Keep proportions"), this);
    keepProportionsCheckBox_->setObjectName(QStringLiteral("imageKeepProportionsCheckBox"));
    keepProportionsCheckBox_->setChecked(true);
    customForm->addRow(QString(), keepProportionsCheckBox_);
    root->addLayout(customForm);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons_);

    updateState();
}

std::filesystem::path ImageExportDialog::outputPath() const {
    return std::filesystem::path(pathEdit_->text().trimmed().toStdWString());
}

ImageExportOptions ImageExportDialog::options() const {
    ImageExportOptions options;
    options.polar = polarCheckBox_->isChecked();
    if (customSizeRadio_->isChecked()) {
        options.width = static_cast<std::uint32_t>(widthField_->value());
        options.height = static_cast<std::uint32_t>(heightField_->value());
    }
    return options;
}

QSize ImageExportDialog::currentFullSize() const { return polarCheckBox_->isChecked() ? fullPolarSize_ : fullSize_; }

void ImageExportDialog::browse() {
    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("Export Image"), pathEdit_->text(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;"
           "TGA (*.tga);;WebP (*.webp)"));
    if (!chosen.isEmpty()) {
        pathEdit_->setText(chosen);
        pathEdited();
    }
}

void ImageExportDialog::formatChosen() {
    // The file name's extension follows the format.
    std::filesystem::path path = outputPath();
    if (path.empty()) {
        return;
    }
    path.replace_extension(kFormats[static_cast<std::size_t>(formatCombo_->currentIndex())].extension);
    const QSignalBlocker blocker(pathEdit_);
    pathEdit_->setText(QString::fromStdWString(path.wstring()));
    updateState();
}

void ImageExportDialog::pathEdited() {
    // ...and the format follows a typed extension.
    const int index = formatIndexFor(outputPath());
    if (index >= 0 && index != formatCombo_->currentIndex()) {
        const QSignalBlocker blocker(formatCombo_);
        formatCombo_->setCurrentIndex(index);
    }
    updateState();
}

void ImageExportDialog::widthChanged(int width) {
    if (linking_) {
        return;
    }
    const QSize full = currentFullSize();
    if (polarCheckBox_->isChecked()) {
        linking_ = true;
        heightField_->setValue(width);  // a Sound Flower is always square.
        linking_ = false;
    } else if (keepProportionsCheckBox_->isChecked() && full.width() > 0) {
        linking_ = true;
        heightField_->setValue(std::max(1, static_cast<int>(std::lround(static_cast<double>(width) * full.height() /
                                                                          full.width()))));
        linking_ = false;
    }
}

void ImageExportDialog::heightChanged(int height) {
    if (linking_) {
        return;
    }
    const QSize full = currentFullSize();
    if (polarCheckBox_->isChecked()) {
        linking_ = true;
        widthField_->setValue(height);
        linking_ = false;
    } else if (keepProportionsCheckBox_->isChecked() && full.height() > 0) {
        linking_ = true;
        widthField_->setValue(std::max(1, static_cast<int>(std::lround(static_cast<double>(height) * full.width() /
                                                                         full.height()))));
        linking_ = false;
    }
}

void ImageExportDialog::updateState() {
    const QSize full = currentFullSize();
    fullResolutionRadio_->setText(tr("Full resolution (%1 x %2)").arg(full.width()).arg(full.height()));
    const bool custom = customSizeRadio_->isChecked();
    widthField_->setEnabled(custom);
    heightField_->setEnabled(custom);
    keepProportionsCheckBox_->setEnabled(custom && !polarCheckBox_->isChecked());
    if (polarCheckBox_->isChecked() && heightField_->value() != widthField_->value()) {
        linking_ = true;
        heightField_->setValue(widthField_->value());
        linking_ = false;
    }
    const std::filesystem::path path = outputPath();
    buttons_->button(QDialogButtonBox::Ok)
        ->setEnabled(!path.filename().empty() && imageFormatForPath(path).has_value());
}

}  // namespace sound_mind::studio
