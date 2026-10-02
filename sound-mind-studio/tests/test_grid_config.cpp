#include "test_grid_config.h"

#include <QtTest/QtTest>

#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/grid_config.h"

using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::FrequencyGridConfig;
using sound_mind::studio::frequencyGridLinesHz;
using sound_mind::studio::nearestFrequencyGridLineHz;
using sound_mind::studio::nearestTimingGridLineSeconds;
using sound_mind::studio::snapToGrid;
using sound_mind::studio::TimingGridConfig;
using sound_mind::studio::TimingGridMode;
using sound_mind::studio::timingGridLinesSeconds;

namespace {

/// @brief A default-tuning (A4 = 440 Hz), wide-range project - matching
/// `ProjectSettings`' own defaults for `referenceHz`/`defaultTempoBpm`,
/// but with an explicit, easy-to-hand-check frequency range.
ProjectSettings testSettings() {
    ProjectSettings settings;
    settings.referenceHz = 440.0;
    settings.defaultTempoBpm = 120.0;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 2000.0f;
    return settings;
}

}  // namespace

void GridConfigTest::frequencyGridConfigIsNotActiveWithNothingEnabled() {
    const FrequencyGridConfig config;
    QVERIFY(!config.isActive());
}

void GridConfigTest::frequencyGridConfigIsActiveWithNoteGridEnabled() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    QVERIFY(config.isActive());
}

void GridConfigTest::frequencyGridConfigIsNotActiveWithHarmonicSeriesEnabledButAZeroFundamental() {
    FrequencyGridConfig config;
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 0.0;
    QVERIFY(!config.isActive());
}

void GridConfigTest::frequencyGridConfigIsNotActiveWithCustomFrequenciesEnabledButAnEmptyList() {
    FrequencyGridConfig config;
    config.customFrequenciesEnabled = true;
    QVERIFY(config.customFrequenciesHz.empty());
    QVERIFY(!config.isActive());
}

void GridConfigTest::timingGridConfigIsNotActiveWhenOff() {
    const TimingGridConfig config;
    QCOMPARE(config.mode, TimingGridMode::Off);
    QVERIFY(!config.isActive());
}

void GridConfigTest::timingGridConfigIsActiveWithAPositiveInterval() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Interval;
    config.intervalSeconds = 0.5;
    QVERIFY(config.isActive());
}

void GridConfigTest::timingGridConfigIsNotActiveWithATempoModeButAZeroBeatFraction() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Tempo;
    config.tempoBeatFraction = 0.0;
    QVERIFY(!config.isActive());
}

void GridConfigTest::frequencyGridLinesHzReturnsNothingWhenNotActive() {
    const FrequencyGridConfig config;
    QVERIFY(frequencyGridLinesHz(config, testSettings()).empty());
}

void GridConfigTest::frequencyGridLinesHzNoteGridProducesEverySemitoneInRange() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 470.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    // A4 (440 Hz) and A#4 (~466.16 Hz) fall in range; G#4 (~415.30 Hz) is
    // just below it.
    QCOMPARE(lines.size(), std::size_t{2});
    QVERIFY(qAbs(lines[0] - 440.0) < 0.01);
    QVERIFY(qAbs(lines[1] - 466.1638) < 0.01);
}

void GridConfigTest::frequencyGridLinesHzHarmonicSeriesProducesIntegerMultiplesOfTheFundamental() {
    FrequencyGridConfig config;
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 100.0;
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 350.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    QCOMPARE(lines.size(), std::size_t{3});
    QCOMPARE(lines[0], 100.0);
    QCOMPARE(lines[1], 200.0);
    QCOMPARE(lines[2], 300.0);
}

