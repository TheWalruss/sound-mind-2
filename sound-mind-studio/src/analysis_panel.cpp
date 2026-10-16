#include "sound_mind/studio/analysis_panel.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "sound_mind/studio/analysis_graphs.h"

namespace sound_mind::studio {

namespace {

QLabel* makeReading(QWidget* parent, const QString& objectName) {
    auto* label = new QLabel(QStringLiteral("—"), parent);
    label->setObjectName(objectName);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void setWarning(QLabel* label, bool warning) {
    label->setStyleSheet(warning ? QStringLiteral("color: #e04040; font-weight: bold;") : QString());
}

}  // namespace

AnalysisPanel::AnalysisPanel(QWidget* parent) : QDockWidget(tr("Analysis"), parent) {
    setObjectName(QStringLiteral("analysisPanel"));
    tabs_ = new QTabWidget(this);

    auto* loudness = new QWidget(tabs_);
    auto* root = new QVBoxLayout(loudness);
    auto* top = new QHBoxLayout();
    analyzeButton_ = new QPushButton(tr("Analyze"), loudness);
    analyzeButton_->setObjectName(QStringLiteral("analyzeLoudnessButton"));
    analyzeButton_->setToolTip(tr("Measure the mix and every audible layer"));
    connect(analyzeButton_, &QPushButton::clicked, this, &AnalysisPanel::analyzeRequested);
    top->addWidget(analyzeButton_);
    statusLabel_ = new QLabel(tr("Not analyzed yet."), loudness);
    statusLabel_->setObjectName(QStringLiteral("analysisStatusLabel"));
    top->addWidget(statusLabel_, 1);
    root->addLayout(top);

    auto* summary = new QFormLayout();
    integratedLabel_ = makeReading(loudness, QStringLiteral("integratedLoudnessLabel"));
    integratedLabel_->setToolTip(tr("Loudness of the whole mix (EBU R128 / ITU-R BS.1770, gated)"));
    summary->addRow(tr("Integrated:"), integratedLabel_);
    maxShortTermLabel_ = makeReading(loudness, QStringLiteral("maxShortTermLabel"));
    summary->addRow(tr("Loudest 3 s:"), maxShortTermLabel_);
    maxMomentaryLabel_ = makeReading(loudness, QStringLiteral("maxMomentaryLabel"));
    summary->addRow(tr("Loudest 400 ms:"), maxMomentaryLabel_);
    truePeakLabel_ = makeReading(loudness, QStringLiteral("truePeakLabel"));
    truePeakLabel_->setToolTip(tr("The highest peak including between samples - above 0 dBTP will distort"));
    summary->addRow(tr("True peak:"), truePeakLabel_);
    samplePeakLabel_ = makeReading(loudness, QStringLiteral("samplePeakLabel"));
    summary->addRow(tr("Sample peak:"), samplePeakLabel_);
    clipsLabel_ = makeReading(loudness, QStringLiteral("clippedSamplesLabel"));
    clipsLabel_->setToolTip(tr("Samples beyond full scale - each one hard-clips on output"));
    summary->addRow(tr("Clipped samples:"), clipsLabel_);
    momentaryNowLabel_ = makeReading(loudness, QStringLiteral("momentaryNowLabel"));
    summary->addRow(tr("At playhead (400 ms):"), momentaryNowLabel_);
    shortTermNowLabel_ = makeReading(loudness, QStringLiteral("shortTermNowLabel"));
    summary->addRow(tr("At playhead (3 s):"), shortTermNowLabel_);
    root->addLayout(summary);

    graph_ = new LoudnessGraphWidget(loudness);
    graph_->setObjectName(QStringLiteral("loudnessGraph"));
    root->addWidget(graph_);
    bands_ = new BandLevelsWidget(loudness);
    bands_->setObjectName(QStringLiteral("bandLevels"));
    root->addWidget(bands_);

    layerTable_ = new QTableWidget(0, 5, loudness);
    layerTable_->setObjectName(QStringLiteral("layerLoudnessTable"));
    layerTable_->setHorizontalHeaderLabels(
        {tr("Layer"), tr("Integrated"), tr("Loudest 3 s"), tr("True peak"), tr("Clips")});
    layerTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    layerTable_->verticalHeader()->setVisible(false);
    layerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layerTable_->setToolTip(tr("Each layer heard on its own"));
    root->addWidget(layerTable_, 1);

    tabs_->addTab(loudness, tr("Loudness"));

    // --- Stereo (installment B) ---
    auto* stereo = new QWidget(tabs_);
    auto* stereoRoot = new QVBoxLayout(stereo);
    auto* stereoSummary = new QFormLayout();
    correlationLabel_ = makeReading(stereo, QStringLiteral("correlationLabel"));
    correlationLabel_->setToolTip(tr("+1 mono, 0 unrelated channels, below 0 the channels cancel when summed to mono"));
    stereoSummary->addRow(tr("Correlation:"), correlationLabel_);
    widthLabel_ = makeReading(stereo, QStringLiteral("stereoWidthLabel"));
    widthLabel_->setToolTip(tr("0 mono, about 0.5 wide, 1 fully out of phase"));
    stereoSummary->addRow(tr("Width:"), widthLabel_);
    correlationNowLabel_ = makeReading(stereo, QStringLiteral("correlationNowLabel"));
    stereoSummary->addRow(tr("Correlation at playhead:"), correlationNowLabel_);
    widthNowLabel_ = makeReading(stereo, QStringLiteral("stereoWidthNowLabel"));
    stereoSummary->addRow(tr("Width at playhead:"), widthNowLabel_);
    stereoRoot->addLayout(stereoSummary);
    stereoGraph_ = new StereoGraphWidget(stereo);
    stereoGraph_->setObjectName(QStringLiteral("stereoGraph"));
    stereoRoot->addWidget(stereoGraph_);
    auto* stereoBottom = new QHBoxLayout();
    goniometer_ = new GoniometerWidget(stereo);
    goniometer_->setObjectName(QStringLiteral("goniometer"));
    stereoBottom->addWidget(goniometer_);
    bandWidths_ = new BandWidthBarsWidget(stereo);
    bandWidths_->setObjectName(QStringLiteral("bandWidths"));
    stereoBottom->addWidget(bandWidths_, 1);
    stereoRoot->addLayout(stereoBottom, 1);
    tabs_->addTab(stereo, tr("Stereo"));
    setWidget(tabs_);
}

QString AnalysisPanel::formatLevel(double value, const QString& unit) {
    if (value <= sound_mind::core::kLoudnessFloorLufs) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 1).arg(unit);
}

void AnalysisPanel::setReport(const sound_mind::core::ProjectLoudnessReport& report) {
    report_ = report;
    const auto& mix = report.mix;
    statusLabel_->setText(tr("Up to date."));
    integratedLabel_->setText(formatLevel(mix.integratedLufs, QStringLiteral("LUFS")));
    maxShortTermLabel_->setText(formatLevel(mix.maxShortTermLufs, QStringLiteral("LUFS")));
    maxMomentaryLabel_->setText(formatLevel(mix.maxMomentaryLufs, QStringLiteral("LUFS")));
    truePeakLabel_->setText(formatLevel(mix.truePeakDbtp, QStringLiteral("dBTP")));
    setWarning(truePeakLabel_, mix.truePeakDbtp > 0.0);
    samplePeakLabel_->setText(formatLevel(mix.samplePeakDbfs, QStringLiteral("dBFS")));
    clipsLabel_->setText(QString::number(mix.clippedSamples));
    setWarning(clipsLabel_, mix.clippedSamples > 0);
    graph_->setSeries(mix.momentaryLufs, mix.shortTermLufs, sound_mind::core::kLoudnessStepSeconds);
    correlationLabel_->setText(QString::number(report.mixStereo.correlation, 'f', 2));
    setWarning(correlationLabel_, report.mixStereo.correlation < 0.0);
    widthLabel_->setText(QString::number(report.mixStereo.width, 'f', 2));
    stereoGraph_->setSeries(report.mixStereo.correlationOverTime, report.mixStereo.widthOverTime,
                            sound_mind::core::kLoudnessStepSeconds);
    goniometer_->setAudio(report.mixAudio);
    bandWidths_->setWidths(report.mixBandWidths);
    bands_->setLevels(report.mixBandLevels, static_cast<int>(sound_mind::core::loudnessBands().size()),
                      static_cast<int>(report.frameCount), report.secondsPerFrame);

    layerTable_->setRowCount(static_cast<int>(report.layers.size()));
    for (int row = 0; row < static_cast<int>(report.layers.size()); ++row) {
        const auto& layer = report.layers[static_cast<std::size_t>(row)];
        const auto& m = layer.measurement;
        const QStringList cells{QString::fromStdString(layer.name), formatLevel(m.integratedLufs, QStringLiteral("LUFS")),
                                formatLevel(m.maxShortTermLufs, QStringLiteral("LUFS")),
                                formatLevel(m.truePeakDbtp, QStringLiteral("dBTP")),
                                QString::number(m.clippedSamples)};
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            if ((column == 4 && m.clippedSamples > 0) || (column == 3 && m.truePeakDbtp > 0.0)) {
                item->setForeground(QColor(224, 64, 64));
            }
            layerTable_->setItem(row, column, item);
        }
    }
    setPlaybackPosition(std::nullopt);
}

