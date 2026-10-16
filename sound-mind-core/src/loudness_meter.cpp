#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/codec/parallel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief A direct-form-I biquad, run over a whole channel in double precision.
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    void run(std::vector<double>& samples) const {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        for (double& sample : samples) {
            const double x = sample;
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            sample = y;
        }
    }
};

/// @brief BS.1770's stage-1 "pre-filter": a high shelf of about +4 dB above
/// ~1.7 kHz, designed for `sampleRateHz` from its analog prototype (the
/// 48 kHz coefficients in the standard are this design evaluated at 48 kHz).
Biquad kWeightingShelf(double sampleRateHz) {
    constexpr double kGainDb = 3.999843853973347;
    constexpr double kCentreHz = 1681.974450955533;
    constexpr double kQ = 0.7071752369554196;
    const double k = std::tan(std::numbers::pi * kCentreHz / sampleRateHz);
    const double vh = std::pow(10.0, kGainDb / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k / kQ + k * k;
    return Biquad{(vh + vb * k / kQ + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / kQ + k * k) / a0,
                  2.0 * (k * k - 1.0) / a0, (1.0 - k / kQ + k * k) / a0};
}

/// @brief BS.1770's stage-2 "RLB" high-pass at ~38 Hz (numerator un-normalized,
/// exactly as the standard specifies it).
Biquad kWeightingHighPass(double sampleRateHz) {
    constexpr double kCentreHz = 38.13547087602444;
    constexpr double kQ = 0.5003270373238773;
    const double k = std::tan(std::numbers::pi * kCentreHz / sampleRateHz);
    const double a0 = 1.0 + k / kQ + k * k;
    return Biquad{1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / kQ + k * k) / a0};
}

/// @brief Running sums of squared K-weighted samples, summed over channels,
/// so any window's mean power is two lookups.
std::vector<double> squaredPrefix(const sound_mind::codec::AudioBuffer& audio) {
    const std::size_t count = audio.frameCount();
    std::vector<double> prefix(count + 1, 0.0);
    const bool stereo = audio.right.size() == count;
    const Biquad shelf = kWeightingShelf(audio.sampleRateHz);
    const Biquad highPass = kWeightingHighPass(audio.sampleRateHz);
    // The K-weighting filters are recursive - sample by sample - but the
    // channels are independent, so each runs on its own thread (v0.1.41.4).
    const std::array<const std::vector<float>*, 2> channels{&audio.left, &audio.right};
    std::array<std::vector<double>, 2> filtered;
    sound_mind::codec::parallelFor(stereo ? 2 : 1, 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t c = begin; c < end; ++c) {
            filtered[c].assign(channels[c]->begin(), channels[c]->end());
            shelf.run(filtered[c]);
            highPass.run(filtered[c]);
        }
    });
    std::vector<double> power(count, 0.0);
    for (std::size_t c = 0; c < (stereo ? 2u : 1u); ++c) {
        for (std::size_t i = 0; i < count; ++i) {
            power[i] += filtered[c][i] * filtered[c][i];
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        prefix[i + 1] = prefix[i] + power[i];
    }
    return prefix;
}

double lufsFromPower(double meanPower) {
    if (meanPower <= 0.0) {
        return kLoudnessFloorLufs;
    }
    return std::max(kLoudnessFloorLufs, -0.691 + 10.0 * std::log10(meanPower));
}

/// @brief The mean power of the window `[end - length, end)`, clipped to the
/// buffer's start (averaged over the samples actually there).
double windowPower(const std::vector<double>& prefix, std::size_t end, std::size_t length) {
    const std::size_t start = end > length ? end - length : 0;
    const std::size_t span = end - start;
    return span == 0 ? 0.0 : (prefix[end] - prefix[start]) / static_cast<double>(span);
}

/// @brief A Hann-windowed sinc kernel value at offset `t` (samples), half-width 8.
double interpolationKernel(double t) {
    constexpr double kHalfWidth = 8.0;
    if (std::abs(t) >= kHalfWidth) {
        return 0.0;
    }
    const double window = 0.5 * (1.0 + std::cos(std::numbers::pi * t / kHalfWidth));
    if (std::abs(t) < 1e-12) {
        return window;
    }
    return window * std::sin(std::numbers::pi * t) / (std::numbers::pi * t);
}

