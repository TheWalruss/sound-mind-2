#include "sound_mind/core/pitch_detection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

std::vector<float> detectPitch(const sound_mind::codec::StreamImage& content, const PitchSettings& settings) {
    const auto& config = content.config;
    std::vector<float> pitch(content.frameCount, 0.0f);
    if (content.frameCount == 0 || config.binCount < 3) {
        return pitch;
    }
    std::vector<double> weightDb(config.binCount);
    for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
        weightDb[bin] = sound_mind::codec::aWeightingOffsetDb(binIndexToFrequency(static_cast<float>(bin), config));
    }
    const auto binOf = [&](double frequencyHz) {
        return static_cast<double>(frequencyToBinIndex(static_cast<float>(frequencyHz), config));
    };
    const auto lowBin = static_cast<std::uint32_t>(std::max(0.0, std::ceil(binOf(settings.minHz))));
    const auto highBin = static_cast<std::uint32_t>(
        std::min(static_cast<double>(config.binCount - 1), std::floor(binOf(settings.maxHz))));

    std::vector<double> amplitude(config.binCount);
    std::vector<double> levelDb(config.binCount);
    for (std::uint32_t frame = 0; frame < content.frameCount; ++frame) {
        double loudestDb = -1e9;
        for (std::uint32_t bin = 0; bin < config.binCount; ++bin) {
            const std::size_t cell = cellIndex(bin, frame, content.frameCount);
            const double left = std::pow(10.0, (content.leftMagnitudeDb[cell] - weightDb[bin]) / 20.0);
            const double right = std::pow(10.0, (content.rightMagnitudeDb[cell] - weightDb[bin]) / 20.0);
            amplitude[bin] = 0.5 * (left + right);
            levelDb[bin] = 20.0 * std::log10(std::max(1e-12, amplitude[bin]));
            loudestDb = std::max(loudestDb, levelDb[bin]);
        }
        if (loudestDb < settings.silenceDb) {
            continue;
        }
        const auto peakNear = [&](double frequencyHz) {
            const double centre = binOf(frequencyHz);
            if (centre > config.binCount - 1) {
                return 0.0;
            }
            double best = 0.0;
            for (auto bin = static_cast<std::int64_t>(std::floor(centre - 0.5));
                 bin <= static_cast<std::int64_t>(std::ceil(centre + 0.5)); ++bin) {
                if (bin >= 0 && bin < static_cast<std::int64_t>(config.binCount)) {
                    best = std::max(best, amplitude[static_cast<std::size_t>(bin)]);
                }
            }
            return best;
        };
        double bestScore = 0.0;
        std::int64_t bestBin = -1;
        for (std::uint32_t bin = lowBin; bin <= highBin; ++bin) {
            if (levelDb[bin] < loudestDb - settings.fundamentalWithinDb) {
                continue;
            }
            // Only local maxima can be a partial's centre.
            if (amplitude[bin] < amplitude[bin - (bin > 0 ? 1 : 0)] ||
                (bin + 1 < config.binCount && amplitude[bin] < amplitude[bin + 1])) {
                continue;
            }
            const double frequencyHz = binIndexToFrequency(static_cast<float>(bin), config);
            double score = 0.0;
            for (int harmonic = 1; harmonic <= 5; ++harmonic) {
                score += peakNear(frequencyHz * harmonic) / harmonic;
            }
            if (score > bestScore) {
                bestScore = score;
                bestBin = bin;
            }
        }
        if (bestBin < 0) {
            continue;
        }
        // Parabolic refinement on the dB levels around the winning bin.
        double refined = static_cast<double>(bestBin);
        if (bestBin > 0 && bestBin + 1 < static_cast<std::int64_t>(config.binCount)) {
            const double a = levelDb[static_cast<std::size_t>(bestBin - 1)];
            const double b = levelDb[static_cast<std::size_t>(bestBin)];
            const double c = levelDb[static_cast<std::size_t>(bestBin + 1)];
            const double denominator = a - 2.0 * b + c;
            if (std::abs(denominator) > 1e-9) {
                refined += std::clamp(0.5 * (a - c) / denominator, -0.5, 0.5);
            }
        }
        double frequencyHz = binIndexToFrequency(static_cast<float>(refined), config);

        // Phase-vocoder refinement: a partial's phase advances 2*pi*f*hop/
        // sampleRate per frame, which pins f down modulo sampleRate/hop -
        // far finer than the magnitude peak, which at low frequencies sits
        // on the nearest linear FFT bin. The coarse estimate above is well
        // within half that spacing, so it picks the right multiple.
        if (content.frameCount > 1) {
            const std::uint32_t current = frame > 0 ? frame : 1;
            const std::uint32_t previous = current - 1;
            const auto bin = static_cast<std::uint32_t>(bestBin);
            const double advance = content.sharedPhaseRadians[cellIndex(bin, current, content.frameCount)] -
                                   content.sharedPhaseRadians[cellIndex(bin, previous, content.frameCount)];
            const double spacingHz = static_cast<double>(config.sampleRateHz) / static_cast<double>(config.hopLength);
            const double fraction = std::remainder(advance, 2.0 * std::numbers::pi) / (2.0 * std::numbers::pi);
            const double candidate = fraction * spacingHz;
            const double cycles = std::round((frequencyHz - candidate) / spacingHz);
            const double phaseHz = candidate + cycles * spacingHz;
            if (phaseHz > 0.0 && std::abs(phaseHz - frequencyHz) < 0.5 * spacingHz) {
                frequencyHz = phaseHz;
            }
        }
        pitch[frame] = static_cast<float>(frequencyHz);
    }
    return pitch;
}

