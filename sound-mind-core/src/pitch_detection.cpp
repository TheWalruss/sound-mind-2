#include "sound_mind/core/pitch_detection.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <limits>
#include <array>
#include <cmath>
#include <numbers>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief One voice found in a frame.
struct PitchCandidate {
    double hz = 0.0;
    double score = 0.0;
};

/// @brief Every frame's voices, strongest first - see detectPitches().
std::vector<std::vector<PitchCandidate>> framePitchCandidates(const sound_mind::codec::StreamImage& content,
                                                              int voiceCount, const PitchSettings& settings) {
    const auto& config = content.config;
    std::vector<std::vector<PitchCandidate>> frames(content.frameCount);
    if (content.frameCount == 0 || config.binCount < 3) {
        return frames;
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
    const double nyquistHz = 0.5 * static_cast<double>(config.sampleRateHz);
    // The codec's linear FFT bin (its FFT is 4 hops long): a partial spreads
    // at least this wide, which at low frequencies covers many log bins.
    const double linearBinHz = static_cast<double>(config.sampleRateHz) / (4.0 * static_cast<double>(config.hopLength));

    std::vector<double> amplitude(config.binCount);
    std::vector<double> levelDb(config.binCount);
    std::vector<double> residual(config.binCount);
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
        residual = amplitude;
        const auto peakNear = [&](double frequencyHz) {
            const double centre = binOf(frequencyHz);
            if (centre > config.binCount - 1) {
                return 0.0;
            }
            double best = 0.0;
            for (auto bin = static_cast<std::int64_t>(std::floor(centre - 0.5));
                 bin <= static_cast<std::int64_t>(std::ceil(centre + 0.5)); ++bin) {
                if (bin >= 0 && bin < static_cast<std::int64_t>(config.binCount)) {
                    best = std::max(best, residual[static_cast<std::size_t>(bin)]);
                }
            }
            return best;
        };
        const auto isKnownVoice = [&](double hz) {
            return std::any_of(frames[frame].begin(), frames[frame].end(), [&](const PitchCandidate& found) {
                return std::abs(12.0 * std::log2(hz / found.hz)) < 1.0;
            });
        };
        const auto removeHarmonics = [&](double hz) {
            for (int harmonic = 1; harmonic <= 12 && hz * harmonic < nyquistHz; ++harmonic) {
                const double centreHz = hz * harmonic;
                const double spreadHz = std::max(0.03 * centreHz, linearBinHz);
                const double low = binOf(std::max(1.0, centreHz - spreadHz));
                const double high = binOf(centreHz + spreadHz);
                for (auto bin = static_cast<std::int64_t>(std::floor(low));
                     bin <= static_cast<std::int64_t>(std::ceil(high)); ++bin) {
                    if (bin >= 0 && bin < static_cast<std::int64_t>(config.binCount)) {
                        residual[static_cast<std::size_t>(bin)] = 0.0;
                    }
                }
            }
        };
        // A few extra searches: a pick that turns out to be a known voice's
        // leftover is discarded rather than counted.
        for (int attempt = 0; attempt < 4 * voiceCount && static_cast<int>(frames[frame].size()) < voiceCount;
             ++attempt) {
            const bool first = frames[frame].empty();
            double bestScore = 0.0;
            std::int64_t bestBin = -1;
            for (std::uint32_t bin = lowBin; bin <= highBin; ++bin) {
                const double residualDb = 20.0 * std::log10(std::max(1e-12, residual[bin]));
                if (residualDb < loudestDb - settings.fundamentalWithinDb) {
                    continue;
                }
                // Only local maxima can be a partial's centre.
                if (residual[bin] < residual[bin - (bin > 0 ? 1 : 0)] ||
                    (bin + 1 < config.binCount && residual[bin] < residual[bin + 1])) {
                    continue;
                }
                const double frequencyHz = binIndexToFrequency(static_cast<float>(bin), config);
                if (isKnownVoice(frequencyHz)) {
                    continue;  // leftovers of a voice this frame already has.
                }
                double score = 0.0;
                for (int harmonic = 1; harmonic <= 5; ++harmonic) {
                    score += peakNear(frequencyHz * harmonic) / harmonic;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestBin = bin;
                }
            }
            if (bestBin < 0 ||
                (!first && bestScore < settings.additionalVoiceMinScoreRatio * frames[frame].front().score)) {
                break;
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
                const double spacingHz =
                    static_cast<double>(config.sampleRateHz) / static_cast<double>(config.hopLength);
                const double fraction = std::remainder(advance, 2.0 * std::numbers::pi) / (2.0 * std::numbers::pi);
                const double candidate = fraction * spacingHz;
                const double cycles = std::round((frequencyHz - candidate) / spacingHz);
                const double phaseHz = candidate + cycles * spacingHz;
                if (phaseHz > 0.0 && std::abs(phaseHz - frequencyHz) < 0.5 * spacingHz) {
                    frequencyHz = phaseHz;
                }
            }
            if (isKnownVoice(frequencyHz)) {
                // Phase refinement pulled a leakage bin onto a voice already found.
                residual[static_cast<std::size_t>(bestBin)] = 0.0;
                removeHarmonics(binIndexToFrequency(static_cast<float>(bestBin), config));
                continue;
            }
            frames[frame].push_back(PitchCandidate{frequencyHz, bestScore});
            removeHarmonics(frequencyHz);  // so the next search finds another voice.
        }
    }
    return frames;
}

