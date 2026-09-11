#include "sound_mind/studio/axis_labels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "sound_mind/core/music_theory.h"
#include "sound_mind/core/paint_application.h"

namespace sound_mind::studio {

namespace {

/// @brief The fewest pixels two adjacent frequency-axis ticks may sit
/// apart before the later one is dropped - matches the legacy Studio's
/// own frequency-label spacing.
constexpr double kMinVerticalTickSpacingPixels = 14.0;

/// @brief The fewest pixels two adjacent time-axis ticks may sit apart
/// before a coarser step is chosen instead - matches the legacy Studio's
/// own time-label spacing.
constexpr double kMinHorizontalTickSpacingPixels = 50.0;

/// @brief A fixed set of musically-legible Hz values `Hertz` mode walks,
/// rather than computing an evenly bin-spaced set - see
/// verticalAxisTicks()'s own docs for why. Matches the legacy Studio's
/// own list.
constexpr std::array<double, 18> kNiceFrequenciesHz = {20,   30,   50,   75,   100,  150,  200,  300,  500, 750,
                                                          1000, 1500, 2000, 3000, 5000, 7500, 10000, 16000};

/// @brief The smallest "nice" step (a 1/2/5 times a power of ten) at
/// least as large as `roughStep` - the classic linear-axis tick-spacing
/// algorithm, shared by the time axis and the frequency axis's own
/// `BinIndex` mode (the one frequency-axis mode that *is* linear).
double niceLinearStep(double roughStep) {
    if (roughStep <= 0.0) {
        return 1.0;
    }
    const double magnitude = std::pow(10.0, std::floor(std::log10(roughStep)));
    const double residual = roughStep / magnitude;
    double niceResidual;
    if (residual <= 1.0) {
        niceResidual = 1.0;
    } else if (residual <= 2.0) {
        niceResidual = 2.0;
    } else if (residual <= 5.0) {
        niceResidual = 5.0;
    } else {
        niceResidual = 10.0;
    }
    return niceResidual * magnitude;
}

/// @brief `frequencyHz` formatted as `"123 Hz"` below 1000 Hz, or
/// `"1.5 kHz"`-style above it.
QString formatHertz(double frequencyHz) {
    if (frequencyHz >= 1000.0) {
        return QString::number(frequencyHz / 1000.0, 'g', 3) + QStringLiteral(" kHz");
    }
    return QString::number(std::lround(frequencyHz)) + QStringLiteral(" Hz");
}

/// @brief `seconds` formatted with just enough decimal places to
/// distinguish ticks `stepSeconds` apart - no trailing-zero clutter for
/// a coarse step, no lost precision for a fine one.
QString formatSeconds(double seconds, double stepSeconds) {
    int decimals = 0;
    if (stepSeconds < 0.01) {
        decimals = 3;
    } else if (stepSeconds < 0.1) {
        decimals = 2;
    } else if (stepSeconds < 1.0) {
        decimals = 1;
    }
    return QString::number(seconds, 'f', decimals) + QStringLiteral("s");
}

}  // namespace

std::vector<AxisTick> verticalAxisTicks(VerticalAxisLabelMode mode, const sound_mind::core::ProjectSettings& settings,
                                          double axisPixelLength) {
    if (mode == VerticalAxisLabelMode::Off || axisPixelLength <= 0.0) {
        return {};
    }

    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    if (config.binCount <= 1) {
        return {};
    }
    const double pixelsPerBin = axisPixelLength / static_cast<double>(config.binCount);
    const double maxFrequencyHz =
        std::min(static_cast<double>(config.maxFrequencyHz), static_cast<double>(config.sampleRateHz) / 2.0);
    const double minFrequencyHz = static_cast<double>(config.minFrequencyHz);

    std::vector<AxisTick> ticks;
    std::optional<float> lastKeptBin;
    const auto tryAdd = [&](double frequencyHz, const QString& label) {
        if (frequencyHz < minFrequencyHz || frequencyHz > maxFrequencyHz) {
            return;
        }
        const float bin = sound_mind::core::frequencyToBinIndex(static_cast<float>(frequencyHz), config);
        if (lastKeptBin.has_value() && std::abs(bin - *lastKeptBin) * pixelsPerBin < kMinVerticalTickSpacingPixels) {
            return;
        }
        ticks.push_back(AxisTick{frequencyHz, label});
        lastKeptBin = bin;
    };

    switch (mode) {
        case VerticalAxisLabelMode::Off:
            break;
        case VerticalAxisLabelMode::Hertz:
            for (const double frequencyHz : kNiceFrequenciesHz) {
                tryAdd(frequencyHz, formatHertz(frequencyHz));
            }
            break;
        case VerticalAxisLabelMode::Notes: {
            // Every semitone across the whole representable range - low
            // enough and high enough that the in-range check above does
            // the real filtering.
            for (int midi = 0; midi <= 135; ++midi) {
                const double frequencyHz = settings.referenceHz * std::pow(2.0, (midi - 69) / 12.0);
                tryAdd(frequencyHz, QString::fromStdString(
                                        sound_mind::core::noteNameForFrequency(frequencyHz, settings.referenceHz)));
            }
            break;
        }
        case VerticalAxisLabelMode::BinIndex: {
            const double roughStep = (kMinVerticalTickSpacingPixels / axisPixelLength) * (config.binCount - 1);
            const double step = niceLinearStep(roughStep);
            for (double bin = 0.0; bin <= static_cast<double>(config.binCount - 1) + 1e-9; bin += step) {
                const float frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(bin), config);
                ticks.push_back(AxisTick{frequencyHz, QString::number(std::lround(bin))});
            }
            break;
        }
    }
    return ticks;
}

std::vector<AxisTick> horizontalAxisTicks(HorizontalAxisLabelMode mode,
                                            const sound_mind::core::ProjectSettings& settings,
                                            double axisPixelLength) {
    if (mode == HorizontalAxisLabelMode::Off || axisPixelLength <= 0.0) {
        return {};
    }

    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const double durationSeconds =
        static_cast<double>(settings.canvasWidth) * settings.timestepMs / 1000.0;
    if (durationSeconds <= 0.0) {
        return {};
    }

    const double roughStep = (kMinHorizontalTickSpacingPixels / axisPixelLength) * durationSeconds;
    const double step = niceLinearStep(roughStep);

    std::vector<AxisTick> ticks;
    for (double seconds = 0.0; seconds <= durationSeconds + 1e-9; seconds += step) {
        QString label;
        switch (mode) {
            case HorizontalAxisLabelMode::Off:
                break;
            case HorizontalAxisLabelMode::Seconds:
                label = formatSeconds(seconds, step);
                break;
            case HorizontalAxisLabelMode::Milliseconds:
                label = QString::number(std::lround(seconds * 1000.0)) + QStringLiteral("ms");
                break;
            case HorizontalAxisLabelMode::FrameIndex:
                label = QString::number(std::lround(sound_mind::core::timeToFrameIndex(seconds, config)));
                break;
        }
        ticks.push_back(AxisTick{seconds, label});
    }
    return ticks;
}

}  // namespace sound_mind::studio
