#include "sound_mind/studio/video_export_dialog.h"

#include <array>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

#include "sound_mind/studio/numeric_field.h"

namespace sound_mind::studio {

namespace {

using sound_mind::codec::VideoCodec;

struct Preset {
    const char* label;
    int width;
    int height;
};

// The legacy Studio's list, plus the canvas's own size; Custom stays last.
constexpr std::array<Preset, 9> kPresets{{
    {QT_TRANSLATE_NOOP("VideoExportDialog", "1080p (1920 x 1080) - YouTube standard"), 1920, 1080},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "4K UHD (3840 x 2160) - YouTube 4K"), 3840, 2160},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "1440p (2560 x 1440) - YouTube 2K, monitors"), 2560, 1440},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "720p (1280 x 720) - streaming"), 1280, 720},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "TikTok / Shorts 1080 (1080 x 1920)"), 1080, 1920},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "TikTok / Shorts 720 (720 x 1280)"), 720, 1280},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "Square 1080 (1080 x 1080) - Instagram"), 1080, 1080},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "Canvas size"), 0, 0},
    {QT_TRANSLATE_NOOP("VideoExportDialog", "Custom..."), -1, -1},
}};
constexpr int kCanvasPreset = 7;
constexpr int kCustomPreset = 8;
constexpr std::array<int, 3> kFrameRates{24, 30, 60};

QString formatLabel(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264:
            return VideoExportDialog::tr("MP4 - H.264 (plays everywhere)");
        case VideoCodec::Hevc:
            return VideoExportDialog::tr("MP4 - H.265 / HEVC (smaller files)");
        case VideoCodec::ProRes:
            return VideoExportDialog::tr("MOV - ProRes 422 (for video editing)");
        case VideoCodec::Mpeg4:
            return VideoExportDialog::tr("MP4 - MPEG-4 Part 2 (older, larger files)");
    }
    return {};
}

QString withExtension(const QString& path, VideoCodec codec) {
    if (path.isEmpty()) {
        return path;
    }
    const QFileInfo info(path);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".") +
                               QString::fromStdString(sound_mind::codec::videoFileExtension(codec)));
}

}  // namespace

