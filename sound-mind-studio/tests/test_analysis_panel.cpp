#include "test_analysis_panel.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest/QtTest>

#include "sound_mind/studio/analysis_graphs.h"
#include "sound_mind/studio/analysis_panel.h"
#include "sound_mind/studio/numeric_field.h"

using sound_mind::core::LayerLoudness;
using sound_mind::core::LoudnessMeasurement;
using sound_mind::core::ProjectLoudnessReport;
using sound_mind::studio::AnalysisPanel;
using sound_mind::studio::BandLevelsWidget;
using sound_mind::studio::BandWidthBarsWidget;
using sound_mind::studio::GoniometerWidget;
using sound_mind::studio::PitchGraphWidget;
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
    report.mixPitchHz = {0.0f, 440.0f, 445.0f, 0.0f};
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

void AnalysisPanelTest::thePitchTabShowsTheNoteAtThePlayhead() {
    AnalysisPanel panel;
    panel.setReport(twoLayerReport());
    QVERIFY(panel.findChild<PitchGraphWidget*>()->hasData());

    panel.setPlaybackPosition(0.6);  // frame 1 (0.5 s per frame): 440 Hz.
    QVERIFY(text(panel, "noteNowLabel").startsWith(QStringLiteral("A4")));
    QVERIFY(text(panel, "noteNowLabel").contains(QStringLiteral("440.0 Hz")));
    panel.setPlaybackPosition(1.1);  // 445 Hz: about +20 cents.
    QVERIFY(text(panel, "noteNowLabel").contains(QStringLiteral("+20 cents")));
    panel.setPlaybackPosition(0.1);  // unvoiced.
    QCOMPARE(text(panel, "noteNowLabel"), QStringLiteral("—"));
}

void AnalysisPanelTest::showPitchOnCanvasEmitsTheTrackOnlyWhileChecked() {
    AnalysisPanel panel;
    std::vector<float> received{1.0f};
    bool enabled = false;
    connect(&panel, &AnalysisPanel::pitchOverlayToggled, [&](bool on, const std::vector<float>& track) {
        enabled = on;
        received = track;
    });
    auto* box = panel.findChild<QCheckBox*>(QStringLiteral("pitchOverlayCheckBox"));

    box->setChecked(true);  // no report yet: nothing to show.
    QVERIFY(enabled);
    QVERIFY(received.empty());
    panel.setReport(twoLayerReport());  // a new report while checked: shown.
    QCOMPARE(received.size(), std::size_t{4});
    box->setChecked(false);
    QVERIFY(received.empty());
}

// v0.1.40.7 - Analyze and Auto-update reachable from every tab; loudness and
// stereo canvas overlays.
void AnalysisPanelTest::analyzeAndAutoUpdateSitAboveTheTabsSoEveryTabHasThem() {
    AnalysisPanel panel;
    auto* tabs = panel.findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    for (const char* name : {"analyzeLoudnessButton", "autoAnalyzeCheckBox", "analysisStatusLabel"}) {
        QWidget* control = panel.findChild<QWidget*>(QString::fromLatin1(name));
        QVERIFY2(control != nullptr, name);
        QVERIFY2(!tabs->isAncestorOf(control), name);  // shown whichever tab is current.
    }
    // One of each, not a copy per tab.
    QCOMPARE(panel.findChildren<QPushButton*>(QStringLiteral("analyzeLoudnessButton")).size(), 1);
    QCOMPARE(panel.findChildren<QCheckBox*>(QStringLiteral("autoAnalyzeCheckBox")).size(), 1);
}

