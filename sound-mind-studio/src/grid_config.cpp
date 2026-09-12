#include "sound_mind/studio/grid_config.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sound_mind::studio {

namespace {

/// @brief The MIDI note range every semitone-enumerating loop here walks -
/// wide enough that `settings`' own frequency range does the real
/// filtering, matching `axis_labels.cpp`'s own `Notes` tick loop.
constexpr int kMinMidiNote = 0;
constexpr int kMaxMidiNote = 135;

/// @brief `midi`'s own frequency, against `referenceHz` - the same
/// formula `axis_labels.cpp`'s own `Notes` tick loop uses (MIDI note 69
/// is always A4, regardless of `referenceHz`'s own value).
double frequencyForMidiNote(int midi, double referenceHz) {
    return referenceHz * std::pow(2.0, (midi - 69) / 12.0);
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
        for (int midi = kMinMidiNote; midi <= kMaxMidiNote; ++midi) {
            const double frequencyHz = frequencyForMidiNote(midi, referenceHz);
            if (frequencyHz >= minFrequencyHz && frequencyHz <= maxFrequencyHz) {
                lines.push_back(frequencyHz);
            }
        }
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

    if (config.noteGridEnabled && settings.referenceHz > 0.0 && frequencyHz > 0.0) {
        const double midi = 69.0 + 12.0 * std::log2(frequencyHz / settings.referenceHz);
        const int nearestMidi =
            static_cast<int>(std::clamp(std::lround(midi), static_cast<long>(kMinMidiNote), static_cast<long>(kMaxMidiNote)));
        consider(frequencyForMidiNote(nearestMidi, settings.referenceHz));
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

}  // namespace sound_mind::studio