void GridConfigTest::frequencyGridLinesHzCustomFrequenciesAreFilteredToTheProjectsOwnRange() {
    FrequencyGridConfig config;
    config.customFrequenciesEnabled = true;
    config.customFrequenciesHz = {10.0, 50.0, 1500.0, 3000.0};
    const ProjectSettings settings = testSettings();  // [20, 2000]

    const auto lines = frequencyGridLinesHz(config, settings);

    QCOMPARE(lines.size(), std::size_t{2});
    QCOMPARE(lines[0], 50.0);
    QCOMPARE(lines[1], 1500.0);
}

void GridConfigTest::frequencyGridLinesHzCombinesAndDeduplicatesMultipleSources() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 440.0;  // Same as A4 - the note grid's own line at 440 Hz.
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 450.0f;  // Only A4 (440 Hz) itself falls in this narrow range for both sources.

    const auto lines = frequencyGridLinesHz(config, settings);

    // Without deduplication this would be {440.0, 440.0} - one from each
    // source.
    QCOMPARE(lines.size(), std::size_t{1});
    QVERIFY(qAbs(lines[0] - 440.0) < 0.01);
}

void GridConfigTest::frequencyGridLinesHzNoteGridExcludedStepsRemovesThatPitchClassInEveryOctave() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    // Step 0 is A under Equal12's own `step == 0 is the reference (A)`
    // convention - excluding it should remove every A across every
    // octave, leaving A#4 (~466.16 Hz) as the only line in this range.
    config.noteGridExcludedSteps.insert(0);
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 470.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    QCOMPARE(lines.size(), std::size_t{1});
    QVERIFY(qAbs(lines[0] - 466.1638) < 0.01);
}

void GridConfigTest::frequencyGridLinesHzNoteGridExcludedOctavesRemovesEveryStepInThatOctave() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    config.noteGridExcludedOctaves.insert(4);  // A4 and A#4 above are both in octave 4.
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 470.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    QVERIFY(lines.empty());
}

void GridConfigTest::frequencyGridLinesHzNoteGridRespectsAlternateEqualTemperamentStepCount() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    config.noteGridTemperament = sound_mind::core::Temperament::Equal24;
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 470.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    // Equal24 has twice as many steps per octave as Equal12 - the same
    // 430-470 Hz window that held 2 Equal12 lines above should now hold
    // roughly double (A4, a quarter-tone above A4, A#4, and whichever
    // falls just past it).
    QVERIFY(lines.size() >= 3);
    QVERIFY(qAbs(lines.front() - 440.0) < 0.01);
}

void GridConfigTest::frequencyGridLinesHzNoteGridRespectsNamedHistoricalTuning() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    config.noteGridTemperament = sound_mind::core::Temperament::Pythagorean;
    ProjectSettings settings = testSettings();
    settings.minFrequencyHz = 430.0f;
    settings.maxFrequencyHz = 475.0f;

    const auto lines = frequencyGridLinesHz(config, settings);

    // A4 itself is still exactly the reference regardless of temperament;
    // Pythagorean's own A#4 sits at a different Hz value than Equal12's
    // (~466.16 Hz) - a real, measurable difference confirming the
    // temperament actually changed the computed frequency, not just
    // which steps are walked.
    QCOMPARE(lines.size(), std::size_t{2});
    QVERIFY(qAbs(lines[0] - 440.0) < 0.01);
    QVERIFY(qAbs(lines[1] - 466.1638) > 0.5);
}

void GridConfigTest::timingGridLinesSecondsReturnsNothingWhenNotActive() {
    const TimingGridConfig config;
    QVERIFY(timingGridLinesSeconds(config, testSettings(), 10.0).empty());
}

void GridConfigTest::timingGridLinesSecondsIntervalModeCoversTheWholeDuration() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Interval;
    config.intervalSeconds = 0.3;

    const auto lines = timingGridLinesSeconds(config, testSettings(), 1.0);

    QCOMPARE(lines.size(), std::size_t{4});
    QVERIFY(qAbs(lines[0] - 0.0) < 1e-9);
    QVERIFY(qAbs(lines[1] - 0.3) < 1e-9);
    QVERIFY(qAbs(lines[2] - 0.6) < 1e-9);
    QVERIFY(qAbs(lines[3] - 0.9) < 1e-9);
}

