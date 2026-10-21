#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/remaster.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::Clip;
using sound_mind::core::ProjectSettings;
using sound_mind::core::TimeFrequencyRect;

namespace {

Clip clipWithMarkedCell(std::uint32_t frames, std::uint32_t bins, std::uint32_t markFrame, std::uint32_t markBin) {
    Clip clip;
    clip.frameCount = frames;
    clip.binCount = bins;
    const std::size_t cells = static_cast<std::size_t>(frames) * bins;
    clip.leftMagnitudeDb.assign(cells, -90.0f);
    clip.rightMagnitudeDb.assign(cells, -90.0f);
    clip.sharedPhaseRadians.assign(cells, 0.0f);
    clip.leftMagnitudeDb[sound_mind::core::cellIndex(markBin, markFrame, frames)] = -6.0f;
    return clip;
}

}  // namespace

TEST_CASE("remasterSettings keeps the duration when the time step changes", "[core][remaster]") {
    ProjectSettings original;
    original.canvasWidth = 1000;
    original.timestepMs = 10.0;
    ProjectSettings wanted = original;
    wanted.timestepMs = 5.0;
    wanted.canvasWidth = 1;  // ignored.
    const ProjectSettings settings = sound_mind::core::remasterSettings(original, wanted);
    REQUIRE(settings.canvasWidth == 2000);
    REQUIRE(settings.timestepMs == 5.0);
}

TEST_CASE("resampleClip leaves a clip alone on the same grid", "[core][remaster]") {
    const StreamCodecConfig config;
    const Clip clip = clipWithMarkedCell(10, 8, 3, 2);
    const Clip same = sound_mind::core::resampleClip(clip, TimeFrequencyRect{1.0, 1.1, 300.0, 600.0}, config, config);
    REQUIRE(same.leftMagnitudeDb == clip.leftMagnitudeDb);
}

TEST_CASE("resampleClip keeps a cell at the same time and frequency on a finer grid", "[core][remaster]") {
    StreamCodecConfig from;
    StreamCodecConfig to = from;
    to.hopLength = from.hopLength / 2;  // twice the time resolution.
    to.binCount = from.binCount * 2;    // twice the frequency resolution.
    const TimeFrequencyRect placement{1.0, 1.2, 300.0, 3000.0};
    const auto origin = [&](const StreamCodecConfig& c) {
        return std::lround(std::min(sound_mind::core::frequencyToBinIndex(300.0f, c),
                                    sound_mind::core::frequencyToBinIndex(3000.0f, c)));
    };
    const std::uint32_t oldBins =
        static_cast<std::uint32_t>(std::lround(sound_mind::core::frequencyToBinIndex(3000.0f, from)) - origin(from) + 1);
    const Clip clip = clipWithMarkedCell(20, oldBins, 6, oldBins / 2);
    const Clip finer = sound_mind::core::resampleClip(clip, placement, from, to);

    REQUIRE(finer.frameCount == 40);
    REQUIRE(finer.binCount > clip.binCount);
    // The marked cell's time and frequency, then where that lands now.
    const double markSeconds =
        sound_mind::core::frameIndexToTime(std::lround(sound_mind::core::timeToFrameIndex(1.0, from)) + 6, from);
    const float markHz = sound_mind::core::binIndexToFrequency(static_cast<float>(origin(from) + oldBins / 2), from);
    const long newFrame = std::lround(sound_mind::core::timeToFrameIndex(markSeconds, to)) -
                          std::lround(sound_mind::core::timeToFrameIndex(1.0, to));
    const long newBin = std::lround(sound_mind::core::frequencyToBinIndex(markHz, to)) - origin(to);
    REQUIRE(finer.leftMagnitudeDb[sound_mind::core::cellIndex(static_cast<std::uint32_t>(newBin),
                                                              static_cast<std::uint32_t>(newFrame), finer.frameCount)] ==
            -6.0f);
}

TEST_CASE("resampleClipProportionally scales a Mind Shot by the grid ratios", "[core][remaster]") {
    StreamCodecConfig from;
    StreamCodecConfig to = from;
    to.hopLength = from.hopLength * 2;
    const Clip clip = clipWithMarkedCell(10, 8, 4, 3);
    const Clip coarser = sound_mind::core::resampleClipProportionally(clip, from, to);
    REQUIRE(coarser.frameCount == 5);
    REQUIRE(coarser.binCount == 8);
    REQUIRE(coarser.leftMagnitudeDb[sound_mind::core::cellIndex(3u, 2u, 5u)] == -6.0f);
}

TEST_CASE("resampleSelectionJson remaps a mask and leaves a path boundary alone", "[core][remaster]") {
    StreamCodecConfig from;
    StreamCodecConfig to = from;
    to.hopLength = from.hopLength / 2;
    const nlohmann::json mask{{"kind", "mask"},   {"frameLow", 10}, {"frameHigh", 11},
                              {"binLow", 100},    {"binHigh", 100}, {"mask", std::vector<bool>{true, false}}};
    const nlohmann::json resampled = sound_mind::core::resampleSelectionJson(mask, from, to);
    REQUIRE(resampled.at("frameLow") == 20);
    REQUIRE(resampled.at("frameHigh") == 22);
    const auto cells = resampled.at("mask").get<std::vector<bool>>();
    REQUIRE(cells.size() == 3);
    REQUIRE(cells[0]);
    REQUIRE_FALSE(cells[2]);

    const nlohmann::json path{{"kind", "path"}, {"path", nlohmann::json::object()}};
    REQUIRE(sound_mind::core::resampleSelectionJson(path, from, to) == path);
}

TEST_CASE("adaptEntryForRemaster converts layer offsets, pasted clips and Mind Shots", "[core][remaster]") {
    ProjectSettings from;
    ProjectSettings to = from;
    to.timestepMs = from.timestepMs / 2.0;

    sound_mind::core::HistoryEntry entry;
    sound_mind::core::HistoryLayerChange change;
    change.id = 3;
    change.after = {{"translationColumns", 7}};
    entry.layers.push_back(change);
    const Clip clip = clipWithMarkedCell(4, 2, 1, 1);
    entry.appendedOperations.push_back(
        {{"kind", "paste"}, {"clip", clip}, {"placement", TimeFrequencyRect{0.5, 0.54, 400.0, 420.0}}});
    entry.resources.push_back({"mindShots", nlohmann::json::array(),
                               nlohmann::json::array({{{"id", 1}, {"name", "S"}, {"clip", clip}}})});

    sound_mind::core::adaptEntryForRemaster(entry, from, to);
    REQUIRE(entry.layers.front().after.at("translationColumns") == 14);
    REQUIRE(entry.appendedOperations.front().at("clip").at("frameCount") == 8);
    REQUIRE(entry.resources.front().after.at(0).at("clip").at("frameCount") == 8);
}
