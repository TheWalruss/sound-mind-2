#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/selection_region.h"

using sound_mind::codec::StreamCodecConfig;
using sound_mind::core::FrameBinRange;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::SelectionRegion;
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

PathNode cornerNodeAt(double timeSeconds, double frequencyHz) {
    PathNode node;
    node.anchor = TimeFrequencyPoint{timeSeconds, frequencyHz};
    node.type = PathNodeType::Corner;
    return node;
}

/// @brief A plain axis-aligned square boundary, matching test_path.cpp's
/// own makeSquarePath() helper - abstract coordinates, never converted
/// through a real StreamCodecConfig, so its own [0, 10] frequency span
/// need not fall within any particular config's own representable range.
Path makeSquarePath() {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    path.addNode(cornerNodeAt(10.0, 0.0));
    path.addNode(cornerNodeAt(10.0, 10.0));
    path.addNode(cornerNodeAt(0.0, 10.0));
    return path;
}

/// @brief A square boundary spanning a frequency range this test file's
/// own makeTestConfig() can actually represent (`[50, 5000]` Hz, safely
/// inside its own `[20, 20000]`) - for containsCell() tests, which convert
/// a cell back to a *real* time/frequency point first (unlike contains(),
/// which tests a given point directly).
Path makeRealisticSquarePath() {
    Path path;
    path.addNode(cornerNodeAt(0.0, 50.0));
    path.addNode(cornerNodeAt(10.0, 50.0));
    path.addNode(cornerNodeAt(10.0, 5000.0));
    path.addNode(cornerNodeAt(0.0, 5000.0));
    return path;
}

/// @brief A 3x3 mask (frames 10-12, bins 5-7) with only the center cell
/// (frame 11, bin 6) selected.
SelectionRegion makeSingleCellMask() {
    std::vector<bool> mask(9, false);
    mask[static_cast<std::size_t>(1) * 3 + 1] = true;  // (bin=6, frame=11) - offset (1, 1).
    return SelectionRegion(10, 12, 5, 7, std::move(mask));
}

}  // namespace

TEST_CASE("A default-constructed SelectionRegion is Path-kind with an empty (degenerate) path",
          "[core][selection_region]") {
    const SelectionRegion region;
    REQUIRE(region.kind() == SelectionRegionKind::Path);
    REQUIRE(region.path().nodes().empty());
}

TEST_CASE("A Path-kind SelectionRegion reports its own kind and wraps the given path",
          "[core][selection_region]") {
    const SelectionRegion region(makeSquarePath());
    REQUIRE(region.kind() == SelectionRegionKind::Path);
    REQUIRE(region.path().nodes().size() == 4);
}

TEST_CASE("A Path-kind SelectionRegion's contains() delegates directly to containsPoint()",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region(makeSquarePath());

    REQUIRE(region.contains(TimeFrequencyPoint{5.0, 5.0}, config));
    REQUIRE_FALSE(region.contains(TimeFrequencyPoint{50.0, 50.0}, config));
}

TEST_CASE("A Path-kind SelectionRegion's containsCell() converts the cell to a real point first",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region(makeRealisticSquarePath());

    // frame 5, bin 40 - both comfortably inside the square's own [0, 10]
    // second / [50, 5000] Hz span at this config's own frame/bin scale.
    const int frame = static_cast<int>(sound_mind::core::timeToFrameIndex(5.0, config));
    const int bin = 40;
    REQUIRE(region.containsCell(bin, frame, config));

    // frame 5, bin 99 (this config's own highest bin, ~20000 Hz) - well
    // outside the square's own frequency span.
    REQUIRE_FALSE(region.containsCell(99, frame, config));
}

TEST_CASE("A Mask-kind SelectionRegion's containsCell() is a direct lookup within its own captured range",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region = makeSingleCellMask();

    REQUIRE(region.kind() == SelectionRegionKind::Mask);
    REQUIRE(region.containsCell(6, 11, config));       // The one selected cell.
    REQUIRE_FALSE(region.containsCell(5, 11, config));  // Adjacent, but not selected.
    REQUIRE_FALSE(region.containsCell(6, 10, config));  // Adjacent, but not selected.
    REQUIRE_FALSE(region.containsCell(6, 50, config));  // Outside the captured range entirely.
}