/// @brief interpolationKernel() at the 16 taps of each of the three
/// quarter-sample points: `[point][j]` is the weight of sample `n - 7 + j`
/// for the point `n + {0.25, 0.5, 0.75}`. The offsets are the same for every
/// `n`, so the kernel's trig is done once, not per sample (v0.1.41.4).
const std::array<std::array<double, 16>, 3>& interpolationTable() {
    static const auto table = [] {
        std::array<std::array<double, 16>, 3> weights{};
        constexpr double kFractions[] = {0.25, 0.5, 0.75};
        for (std::size_t point = 0; point < 3; ++point) {
            for (std::size_t j = 0; j < 16; ++j) {
                weights[point][j] = interpolationKernel(kFractions[point] + 7.0 - static_cast<double>(j));
            }
        }
        return weights;
    }();
    return table;
}

/// @brief The largest |value| at the three quarter-sample points between
/// samples, examined only near samples within 6 dB of `samplePeak`. Blocks
/// of samples run in parallel (v0.1.41.4); the maximum doesn't depend on
/// the order.
double interSamplePeak(const std::vector<float>& channel, double samplePeak) {
    const double threshold = samplePeak * 0.5;
    const auto count = static_cast<std::int64_t>(channel.size());
    if (count < 2) {
        return 0.0;
    }
    const auto& table = interpolationTable();
    constexpr std::size_t kBlock = 1 << 16;
    const std::size_t pairs = static_cast<std::size_t>(count - 1);
    std::vector<double> blockPeaks((pairs + kBlock - 1) / kBlock, 0.0);
    sound_mind::codec::parallelFor(pairs, kBlock, [&](std::size_t begin, std::size_t end) {
        double peak = 0.0;
        for (auto n = static_cast<std::int64_t>(begin); n < static_cast<std::int64_t>(end); ++n) {
            if (std::abs(channel[static_cast<std::size_t>(n)]) < threshold &&
                std::abs(channel[static_cast<std::size_t>(n + 1)]) < threshold) {
                continue;
            }
            for (std::size_t point = 0; point < 3; ++point) {
                double value = 0.0;
                for (std::int64_t j = 0; j < 16; ++j) {
                    const std::int64_t k = n - 7 + j;
                    if (k < 0 || k >= count) {
                        continue;
                    }
                    value += channel[static_cast<std::size_t>(k)] * table[point][static_cast<std::size_t>(j)];
                }
                peak = std::max(peak, std::abs(value));
            }
        }
        blockPeaks[begin / kBlock] = peak;
    });
    return blockPeaks.empty() ? 0.0 : *std::max_element(blockPeaks.begin(), blockPeaks.end());
}

/// @brief BS.1770 integrated loudness from running squared sums: complete
/// 400 ms blocks every 100 ms, then the -70 LUFS absolute and -10 LU relative
/// gates (a buffer shorter than one block is one block).
double gatedIntegrated(const std::vector<double>& prefix, std::size_t count, std::uint32_t sampleRateHz) {
    const double rate = static_cast<double>(sampleRateHz);
    const auto block = std::min(count, static_cast<std::size_t>(std::lround(0.4 * rate)));
    const auto step = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(kLoudnessStepSeconds * rate)));
    std::vector<double> blockPowers;
    for (std::size_t end = block; end <= count; end += step) {
        blockPowers.push_back(windowPower(prefix, end, block));
        if (block >= count) {
            break;
        }
    }
    const auto gatedMean = [&](double thresholdLufs) {
        double sum = 0.0;
        std::size_t kept = 0;
        for (const double power : blockPowers) {
            if (lufsFromPower(power) > thresholdLufs) {
                sum += power;
                ++kept;
            }
        }
        return kept == 0 ? 0.0 : sum / static_cast<double>(kept);
    };
    const double absolute = gatedMean(kLoudnessFloorLufs);
    return absolute > 0.0 ? lufsFromPower(gatedMean(lufsFromPower(absolute) - 10.0)) : kLoudnessFloorLufs;
}

double toDb(double amplitude) {
    return amplitude > 0.0 ? std::max(kLoudnessFloorLufs, 20.0 * std::log10(amplitude)) : kLoudnessFloorLufs;
}

}  // namespace