NoteName noteFor(double frequencyHz, double a4Hz) {
    static const std::array<const char*, 12> kNames{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const double midi = 69.0 + 12.0 * std::log2(frequencyHz / a4Hz);
    const double nearest = std::round(midi);
    const auto note = static_cast<int>(nearest);
    NoteName result;
    result.name = kNames[static_cast<std::size_t>(((note % 12) + 12) % 12)];
    result.octave = static_cast<int>(std::floor(nearest / 12.0)) - 1;
    result.cents = (midi - nearest) * 100.0;
    return result;
}

std::vector<float> removePitchSpikes(const std::vector<float>& hzPerFrame, double secondsPerFrame,
                                     double maxSpikeSeconds) {
    std::vector<float> cleaned = hzPerFrame;
    if (maxSpikeSeconds <= 0.0 || secondsPerFrame <= 0.0 || hzPerFrame.empty()) {
        return cleaned;
    }
    const auto semitonesApart = [](float a, float b) {
        return std::abs(12.0 * std::log2(static_cast<double>(a) / static_cast<double>(b)));
    };
    const auto isShort = [&](std::size_t frames) {
        // A small allowance so a run of exactly the limit isn't caught by rounding.
        return static_cast<double>(frames) * secondsPerFrame < maxSpikeSeconds - 1e-9;
    };

    // Runs: [start, end) of voiced frames within tolerance of their predecessor.
    struct Run {
        std::size_t start;
        std::size_t end;
    };
    std::vector<Run> runs;
    for (std::size_t i = 0; i < hzPerFrame.size(); ++i) {
        if (hzPerFrame[i] <= 0.0f) {
            continue;
        }
        const bool continues = !runs.empty() && runs.back().end == i &&
                               semitonesApart(hzPerFrame[i], hzPerFrame[i - 1]) <= kPitchRunToleranceSemitones;
        if (continues) {
            runs.back().end = i + 1;
        } else {
            runs.push_back(Run{i, i + 1});
        }
    }

    // Back-to-back short runs form one spike block, judged by its total length.
    std::size_t r = 0;
    while (r < runs.size()) {
        if (!isShort(runs[r].end - runs[r].start)) {
            ++r;
            continue;
        }
        std::size_t last = r;
        while (last + 1 < runs.size() && runs[last + 1].start == runs[last].end &&
               isShort(runs[last + 1].end - runs[last + 1].start)) {
            ++last;
        }
        const std::size_t start = runs[r].start;
        const std::size_t end = runs[last].end;
        if (isShort(end - start)) {
            const bool before = start > 0 && hzPerFrame[start - 1] > 0.0f;
            const bool after = end < hzPerFrame.size() && hzPerFrame[end] > 0.0f;
            if (before && after &&
                semitonesApart(hzPerFrame[start - 1], hzPerFrame[end]) <= kPitchRunToleranceSemitones) {
                // Smooth: glide from the frame before to the frame after, in log frequency.
                const double from = std::log(static_cast<double>(hzPerFrame[start - 1]));
                const double to = std::log(static_cast<double>(hzPerFrame[end]));
                const auto span = static_cast<double>(end - start + 1);
                for (std::size_t i = start; i < end; ++i) {
                    const double t = static_cast<double>(i - start + 1) / span;
                    cleaned[i] = static_cast<float>(std::exp(from + (to - from) * t));
                }
            } else {
                std::fill(cleaned.begin() + static_cast<std::ptrdiff_t>(start),
                          cleaned.begin() + static_cast<std::ptrdiff_t>(end), 0.0f);
            }
        }
        r = last + 1;
    }
    return cleaned;
}

}  // namespace sound_mind::core
