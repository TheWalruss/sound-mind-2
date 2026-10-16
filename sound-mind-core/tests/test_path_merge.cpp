#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <queue>
#include <set>

#include "sound_mind/core/path_merge.h"
#include "sound_mind/core/resonant_instrument.h"

using sound_mind::core::BranchGraft;
using sound_mind::core::CurveGraph;
using sound_mind::core::mergeIntoBranchingPath;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;

namespace {

constexpr double kScale = 1000.0;  // Hz per second-equivalent.

Path line(std::initializer_list<TimeFrequencyPoint> points) {
    Path path;
    for (const auto& point : points) {
        PathNode node;
        node.anchor = point;
        node.type = PathNodeType::Corner;
        path.addNode(node);
    }
    return path;
}

std::vector<BranchGraft> single(Path path) { return {BranchGraft{std::move(path), std::nullopt, {}}}; }

/// @brief Whether every node of `graph` is reachable from node 0.
bool isConnected(const CurveGraph& graph) {
    if (graph.nodes().empty()) {
        return false;
    }
    std::set<std::size_t> seen{0};
    std::queue<std::size_t> pending;
    pending.push(0);
    while (!pending.empty()) {
        const std::size_t node = pending.front();
        pending.pop();
        for (const std::size_t next : graph.nodes()[node].neighbors) {
            if (seen.insert(next).second) {
                pending.push(next);
            }
        }
    }
    return seen.size() == graph.nodes().size();
}

}  // namespace

TEST_CASE("Merging two paths joins their two closest nodes", "[core][path_merge]") {
    const auto merged = mergeIntoBranchingPath(
        {single(line({{0.0, 100.0}, {1.0, 100.0}})), single(line({{1.2, 150.0}, {2.0, 600.0}}))}, kScale);

    REQUIRE(merged.size() == 2);
    CHECK_FALSE(merged[0].parentIndex.has_value());
    CHECK(merged[1].parentIndex == std::size_t{0});
    CHECK(merged[1].graftPoint.timeSeconds == Catch::Approx(1.0));  // the first path's end...
    REQUIRE(merged[1].attachPoint.has_value());
    CHECK(merged[1].attachPoint->timeSeconds == Catch::Approx(1.2));  // ...joined to the second's start.
}

TEST_CASE("Merging k paths adds exactly k-1 joins and connects everything", "[core][path_merge]") {
    const auto merged = mergeIntoBranchingPath({single(line({{0.0, 100.0}, {0.5, 100.0}})),
                                                single(line({{0.6, 300.0}, {1.0, 300.0}})),
                                                single(line({{0.2, 900.0}, {0.4, 950.0}}))},
                                               kScale);

    REQUIRE(merged.size() == 3);
    for (std::size_t i = 1; i < merged.size(); ++i) {
        REQUIRE(merged[i].parentIndex.has_value());
        CHECK(*merged[i].parentIndex < i);  // parents always come first.
    }
    const CurveGraph graph = sound_mind::core::curveGraphFromBranches(merged, kScale, 8);
    CHECK(isConnected(graph));
    std::size_t edges = 0;
    for (const auto& node : graph.nodes()) {
        edges += node.neighbors.size();
    }
    CHECK(edges / 2 == 3 * 7 + 2);  // three 8-node chains plus two joins.
}

TEST_CASE("Merging keeps a branching path's own grafts, re-rooted where needed", "[core][path_merge]") {
    // A branching path whose branch is the part nearest the other path - the
    // join lands on the branch, so the tree is re-rooted through it.
    std::vector<BranchGraft> tree{
        BranchGraft{line({{0.0, 100.0}, {0.5, 100.0}}), std::nullopt, {}},
        BranchGraft{line({{0.5, 100.0}, {0.9, 400.0}}), std::size_t{0}, TimeFrequencyPoint{0.5, 100.0}}};
    const auto merged =
        mergeIntoBranchingPath({single(line({{1.0, 420.0}, {1.5, 420.0}})), tree}, kScale);

    REQUIRE(merged.size() == 3);
    const CurveGraph graph = sound_mind::core::curveGraphFromBranches(merged, kScale, 6);
    CHECK(isConnected(graph));
    // The old branch now hangs off the new trunk; the old trunk hangs off it.
    CHECK(merged[1].path.nodes().back().anchor.timeSeconds == Catch::Approx(0.9));
    CHECK(merged[2].parentIndex == std::size_t{1});
}

TEST_CASE("Merging drops empty strokes and returns nothing for nothing", "[core][path_merge]") {
    CHECK(mergeIntoBranchingPath({}, kScale).empty());
    CHECK(mergeIntoBranchingPath({single(Path{})}, kScale).empty());
    const auto merged = mergeIntoBranchingPath({single(Path{}), single(line({{0.0, 100.0}, {1.0, 100.0}}))}, kScale);
    CHECK(merged.size() == 1);
}

TEST_CASE("An attach point welds the child's nearest node, not its start", "[core][path_merge]") {
    std::vector<BranchGraft> branches{
        BranchGraft{line({{0.0, 100.0}, {1.0, 100.0}}), std::nullopt, {}},
        BranchGraft{line({{0.0, 500.0}, {1.0, 500.0}}), std::size_t{0}, TimeFrequencyPoint{1.0, 100.0},
                    TimeFrequencyPoint{1.0, 500.0}}};
    const CurveGraph graph = sound_mind::core::curveGraphFromBranches(branches, kScale, 2);

    REQUIRE(graph.nodes().size() == 4);
    // Node 1 is the trunk's end, node 3 the branch's end: the weld joins them.
    const auto& neighbors = graph.nodes()[1].neighbors;
    CHECK(std::find(neighbors.begin(), neighbors.end(), std::size_t{3}) != neighbors.end());
}
