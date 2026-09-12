#include "sound_mind/core/filter_application.h"

namespace sound_mind::core {

namespace {

using sound_mind::codec::StreamImage;

/// @brief `applyFilter()`'s own `FrequencyAxisGradient` implementation -
/// see its docs for the exact per-bin blend.
StreamImage applyFrequencyAxisGradient(const StreamImage& composite, const Gradient& gradient) {
    StreamImage result = composite;
    const std::uint32_t binCount = composite.config.binCount;
    const std::uint32_t frameCount = composite.frameCount;
    if (binCount == 0 || frameCount == 0) {
        return result;
    }

    for (std::uint32_t bin = 0; bin < binCount; ++bin) {
        // binIndexToFrequency()'s own log-scaled bin-to-Hz mapping is
        // already linear in bin index, so a bin-index fraction *is*
        // "normalized position across the encoded frequency range" - no
        // separate Hz conversion needed (see applyFilter()'s own docs).
        const float t = (binCount > 1) ? static_cast<float>(bin) / static_cast<float>(binCount - 1) : 0.0f;
        const GradientStop stop = gradient.evaluate(t);

        for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
            const std::size_t cell = std::size_t{bin} * frameCount + frame;
            result.leftMagnitudeDb[cell] += (stop.leftIntensity - result.leftMagnitudeDb[cell]) * stop.leftOpacity;
            result.rightMagnitudeDb[cell] +=
                (stop.rightIntensity - result.rightMagnitudeDb[cell]) * stop.rightOpacity;
            // Phase is left untouched - the gradient only ever targets
            // amplitude, the same as Fill/Paint's own use of a Gradient.
        }
    }
    return result;
}

}  // namespace

StreamImage applyFilter(const StreamImage& composite, const FilterConfiguration& config,
                          const ProjectSettings& /*settings*/) {
    switch (config.type()) {
        case FilterType::FrequencyAxisGradient:
            return applyFrequencyAxisGradient(composite, config.frequencyGradient());
        case FilterType::UniformBlur:
        case FilterType::EdgePreservingBlur:
        case FilterType::DirectionalBlur:
        case FilterType::Sharpen:
        case FilterType::ToneCurve:
            // Not implemented yet - see this function's own docs. A
            // harmless passthrough, not a silent wrong answer, until
            // each lands in a later installment of this same milestone.
            return composite;
    }
    return composite;
}

}  // namespace sound_mind::core
