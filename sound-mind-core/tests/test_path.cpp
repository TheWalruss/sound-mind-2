#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/path.h"

using sound_mind::core::fitPathToPoints;
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

bool approximatelyEqual(double a, double b, double epsilon = 1e-6) { return std::abs(a - b) < epsilon; }

}  // namespace

TEST_CASE("A fresh Path has no nodes", "[core][path]") {
    const Path path;
    REQUIRE(path.nodes().empty());
}

TEST_CASE("A fresh Path has a fresh, fully transparent Gradient", "[core][path]") {
    const Path path;
    REQUIRE(path.gradient().stops().size() == 2);
    REQUIRE(path.gradient().stops().front().leftOpacity == 0.0f);
}

TEST_CASE("addNode appends and returns the new index", "[core][path]") {
    Path path;
    const std::size_t first = path.addNode(cornerNodeAt(0.0, 100.0));
    const std::size_t second = path.addNode(cornerNodeAt(1.0, 200.0));
    REQUIRE(first == 0);
    REQUIRE(second == 1);
    REQUIRE(path.nodes().size() == 2);
}

TEST_CASE("insertNode inserts at a specific position, shifting later nodes up", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    path.addNode(cornerNodeAt(2.0, 0.0));

    REQUIRE(path.insertNode(1, cornerNodeAt(1.0, 0.0)));
    REQUIRE(path.nodes().size() == 3);
    REQUIRE(path.nodes()[1].anchor.timeSeconds == 1.0);
    REQUIRE(path.nodes()[2].anchor.timeSeconds == 2.0);
}

TEST_CASE("insertNode refuses an out-of-range index", "[core][path]") {
    Path path;
    REQUIRE_FALSE(path.insertNode(5, cornerNodeAt(0.0, 0.0)));
}

TEST_CASE("insertNode at nodes().size() behaves like addNode", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    REQUIRE(path.insertNode(1, cornerNodeAt(1.0, 0.0)));
    REQUIRE(path.nodes().size() == 2);
}

TEST_CASE("removeNode removes a real node", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    path.addNode(cornerNodeAt(1.0, 0.0));
    REQUIRE(path.removeNode(0));
    REQUIRE(path.nodes().size() == 1);
    REQUIRE(path.nodes()[0].anchor.timeSeconds == 1.0);
}

TEST_CASE("removeNode refuses an out-of-range index", "[core][path]") {
    Path path;
    REQUIRE_FALSE(path.removeNode(0));
}

TEST_CASE("setNode replaces a node in place", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    REQUIRE(path.setNode(0, cornerNodeAt(5.0, 500.0)));
    REQUIRE(path.nodes()[0].anchor.timeSeconds == 5.0);
    REQUIRE(path.nodes()[0].anchor.frequencyHz == 500.0);
}

TEST_CASE("setNode refuses an out-of-range index", "[core][path]") {
    Path path;
    REQUIRE_FALSE(path.setNode(0, cornerNodeAt(0.0, 0.0)));
}

TEST_CASE("bounds() is all-zero for a path with no nodes", "[core][path]") {
    const Path path;
    const auto rect = path.bounds();
    REQUIRE(rect.startTimeSeconds == 0.0);
    REQUIRE(rect.endTimeSeconds == 0.0);
    REQUIRE(rect.lowFrequencyHz == 0.0);
    REQUIRE(rect.highFrequencyHz == 0.0);
}

TEST_CASE("bounds() spans every node's anchor", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(1.0, 300.0));
    path.addNode(cornerNodeAt(3.0, 100.0));
    path.addNode(cornerNodeAt(2.0, 500.0));

    const auto rect = path.bounds();
    REQUIRE(rect.startTimeSeconds == 1.0);
    REQUIRE(rect.endTimeSeconds == 3.0);
    REQUIRE(rect.lowFrequencyHz == 100.0);
    REQUIRE(rect.highFrequencyHz == 500.0);
}

TEST_CASE("bounds() also spans handle points, not just anchors", "[core][path]") {
    Path path;
    PathNode smooth;
    smooth.anchor = TimeFrequencyPoint{1.0, 100.0};
    smooth.type = PathNodeType::Smooth;
    smooth.handleIn = TimeFrequencyPoint{0.5, 50.0};
    smooth.handleOut = TimeFrequencyPoint{1.5, 1000.0};  // deliberately far outside the anchor range.
    path.addNode(smooth);

    const auto rect = path.bounds();
    REQUIRE(rect.highFrequencyHz == 1000.0);
    REQUIRE(rect.lowFrequencyHz == 50.0);
}

