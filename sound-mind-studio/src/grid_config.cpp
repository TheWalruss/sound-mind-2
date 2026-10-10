#include "sound_mind/studio/grid_config.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "sound_mind/core/music_theory.h"

namespace sound_mind::studio {

namespace {

/// @brief The step range every note-grid-enumerating loop here walks, at
/// standard 12-TET's own 12 steps/octave - wide enough that `settings`'
/// own frequency range does the real filtering, matching
/// `axis_labels.cpp`'s own `Notes` tick loop (`kMinMidiNote`/
/// `kMaxMidiNote` there). Scaled by `stepsPerOctave/12` for any other
/// temperament, so every temperament covers the same real frequency span
/// regardless of how many steps its own octave has - see
/// `noteGridFrequencies()`'s own docs.
constexpr int kMinEqual12Step = 0 - 69;
constexpr int kMaxEqual12Step = 135 - 69;

/// @brief Floor-mod, not C++'s own truncate-toward-zero `%` - keeps the
/// result in `[0, modulus)` even for a negative `value`.
int floorMod(int value, int modulus) {
    const int remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

/// @brief Every note-grid line `config` currently activates, within
/// `[minFrequencyHz, maxFrequencyHz]`, honoring `config.noteGridTemperament`'s
/// own step count and `config.noteGridExcludedSteps`/
/// `config.noteGridExcludedOctaves` - shared by collectFrequencyGridLines()
/// (the full grid) and nearestFrequencyGridLineHz() (which just needs the
/// closest one), so both always agree on exactly which notes are
/// currently included.
///
/// Walks every step across `kMinEqual12Step`/`kMaxEqual12Step` scaled by
/// `stepsPerOctave/12` - cheap even for the densest temperament here
/// (`Equal72`'s own ~810 steps across the scaled range is still a handful
/// of microseconds of floating-point work, negligible next to a mouse-
/// drag's own Snap to Grid call), so there's no need for a closed-form
/// "nearest step" shortcut the way a single, unfiltered 12-TET lookup
/// once used - the exclusion sets make a closed-form shortcut
/// meaningfully harder to get right than iterating does.
std::vector<double> noteGridFrequencies(const FrequencyGridConfig& config, double referenceHz, double minFrequencyHz,
                                           double maxFrequencyHz) {
    std::vector<double> lines;
    const int divisions = sound_mind::core::stepsPerOctave(config.noteGridTemperament);
    const int minStep = static_cast<int>(std::floor(kMinEqual12Step * divisions / 12.0));
    const int maxStep = static_cast<int>(std::ceil(kMaxEqual12Step * divisions / 12.0));
    for (int step = minStep; step <= maxStep; ++step) {
        const int stepWithinOctave = floorMod(step, divisions);
        if (config.noteGridExcludedSteps.count(stepWithinOctave) > 0) {
            continue;
        }
        const double frequencyHz =
            sound_mind::core::frequencyForTemperamentStep(config.noteGridTemperament, step, referenceHz);
        if (frequencyHz < minFrequencyHz || frequencyHz > maxFrequencyHz) {
            continue;
        }
        const int octave = sound_mind::core::octaveNumberForFrequency(frequencyHz, referenceHz);
        if (config.noteGridExcludedOctaves.count(octave) > 0) {
            continue;
        }
        lines.push_back(frequencyHz);
    }
    return lines;
}

/// @brief Appends every note-grid/harmonic-series/custom-frequency line
/// `config` currently activates, within `[minFrequencyHz, maxFrequencyHz]`,
/// unsorted and not yet deduplicated - shared by frequencyGridLinesHz()
/// (which sorts/dedupes the result) and nearestFrequencyGridLineHz()
/// (which only needs the minimum distance, not a clean sorted list).
std::vector<double> collectFrequencyGridLines(const FrequencyGridConfig& config, double referenceHz,
                                                 double minFrequencyHz, double maxFrequencyHz) {
    std::vector<double> lines;
    if (minFrequencyHz > maxFrequencyHz) {
        return lines;
    }

    if (config.noteGridEnabled) {
        const auto noteLines = noteGridFrequencies(config, referenceHz, minFrequencyHz, maxFrequencyHz);
        lines.insert(lines.end(), noteLines.begin(), noteLines.end());
    }

    if (config.harmonicSeriesEnabled && config.harmonicFundamentalHz > 0.0) {
        for (double frequencyHz = config.harmonicFundamentalHz; frequencyHz <= maxFrequencyHz;
             frequencyHz += config.harmonicFundamentalHz) {
            if (frequencyHz >= minFrequencyHz) {
                lines.push_back(frequencyHz);
            }
        }
    }

    if (config.customFrequenciesEnabled) {
        for (const double frequencyHz : config.customFrequenciesHz) {
            if (frequencyHz >= minFrequencyHz && frequencyHz <= maxFrequencyHz) {
                lines.push_back(frequencyHz);
            }
        }
    }

    return lines;
}

}  // namespace

std::vector<double> frequencyGridLinesHz(const FrequencyGridConfig& config,
                                            const sound_mind::core::ProjectSettings& settings) {
    if (!config.isActive()) {
        return {};
    }
    std::vector<double> lines = collectFrequencyGridLines(config, settings.referenceHz, settings.minFrequencyHz,
                                                              settings.maxFrequencyHz);
    std::sort(lines.begin(), lines.end());
    // Merges near-duplicates (e.g. the note grid and a harmonic series
    // both happening to land on the same Hz value) within a tiny
    // relative tolerance - exact equality would miss the floating-point
    // rounding two different formulas landing on "the same" frequency
    // can produce.
    lines.erase(std::unique(lines.begin(), lines.end(),
                              [](double a, double b) { return std::abs(a - b) <= 1e-6 * std::max(a, b); }),
                 lines.end());
    return lines;
}

std::vector<double> timingGridLinesSeconds(const TimingGridConfig& config,
                                              const sound_mind::core::ProjectSettings& settings,
                                              double durationSeconds) {
    if (!config.isActive() || durationSeconds <= 0.0) {
        return {};
    }
    const double stepSeconds = config.mode == TimingGridMode::Interval
                                    ? config.intervalSeconds
                                    : (60.0 / settings.defaultTempoBpm) * config.tempoBeatFraction;
    if (!(stepSeconds > 0.0)) {
        return {};
    }

    std::vector<double> lines;
    for (double seconds = 0.0; seconds <= durationSeconds + 1e-9; seconds += stepSeconds) {
        lines.push_back(seconds);
    }
    return lines;
}

std::optional<double> nearestFrequencyGridLineHz(double frequencyHz, const FrequencyGridConfig& config,
                                                    const sound_mind::core::ProjectSettings& settings) {
    if (!config.isActive()) {
        return std::nullopt;
    }

    std::optional<double> best;
    double bestDistance = std::numeric_limits<double>::infinity();
    const auto consider = [&](double candidate) {
        const double distance = std::abs(candidate - frequencyHz);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    };

    if (config.noteGridEnabled && settings.referenceHz > 0.0) {
        for (const double candidate : noteGridFrequencies(config, settings.referenceHz, settings.minFrequencyHz,
                                                              settings.maxFrequencyHz)) {
            consider(candidate);
        }
    }

    if (config.harmonicSeriesEnabled && config.harmonicFundamentalHz > 0.0) {
        const double nearestMultiple = std::max(1.0, std::round(frequencyHz / config.harmonicFundamentalHz));
        consider(nearestMultiple * config.harmonicFundamentalHz);
    }

    if (config.customFrequenciesEnabled) {
        for (const double candidate : config.customFrequenciesHz) {
            consider(candidate);
        }
    }

    return best;
}

std::optional<double> nearestTimingGridLineSeconds(double seconds, const TimingGridConfig& config,
                                                      const sound_mind::core::ProjectSettings& settings) {
    if (!config.isActive()) {
        return std::nullopt;
    }
    const double stepSeconds = config.mode == TimingGridMode::Interval
                                    ? config.intervalSeconds
                                    : (60.0 / settings.defaultTempoBpm) * config.tempoBeatFraction;
    if (!(stepSeconds > 0.0)) {
        return std::nullopt;
    }
    return std::round(seconds / stepSeconds) * stepSeconds;
}

sound_mind::core::TimeFrequencyPoint snapToGrid(sound_mind::core::TimeFrequencyPoint point,
                                                   const FrequencyGridConfig& frequencyGridConfig,
                                                   const TimingGridConfig& timingGridConfig,
                                                   const sound_mind::core::ProjectSettings& settings) {
    if (const auto snappedFrequencyHz = nearestFrequencyGridLineHz(point.frequencyHz, frequencyGridConfig, settings);
        snappedFrequencyHz.has_value()) {
        point.frequencyHz = *snappedFrequencyHz;
    }
    if (const auto snappedSeconds = nearestTimingGridLineSeconds(point.timeSeconds, timingGridConfig, settings);
        snappedSeconds.has_value()) {
        point.timeSeconds = *snappedSeconds;
    }
    return point;
}

namespace {

sound_mind::core::GridLinePresetStyle toLinePresetStyle(const QColor& color, double widthPixels, Qt::PenStyle style) {
    sound_mind::core::GridLinePresetStyle result;
    result.colorR = static_cast<std::uint8_t>(color.red());
    result.colorG = static_cast<std::uint8_t>(color.green());
    result.colorB = static_cast<std::uint8_t>(color.blue());
    result.widthPixels = widthPixels;
    // Only the three styles GridPanel's own kDashStyles offers ever reach
    // here - anything else (not reachable through this panel's own UI)
    // falls back to Solid rather than asserting over a cosmetic mismatch.
    switch (style) {
        case Qt::DashLine:
            result.dashStyle = sound_mind::core::GridLinePresetStyle::DashStyle::Dash;
            break;
        case Qt::DotLine:
            result.dashStyle = sound_mind::core::GridLinePresetStyle::DashStyle::Dot;
            break;
        default:
            result.dashStyle = sound_mind::core::GridLinePresetStyle::DashStyle::Solid;
            break;
    }
    return result;
}

QColor fromLinePresetStyleColor(const sound_mind::core::GridLinePresetStyle& style) {
    return QColor(style.colorR, style.colorG, style.colorB);
}

Qt::PenStyle fromLinePresetStyleDash(const sound_mind::core::GridLinePresetStyle& style) {
    switch (style.dashStyle) {
        case sound_mind::core::GridLinePresetStyle::DashStyle::Dash:
            return Qt::DashLine;
        case sound_mind::core::GridLinePresetStyle::DashStyle::Dot:
            return Qt::DotLine;
        case sound_mind::core::GridLinePresetStyle::DashStyle::Solid:
        default:
            return Qt::SolidLine;
    }
}

}  // namespace

sound_mind::core::FrequencyGridPresetConfig toFrequencyGridPresetConfig(const FrequencyGridConfig& config) {
    sound_mind::core::FrequencyGridPresetConfig result;
    result.noteGridEnabled = config.noteGridEnabled;
    result.noteGridTemperament = config.noteGridTemperament;
    result.noteGridKey = config.noteGridKey;
    result.noteGridScale = config.noteGridScale;
    result.noteGridExcludedSteps = config.noteGridExcludedSteps;
    result.noteGridExcludedOctaves = config.noteGridExcludedOctaves;
    result.harmonicSeriesEnabled = config.harmonicSeriesEnabled;
    result.harmonicFundamentalHz = config.harmonicFundamentalHz;
    result.customFrequenciesEnabled = config.customFrequenciesEnabled;
    result.customFrequenciesHz = config.customFrequenciesHz;
    result.lineStyle = toLinePresetStyle(config.lineColor, config.lineWidthPixels, config.lineStyle);
    return result;
}

FrequencyGridConfig fromFrequencyGridPresetConfig(const sound_mind::core::FrequencyGridPresetConfig& config) {
    FrequencyGridConfig result;
    result.noteGridEnabled = config.noteGridEnabled;
    result.noteGridTemperament = config.noteGridTemperament;
    result.noteGridKey = config.noteGridKey;
    result.noteGridScale = config.noteGridScale;
    result.noteGridExcludedSteps = config.noteGridExcludedSteps;
    result.noteGridExcludedOctaves = config.noteGridExcludedOctaves;
    result.harmonicSeriesEnabled = config.harmonicSeriesEnabled;
    result.harmonicFundamentalHz = config.harmonicFundamentalHz;
    result.customFrequenciesEnabled = config.customFrequenciesEnabled;
    result.customFrequenciesHz = config.customFrequenciesHz;
    result.lineColor = fromLinePresetStyleColor(config.lineStyle);
    result.lineWidthPixels = config.lineStyle.widthPixels;
    result.lineStyle = fromLinePresetStyleDash(config.lineStyle);
    return result;
}

sound_mind::core::TimingGridPresetConfig toTimingGridPresetConfig(const TimingGridConfig& config) {
    sound_mind::core::TimingGridPresetConfig result;
    switch (config.mode) {
        case TimingGridMode::Interval:
            result.mode = sound_mind::core::GridTimingPresetMode::Interval;
            break;
        case TimingGridMode::Tempo:
            result.mode = sound_mind::core::GridTimingPresetMode::Tempo;
            break;
        case TimingGridMode::Off:
        default:
            result.mode = sound_mind::core::GridTimingPresetMode::Off;
            break;
    }
    result.intervalSeconds = config.intervalSeconds;
    result.tempoBeatFraction = config.tempoBeatFraction;
    result.lineStyle = toLinePresetStyle(config.lineColor, config.lineWidthPixels, config.lineStyle);
    return result;
}

TimingGridConfig fromTimingGridPresetConfig(const sound_mind::core::TimingGridPresetConfig& config) {
    TimingGridConfig result;
    switch (config.mode) {
        case sound_mind::core::GridTimingPresetMode::Interval:
            result.mode = TimingGridMode::Interval;
            break;
        case sound_mind::core::GridTimingPresetMode::Tempo:
            result.mode = TimingGridMode::Tempo;
            break;
        case sound_mind::core::GridTimingPresetMode::Off:
        default:
            result.mode = TimingGridMode::Off;
            break;
    }
    result.intervalSeconds = config.intervalSeconds;
    result.tempoBeatFraction = config.tempoBeatFraction;
    result.lineColor = fromLinePresetStyleColor(config.lineStyle);
    result.lineWidthPixels = config.lineStyle.widthPixels;
    result.lineStyle = fromLinePresetStyleDash(config.lineStyle);
    return result;
}

}  // namespace sound_mind::studio
