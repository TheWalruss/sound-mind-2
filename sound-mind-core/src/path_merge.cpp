#include "sound_mind/core/path_merge.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <queue>

namespace sound_mind::core {

namespace {

/// @brief One undirected join between two strokes: a point on each.
struct Join {
    std::size_t first;
    std::size_t second;
    TimeFrequencyPoint onFirst;
    TimeFrequencyPoint onSecond;
};

/// @brief A candidate join between two pieces, for Kruskal's algorithm.
struct Candidate {
    double distance;
    std::size_t firstStroke;
    std::size_t secondStroke;
    TimeFrequencyPoint onFirst;
    TimeFrequencyPoint onSecond;
};

std::size_t findRoot(std::vector<std::size_t>& parent, std::size_t i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

}  // namespace

std::vector<BranchGraft> mergeIntoBranchingPath(const std::vector<std::vector<BranchGraft>>& paths,
                                                double frequencyToTimeScale) {
    // Flatten every stroke, remembering which piece it came from; the
    // internal grafts become undirected joins.
    std::vector<Path> strokes;
    std::vector<std::size_t> pieceOf;
    std::vector<Join> joins;
    std::size_t pieceCount = 0;
    for (const auto& path : paths) {
        std::vector<std::optional<std::size_t>> flatIndex(path.size());
        bool any = false;
        for (std::size_t i = 0; i < path.size(); ++i) {
            if (path[i].path.nodes().empty()) {
                continue;
            }
            flatIndex[i] = strokes.size();
            strokes.push_back(path[i].path);
            pieceOf.push_back(pieceCount);
            any = true;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const auto& parent = path[i].parentIndex;
            if (!flatIndex[i].has_value() || !parent.has_value() || *parent >= path.size() ||
                !flatIndex[*parent].has_value()) {
                continue;
            }
            joins.push_back(Join{*flatIndex[*parent], *flatIndex[i], path[i].graftPoint,
                                 path[i].attachPoint.value_or(path[i].path.nodes().front().anchor)});
        }
        if (any) {
            ++pieceCount;
        }
    }
    if (strokes.empty()) {
        return {};
    }

    // Kruskal: every anchor pair across two different pieces.
    std::vector<Candidate> candidates;
    for (std::size_t a = 0; a < strokes.size(); ++a) {
        for (std::size_t b = a + 1; b < strokes.size(); ++b) {
            if (pieceOf[a] == pieceOf[b]) {
                continue;
            }
            for (const PathNode& u : strokes[a].nodes()) {
                for (const PathNode& v : strokes[b].nodes()) {
                    const double dt = u.anchor.timeSeconds - v.anchor.timeSeconds;
                    const double df = (u.anchor.frequencyHz - v.anchor.frequencyHz) / frequencyToTimeScale;
                    candidates.push_back(Candidate{std::hypot(dt, df), a, b, u.anchor, v.anchor});
                }
            }
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& x, const Candidate& y) { return x.distance < y.distance; });
    std::vector<std::size_t> groupParent(pieceCount);
    std::iota(groupParent.begin(), groupParent.end(), std::size_t{0});
    std::size_t joined = 0;
    for (const Candidate& candidate : candidates) {
        if (joined + 1 >= pieceCount) {
            break;
        }
        const std::size_t x = findRoot(groupParent, pieceOf[candidate.firstStroke]);
        const std::size_t y = findRoot(groupParent, pieceOf[candidate.secondStroke]);
        if (x == y) {
            continue;
        }
        groupParent[x] = y;
        joins.push_back(Join{candidate.firstStroke, candidate.secondStroke, candidate.onFirst, candidate.onSecond});
        ++joined;
    }

    // Breadth-first from the first stroke, so every parent precedes its
    // children; each join is oriented away from the root as it's crossed.
    std::vector<std::vector<std::size_t>> joinsAt(strokes.size());
    for (std::size_t j = 0; j < joins.size(); ++j) {
        joinsAt[joins[j].first].push_back(j);
        joinsAt[joins[j].second].push_back(j);
    }
    std::vector<std::optional<std::size_t>> newIndex(strokes.size());
    std::vector<BranchGraft> result;
    std::queue<std::size_t> pending;
    newIndex[0] = 0;
    result.push_back(BranchGraft{strokes[0], std::nullopt, TimeFrequencyPoint{}, std::nullopt});
    pending.push(0);
    while (!pending.empty()) {
        const std::size_t stroke = pending.front();
        pending.pop();
        for (const std::size_t j : joinsAt[stroke]) {
            const Join& join = joins[j];
            const bool fromFirst = join.first == stroke;
            const std::size_t other = fromFirst ? join.second : join.first;
            if (newIndex[other].has_value()) {
                continue;
            }
            newIndex[other] = result.size();
            result.push_back(BranchGraft{strokes[other], newIndex[stroke],
                                         fromFirst ? join.onFirst : join.onSecond,
                                         fromFirst ? join.onSecond : join.onFirst});
            pending.push(other);
        }
    }
    return result;
}

}  // namespace sound_mind::core