LoudnessMeasurement measureLoudness(const sound_mind::codec::AudioBuffer& audio,
                                    const std::function<bool()>& shouldCancel) {
    LoudnessMeasurement result;
    const std::size_t count = audio.frameCount();
    if (count == 0 || audio.sampleRateHz == 0) {
        return result;
    }
    const auto cancelPoint = [&shouldCancel]() {
        if (shouldCancel && shouldCancel()) {
            throw std::runtime_error("cancelled");
        }
    };

    // Peaks and clipping.
    double samplePeak = 0.0;
    for (const std::vector<float>* channel : {&audio.left, &audio.right}) {
        for (const float sample : *channel) {
            const double magnitude = std::abs(static_cast<double>(sample));
            samplePeak = std::max(samplePeak, magnitude);
            if (magnitude > 1.0) {
                ++result.clippedSamples;
            }
        }
    }
    cancelPoint();
    double truePeak = samplePeak;
    for (const std::vector<float>* channel : {&audio.left, &audio.right}) {
        truePeak = std::max(truePeak, interSamplePeak(*channel, samplePeak));
    }
    result.samplePeakDbfs = toDb(samplePeak);
    result.truePeakDbtp = toDb(truePeak);
    cancelPoint();

    // Loudness.
    const std::vector<double> prefix = squaredPrefix(audio);
    cancelPoint();
    const double rate = static_cast<double>(audio.sampleRateHz);
    const auto block = static_cast<std::size_t>(std::lround(0.4 * rate));
    const auto shortTerm = static_cast<std::size_t>(std::lround(3.0 * rate));
    const auto step = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(kLoudnessStepSeconds * rate)));
    for (std::size_t end = step; end <= count; end += step) {
        const auto momentary = static_cast<float>(lufsFromPower(windowPower(prefix, end, block)));
        const auto longer = static_cast<float>(lufsFromPower(windowPower(prefix, end, shortTerm)));
        result.momentaryLufs.push_back(momentary);
        result.shortTermLufs.push_back(longer);
        result.maxMomentaryLufs = std::max(result.maxMomentaryLufs, static_cast<double>(momentary));
        result.maxShortTermLufs = std::max(result.maxShortTermLufs, static_cast<double>(longer));
    }

    result.integratedLufs = gatedIntegrated(prefix, count, audio.sampleRateHz);
    return result;
}

double integratedLoudnessLufs(const sound_mind::codec::AudioBuffer& audio) {
    const std::size_t count = audio.frameCount();
    if (count == 0 || audio.sampleRateHz == 0) {
        return kLoudnessFloorLufs;
    }
    return gatedIntegrated(squaredPrefix(audio), count, audio.sampleRateHz);
}

const std::vector<LoudnessBand>& loudnessBands() {
    static const std::vector<LoudnessBand> bands{
        {"Sub (20–60 Hz)", 20.0, 60.0},          {"Bass (60–250 Hz)", 60.0, 250.0},
        {"Low-mid (250 Hz–2 kHz)", 250.0, 2000.0}, {"Mid (2–6 kHz)", 2000.0, 6000.0},
        {"High-mid (6–10 kHz)", 6000.0, 10000.0}, {"Air (10–20 kHz)", 10000.0, 20000.0},
    };
    return bands;
}

std::vector<float> bandLevelsOverTime(const sound_mind::codec::StreamImage& content) {
    const auto& bands = loudnessBands();
    std::vector<float> levels(bands.size() * content.frameCount, static_cast<float>(kLoudnessFloorLufs));
    // Each band's frames are independent: blocks of frames run in parallel
    // (v0.1.41.4), each writing only its own cells.
    for (std::size_t band = 0; band < bands.size(); ++band) {
        std::vector<std::uint32_t> bins;
        std::vector<double> weights;
        for (std::uint32_t bin = 0; bin < content.config.binCount; ++bin) {
            const float frequencyHz = binIndexToFrequency(static_cast<float>(bin), content.config);
            if (frequencyHz >= bands[band].lowHz && frequencyHz < bands[band].highHz) {
                bins.push_back(bin);
                weights.push_back(sound_mind::codec::aWeightingOffsetDb(frequencyHz));
            }
        }
        if (bins.empty()) {
            continue;
        }
        sound_mind::codec::parallelFor(content.frameCount, 256, [&](std::size_t begin, std::size_t end) {
            for (auto frame = static_cast<std::uint32_t>(begin); frame < end; ++frame) {
                double power = 0.0;
                for (std::size_t i = 0; i < bins.size(); ++i) {
                    const std::size_t cell = cellIndex(bins[i], frame, content.frameCount);
                    power += std::pow(10.0, (content.leftMagnitudeDb[cell] - weights[i]) / 10.0) +
                             std::pow(10.0, (content.rightMagnitudeDb[cell] - weights[i]) / 10.0);
                }
                power /= 2.0 * static_cast<double>(bins.size());
                levels[band * content.frameCount + frame] =
                    static_cast<float>(power > 0.0 ? std::max(kLoudnessFloorLufs, 10.0 * std::log10(power))
                                                   : kLoudnessFloorLufs);
            }
        });
    }
    return levels;
}

}  // namespace sound_mind::core