void AnalysisPanelTest::autoUpdateCheckBoxEmitsAndItsSetterDoesNot() {
    AnalysisPanel panel;
    QVERIFY(!panel.autoAnalyze());
    QSignalSpy spy(&panel, &AnalysisPanel::autoAnalyzeToggled);
    auto* box = panel.findChild<QCheckBox*>(QStringLiteral("autoAnalyzeCheckBox"));
    box->setChecked(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
    QVERIFY(panel.autoAnalyze());

    panel.setAutoAnalyze(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!box->isChecked());
}

void AnalysisPanelTest::showLoudnessOnCanvasEmitsTheSeriesOnlyWhileChecked() {
    AnalysisPanel panel;
    bool enabled = false;
    std::vector<float> momentary{1.0f};
    std::vector<float> shortTerm{1.0f};
    connect(&panel, &AnalysisPanel::loudnessOverlayToggled,
            [&](bool on, const std::vector<float>& m, const std::vector<float>& st) {
                enabled = on;
                momentary = m;
                shortTerm = st;
            });
    auto* box = panel.findChild<QCheckBox*>(QStringLiteral("loudnessOverlayCheckBox"));
    QVERIFY(box != nullptr);
    box->setChecked(true);  // no report yet: nothing to show.
    QVERIFY(enabled);
    QVERIFY(momentary.empty());
    panel.setReport(twoLayerReport());  // a new report while checked: shown.
    QCOMPARE(momentary.size(), std::size_t{20});
    QCOMPARE(shortTerm.size(), std::size_t{20});
    panel.clearReport();
    QVERIFY(momentary.empty());
    panel.setReport(twoLayerReport());
    box->setChecked(false);
    QVERIFY(!enabled);
    QVERIFY(momentary.empty());
}

void AnalysisPanelTest::showStereoOnCanvasEmitsTheSeriesOnlyWhileChecked() {
    AnalysisPanel panel;
    bool enabled = false;
    std::vector<float> correlation;
    std::vector<float> width;
    connect(&panel, &AnalysisPanel::stereoOverlayToggled,
            [&](bool on, const std::vector<float>& c, const std::vector<float>& w) {
                enabled = on;
                correlation = c;
                width = w;
            });
    auto* box = panel.findChild<QCheckBox*>(QStringLiteral("stereoOverlayCheckBox"));
    QVERIFY(box != nullptr);
    panel.setReport(twoLayerReport());
    QVERIFY(correlation.empty());  // unchecked: nothing.
    box->setChecked(true);
    QVERIFY(enabled);
    QCOMPARE(correlation.size(), std::size_t{20});
    QCOMPARE(width.size(), std::size_t{20});
    QCOMPARE(correlation.front(), 0.75f);
    box->setChecked(false);
    QVERIFY(correlation.empty());
}

// v0.1.40.9 - ignoring short pitch spikes.
void AnalysisPanelTest::pitchSpikeFilterCleansTheGraphNoteAndOverlay() {
    AnalysisPanel panel;
    ProjectLoudnessReport report = twoLayerReport();
    report.secondsPerFrame = 0.01;
    report.mixPitchHz.assign(20, 220.0f);
    report.mixPitchHz[10] = 880.0f;  // a 10 ms octave spike.
    std::vector<float> overlay;
    connect(&panel, &AnalysisPanel::pitchOverlayToggled,
            [&](bool, const std::vector<float>& track) { overlay = track; });
    QSignalSpy spy(&panel, &AnalysisPanel::pitchSpikeFilterChanged);
    panel.findChild<QCheckBox*>(QStringLiteral("pitchOverlayCheckBox"))->setChecked(true);
    panel.setReport(report);
    panel.setPlaybackPosition(0.105);
    QVERIFY(text(panel, "noteNowLabel").startsWith(QStringLiteral("A5")));  // off by default: the spike shows.
    QCOMPARE(overlay[10], 880.0f);

    auto* field = panel.findChild<sound_mind::studio::NumericField*>(QStringLiteral("pitchSpikeFilterField"));
    QVERIFY(field != nullptr);
    QCOMPARE(field->value(), 0.0);
    field->setValue(30.0);  // ms
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toDouble(), 30.0);
    QVERIFY(text(panel, "noteNowLabel").startsWith(QStringLiteral("A3")));  // smoothed back onto the note.
    QVERIFY(std::abs(overlay[10] - 220.0f) < 0.01f);
    QVERIFY(std::abs(panel.findChild<PitchGraphWidget*>()->track()[10] - 220.0f) < 0.01f);

    panel.setPitchSpikeFilterMs(0.0);  // the setter applies without emitting.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(overlay[10], 880.0f);
}

