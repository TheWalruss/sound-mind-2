#include <cmath>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/wand_selection.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::codec::StreamImage;
using sound_mind::core::binIndexToFrequency;
using sound_mind::core::frameIndexToTime;
using sound_mind::core::frequencyToBinIndex;
using sound_mind::core::selectByAmplitudeSimilarity;
using sound_mind::core::SelectionRegionKind;
using sound_mind::core::TimeFrequencyPoint;

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

StreamImage makeUniformContent(const StreamCodecConfig& config, std::uint32_t frameCount, float baseDb) {
    StreamImage content;
    content.config = config;
    content.frameCount = frameCount;
    content.leftMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, baseDb);
    content.rightMagnitudeDb.assign(std::size_t{config.binCount} * frameCount, baseDb);
    content.sharedPhaseRadians.assign(std::size_t{config.binCount} * frameCount, 0.0f);
    return content;
}

void setCell(StreamImage& content, int bin, int frame, float db) {
    const std::size_t index = static_cast<std::size_t>(bin) * content.frameCount + static_cast<std::size_t>(frame);
    content.leftMagnitudeDb[index] = db;
    content.rightMagnitudeDb[index] = db;
}

TimeFrequencyPoint pointAt(const StreamCodecConfig& config, int bin, int frame) {
    return TimeFrequencyPoint{frameIndexToTime(static_cast<double>(frame), config),
                               static_cast<double>(binIndexToFrequency(static_cast<float>(bin), config))};
}

}  // namespace

TEST_CASE("selectByAmplitudeSimilarity selects a contiguous blob of similar-amplitude cells",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 30, -50.0f);
    // A 3x3 blob at frames 5-7, bins 5-7, all at -10dB - well outside the
    // background's own -50dB.
    for (int bin = 5; bin <= 7; ++bin) {
        for (int frame = 5; frame <= 7; ++frame) {
            setCell(content, bin, frame, -10.0f);
        }
    }

    const auto region = selectByAmplitudeSimilarity(content, pointAt(config, 6, 6), /*tolerancePercent=*/5.0, false);

    REQUIRE(region.kind() == SelectionRegionKind::Mask);
    for (int bin = 5; bin <= 7; ++bin) {
        for (int frame = 5; frame <= 7; ++frame) {
            REQUIRE(region.containsCell(bin, frame, config));
        }
    }
    // The background, just outside the blob, is not selected.
    REQUIRE_FALSE(region.containsCell(8, 8, config));
    REQUIRE_FALSE(region.containsCell(0, 0, config));
}

TEST_CASE("selectByAmplitudeSimilarity does not leap across a gap to a disconnected similar-amplitude island",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 30, -50.0f);
    setCell(content, 5, 5, -10.0f);    // The anchor's own single cell.
    setCell(content, 20, 20, -10.0f);  // A disconnected cell, same amplitude, far away.

    const auto region = selectByAmplitudeSimilarity(content, pointAt(config, 5, 5), /*tolerancePercent=*/5.0, false);

    REQUIRE(region.containsCell(5, 5, config));
    REQUIRE_FALSE(region.containsCell(20, 20, config));
}

TEST_CASE("selectByAmplitudeSimilarity with 0% tolerance only selects cells at exactly the anchor's own value",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 10, -50.0f);
    setCell(content, 5, 5, -10.0f);
    setCell(content, 5, 6, -10.5f);  // Adjacent, but not exactly the same.

    const auto region = selectByAmplitudeSimilarity(content, pointAt(config, 5, 5), /*tolerancePercent=*/0.0, false);

    REQUIRE(region.containsCell(5, 5, config));
    REQUIRE_FALSE(region.containsCell(5, 6, config));
}

TEST_CASE("selectByAmplitudeSimilarity with 100% tolerance selects every reachable cell regardless of amplitude",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 5, -50.0f);
    setCell(content, 2, 2, 0.0f);  // A wildly different value, still reachable.

    const auto region = selectByAmplitudeSimilarity(content, pointAt(config, 0, 0), /*tolerancePercent=*/100.0, false);

    REQUIRE(region.containsCell(0, 0, config));
    REQUIRE(region.containsCell(2, 2, config));
    REQUIRE(region.containsCell(static_cast<int>(config.binCount) - 1, 4, config));
}

TEST_CASE("selectByAmplitudeSimilarity is harmonics-aware when asked, unioning the fundamental with its overtones",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 10, -50.0f);

    const int fundamentalBin = 20;
    const float fundamentalHz = binIndexToFrequency(static_cast<float>(fundamentalBin), config);
    const int harmonicBin =
        static_cast<int>(std::lround(static_cast<double>(frequencyToBinIndex(fundamentalHz * 2.0f, config))));
    REQUIRE(harmonicBin != fundamentalBin);  // Sanity - the log scale actually moved it somewhere new.

    setCell(content, fundamentalBin, 3, -10.0f);
    setCell(content, harmonicBin, 3, -10.0f);

    const auto withoutHarmonics =
        selectByAmplitudeSimilarity(content, pointAt(config, fundamentalBin, 3), /*tolerancePercent=*/5.0, false);
    REQUIRE(withoutHarmonics.containsCell(fundamentalBin, 3, config));
    REQUIRE_FALSE(withoutHarmonics.containsCell(harmonicBin, 3, config));

    const auto withHarmonics =
        selectByAmplitudeSimilarity(content, pointAt(config, fundamentalBin, 3), /*tolerancePercent=*/5.0, true);
    REQUIRE(withHarmonics.containsCell(fundamentalBin, 3, config));
    REQUIRE(withHarmonics.containsCell(harmonicBin, 3, config));
}

TEST_CASE("selectByAmplitudeSimilarity skips a harmonic with no real signal (at or below the silence floor)",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    // Every cell (including every harmonic's own starting cell) sits at
    // the silence floor except the fundamental itself.
    StreamImage content = makeUniformContent(config, 10, -96.0f);
    setCell(content, 20, 3, -10.0f);

    const auto region =
        selectByAmplitudeSimilarity(content, pointAt(config, 20, 3), /*tolerancePercent=*/5.0, true);

    // Only the fundamental's own single cell - every harmonic's own
    // starting point was silent, so none contributed anything.
    std::size_t selectedCount = 0;
    for (int bin = 0; bin < static_cast<int>(config.binCount); ++bin) {
        for (int frame = 0; frame < 10; ++frame) {
            if (region.containsCell(bin, frame, config)) {
                ++selectedCount;
            }
        }
    }
    REQUIRE(selectedCount == 1);
}

TEST_CASE("selectByAmplitudeSimilarity returns an empty region for an anchor outside the content",
          "[core][wand_selection]") {
    const auto config = makeTestConfig();
    StreamImage content = makeUniformContent(config, 10, -50.0f);

    const auto region =
        selectByAmplitudeSimilarity(content, TimeFrequencyPoint{-100.0, 50.0}, /*tolerancePercent=*/5.0, false);

    REQUIRE_FALSE(region.containsCell(0, 0, config));
}

TEST_CASE("selectByAmplitudeSimilarity returns an empty region for a degenerate (zero-sized) content buffer",
          "[core][wand_selection]") {
    StreamImage content;
    content.config = makeTestConfig();
    content.frameCount = 0;

    const auto region = selectByAmplitudeSimilarity(content, TimeFrequencyPoint{0.0, 0.0}, 5.0, false);

    REQUIRE(region.kind() == SelectionRegionKind::Mask);
    REQUIRE(region.maskCells().empty());
}