double semitonesBetween(double a, double b) { return std::abs(12.0 * std::log2(a / b)); }

}  // namespace

std::vector<std::vector<float>> detectPitches(const sound_mind::codec::StreamImage& content, int voiceCount,
                                              const PitchSettings& settings) {
    const int voices = std::clamp(voiceCount, 1, kMaxPitchVoices);
    auto frames = framePitchCandidates(content, voices, settings);
    std::vector<std::vector<float>> tracks(static_cast<std::size_t>(voices),
                                           std::vector<float>(content.frameCount, 0.0f));
    if (frames.empty()) {
        return tracks;
    }
    const double secondsPerFrame =
        static_cast<double>(content.config.hopLength) / static_cast<double>(content.config.sampleRateHz);
    const auto memoryFrames = static_cast<std::int64_t>(std::lround(0.25 / secondsPerFrame));
    // A voice's register follows its own pitch with a ~2 s memory, so a
    // part that moves over a song is followed.
    const double registerRate = 1.0 - std::exp(-secondsPerFrame / 2.0);

    struct VoiceState {
        double lastHz = 0.0;
        std::int64_t lastFrame = -1;
        double registerLogHz = 0.0;  // a default until heard - see below.
    };
    std::vector<VoiceState> state(static_cast<std::size_t>(voices));
    // Default registers, spread evenly (in log frequency) from about 800 Hz
    // down to 80 Hz, highest voice first: two voices start near 450 and
    // 140 Hz - a lone guitar line goes low, a singer coming in over it high.
    for (int voice = 0; voice < voices; ++voice) {
        const double t = (static_cast<double>(voice) + 0.5) / static_cast<double>(voices);
        state[static_cast<std::size_t>(voice)].registerLogHz = std::log(800.0) + t * (std::log(80.0) - std::log(800.0));
    }
    const auto cost = [&](std::size_t voice, double hz, std::int64_t frame) {
        const VoiceState& v = state[voice];
        if (v.lastFrame >= 0 && frame - v.lastFrame <= memoryFrames) {
            return semitonesBetween(hz, v.lastHz);
        }
        return 1.0 + 0.5 * semitonesBetween(hz, std::exp(v.registerLogHz));
    };
    // Leaving a weaker pitch unassigned: cheaper than forcing it onto a voice
    // far from it (a stray second pitch where a note changes, say).
    constexpr double kDropCost = 14.0;

    std::vector<std::optional<std::size_t>> best;
    std::vector<std::optional<std::size_t>> trial;
    for (std::size_t frame = 0; frame < frames.size(); ++frame) {
        auto& candidates = frames[frame];
        if (candidates.empty()) {
            continue;
        }
        const double strongestHz = candidates.front().hz;
        // Highest pitch first: the assignment below keeps that order across
        // voices, so voice 1 is never below voice 2 in any frame.
        std::sort(candidates.begin(), candidates.end(),
                  [](const PitchCandidate& a, const PitchCandidate& b) { return a.hz > b.hz; });
        double bestCost = std::numeric_limits<double>::infinity();
        trial.assign(candidates.size(), std::nullopt);
        const std::function<void(std::size_t, std::size_t, double)> search = [&](std::size_t c, std::size_t firstVoice,
                                                                                 double total) {
            if (total >= bestCost) {
                return;
            }
            if (c == candidates.size()) {
                bestCost = total;
                best = trial;
                return;
            }
            for (std::size_t voice = firstVoice; voice < state.size(); ++voice) {
                trial[c] = voice;
                search(c + 1, voice + 1, total + cost(voice, candidates[c].hz, static_cast<std::int64_t>(frame)));
            }
            if (candidates[c].hz != strongestHz) {  // the frame's strongest pitch is always kept.
                trial[c] = std::nullopt;
                search(c + 1, firstVoice, total + kDropCost);
            }
        };
        search(0, 0, 0.0);
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            if (!best[c].has_value()) {
                continue;
            }
            VoiceState& v = state[*best[c]];
            const double logHz = std::log(candidates[c].hz);
            v.registerLogHz = v.lastFrame < 0 ? logHz : v.registerLogHz + registerRate * (logHz - v.registerLogHz);
            v.lastHz = candidates[c].hz;
            v.lastFrame = static_cast<std::int64_t>(frame);
            tracks[*best[c]][frame] = static_cast<float>(candidates[c].hz);
        }
    }
    return tracks;
}

std::vector<float> detectPitch(const sound_mind::codec::StreamImage& content, const PitchSettings& settings) {
    return detectPitches(content, 1, settings).front();
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