TEST_CASE("A Path round-trips through JSON, including Smooth handles", "[core][path]") {
    Path path;
    path.addNode(cornerNodeAt(0.0, 0.0));
    PathNode smooth;
    smooth.anchor = TimeFrequencyPoint{1.0, 100.0};
    smooth.type = PathNodeType::Smooth;
    smooth.handleIn = TimeFrequencyPoint{0.9, 90.0};
    smooth.handleOut = TimeFrequencyPoint{1.1, 110.0};
    path.addNode(smooth);
    path.gradient().setLinkChannels(true);

    const nlohmann::json json = path;
    const Path roundTripped = json.get<Path>();

    REQUIRE(roundTripped.nodes().size() == 2);
    REQUIRE(roundTripped.nodes()[0].type == PathNodeType::Corner);
    REQUIRE_FALSE(roundTripped.nodes()[0].handleIn.has_value());
    REQUIRE(roundTripped.nodes()[1].type == PathNodeType::Smooth);
    REQUIRE(roundTripped.nodes()[1].handleIn.has_value());
    REQUIRE(roundTripped.nodes()[1].handleIn->timeSeconds == 0.9);
    REQUIRE(roundTripped.nodes()[1].handleOut->frequencyHz == 110.0);
    REQUIRE(roundTripped.gradient().linkChannels());
}

TEST_CASE("fitPathToPoints returns an empty path for fewer than two raw points", "[core][path]") {
    REQUIRE(fitPathToPoints({}, 1000.0, 0.01).nodes().empty());
    REQUIRE(fitPathToPoints({TimeFrequencyPoint{0.0, 0.0}}, 1000.0, 0.01).nodes().empty());
}

TEST_CASE("fitPathToPoints returns an empty path for a non-positive frequencyToTimeScale", "[core][path]") {
    const std::vector<TimeFrequencyPoint> points = {TimeFrequencyPoint{0.0, 0.0}, TimeFrequencyPoint{1.0, 100.0}};
    REQUIRE(fitPathToPoints(points, 0.0, 0.01).nodes().empty());
    REQUIRE(fitPathToPoints(points, -1.0, 0.01).nodes().empty());
}

TEST_CASE("fitPathToPoints simplifies a straight line down to just its two endpoints", "[core][path]") {
    std::vector<TimeFrequencyPoint> points;
    for (int i = 0; i <= 100; ++i) {
        const double t = static_cast<double>(i) / 100.0;
        points.push_back(TimeFrequencyPoint{t, t * 1000.0});  // dead straight line.
    }

    const Path path = fitPathToPoints(points, 1000.0, 0.01);
    REQUIRE(path.nodes().size() == 2);
    REQUIRE(path.nodes().front().type == PathNodeType::Corner);
    REQUIRE(path.nodes().back().type == PathNodeType::Corner);
    REQUIRE(approximatelyEqual(path.nodes().front().anchor.timeSeconds, 0.0));
    REQUIRE(approximatelyEqual(path.nodes().back().anchor.timeSeconds, 1.0));
    REQUIRE(approximatelyEqual(path.nodes().back().anchor.frequencyHz, 1000.0));
}

TEST_CASE("fitPathToPoints keeps a real bend as a Smooth interior node", "[core][path]") {
    std::vector<TimeFrequencyPoint> points;
    // A sharp V shape: straight from (0,0) to (1, 1000), then straight
    // back down from (1, 1000) to (2, 0) - the midpoint is a real corner
    // no straight-line simplification down to two points could represent.
    for (int i = 0; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50.0;
        points.push_back(TimeFrequencyPoint{t, t * 1000.0});
    }
    for (int i = 1; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50.0;
        points.push_back(TimeFrequencyPoint{1.0 + t, 1000.0 - t * 1000.0});
    }

    const Path path = fitPathToPoints(points, 1000.0, 0.01);
    REQUIRE(path.nodes().size() >= 3);
    // The interior node nearest the real bend should be Smooth (it has
    // real neighbors on both sides to derive a tangent from).
    bool foundSmoothInterior = false;
    for (std::size_t i = 1; i + 1 < path.nodes().size(); ++i) {
        if (path.nodes()[i].type == PathNodeType::Smooth) {
            foundSmoothInterior = true;
            REQUIRE(path.nodes()[i].handleIn.has_value());
            REQUIRE(path.nodes()[i].handleOut.has_value());
        }
    }
    REQUIRE(foundSmoothInterior);
}

TEST_CASE("fitPathToPoints's endpoints are always Corner nodes with no handles", "[core][path]") {
    std::vector<TimeFrequencyPoint> points;
    for (int i = 0; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50.0;
        points.push_back(TimeFrequencyPoint{t, std::sin(t * 6.28) * 1000.0});
    }

    const Path path = fitPathToPoints(points, 1000.0, 0.01);
    REQUIRE(path.nodes().front().type == PathNodeType::Corner);
    REQUIRE_FALSE(path.nodes().front().handleIn.has_value());
    REQUIRE(path.nodes().back().type == PathNodeType::Corner);
    REQUIRE_FALSE(path.nodes().back().handleOut.has_value());
}

TEST_CASE("fitPathToPoints produces a fresh, transparent Gradient", "[core][path]") {
    const std::vector<TimeFrequencyPoint> points = {TimeFrequencyPoint{0.0, 0.0}, TimeFrequencyPoint{1.0, 100.0}};
    const Path path = fitPathToPoints(points, 1000.0, 0.01);
    REQUIRE(path.gradient().stops().size() == 2);
    REQUIRE(path.gradient().stops().front().leftOpacity == 0.0f);
}
