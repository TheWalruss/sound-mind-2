#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/path.h"
#include "sound_mind/core/resonant_instrument.h"

using sound_mind::core::computeWaveKernelSignature;
using sound_mind::core::CurveGraph;
using sound_mind::core::curveGraphFromPath;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;

namespace {

PathNode cornerNodeAt(double timeSeconds, double frequencyHz) {
    PathNode node;
    node.anchor = TimeFrequencyPoint{timeSeconds, frequencyHz};
    node.type = PathNodeType::Corner;
    return node;
}

/// @brief A plain straight-line Path from (0, 0) to (timeSeconds, 0).
Path straightLinePath(double timeSeconds) {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    path.addNode(cornerNodeAt(timeSeconds, 0.0));
    return path;
}

/// @brief A jagged zigzag Path spanning the same overall time range as
/// straightLinePath() above, for shape-sensitivity comparisons.
Path zigzagPath(double timeSeconds, double amplitudeHz) {
    Path path;
    constexpr int kZigzags = 6;
    for (int i = 0; i <= kZigzags; ++i) {
        const double t = static_cast<double>(i) / kZigzags;
        const double frequency = (i % 2 == 0) ? 0.0 : amplitudeHz;
        path.addNode(cornerNodeAt(t * timeSeconds, frequency));
    }
    return path;
}

bool allInUnitRange(const std::vector<float>& spectrum) {
    for (float value : spectrum) {
        if (value < 0.0f || value > 1.0f) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("A fresh CurveGraph has no nodes", "[core][resonant_instrument]") {
    const CurveGraph graph;
    REQUIRE(graph.nodes().empty());
}

TEST_CASE("addNode appends and addEdge connects symmetrically", "[core][resonant_instrument]") {
    CurveGraph graph;
    const std::size_t a = graph.addNode({0.0, 0.0});
    const std::size_t b = graph.addNode({1.0, 0.0});
    REQUIRE(a == 0);
    REQUIRE(b == 1);

    const bool added = graph.addEdge(a, b);

    REQUIRE(added);
    REQUIRE(graph.nodes()[a].neighbors == std::vector<std::size_t>{b});
    REQUIRE(graph.nodes()[b].neighbors == std::vector<std::size_t>{a});
}

TEST_CASE("addEdge is a no-op for invalid indices, self-loops, or duplicates", "[core][resonant_instrument]") {
    CurveGraph graph;
    const std::size_t a = graph.addNode({0.0, 0.0});
    const std::size_t b = graph.addNode({1.0, 0.0});
    REQUIRE(graph.addEdge(a, b));

    REQUIRE_FALSE(graph.addEdge(a, b));    // Already connected.
    REQUIRE_FALSE(graph.addEdge(a, a));    // Self-loop.
    REQUIRE_FALSE(graph.addEdge(a, 99));   // Out of range.
    REQUIRE(graph.nodes()[a].neighbors == std::vector<std::size_t>{b});
}

TEST_CASE("curveGraphFromPath returns an empty graph for a degenerate Path", "[core][resonant_instrument]") {
    const Path empty;
    Path singleNode;
    singleNode.addNode(cornerNodeAt(0.0, 0.0));

    REQUIRE(curveGraphFromPath(empty, 1.0, 10).nodes().empty());
    REQUIRE(curveGraphFromPath(singleNode, 1.0, 10).nodes().empty());
}

TEST_CASE("curveGraphFromPath resamples a straight line into evenly-spaced nodes", "[core][resonant_instrument]") {
    const Path path = straightLinePath(10.0);

    const CurveGraph graph = curveGraphFromPath(path, 1.0, 5);

    REQUIRE(graph.nodes().size() == 5);
    // Evenly spaced along the line from (0, 0) to (10, 0).
    for (std::size_t i = 0; i < 5; ++i) {
        const double expectedX = 10.0 * static_cast<double>(i) / 4.0;
        REQUIRE(graph.nodes()[i].position.x == Catch::Approx(expectedX).margin(1e-6));
        REQUIRE(graph.nodes()[i].position.y == Catch::Approx(0.0).margin(1e-6));
    }
    // Connected as a single open chain.
    REQUIRE(graph.nodes().front().neighbors == std::vector<std::size_t>{1});
    REQUIRE(graph.nodes().back().neighbors == std::vector<std::size_t>{3});
    REQUIRE(graph.nodes()[2].neighbors.size() == 2);
}

TEST_CASE("curveGraphFromPath normalizes frequency by frequencyToTimeScale", "[core][resonant_instrument]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    path.addNode(cornerNodeAt(0.0, 200.0));

    const CurveGraph graph = curveGraphFromPath(path, 100.0, 2);

    REQUIRE(graph.nodes().size() == 2);
    REQUIRE(graph.nodes()[0].position.y == Catch::Approx(0.0).margin(1e-6));
    REQUIRE(graph.nodes()[1].position.y == Catch::Approx(2.0).margin(1e-6));  // 200 Hz / 100 scale.
}

TEST_CASE("curveGraphFromPath clamps targetNodeCount up to at least 2", "[core][resonant_instrument]") {
    const Path path = straightLinePath(10.0);

    const CurveGraph graph = curveGraphFromPath(path, 1.0, 1);

    REQUIRE(graph.nodes().size() == 2);
}

TEST_CASE("computeWaveKernelSignature returns an all-zero spectrum of the requested size for degenerate graphs",
          "[core][resonant_instrument]") {
    const CurveGraph empty;
    CurveGraph singleNode;
    singleNode.addNode({0.0, 0.0});
    CurveGraph disconnected;
    disconnected.addNode({0.0, 0.0});
    disconnected.addNode({1.0, 0.0});  // No addEdge() call - no connectivity at all.

    const std::vector<const CurveGraph*> graphs{&empty, &singleNode, &disconnected};
    for (const CurveGraph* graph : graphs) {
        const auto spectrum = computeWaveKernelSignature(*graph, 16);
        REQUIRE(spectrum.size() == 16);
        for (float value : spectrum) {
            REQUIRE(value == 0.0f);
        }
    }
}

TEST_CASE("computeWaveKernelSignature floors spectrumSize at 1", "[core][resonant_instrument]") {
    const CurveGraph empty;
    REQUIRE(computeWaveKernelSignature(empty, 0).size() == 1);
}

TEST_CASE("computeWaveKernelSignature returns a correctly-sized spectrum with every value in [0, 1]",
          "[core][resonant_instrument]") {
    const CurveGraph graph = curveGraphFromPath(straightLinePath(10.0), 1.0, 24);

    const auto spectrum = computeWaveKernelSignature(graph, 32);

    REQUIRE(spectrum.size() == 32);
    REQUIRE(allInUnitRange(spectrum));
    const bool anyNonZero = std::any_of(spectrum.begin(), spectrum.end(), [](float v) { return v > 0.0f; });
    REQUIRE(anyNonZero);
}

TEST_CASE("computeWaveKernelSignature is invariant to node order (reversing a straight chain)",
          "[core][resonant_instrument]") {
    const CurveGraph forward = curveGraphFromPath(straightLinePath(10.0), 1.0, 20);
    // Same physical graph, built with nodes added in reverse order.
    Path reversedPath;
    reversedPath.addNode(cornerNodeAt(10.0, 0.0));
    reversedPath.addNode(cornerNodeAt(0.0, 0.0));
    const CurveGraph reversed = curveGraphFromPath(reversedPath, 1.0, 20);

    const auto forwardSpectrum = computeWaveKernelSignature(forward, 32);
    const auto reversedSpectrum = computeWaveKernelSignature(reversed, 32);

    for (std::size_t i = 0; i < forwardSpectrum.size(); ++i) {
        REQUIRE(forwardSpectrum[i] == Catch::Approx(reversedSpectrum[i]).margin(1e-4));
    }
}

TEST_CASE("computeWaveKernelSignature is invariant to uniformly rescaling the whole curve",
          "[core][resonant_instrument]") {
    const CurveGraph small = curveGraphFromPath(straightLinePath(10.0), 1.0, 20);
    const CurveGraph large = curveGraphFromPath(straightLinePath(1000.0), 1.0, 20);

    const auto smallSpectrum = computeWaveKernelSignature(small, 32);
    const auto largeSpectrum = computeWaveKernelSignature(large, 32);

    for (std::size_t i = 0; i < smallSpectrum.size(); ++i) {
        REQUIRE(smallSpectrum[i] == Catch::Approx(largeSpectrum[i]).margin(1e-3));
    }
}

TEST_CASE("computeWaveKernelSignature distinguishes a jagged shape from a straight line of the same span",
          "[core][resonant_instrument]") {
    const CurveGraph straight = curveGraphFromPath(straightLinePath(10.0), 1.0, 30);
    const CurveGraph zigzag = curveGraphFromPath(zigzagPath(10.0, 5.0), 1.0, 30);

    const auto straightSpectrum = computeWaveKernelSignature(straight, 32);
    const auto zigzagSpectrum = computeWaveKernelSignature(zigzag, 32);

    bool foundDifference = false;
    for (std::size_t i = 0; i < straightSpectrum.size(); ++i) {
        if (std::abs(straightSpectrum[i] - zigzagSpectrum[i]) > 1e-3) {
            foundDifference = true;
            break;
        }
    }
    REQUIRE(foundDifference);
}