void AnalysisPanel::clearReport() {
    report_.reset();
    statusLabel_->setText(tr("Not analyzed yet."));
    for (QLabel* label : {integratedLabel_, maxShortTermLabel_, maxMomentaryLabel_, truePeakLabel_, samplePeakLabel_,
                          clipsLabel_, momentaryNowLabel_, shortTermNowLabel_, correlationLabel_, widthLabel_,
                          correlationNowLabel_, widthNowLabel_}) {
        label->setText(QStringLiteral("—"));
        setWarning(label, false);
    }
    graph_->setSeries({}, {}, sound_mind::core::kLoudnessStepSeconds);
    bands_->setLevels({}, 0, 0, 0.0);
    stereoGraph_->setSeries({}, {}, sound_mind::core::kLoudnessStepSeconds);
    goniometer_->setAudio(nullptr);
    bandWidths_->setWidths({});
    layerTable_->setRowCount(0);
}

void AnalysisPanel::setStale() {
    if (report_.has_value()) {
        statusLabel_->setText(tr("Out of date - the project changed. Analyze again."));
    }
}

void AnalysisPanel::setAnalyzing(bool analyzing) {
    analyzeButton_->setEnabled(!analyzing);
    if (analyzing) {
        statusLabel_->setText(tr("Analyzing…"));
    } else if (!report_.has_value()) {
        statusLabel_->setText(tr("Not analyzed yet."));
    }
}

