#include "test_analysis_panel.h"

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QtTest/QtTest>

#include "sound_mind/studio/analysis_graphs.h"
#include "sound_mind/studio/analysis_panel.h"

using sound_mind::core::LayerLoudness;
using sound_mind::core::LoudnessMeasurement;
using sound_mind::core::ProjectLoudnessReport;
using sound_mind::studio::AnalysisPanel;
using sound_mind::studio::BandLevelsWidget;
using sound_mind::studio::BandWidthBarsWidget;
using sound_mind::studio::GoniometerWidget;
using sound_mind::studio::StereoGraphWidget;
using sound_mind::studio::LoudnessGraphWidget;

namespace {

LoudnessMeasurement steady(double lufs, std::size_t readings) {
    LoudnessMeasurement m;
    m.integratedLufs = lufs;
    m.maxShortTermLufs = lufs + 1.0;
    m.maxMomentaryLufs = lufs + 2.0;
    m.samplePeakDbfs = lufs + 8.0;
    m.truePeakDbtp = lufs + 8.5;
    for (std::size_t i = 0; i < readings; ++i) {
        m.momentaryLufs.push_back(static_cast<float>(lufs - static_cast<double>(i)));
        m.shortTermLufs.push_back(static_cast<float>(lufs));
    }
    return m;
}

ProjectLoudnessReport twoLayerReport() {
    ProjectLoudnessReport report;
    report.mix = steady(-14.0, 20);
    report.frameCount = 4;
    report.secondsPerFrame = 0.5;
    report.mixBandLevels.assign(sound_mind::core::loudnessBands().size() * 4, -30.0f);
    report.mixStereo.correlation = 0.8;
    report.mixStereo.width = 0.25;
    report.mixStereo.correlationOverTime.assign(20, 0.75f);
    report.mixStereo.widthOverTime.assign(20, 0.3f);
    report.mixBandWidths = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    report.layers.push_back(LayerLoudness{1, "Drums", steady(-16.0, 20)});
    report.layers.push_back(LayerLoudness{2, "Pad", steady(-24.0, 20)});
    return report;
}

QString text(const AnalysisPanel& panel, const char* name) {
    return panel.findChild<QLabel*>(QString::fromLatin1(name))->text();
}

}  // namespace

void AnalysisPanelTest::freshPanelShowsNoReport() {
    const AnalysisPanel panel;
    QVERIFY(!panel.hasReport());
    QCOMPARE(text(panel, "integratedLoudnessLabel"), QStringLiteral("—"));
    QCOMPARE(panel.findChild<QTableWidget*>(QStringLiteral("layerLoudnessTable"))->rowCount(), 0);
    QVERIFY(!panel.findChild<LoudnessGraphWidget*>()->hasData());
}

void AnalysisPanelTest::setReportFillsTheSummaryGraphsAndLayerTable() {
    AnalysisPanel panel;
    panel.setReport(twoLayerReport());

    QVERIFY(panel.hasReport());
    QCOMPARE(text(panel, "integratedLoudnessLabel"), QStringLiteral("-14.0 LUFS"));
    QCOMPARE(text(panel, "maxShortTermLabel"), QStringLiteral("-13.0 LUFS"));
    QCOMPARE(text(panel, "truePeakLabel"), QStringLiteral("-5.5 dBTP"));
    QCOMPARE(text(panel, "clippedSamplesLabel"), QStringLiteral("0"));
    QVERIFY(panel.findChild<LoudnessGraphWidget*>()->hasData());
    QVERIFY(panel.findChild<BandLevelsWidget*>()->hasData());
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("layerLoudnessTable"));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Drums"));
    QCOMPARE(table->item(1, 1)->text(), QStringLiteral("-24.0 LUFS"));
}

void AnalysisPanelTest::clippingAndOverTruePeakAreFlaggedInRed() {
    AnalysisPanel panel;
    auto report = twoLayerReport();
    report.mix.clippedSamples = 12;
    report.mix.truePeakDbtp = 1.3;
    report.layers[1].measurement.clippedSamples = 3;
    panel.setReport(report);

    auto* clips = panel.findChild<QLabel*>(QStringLiteral("clippedSamplesLabel"));
    QCOMPARE(clips->text(), QStringLiteral("12"));
    QVERIFY(clips->styleSheet().contains(QStringLiteral("color")));
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("truePeakLabel"))->styleSheet().contains(QStringLiteral("color")));
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("layerLoudnessTable"));
    QCOMPARE(table->item(1, 4)->foreground().color(), QColor(224, 64, 64));
    QVERIFY(table->item(0, 4)->foreground().color() != QColor(224, 64, 64));

    report.mix.clippedSamples = 0;
    panel.setReport(report);
    QVERIFY(!clips->styleSheet().contains(QStringLiteral("color")));
}