void GridConfigTest::timingGridLinesSecondsTempoModeDerivesStepFromTheProjectsOwnBpm() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Tempo;
    config.tempoBeatFraction = 1.0;
    ProjectSettings settings = testSettings();
    settings.defaultTempoBpm = 120.0;  // 0.5 seconds per beat.

    const auto lines = timingGridLinesSeconds(config, settings, 1.6);

    QCOMPARE(lines.size(), std::size_t{4});
    QVERIFY(qAbs(lines[0] - 0.0) < 1e-9);
    QVERIFY(qAbs(lines[1] - 0.5) < 1e-9);
    QVERIFY(qAbs(lines[2] - 1.0) < 1e-9);
    QVERIFY(qAbs(lines[3] - 1.5) < 1e-9);
}

void GridConfigTest::nearestFrequencyGridLineHzReturnsNulloptWhenNotActive() {
    const FrequencyGridConfig config;
    QVERIFY(!nearestFrequencyGridLineHz(440.0, config, testSettings()).has_value());
}

void GridConfigTest::nearestFrequencyGridLineHzSnapsToTheNearestSemitone() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;

    // 450 Hz sits between A4 (440 Hz) and A#4 (~466.16 Hz), closer to A4.
    const auto snapped = nearestFrequencyGridLineHz(450.0, config, testSettings());

    QVERIFY(snapped.has_value());
    QVERIFY(qAbs(*snapped - 440.0) < 0.01);
}

void GridConfigTest::nearestFrequencyGridLineHzSkipsAnExcludedStepInFavorOfTheNextNearest() {
    FrequencyGridConfig config;
    config.noteGridEnabled = true;
    config.noteGridExcludedSteps.insert(0);  // Excludes every A, including A4 itself.

    // 450 Hz is still closer to (now-excluded) A4 than to A#4, so the
    // nearest surviving line should be A#4 (~466.16 Hz), not A4.
    const auto snapped = nearestFrequencyGridLineHz(450.0, config, testSettings());

    QVERIFY(snapped.has_value());
    QVERIFY(qAbs(*snapped - 466.1638) < 0.01);
}

void GridConfigTest::nearestFrequencyGridLineHzSnapsToTheNearestHarmonic() {
    FrequencyGridConfig config;
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 100.0;

    // 340 Hz is between the 3rd (300) and 4th (400) harmonics, closer to
    // the 3rd.
    const auto snapped = nearestFrequencyGridLineHz(340.0, config, testSettings());

    QVERIFY(snapped.has_value());
    QCOMPARE(*snapped, 300.0);
}

void GridConfigTest::nearestFrequencyGridLineHzSnapsToTheNearestCustomFrequency() {
    FrequencyGridConfig config;
    config.customFrequenciesEnabled = true;
    config.customFrequenciesHz = {50.0, 300.0, 999.0};

    const auto snapped = nearestFrequencyGridLineHz(310.0, config, testSettings());

    QVERIFY(snapped.has_value());
    QCOMPARE(*snapped, 300.0);
}

void GridConfigTest::nearestFrequencyGridLineHzPicksTheGloballyNearestAcrossCombinedSources() {
    FrequencyGridConfig config;
    config.harmonicSeriesEnabled = true;
    config.harmonicFundamentalHz = 100.0;  // Nearest harmonic to 505 Hz is 500 Hz (distance 5).
    config.customFrequenciesEnabled = true;
    config.customFrequenciesHz = {505.0};  // Exact match (distance 0).

    const auto snapped = nearestFrequencyGridLineHz(505.0, config, testSettings());

    QVERIFY(snapped.has_value());
    QCOMPARE(*snapped, 505.0);
}