VideoExportDialog::VideoExportDialog(const QString& title, const std::filesystem::path& suggestedPath, bool polar,
                                     std::uint32_t canvasWidth, std::uint32_t canvasHeight, QWidget* parent)
    : QDialog(parent), polar_(polar), canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {
    setWindowTitle(title);
    setMinimumWidth(560);
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    polarBanner_ = new QLabel();
    polarBanner_->setObjectName(QStringLiteral("polarBanner"));
    polarBanner_->setWordWrap(true);
    polarBanner_->setStyleSheet(QStringLiteral("QLabel { background: #1a3040; color: #80d4cc; border: 1px solid "
                                               "#286070; border-radius: 4px; padding: 7px 10px; }"));
    root->addWidget(polarBanner_);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    root->addLayout(form);

    auto* pathRow = new QWidget();
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathEdit_ = new QLineEdit();
    pathEdit_->setObjectName(QStringLiteral("videoExportPath"));
    auto* browseButton = new QPushButton(tr("Browse..."));
    browseButton->setAutoDefault(false);
    pathLayout->addWidget(pathEdit_, 1);
    pathLayout->addWidget(browseButton);
    form->addRow(tr("Output file:"), pathRow);

    formatCombo_ = new QComboBox();
    formatCombo_->setObjectName(QStringLiteral("videoExportFormat"));
    for (const VideoCodec codec : sound_mind::codec::availableVideoCodecs()) {
        formatCombo_->addItem(formatLabel(codec), static_cast<int>(codec));
    }
    form->addRow(tr("Format:"), formatCombo_);

    resolutionCombo_ = new QComboBox();
    resolutionCombo_->setObjectName(QStringLiteral("videoExportResolution"));
    for (std::size_t i = 0; i < kPresets.size(); ++i) {
        QString label = tr(kPresets[i].label);
        if (static_cast<int>(i) == kCanvasPreset) {
            label += QStringLiteral(" (%1 x %2)").arg(canvasWidth).arg(canvasHeight);
        }
        resolutionCombo_->addItem(label);
    }
    form->addRow(tr("Resolution:"), resolutionCombo_);

    customSize_ = new QWidget();
    auto* customLayout = new QHBoxLayout(customSize_);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customWidth_ = new IntegerField();
    customWidth_->setRange(64, 7680);
    customWidth_->setSuffix(tr(" px"));
    customWidth_->setValue(1920);
    customHeight_ = new IntegerField();
    customHeight_->setRange(64, 7680);
    customHeight_->setSuffix(tr(" px"));
    customHeight_->setValue(1080);
    customLayout->addWidget(customWidth_);
    customLayout->addWidget(new QLabel(QStringLiteral("x")));
    customLayout->addWidget(customHeight_);
    customLabel_ = new QLabel(tr("Custom size:"));
    form->addRow(customLabel_, customSize_);

    frameRateCombo_ = new QComboBox();
    for (const int rate : kFrameRates) {
        frameRateCombo_->addItem(tr("%1 fps").arg(rate), rate);
    }
    frameRateCombo_->setCurrentIndex(1);
    form->addRow(tr("Frame rate:"), frameRateCombo_);

    auto* qualityRow = new QWidget();
    auto* qualityLayout = new QHBoxLayout(qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    qualityGroup_ = new QButtonGroup(this);
    const std::array<QString, 3> qualityLabels{tr("Low"), tr("Medium"), tr("High")};
    for (int i = 0; i < 3; ++i) {
        auto* button = new QRadioButton(qualityLabels[static_cast<std::size_t>(i)]);
        qualityGroup_->addButton(button, i);
        qualityLayout->addWidget(button);
    }
    qualityGroup_->button(1)->setChecked(true);
    qualityLayout->addStretch(1);
    form->addRow(tr("Quality:"), qualityRow);

    auto* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    root->addWidget(separator);

    auto* animationLabel = new QLabel(tr("Animation"));
    animationLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    root->addWidget(animationLabel);
    wholeButton_ = new QRadioButton(tr("Whole canvas - the full image stays in view; the playhead sweeps across it"));
    scrollingButton_ = new QRadioButton(tr("Scrolling - a window of the canvas pans along as it plays"));
    wholeButton_->setChecked(true);
    auto* animationGroup = new QButtonGroup(this);
    animationGroup->addButton(wholeButton_);
    animationGroup->addButton(scrollingButton_);
    root->addWidget(wholeButton_);
    root->addWidget(scrollingButton_);

    scrollingOptions_ = new QWidget();
    auto* scrollingForm = new QFormLayout(scrollingOptions_);
    scrollingForm->setContentsMargins(24, 4, 0, 0);
    windowField_ = new NumericField();
    windowField_->setObjectName(QStringLiteral("videoExportWindow"));
    windowField_->setRange(1.0, 600.0);
    windowField_->setDecimals(1);
    windowField_->setSuffix(tr(" s"));
    windowField_->setValue(30.0);
    windowField_->setToolTip(tr("How many seconds of the canvas are in view at once (no effect in Sound Flower view)."));
    scrollingForm->addRow(tr("Window:"), windowField_);
    playheadCheck_ = new QCheckBox(tr("Show the playhead"));
    playheadCheck_->setChecked(true);
    scrollingForm->addRow(QString(), playheadCheck_);
    root->addWidget(scrollingOptions_);

    auto* separator2 = new QFrame();
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    root->addWidget(separator2);

    openCheck_ = new QCheckBox(tr("Open file after export"));
    root->addWidget(openCheck_);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    root->addWidget(buttons_);

    connect(browseButton, &QPushButton::clicked, this, &VideoExportDialog::browse);
    connect(pathEdit_, &QLineEdit::textChanged, this, &VideoExportDialog::validate);
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, &VideoExportDialog::updateForFormat);
    connect(resolutionCombo_, &QComboBox::currentIndexChanged, this, &VideoExportDialog::updateVisibility);
    connect(wholeButton_, &QRadioButton::toggled, this, &VideoExportDialog::updateVisibility);
    connect(buttons_, &QDialogButtonBox::accepted, this, &VideoExportDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    restore();
    pathEdit_->setText(withExtension(QString::fromStdWString(suggestedPath.wstring()), options().codec));
    updateVisibility();
    validate();
}

sound_mind::codec::VideoExportOptions VideoExportDialog::options() const {
    sound_mind::codec::VideoExportOptions options;
    options.codec = formatCombo_->count() > 0 ? static_cast<VideoCodec>(formatCombo_->currentData().toInt())
                                              : VideoCodec::Mpeg4;
    options.quality = static_cast<sound_mind::codec::VideoQuality>(qualityGroup_->checkedId());
    const int preset = resolutionCombo_->currentIndex();
    if (preset == kCustomPreset) {
        options.width = static_cast<std::uint32_t>(customWidth_->value());
        options.height = static_cast<std::uint32_t>(customHeight_->value());
    } else if (preset != kCanvasPreset) {
        options.width = static_cast<std::uint32_t>(kPresets[static_cast<std::size_t>(preset)].width);
        options.height = static_cast<std::uint32_t>(kPresets[static_cast<std::size_t>(preset)].height);
    }
    options.frameRate = frameRateCombo_->currentData().toInt();
    options.animation = scrollingButton_->isChecked() ? sound_mind::codec::VideoAnimation::Scrolling
                                                      : sound_mind::codec::VideoAnimation::WholeCanvas;
    options.windowSeconds = windowField_->value();
    options.showPlayhead = !scrollingButton_->isChecked() || playheadCheck_->isChecked();
    options.polar = polar_;
    return options;
}

std::filesystem::path VideoExportDialog::outputPath() const {
    return std::filesystem::path(pathEdit_->text().trimmed().toStdWString());
}

bool VideoExportDialog::openWhenDone() const { return openCheck_->isChecked(); }

void VideoExportDialog::setCodec(VideoCodec codec) {
    const int index = formatCombo_->findData(static_cast<int>(codec));
    if (index >= 0) {
        formatCombo_->setCurrentIndex(index);
    }
}

void VideoExportDialog::setOutputPath(const std::filesystem::path& path) {
    pathEdit_->setText(QString::fromStdWString(path.wstring()));
}

void VideoExportDialog::setScrolling(double windowSeconds) {
    scrollingButton_->setChecked(true);
    windowField_->setValue(windowSeconds);
}

void VideoExportDialog::setResolutionPreset(int index) { resolutionCombo_->setCurrentIndex(index); }

void VideoExportDialog::setCustomSize(int width, int height) {
    customWidth_->setValue(width);
    customHeight_->setValue(height);
}

void VideoExportDialog::accept() {
    const QFileInfo target(pathEdit_->text().trimmed());
    if (!QFileInfo(target.absolutePath()).isWritable()) {
        QMessageBox::warning(this, tr("Can't Write There"),
                             tr("The folder isn't writable:\n%1").arg(target.absolutePath()));
        return;
    }
    remember();
    QDialog::accept();
}

void VideoExportDialog::browse() {
    const VideoCodec codec = options().codec;
    const QString extension = QString::fromStdString(sound_mind::codec::videoFileExtension(codec));
    const QString filter = extension == QStringLiteral("mov") ? tr("MOV video (*.mov)") : tr("MP4 video (*.mp4)");
    const QString chosen = QFileDialog::getSaveFileName(this, tr("Export Video As"), pathEdit_->text(), filter);
    if (!chosen.isEmpty()) {
        pathEdit_->setText(withExtension(chosen, codec));
    }
}

void VideoExportDialog::updateForFormat() { pathEdit_->setText(withExtension(pathEdit_->text().trimmed(), options().codec)); }

void VideoExportDialog::updateVisibility() {
    const bool custom = resolutionCombo_->currentIndex() == kCustomPreset;
    customSize_->setVisible(custom);
    customLabel_->setVisible(custom);
    const bool scrolling = scrollingButton_->isChecked();
    scrollingOptions_->setVisible(scrolling);
    windowField_->setEnabled(!polar_);
    polarBanner_->setVisible(polar_);
    polarBanner_->setText(scrolling ? tr("Sound Flower view - Scrolling: the flower turns counter-clockwise under a "
                                         "playhead fixed at twelve o'clock. The window length has no effect.")
                                    : tr("Sound Flower view - Whole canvas: the playhead turns clockwise from twelve "
                                         "o'clock."));
    adjustSize();
}

void VideoExportDialog::validate() {
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(!pathEdit_->text().trimmed().isEmpty());
}

void VideoExportDialog::remember() const {
    // The Studio's own ini store - the one MainWindow keeps recent projects
    // in, which the tests sandbox.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SoundMind"),
                       QStringLiteral("SoundMindStudio"));
    settings.beginGroup(QStringLiteral("videoExport"));
    const auto chosen = options();
    settings.setValue(QStringLiteral("format"), static_cast<int>(chosen.codec));
    settings.setValue(QStringLiteral("resolution"), resolutionCombo_->currentIndex());
    settings.setValue(QStringLiteral("customWidth"), customWidth_->value());
    settings.setValue(QStringLiteral("customHeight"), customHeight_->value());
    settings.setValue(QStringLiteral("frameRate"), chosen.frameRate);
    settings.setValue(QStringLiteral("quality"), qualityGroup_->checkedId());
    settings.setValue(QStringLiteral("scrolling"), scrollingButton_->isChecked());
    settings.setValue(QStringLiteral("windowSeconds"), windowField_->value());
    settings.setValue(QStringLiteral("showPlayhead"), playheadCheck_->isChecked());
    settings.setValue(QStringLiteral("openWhenDone"), openCheck_->isChecked());
}