void AnalysisPanelTest::readingsAtThePlayheadFollowThePosition() {
    AnalysisPanel panel;
    panel.setReport(twoLayerReport());

    panel.setPlaybackPosition(0.5);  // reading 4 ends at 0.5 s.
    QCOMPARE(text(panel, "momentaryNowLabel"), QStringLiteral("-18.0 LUFS"));
    QCOMPARE(text(panel, "shortTermNowLabel"), QStringLiteral("-14.0 LUFS"));

    panel.setPlaybackPosition(99.0);  // past the end.
    QCOMPARE(text(panel, "momentaryNowLabel"), QStringLiteral("—"));
    panel.setPlaybackPosition(std::nullopt);
    QCOMPARE(text(panel, "shortTermNowLabel"), QStringLiteral("—"));
}

void AnalysisPanelTest::staleAndClearAndAnalyzingUpdateTheStatus() {
    AnalysisPanel panel;
    auto* status = panel.findChild<QLabel*>(QStringLiteral("analysisStatusLabel"));
    auto* analyze = panel.findChild<QPushButton*>(QStringLiteral("analyzeLoudnessButton"));
    panel.setStale();  // nothing to be stale yet.
    QVERIFY(!status->text().contains(QStringLiteral("Out of date")));

    panel.setAnalyzing(true);
    QVERIFY(!analyze->isEnabled());
    panel.setAnalyzing(false);
    QVERIFY(analyze->isEnabled());

    panel.setReport(twoLayerReport());
    panel.setStale();
    QVERIFY(status->text().contains(QStringLiteral("Out of date")));
    panel.setReport(twoLayerReport());
    QVERIFY(!status->text().contains(QStringLiteral("Out of date")));

    panel.clearReport();
    QVERIFY(!panel.hasReport());
    QCOMPARE(panel.findChild<QTableWidget*>(QStringLiteral("layerLoudnessTable"))->rowCount(), 0);
}

void AnalysisPanelTest::analyzeButtonEmitsAnalyzeRequested() {
    AnalysisPanel panel;
    QSignalSpy spy(&panel, &AnalysisPanel::analyzeRequested);
    panel.findChild<QPushButton*>(QStringLiteral("analyzeLoudnessButton"))->click();
    QCOMPARE(spy.count(), 1);
}

void AnalysisPanelTest::formatLevelShowsADashAtTheFloor() {
    QCOMPARE(AnalysisPanel::formatLevel(sound_mind::core::kLoudnessFloorLufs, QStringLiteral("LUFS")),
             QStringLiteral("—"));
    QCOMPARE(AnalysisPanel::formatLevel(-9.04, QStringLiteral("dBTP")), QStringLiteral("-9.0 dBTP"));
}

void AnalysisPanelTest::setReportFillsTheStereoTab() {
    AnalysisPanel panel;
    panel.setReport(twoLayerReport());

    QCOMPARE(text(panel, "correlationLabel"), QStringLiteral("0.80"));
    QCOMPARE(text(panel, "stereoWidthLabel"), QStringLiteral("0.25"));
    QVERIFY(panel.findChild<StereoGraphWidget*>()->hasData());
    QCOMPARE(panel.findChild<BandWidthBarsWidget*>()->widths().size(), std::size_t{6});
    panel.setPlaybackPosition(1.0);
    QCOMPARE(text(panel, "correlationNowLabel"), QStringLiteral("0.75"));
    QCOMPARE(text(panel, "stereoWidthNowLabel"), QStringLiteral("0.30"));

    panel.clearReport();
    QCOMPARE(text(panel, "correlationLabel"), QStringLiteral("—"));
    QVERIFY(!panel.findChild<StereoGraphWidget*>()->hasData());
}

void AnalysisPanelTest::negativeCorrelationIsFlaggedInRed() {
    AnalysisPanel panel;
    auto report = twoLayerReport();
    report.mixStereo.correlation = -0.4;
    panel.setReport(report);
    QVERIFY(panel.findChild<QLabel*>(QStringLiteral("correlationLabel"))->styleSheet().contains(QStringLiteral("color")));
}

void AnalysisPanelTest::theGoniometerPlotsMonoAsAVerticalLineAroundThePlayhead() {
    auto audio = std::make_shared<sound_mind::codec::AudioBuffer>();
    audio->sampleRateHz = 1000;
    for (int i = 0; i < 10000; ++i) {
        const float value = static_cast<float>(i % 7) / 10.0f;
        audio->left.push_back(value);
        audio->right.push_back(value);
    }
    GoniometerWidget goniometer;
    QVERIFY(goniometer.points().empty());
    goniometer.setAudio(audio);

    goniometer.setPosition(5.0);
    const auto around = goniometer.points();
    QCOMPARE(static_cast<int>(around.size()), GoniometerWidget::kWindowSamples);
    for (const QPointF& point : around) {
        QVERIFY(std::abs(point.x()) < 1e-6);  // mono: no side.
    }
    goniometer.setPosition(std::nullopt);  // the whole buffer, spread out.
    QVERIFY(!goniometer.points().empty());
    QVERIFY(static_cast<int>(goniometer.points().size()) <= GoniometerWidget::kWindowSamples);
}