TEST_CASE("A Mask-kind SelectionRegion's contains() rounds a continuous point to its nearest cell",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region = makeSingleCellMask();

    const TimeFrequencyPoint centerPoint{sound_mind::core::frameIndexToTime(11.0, config),
                                          static_cast<double>(sound_mind::core::binIndexToFrequency(6.0f, config))};
    REQUIRE(region.contains(centerPoint, config));

    const TimeFrequencyPoint farPoint{sound_mind::core::frameIndexToTime(90.0, config),
                                       static_cast<double>(sound_mind::core::binIndexToFrequency(90.0f, config))};
    REQUIRE_FALSE(region.contains(farPoint, config));
}

TEST_CASE("SelectionRegion::translated() shifts a Path-kind region via Path::translated()",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region(makeSquarePath());

    const SelectionRegion translated = region.translated(0.1, 5.0, config);

    REQUIRE(translated.kind() == SelectionRegionKind::Path);
    REQUIRE(translated.path().nodes()[0].anchor.timeSeconds == 0.1);
    // Original untouched.
    REQUIRE(region.path().nodes()[0].anchor.timeSeconds == 0.0);
}

TEST_CASE("SelectionRegion::translated() shifts a Mask-kind region's own captured range by rounded frame/bin deltas",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion region = makeSingleCellMask();

    // 10 frames' worth of seconds, at this config's own ~10ms/frame.
    const double deltaTimeSeconds = 10.0 * config.hopLength / static_cast<double>(config.sampleRateHz);
    const SelectionRegion translated = region.translated(deltaTimeSeconds, 3.0, config);

    REQUIRE(translated.kind() == SelectionRegionKind::Mask);
    REQUIRE(translated.maskFrameLow() == 20);
    REQUIRE(translated.maskFrameHigh() == 22);
    REQUIRE(translated.maskBinLow() == 8);
    REQUIRE(translated.maskBinHigh() == 10);
    // The originally-selected cell (offset (1,1) within its own range)
    // moved along with the range - now at (bin=9, frame=21).
    REQUIRE(translated.containsCell(9, 21, config));
    REQUIRE_FALSE(translated.containsCell(8, 20, config));
}

TEST_CASE("SelectionRegion::combine() Add unions two masks", "[core][selection_region]") {
    const auto config = makeTestConfig();
    // a: cell (bin=5, frame=10) only, within a 1x1 range.
    const SelectionRegion a(10, 10, 5, 5, std::vector<bool>{true});
    // b: cell (bin=6, frame=11) only, within a 1x1 range.
    const SelectionRegion b(11, 11, 6, 6, std::vector<bool>{true});
    const FrameBinRange aRange{10, 10, 5, 5};
    const FrameBinRange bRange{11, 11, 6, 6};
    const FrameBinRange resultRange{10, 11, 5, 6};

    const SelectionRegion result =
        SelectionRegion::combine(a, aRange, b, bRange, resultRange, SelectionRegion::BooleanOp::Add, config);

    REQUIRE(result.kind() == SelectionRegionKind::Mask);
    REQUIRE(result.containsCell(5, 10, config));
    REQUIRE(result.containsCell(6, 11, config));
    REQUIRE_FALSE(result.containsCell(5, 11, config));
    REQUIRE_FALSE(result.containsCell(6, 10, config));
}