void GridConfigTest::nearestTimingGridLineSecondsReturnsNulloptWhenNotActive() {
    const TimingGridConfig config;
    QVERIFY(!nearestTimingGridLineSeconds(1.0, config, testSettings()).has_value());
}

void GridConfigTest::nearestTimingGridLineSecondsSnapsToTheNearestIntervalMultiple() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Interval;
    config.intervalSeconds = 0.25;

    // 1.1 is between 1.0 and 1.25, closer to 1.0.
    const auto snapped = nearestTimingGridLineSeconds(1.1, config, testSettings());

    QVERIFY(snapped.has_value());
    QVERIFY(qAbs(*snapped - 1.0) < 1e-9);
}

void GridConfigTest::nearestTimingGridLineSecondsSnapsToTheNearestTempoMultiple() {
    TimingGridConfig config;
    config.mode = TimingGridMode::Tempo;
    config.tempoBeatFraction = 1.0;
    ProjectSettings settings = testSettings();
    settings.defaultTempoBpm = 120.0;  // 0.5 seconds per beat.

    // 0.6 is between 0.5 and 1.0, closer to 0.5.
    const auto snapped = nearestTimingGridLineSeconds(0.6, config, settings);

    QVERIFY(snapped.has_value());
    QVERIFY(qAbs(*snapped - 0.5) < 1e-9);
}

void GridConfigTest::snapToGridLeavesAPointUnchangedWhenNeitherGridIsActive() {
    const TimeFrequencyPoint point{1.234, 567.0};

    const auto snapped = snapToGrid(point, FrequencyGridConfig{}, TimingGridConfig{}, testSettings());

    QCOMPARE(snapped.timeSeconds, point.timeSeconds);
    QCOMPARE(snapped.frequencyHz, point.frequencyHz);
}

void GridConfigTest::snapToGridSnapsOnlyFrequencyWhenOnlyTheFrequencyGridIsActive() {
    FrequencyGridConfig frequencyConfig;
    frequencyConfig.harmonicSeriesEnabled = true;
    frequencyConfig.harmonicFundamentalHz = 100.0;
    const TimeFrequencyPoint point{1.234, 340.0};  // Nearest harmonic: 300 Hz.

    const auto snapped = snapToGrid(point, frequencyConfig, TimingGridConfig{}, testSettings());

    QCOMPARE(snapped.timeSeconds, point.timeSeconds);  // Timing Grid off - untouched.
    QCOMPARE(snapped.frequencyHz, 300.0);
}

void GridConfigTest::snapToGridSnapsOnlyTimeWhenOnlyTheTimingGridIsActive() {
    TimingGridConfig timingConfig;
    timingConfig.mode = TimingGridMode::Interval;
    timingConfig.intervalSeconds = 0.25;
    const TimeFrequencyPoint point{1.1, 567.0};  // Nearest interval multiple: 1.0 s.

    const auto snapped = snapToGrid(point, FrequencyGridConfig{}, timingConfig, testSettings());

    QVERIFY(qAbs(snapped.timeSeconds - 1.0) < 1e-9);
    QCOMPARE(snapped.frequencyHz, point.frequencyHz);  // Frequency Grid off - untouched.
}

void GridConfigTest::snapToGridSnapsBothAxesWhenBothGridsAreActive() {
    FrequencyGridConfig frequencyConfig;
    frequencyConfig.harmonicSeriesEnabled = true;
    frequencyConfig.harmonicFundamentalHz = 100.0;
    TimingGridConfig timingConfig;
    timingConfig.mode = TimingGridMode::Interval;
    timingConfig.intervalSeconds = 0.25;
    const TimeFrequencyPoint point{1.1, 340.0};

    const auto snapped = snapToGrid(point, frequencyConfig, timingConfig, testSettings());

    QVERIFY(qAbs(snapped.timeSeconds - 1.0) < 1e-9);
    QCOMPARE(snapped.frequencyHz, 300.0);
}
