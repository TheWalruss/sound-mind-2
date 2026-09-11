#include "test_axis_labels.h"

#include <algorithm>
#include <cmath>

#include <QtTest/QtTest>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/axis_labels.h"

using sound_mind::core::ProjectSettings;
using sound_mind::studio::AxisTick;
using sound_mind::studio::HorizontalAxisLabelMode;
using sound_mind::studio::horizontalAxisTicks;
using sound_mind::studio::VerticalAxisLabelMode;
using sound_mind::studio::verticalAxisTicks;

namespace {

ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 1000;
    settings.canvasHeight = 100;
    settings.binCount = 100;
    settings.minFrequencyHz = 100.0f;
    settings.maxFrequencyHz = 1000.0f;
    settings.timestepMs = 10.0;
    settings.referenceHz = 440.0;
    return settings;
}

bool anyTickNear(const std::vector<AxisTick>& ticks, double value, double tolerance) {
    return std::any_of(ticks.begin(), ticks.end(),
                        [&](const AxisTick& tick) { return std::abs(tick.domainValue - value) <= tolerance; });
}

}  // namespace

void AxisLabelsTest::verticalAxisTicksOffModeReturnsNothing() {
    const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::Off, testSettings(), 2000.0);
    QVERIFY(ticks.empty());
}

void AxisLabelsTest::verticalAxisTicksHertzModeStaysWithinTheProjectsOwnFrequencyRange() {
    const auto settings = testSettings();  // 100-1000 Hz.
    const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::Hertz, settings, 2000.0);

    QVERIFY(!ticks.empty());
    for (const auto& tick : ticks) {
        QVERIFY(tick.domainValue >= settings.minFrequencyHz);
        QVERIFY(tick.domainValue <= settings.maxFrequencyHz);
    }
    // 150 Hz is one of the fixed "nice" candidates and falls inside 100-1000.
    QVERIFY(anyTickNear(ticks, 150.0, 0.01));
    // 20 Hz is also a candidate, but well outside this project's own range.
    QVERIFY(!anyTickNear(ticks, 20.0, 0.01));
}

void AxisLabelsTest::verticalAxisTicksHertzModeThinsTicksThatWouldCrowdTogether() {
    const auto settings = testSettings();
    const auto roomy = verticalAxisTicks(VerticalAxisLabelMode::Hertz, settings, 2000.0);
    const auto cramped = verticalAxisTicks(VerticalAxisLabelMode::Hertz, settings, 20.0);

    QVERIFY(cramped.size() < roomy.size());
}

void AxisLabelsTest::verticalAxisTicksNotesModeLabelsWithNoteNames() {
    auto settings = testSettings();
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2000.0f;
    const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::Notes, settings, 2000.0);

    const auto it = std::find_if(ticks.begin(), ticks.end(),
                                   [](const AxisTick& tick) { return std::abs(tick.domainValue - 440.0) < 1.0; });
    QVERIFY(it != ticks.end());
    QCOMPARE(it->label, QStringLiteral("A4"));
}

void AxisLabelsTest::verticalAxisTicksNotesModeRespectsARetunedReference() {
    auto settings = testSettings();
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2000.0f;
    settings.referenceHz = 432.0;
    const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::Notes, settings, 2000.0);

    const auto it = std::find_if(ticks.begin(), ticks.end(),
                                   [](const AxisTick& tick) { return std::abs(tick.domainValue - 432.0) < 1.0; });
    QVERIFY(it != ticks.end());
    QCOMPARE(it->label, QStringLiteral("A4"));
}

void AxisLabelsTest::verticalAxisTicksBinIndexModeCoversTheFullBinRange() {
    const auto settings = testSettings();
    const auto ticks = verticalAxisTicks(VerticalAxisLabelMode::BinIndex, settings, 2000.0);

    QVERIFY(!ticks.empty());
    QCOMPARE(ticks.front().label, QStringLiteral("0"));
    for (const auto& tick : ticks) {
        QVERIFY(tick.label.toInt() <= static_cast<int>(settings.binCount) - 1);
    }
}

void AxisLabelsTest::horizontalAxisTicksOffModeReturnsNothing() {
    const auto ticks = horizontalAxisTicks(HorizontalAxisLabelMode::Off, testSettings(), 2000.0);
    QVERIFY(ticks.empty());
}

void AxisLabelsTest::horizontalAxisTicksSecondsModeStartsAtZeroAndStaysWithinTheCanvassOwnDuration() {
    const auto settings = testSettings();  // canvasWidth 1000 @ 10ms/frame = 10s.
    const auto ticks = horizontalAxisTicks(HorizontalAxisLabelMode::Seconds, settings, 2000.0);

    QVERIFY(!ticks.empty());
    QCOMPARE(ticks.front().domainValue, 0.0);
    for (const auto& tick : ticks) {
        QVERIFY(tick.domainValue <= 10.0 + 1e-9);
    }
}

void AxisLabelsTest::horizontalAxisTicksMillisecondsModeLabelsInWholeMilliseconds() {
    const auto settings = testSettings();
    const auto ticks = horizontalAxisTicks(HorizontalAxisLabelMode::Milliseconds, settings, 2000.0);

    QVERIFY(!ticks.empty());
    for (const auto& tick : ticks) {
        QVERIFY(tick.label.endsWith(QStringLiteral("ms")));
        QCOMPARE(tick.label.chopped(2).toLongLong(), std::lround(tick.domainValue * 1000.0));
    }
}

void AxisLabelsTest::horizontalAxisTicksFrameIndexModeLabelsWithRoundedFrameNumbers() {
    const auto settings = testSettings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto ticks = horizontalAxisTicks(HorizontalAxisLabelMode::FrameIndex, settings, 2000.0);

    QVERIFY(!ticks.empty());
    for (const auto& tick : ticks) {
        const auto expectedFrame = std::lround(sound_mind::core::timeToFrameIndex(tick.domainValue, config));
        QCOMPARE(tick.label.toLongLong(), static_cast<qlonglong>(expectedFrame));
    }
}

void AxisLabelsTest::horizontalAxisTicksChoosesACoarserStepForANarrowerAxis() {
    const auto settings = testSettings();
    const auto roomy = horizontalAxisTicks(HorizontalAxisLabelMode::Seconds, settings, 2000.0);
    const auto cramped = horizontalAxisTicks(HorizontalAxisLabelMode::Seconds, settings, 50.0);

    QVERIFY(cramped.size() < roomy.size());
}