void AnalysisPanel::setPlaybackPosition(std::optional<double> seconds) {
    graph_->setPlayhead(seconds);
    bands_->setPlayhead(seconds);
    stereoGraph_->setPlayhead(seconds);
    goniometer_->setPosition(seconds);
    for (QLabel* label : {momentaryNowLabel_, shortTermNowLabel_, correlationNowLabel_, widthNowLabel_}) {
        label->setText(QStringLiteral("—"));
    }
    if (!report_.has_value() || !seconds.has_value()) {
        return;
    }
    // Reading i covers the window ending at (i + 1) steps.
    const auto& mix = report_->mix;
    const auto index = static_cast<std::ptrdiff_t>(*seconds / sound_mind::core::kLoudnessStepSeconds) - 1;
    const auto& stereo = report_->mixStereo;
    if (index >= 0 && index < static_cast<std::ptrdiff_t>(stereo.correlationOverTime.size())) {
        correlationNowLabel_->setText(
            QString::number(stereo.correlationOverTime[static_cast<std::size_t>(index)], 'f', 2));
        widthNowLabel_->setText(QString::number(stereo.widthOverTime[static_cast<std::size_t>(index)], 'f', 2));
    }
    if (index < 0 || index >= static_cast<std::ptrdiff_t>(mix.momentaryLufs.size())) {
        return;
    }
    momentaryNowLabel_->setText(formatLevel(mix.momentaryLufs[static_cast<std::size_t>(index)], QStringLiteral("LUFS")));
    shortTermNowLabel_->setText(formatLevel(mix.shortTermLufs[static_cast<std::size_t>(index)], QStringLiteral("LUFS")));
}

}  // namespace sound_mind::studio
