#include <cstddef>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/sequence_application.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::applySequenceOperation;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::GradientStop;
using sound_mind::core::LayerId;
using sound_mind::core::NoteEvent;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::SequenceOperation;
using sound_mind::core::timeToFrameIndex;
using sound_mind::core::ToolConfiguration;

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

std::size_t pixelIndex(const StreamImage& content, int frame, int bin) {
    return static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
}

/// @brief A `ProceduralConfiguration` with a small, sharp (no falloff)
/// circular tip and a fully opaque, uniform-intensity default gradient -
/// every note in a sequence plays through this same shared configuration.
std::unique_ptr<ProceduralConfiguration> makeOpaqueTool(double size = 0.02, float intensity = -10.0f) {
    auto config = std::make_unique<ProceduralConfiguration>();
    config->setSize(size);
    config->setFalloff(0.0f);
    GradientStop stop = config->defaultGradient().stops().front();
    stop.leftIntensity = intensity;
    stop.rightIntensity = intensity;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    config->defaultGradient().setStopValues(0, stop);
    config->defaultGradient().setStopValues(1, stop);
    return config;
}

}  // namespace

TEST_CASE("applySequenceOperation stamps a zero-duration note as a single tap at its own start time and frequency",
          "[core][sequence_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const std::vector<NoteEvent> notes{NoteEvent{0.3, 0.0, 1000.0}};
    const SequenceOperation op(1, LayerId{1}, notes, makeOpaqueTool());

    applySequenceOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int bin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, bin)] == -10.0f);
}

TEST_CASE("applySequenceOperation stamps a held note as a horizontal band spanning its own duration",
          "[core][sequence_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const std::vector<NoteEvent> notes{NoteEvent{0.1, 0.3, 1000.0}};  // Held from 0.1s to 0.4s.
    const SequenceOperation op(1, LayerId{1}, notes, makeOpaqueTool());

    applySequenceOperation(op, 2000.0, content);

    const int bin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    const int startFrame = static_cast<int>(std::lround(timeToFrameIndex(0.1, config)));
    const int midFrame = static_cast<int>(std::lround(timeToFrameIndex(0.25, config)));
    const int endFrame = static_cast<int>(std::lround(timeToFrameIndex(0.4, config)));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, startFrame, bin)] == -10.0f);
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, midFrame, bin)] == -10.0f);
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, endFrame, bin)] == -10.0f);

    // Well before/after the held span, nothing is painted.
    const int beforeFrame = static_cast<int>(std::lround(timeToFrameIndex(0.0, config)));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, beforeFrame, bin)] == 0.0f);
}

TEST_CASE("applySequenceOperation stamps every note in a block chord (same start time, different pitches)",
          "[core][sequence_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const std::vector<NoteEvent> notes{
        NoteEvent{0.3, 0.0, 300.0},
        NoteEvent{0.3, 0.0, 500.0},
        NoteEvent{0.3, 0.0, 900.0},
    };
    const SequenceOperation op(1, LayerId{1}, notes, makeOpaqueTool());

    applySequenceOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    for (const double freq : {300.0, 500.0, 900.0}) {
        const int bin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(
            static_cast<float>(freq), config))));
        REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, bin)] == -10.0f);
    }
}

TEST_CASE("applySequenceOperation with no notes paints nothing", "[core][sequence_application]") {
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    const SequenceOperation op(1, LayerId{1}, {}, makeOpaqueTool());

    applySequenceOperation(op, 2000.0, content);

    for (const float value : content.leftMagnitudeDb) {
        REQUIRE(value == 0.0f);
    }
}

TEST_CASE("applySequenceOperation renders through whichever tool type its own config actually is",
          "[core][sequence_application]") {
    // Reuses InstrumentConfiguration - a distinct rendering path
    // (harmonic-series spikes, no frequency-axis falloff) from
    // ProceduralConfiguration's own soft geometric blob, confirming this
    // function has no per-tool-type logic of its own and genuinely
    // dispatches through applyPaintOperation().
    const auto config = makeTestConfig();
    StreamImage content = makeBlankContent(config, 100);
    auto instrument = std::make_unique<sound_mind::core::InstrumentConfiguration>();
    instrument->setHarmonicStrengths({1.0});
    instrument->setSize(0.02);
    instrument->setFalloff(0.0f);
    GradientStop stop = instrument->defaultGradient().stops().front();
    stop.leftIntensity = -10.0f;
    stop.rightIntensity = -10.0f;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    instrument->defaultGradient().setStopValues(0, stop);
    instrument->defaultGradient().setStopValues(1, stop);
    const std::vector<NoteEvent> notes{NoteEvent{0.3, 0.0, 1000.0}};
    const SequenceOperation op(1, LayerId{1}, notes, std::move(instrument));

    applySequenceOperation(op, 2000.0, content);

    const int centerFrame = static_cast<int>(std::lround(timeToFrameIndex(0.3, config)));
    const int bin = static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(1000.0f, config))));
    REQUIRE(content.leftMagnitudeDb[pixelIndex(content, centerFrame, bin)] == -10.0f);
}
