#include "sound_mind/core/loudness_analysis.h"

#include <algorithm>
#include <cmath>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief The smallest linear amplitude this file's own dB conversions
/// will ever report as anything but this floor's own dB value - the same
/// value/reasoning `compositor.cpp`'s own private copy already uses;
/// duplicated rather than shared, matching this codebase's own existing
/// "not worth promoting a two-line formula to a shared header" precedent.
constexpr float kMinLinearAmplitude = 1e-7f;

[[nodiscard]] float dbToLinearAmplitude(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

[[nodiscard]] float linearAmplitudeToDb(float amplitude) noexcept {
    return 20.0f * std::log10(std::max(amplitude, kMinLinearAmplitude));
}

}  // namespace

std::vector<float> computeLoudnessProfile(const sound_mind::codec::StreamImage& content) {
    if (content.frameCount == 0 || content.config.binCount == 0) {
        return {};
    }

    std::vector<float> profile(content.frameCount);
    for (std::uint32_t frame = 0; frame < content.frameCount; ++frame) {
        double leftLinearSum = 0.0;
        double rightLinearSum = 0.0;
        for (std::uint32_t bin = 0; bin < content.config.binCount; ++bin) {
            const std::size_t index = cellIndex(bin, frame, content.frameCount);
            leftLinearSum += dbToLinearAmplitude(content.leftMagnitudeDb[index]);
            rightLinearSum += dbToLinearAmplitude(content.rightMagnitudeDb[index]);
        }
        const float leftAverage = static_cast<float>(leftLinearSum / content.config.binCount);
        const float rightAverage = static_cast<float>(rightLinearSum / content.config.binCount);
        // The two channels' own combined loudness for this column -
        // averaged in linear space together with the per-bin average
        // above, one single pass rather than two.
        profile[frame] = linearAmplitudeToDb((leftAverage + rightAverage) / 2.0f);
    }
    return profile;
}

float averageLoudnessDb(const std::vector<float>& profile) noexcept {
    if (profile.empty()) {
        return linearAmplitudeToDb(0.0f);
    }
    double linearSum = 0.0;
    for (const float value : profile) {
        linearSum += dbToLinearAmplitude(value);
    }
    return linearAmplitudeToDb(static_cast<float>(linearSum / profile.size()));
}

float peakLoudnessDb(const std::vector<float>& profile) noexcept {
    if (profile.empty()) {
        return linearAmplitudeToDb(0.0f);
    }
    return *std::max_element(profile.begin(), profile.end());
}

}  // namespace sound_mind::core