TEST_CASE("SelectionRegion::combine() Subtract removes the second operand's own cells from the first",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    // a: a 2x1 strip, both cells selected.
    const SelectionRegion a(10, 11, 5, 5, std::vector<bool>{true, true});
    // b: just the second of those two cells.
    const SelectionRegion b(11, 11, 5, 5, std::vector<bool>{true});
    const FrameBinRange aRange{10, 11, 5, 5};
    const FrameBinRange bRange{11, 11, 5, 5};
    const FrameBinRange resultRange = aRange;  // Subtract never exceeds a's own extent.

    const SelectionRegion result =
        SelectionRegion::combine(a, aRange, b, bRange, resultRange, SelectionRegion::BooleanOp::Subtract, config);

    REQUIRE(result.containsCell(5, 10, config));
    REQUIRE_FALSE(result.containsCell(5, 11, config));
}

TEST_CASE("SelectionRegion::combine() Intersect keeps only cells both operands select",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const SelectionRegion a(10, 11, 5, 5, std::vector<bool>{true, true});
    const SelectionRegion b(11, 12, 5, 5, std::vector<bool>{true, true});
    const FrameBinRange aRange{10, 11, 5, 5};
    const FrameBinRange bRange{11, 12, 5, 5};
    const FrameBinRange resultRange = aRange;

    const SelectionRegion result =
        SelectionRegion::combine(a, aRange, b, bRange, resultRange, SelectionRegion::BooleanOp::Intersect, config);

    REQUIRE_FALSE(result.containsCell(5, 10, config));  // Only in a.
    REQUIRE(result.containsCell(5, 11, config));         // In both.
}

TEST_CASE("SelectionRegion::combine() treats an absent operand as a plain rectangle over its own given range",
          "[core][selection_region]") {
    const auto config = makeTestConfig();
    const FrameBinRange aRange{10, 12, 5, 7};  // "a" is a plain rectangle - no SelectionRegion at all.
    const SelectionRegion b = makeSingleCellMask();
    const FrameBinRange bRange{10, 12, 5, 7};
    const FrameBinRange resultRange = aRange;

    const SelectionRegion added = SelectionRegion::combine(std::nullopt, aRange, b, bRange, resultRange,
                                                            SelectionRegion::BooleanOp::Add, config);
    // Every cell in aRange is selected (it's a plain rectangle) - Add with
    // anything still covers the whole rectangle.
    REQUIRE(added.containsCell(5, 10, config));
    REQUIRE(added.containsCell(7, 12, config));

    const SelectionRegion subtracted = SelectionRegion::combine(std::nullopt, aRange, b, bRange, resultRange,
                                                                 SelectionRegion::BooleanOp::Subtract, config);
    // The single cell b selects is carved out of the rectangle.
    REQUIRE_FALSE(subtracted.containsCell(6, 11, config));
    REQUIRE(subtracted.containsCell(5, 10, config));
}

TEST_CASE("A Path-kind SelectionRegion round-trips through JSON", "[core][selection_region]") {
    const SelectionRegion region(makeSquarePath());

    const nlohmann::json json = region;
    REQUIRE(json.at("kind").get<std::string>() == "path");
    const SelectionRegion restored = json.get<SelectionRegion>();

    REQUIRE(restored.kind() == SelectionRegionKind::Path);
    REQUIRE(restored.path().nodes().size() == 4);
}

TEST_CASE("A Mask-kind SelectionRegion round-trips through JSON", "[core][selection_region]") {
    const SelectionRegion region = makeSingleCellMask();

    const nlohmann::json json = region;
    REQUIRE(json.at("kind").get<std::string>() == "mask");
    const SelectionRegion restored = json.get<SelectionRegion>();

    REQUIRE(restored.kind() == SelectionRegionKind::Mask);
    REQUIRE(restored.maskFrameLow() == 10);
    REQUIRE(restored.maskFrameHigh() == 12);
    REQUIRE(restored.maskBinLow() == 5);
    REQUIRE(restored.maskBinHigh() == 7);
    REQUIRE(restored.maskCells() == region.maskCells());
}

TEST_CASE("A SelectionRegion fails to load JSON with an unrecognized kind", "[core][selection_region]") {
    const nlohmann::json malformed = nlohmann::json{{"kind", "not-a-real-kind"}};
    REQUIRE_THROWS_AS(malformed.get<SelectionRegion>(), std::invalid_argument);
}
