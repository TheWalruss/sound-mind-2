#include "sound_mind/benchmark/scenarios.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>

#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::AudioBuffer;
using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;

/// @brief The durations swept: a short clip, a typical song section, a
/// whole song - every caller of decode() (playback, export, analysis) scales
/// with the project's length.
constexpr int kDurationsSeconds[] = {10, 60, 180};

}  // namespace

StreamCodecConfig defaultBenchmarkCodecConfig() {
    return sound_mind::core::streamCodecConfigFor(sound_mind::core::ProjectSettings{});
}

AudioBuffer musicLikeAudio(double seconds, std::uint32_t sampleRateHz, unsigned seed) {
    // A few harmonic voices moving through a short progression, plus a
    // little noise - dense enough that no bin is trivially empty, which is
    // what real material looks like to the codec.
    AudioBuffer audio;
    audio.sampleRateHz = sampleRateHz;
    const auto count = static_cast<std::size_t>(seconds * sampleRateHz);
    audio.left.resize(count);
    audio.right.resize(count);
    constexpr double kRoots[] = {110.0, 146.83, 164.81, 130.81};
    std::uint32_t random = 2654435761u * (seed + 1);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / sampleRateHz;
        const double root = kRoots[static_cast<std::size_t>(t / 2.0) % 4];
        double left = 0.0;
        double right = 0.0;
        for (const double ratio : {1.0, 1.5, 2.0, 2.5, 4.0}) {
            for (int harmonic = 1; harmonic <= 4; ++harmonic) {
                const double phase = 2.0 * std::numbers::pi * root * ratio * harmonic * t;
                left += 0.04 / harmonic * std::sin(phase);
                right += 0.04 / harmonic * std::sin(phase + 0.3 * ratio);
            }
        }
        random = random * 1664525u + 1013904223u;
        const double noise = 0.02 * (static_cast<double>(random) / 4294967296.0 - 0.5);
        audio.left[i] = static_cast<float>(left + noise);
        audio.right[i] = static_cast<float>(right - noise);
    }
    return audio;
}

std::vector<BenchmarkCase> casesInCategories(std::vector<BenchmarkCase> cases,
                                             const std::vector<std::string>& categories) {
    if (categories.empty()) {
        return cases;
    }
    std::vector<BenchmarkCase> kept;
    for (BenchmarkCase& benchmarkCase : cases) {
        if (std::find(categories.begin(), categories.end(), benchmarkCase.category) != categories.end()) {
            kept.push_back(std::move(benchmarkCase));
        }
    }
    return kept;
}

std::vector<BenchmarkCase> buildCodecScenarios() {
    std::vector<BenchmarkCase> cases;
    const StreamCodecConfig config = defaultBenchmarkCodecConfig();
    for (const int seconds : kDurationsSeconds) {
        auto audio = std::make_shared<AudioBuffer>(musicLikeAudio(seconds, config.sampleRateHz, 0));
        auto image = std::make_shared<StreamImage>(sound_mind::codec::encode(*audio, config));

        nlohmann::json parameters;
        parameters["seconds"] = seconds;
        parameters["binCount"] = config.binCount;
        parameters["hopLength"] = config.hopLength;
        parameters["frames"] = image->frameCount;

        cases.push_back(BenchmarkCase{"encode,seconds=" + std::to_string(seconds), "codec", parameters,
                                       /*gpuEligible=*/false, [audio, config]() {
                                           const auto result = sound_mind::codec::encode(*audio, config);
                                           static_cast<void>(result);
                                       }});
        cases.push_back(BenchmarkCase{"decode,seconds=" + std::to_string(seconds), "codec", parameters,
                                       /*gpuEligible=*/false, [image]() {
                                           const auto result = sound_mind::codec::decode(*image);
                                           static_cast<void>(result);
                                       }});
    }
    return cases;
}

}  // namespace sound_mind::benchmark
