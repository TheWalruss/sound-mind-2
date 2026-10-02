#include "sound_mind/benchmark/scenarios.h"

#include <array>
#include <memory>
#include <random>
#include <string>

#include "sound_mind/core/filter_application.h"
#include "sound_mind/core/filter_configuration.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::benchmark {

namespace {

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyFilter;
using sound_mind::core::FilterConfiguration;
using sound_mind::core::FilterType;
using sound_mind::core::ProjectSettings;

/// @brief One named canvas size this suite sweeps across - bin count
/// (frequency/vertical resolution) and frame count (time/horizontal
/// resolution) independently, so a sweep can show whether cost scales
/// with total cell count alone or with one axis more than the other.
struct CanvasSize {
    const char* label;
    std::uint32_t binCount;
    std::uint32_t frameCount;
};

/// @brief The shared "typical project" size every baseline/kernel-size
/// case below runs at, so they're comparable to each other - only the
/// dedicated canvas-size sweep actually varies this.
constexpr CanvasSize kMediumCanvas{"medium", 512, 2000};

/// @brief Canvas sizes the dedicated sweep below covers - small, the
/// shared baseline, large, and two deliberately extreme aspect ratios
/// (very wide/short, very narrow/tall), since a filter's own per-axis
/// algorithm (e.g. `DirectionalBlur`'s own angle) can behave very
/// differently at an extreme aspect ratio than at a square-ish one.
constexpr std::array<CanvasSize, 5> kCanvasSweep{{
    {"small", 128, 500},
    {"medium", 512, 2000},
    {"large", 1024, 8000},
    {"wide", 128, 16000},
    {"tall", 2048, 500},
}};

StreamCodecConfig codecConfigFor(const CanvasSize& size) {
    StreamCodecConfig config;
    config.binCount = size.binCount;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

ProjectSettings projectSettingsFor(const CanvasSize& size) {
    ProjectSettings settings;
    settings.binCount = size.binCount;
    settings.minFrequencyHz = 20.0f;
    settings.maxFrequencyHz = 20000.0f;
    settings.canvasWidth = size.frameCount;
    settings.canvasHeight = size.binCount;
    return settings;
}

/// @brief A deterministic pseudo-random `StreamImage` of `size` - not
/// all-zero/all-identical content, so a filter whose own cost or behavior
/// depends on the data itself (`Denoise`'s threshold, `SpeckleRemove`'s
/// outlier detection, ...) exercises real per-cell work instead of a
/// degenerate uniform-input shortcut.
StreamImage randomContent(const CanvasSize& size) {
    StreamImage image;
    image.config = codecConfigFor(size);
    image.frameCount = size.frameCount;
    const auto cellCount = static_cast<std::size_t>(size.binCount) * size.frameCount;
    image.leftMagnitudeDb.resize(cellCount);
    image.rightMagnitudeDb.resize(cellCount);
    image.sharedPhaseRadians.assign(cellCount, 0.0f);

    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> db(-60.0f, 0.0f);
    std::uniform_real_distribution<float> phase(-3.14159f, 3.14159f);
    for (std::size_t i = 0; i < cellCount; ++i) {
        image.leftMagnitudeDb[i] = db(rng);
        image.rightMagnitudeDb[i] = db(rng);
        image.sharedPhaseRadians[i] = phase(rng);
    }
    return image;
}

/// @brief A plain, normalized N x N averaging kernel - enough to exercise
/// `Convolve`'s own cost at a given kernel size without needing a
/// meaningful visual result (this suite only measures time, never output
/// correctness - see `BenchmarkCase::run`'s own docs).
std::vector<float> averagingKernel(int size) {
    const auto cellCount = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
    return std::vector<float>(cellCount, 1.0f / static_cast<float>(cellCount));
}

BenchmarkCase makeFilterCase(std::string name, FilterType type, const CanvasSize& canvas,
                              const std::function<void(FilterConfiguration&)>& configure, bool gpuEligible) {
    auto content = std::make_shared<StreamImage>(randomContent(canvas));
    auto settings = std::make_shared<ProjectSettings>(projectSettingsFor(canvas));
    auto config = std::make_shared<FilterConfiguration>();
    config->setType(type);
    configure(*config);

    nlohmann::json parameters;
    parameters["canvas"] = canvas.label;
    parameters["binCount"] = canvas.binCount;
    parameters["frameCount"] = canvas.frameCount;

    return BenchmarkCase{std::move(name), "filter", std::move(parameters), gpuEligible,
                          [content, settings, config]() {
                              const auto result = applyFilter(*content, *config, *settings);
                              static_cast<void>(result);
                          }};
}

/// @brief Every `FilterType`, paired with a representative "just needs a
/// non-default scalar to do real work" configuration lambda - most types
/// need nothing beyond `setType()` itself (their own natural defaults
/// already produce real per-cell work); a few (`FrequencyAxisGradient`'s
/// own fully-transparent default gradient, `Convolve`'s own identity-like
/// default kernel) would otherwise measure a near-no-op, so those get a
/// small explicit nudge instead.
void appendOneBaselineCasePerFilterType(std::vector<BenchmarkCase>& cases) {
    constexpr std::array<FilterType, 21> kEveryFilterType{{
        FilterType::UniformBlur,
        FilterType::EdgePreservingBlur,
        FilterType::DirectionalBlur,
        FilterType::Sharpen,
        FilterType::ToneCurve,
        FilterType::FrequencyAxisGradient,
        FilterType::SpeckleAdd,
        FilterType::SpeckleRemove,
        FilterType::Denoise,
        FilterType::BitDepthCrush,
        FilterType::GranularNoise,
        FilterType::DynamicSpeckle,
        FilterType::FeedbackDistortion,
        FilterType::SpectralWavefold,
        FilterType::ChannelBalance,
        FilterType::Invert,
        FilterType::Convolve,
        FilterType::Displace,
        FilterType::ChannelCycle,
        FilterType::SpectralReverb,
        FilterType::Downsample,
    }};

    for (const FilterType type : kEveryFilterType) {
        const bool isUniformBlur = type == FilterType::UniformBlur;
        cases.push_back(makeFilterCase(
            "baseline", type, kMediumCanvas,
            [type](FilterConfiguration& config) {
                if (type == FilterType::FrequencyAxisGradient) {
                    // A straight ramp, fully written at both ends - the
                    // default gradient is fully transparent (no-op),
                    // which would measure nothing real.
                    auto& gradient = config.frequencyGradient();
                    sound_mind::core::GradientStop start;
                    start.t = 0.0f;
                    start.leftIntensity = 0.0f;
                    start.rightIntensity = 0.0f;
                    start.leftOpacity = 1.0f;
                    start.rightOpacity = 1.0f;
                    gradient.setStopValues(0, start);
                    sound_mind::core::GradientStop end;
                    end.t = 1.0f;
                    end.leftIntensity = 1.0f;
                    end.rightIntensity = 1.0f;
                    end.leftOpacity = 1.0f;
                    end.rightOpacity = 1.0f;
                    gradient.setStopValues(1, end);
                } else if (type == FilterType::Convolve) {
                    config.setConvolveKernelSize(5);
                    config.setConvolveKernel(averagingKernel(5));
                    config.setConvolveAmount(1.0f);
                }
            },
            isUniformBlur));
    }
}

void appendKernelSizeSweeps(std::vector<BenchmarkCase>& cases) {
    for (const int sigma : {1, 8, 32}) {
        cases.push_back(makeFilterCase("UniformBlur sigma=" + std::to_string(sigma), FilterType::UniformBlur,
                                        kMediumCanvas,
                                        [sigma](FilterConfiguration& config) {
                                            config.setBlurSigma(static_cast<float>(sigma));
                                        },
                                        /*gpuEligible=*/true));
    }
    // medianSize=9 already takes ~3.3s (mean of 5) at the medium canvas size;
    // the filter's cost scales pathologically with window size (O(size^2)
    // per cell for a windowed median), so this sweep's own upper bound is
    // deliberately kept well below what a naive doubling would suggest -
    // medianSize=25 was clocked at 20+ minutes for its 5 sample runs and is
    // not needed to make the point that large median windows are expensive.
    for (const int size : {3, 7, 11}) {
        cases.push_back(makeFilterCase("EdgePreservingBlur medianSize=" + std::to_string(size),
                                        FilterType::EdgePreservingBlur, kMediumCanvas,
                                        [size](FilterConfiguration& config) { config.setMedianSize(size); }, false));
    }
    for (const int length : {5, 15, 35}) {
        cases.push_back(makeFilterCase("DirectionalBlur length=" + std::to_string(length),
                                        FilterType::DirectionalBlur, kMediumCanvas,
                                        [length](FilterConfiguration& config) {
                                            config.setDirectionalBlurLength(length);
                                        },
                                        false));
    }
    // Shares EdgePreservingBlur's O(size^2)-per-cell cost profile (general
    // 2D convolution over a size*size kernel) - kept to the same reduced
    // upper bound rather than risking the same multi-minute stall.
    for (const int size : {3, 7, 11}) {
        cases.push_back(makeFilterCase(
            "Convolve kernelSize=" + std::to_string(size), FilterType::Convolve, kMediumCanvas,
            [size](FilterConfiguration& config) {
                config.setConvolveKernelSize(size);
                config.setConvolveKernel(averagingKernel(size));
                config.setConvolveAmount(1.0f);
            },
            false));
    }
    for (const int size : {2, 8, 32}) {
        cases.push_back(makeFilterCase("Downsample blockSize=" + std::to_string(size), FilterType::Downsample,
                                        kMediumCanvas,
                                        [size](FilterConfiguration& config) { config.setDownsampleBlockSize(size); },
                                        false));
    }
}

void appendCanvasSizeSweep(std::vector<BenchmarkCase>& cases) {
    for (const CanvasSize& canvas : kCanvasSweep) {
        cases.push_back(makeFilterCase("UniformBlur", FilterType::UniformBlur, canvas,
                                        [](FilterConfiguration& config) { config.setBlurSigma(8.0f); },
                                        /*gpuEligible=*/true));
    }
}

}  // namespace

std::vector<BenchmarkCase> buildFilterScenarios() {
    std::vector<BenchmarkCase> cases;
    appendOneBaselineCasePerFilterType(cases);
    appendKernelSizeSweeps(cases);
    appendCanvasSizeSweep(cases);
    return cases;
}

}  // namespace sound_mind::benchmark