void VideoExportDialog::restore() {
    // The Studio's own ini store - the one MainWindow keeps recent projects
    // in, which the tests sandbox.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("SoundMind"),
                       QStringLiteral("SoundMindStudio"));
    settings.beginGroup(QStringLiteral("videoExport"));
    setCodec(static_cast<VideoCodec>(
        settings.value(QStringLiteral("format"), static_cast<int>(sound_mind::codec::preferredVideoCodec())).toInt()));
    resolutionCombo_->setCurrentIndex(std::clamp(settings.value(QStringLiteral("resolution"), 0).toInt(), 0,
                                                 static_cast<int>(kPresets.size()) - 1));
    customWidth_->setValue(settings.value(QStringLiteral("customWidth"), 1920).toInt());
    customHeight_->setValue(settings.value(QStringLiteral("customHeight"), 1080).toInt());
    const int rate = frameRateCombo_->findData(settings.value(QStringLiteral("frameRate"), 30).toInt());
    frameRateCombo_->setCurrentIndex(rate >= 0 ? rate : 1);
    if (auto* quality = qualityGroup_->button(settings.value(QStringLiteral("quality"), 1).toInt())) {
        quality->setChecked(true);
    }
    (settings.value(QStringLiteral("scrolling"), false).toBool() ? scrollingButton_ : wholeButton_)->setChecked(true);
    windowField_->setValue(settings.value(QStringLiteral("windowSeconds"), 30.0).toDouble());
    playheadCheck_->setChecked(settings.value(QStringLiteral("showPlayhead"), true).toBool());
    openCheck_->setChecked(settings.value(QStringLiteral("openWhenDone"), false).toBool());
}

}  // namespace sound_mind::studio
