#include "sound_mind/core/loudness_meter.h"

#include <algorithm>
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
    std::vector<double> power(count, 0.0);
    for (const std::vector<float>* channel : {&audio.left, &audio.right}) {
        if (channel == &audio.right && !stereo) {
            break;
        }
        std::vector<double> filtered(channel->begin(), channel->end());
        shelf.run(filtered);
        highPass.run(filtered);
        for (std::size_t i = 0; i < count; ++i) {
            power[i] += filtered[i] * filtered[i];
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

/// @brief The largest |value| at the three quarter-sample points between
/// samples, examined only near samples within 6 dB of `samplePeak`.
double interSamplePeak(const std::vector<float>& channel, double samplePeak) {
    double peak = 0.0;
    const double threshold = samplePeak * 0.5;
    const auto count = static_cast<std::int64_t>(channel.size());
    for (std::int64_t n = 0; n + 1 < count; ++n) {
        if (std::abs(channel[static_cast<std::size_t>(n)]) < threshold &&
            std::abs(channel[static_cast<std::size_t>(n + 1)]) < threshold) {
            continue;
        }
        for (const double fraction : {0.25, 0.5, 0.75}) {
            double value = 0.0;
            for (std::int64_t k = n - 7; k <= n + 8; ++k) {
                if (k < 0 || k >= count) {
                    continue;
                }
                value += channel[static_cast<std::size_t>(k)] *
                         interpolationKernel(static_cast<double>(n) + fraction - static_cast<double>(k));
            }
            peak = std::max(peak, std::abs(value));
        }
    }
    return peak;
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

    // Integrated: complete 400 ms blocks every 100 ms, absolute then
    // relative gate (a buffer shorter than one block is one block).
    std::vector<double> blockPowers;
    for (std::size_t end = std::min(block, count); end <= count; end += step) {
        blockPowers.push_back(windowPower(prefix, end, std::min(block, count)));
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
    if (absolute > 0.0) {
        result.integratedLufs = lufsFromPower(gatedMean(lufsFromPower(absolute) - 10.0));
    }
    return result;
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
        for (std::uint32_t frame = 0; frame < content.frameCount; ++frame) {
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
    }
    return levels;
}

}  // namespace sound_mind::core
