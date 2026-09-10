#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/fill_application.h"
#include "sound_mind/core/paint_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applyFillOperation;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::FillOperation;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::Gradient;
using sound_mind::core::GradientStop;
using sound_mind::core::LayerId;
using sound_mind::core::TimeFrequencyRect;

namespace {

StreamCodecConfig makeTestConfig() {
    StreamCodecConfig config;
    config.sampleRateHz = 44100;
    config.hopLength = 441;  // ~10ms/frame.
    config.binCount = 100;
    config.minFrequencyHz = 20.0f;
    config.maxFrequencyHz = 20000.0f;
    return config;
}

StreamImage makeBlankContent(const StreamCodecConfig& config, std::uint32_t frameCount) {
    StreamImage content;
    content.config = config;
    content.frameCount = frameCount;
    content.leftMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.rightMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    content.sharedPhaseRadians.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    return content;
}

/// @brief A uniform (same value at both stops) opaque gradient.
Gradient makeUniformGradient(float intensity, float opacity) {
    Gradient gradient;
    GradientStop stop;
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = opacity;
    stop.rightOpacity = opacity;
    gradient.setStopValues(0, stop);
    gradient.setStopValues(1, stop);
    return gradient;
}

}  // namespace

TEST_CASE("applyFillOperation writes across every cell inside bounds, leaves cells outside it unchanged",
          "[core][fill_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(20.0, config);
    bounds.endTimeSeconds = frameIndexToTime(40.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(30.0f, config);
    const FillOperation op(1, LayerId{1}, bounds, makeUniformGradient(-10.0f, 1.0f));

    applyFillOperation(op, content);

    // Inside bounds: filled.
    const std::size_t insideIndex = std::size_t{20} * content.frameCount + 30;
    REQUIRE(content.leftMagnitudeDb[insideIndex] == Catch::Approx(-10.0f));
    REQUIRE(content.rightMagnitudeDb[insideIndex] == Catch::Approx(-10.0f));

    // Outside bounds: untouched (still the blank 0.0f it started as).
    const std::size_t outsideIndex = std::size_t{50} * content.frameCount + 60;
    REQUIRE(content.leftMagnitudeDb[outsideIndex] == 0.0f);
    REQUIRE(content.rightMagnitudeDb[outsideIndex] == 0.0f);
}

TEST_CASE("applyFillOperation's gradient runs left-to-right across the selection's own time axis",
          "[core][fill_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(10.0, config);
    bounds.endTimeSeconds = frameIndexToTime(30.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(0.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(99.0f, config);

    Gradient gradient;
    GradientStop startStop = gradient.stops().front();
    startStop.leftIntensity = 0.0f;
    startStop.leftOpacity = 1.0f;
    gradient.setStopValues(0, startStop);
    GradientStop endStop = gradient.stops().back();
    endStop.leftIntensity = -80.0f;
    endStop.leftOpacity = 1.0f;
    gradient.setStopValues(1, endStop);
    const FillOperation op(1, LayerId{1}, bounds, gradient);

    applyFillOperation(op, content);

    const std::size_t leftEdgeIndex = std::size_t{50} * content.frameCount + 10;
    const std::size_t rightEdgeIndex = std::size_t{50} * content.frameCount + 30;
    REQUIRE(content.leftMagnitudeDb[leftEdgeIndex] == Catch::Approx(0.0f).margin(0.01));
    REQUIRE(content.leftMagnitudeDb[rightEdgeIndex] == Catch::Approx(-80.0f).margin(0.01));
}

TEST_CASE("applyFillOperation with zero opacity leaves content unchanged", "[core][fill_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);

    TimeFrequencyRect bounds;
    bounds.startTimeSeconds = frameIndexToTime(20.0, config);
    bounds.endTimeSeconds = frameIndexToTime(40.0, config);
    bounds.lowFrequencyHz = binIndexToFrequency(10.0f, config);
    bounds.highFrequencyHz = binIndexToFrequency(30.0f, config);
    const FillOperation op(1, LayerId{1}, bounds, makeUniformGradient(-10.0f, 0.0f));

    applyFillOperation(op, content);

    const std::size_t insideIndex = std::size_t{20} * content.frameCount + 30;
    REQUIRE(content.leftMagnitudeDb[insideIndex] == 0.0f);
    REQUIRE(content.rightMagnitudeDb[insideIndex] == 0.0f);
}

TEST_CASE("applyFillOperation does nothing for a degenerate (zero-sized) content buffer", "[core][fill_application]") {
    StreamImage content;
    content.config = makeTestConfig();
    content.frameCount = 0;

    const FillOperation op(1, LayerId{1}, TimeFrequencyRect{}, makeUniformGradient(-10.0f, 1.0f));

    applyFillOperation(op, content);  // must not crash.

    REQUIRE(content.leftMagnitudeDb.empty());
}
