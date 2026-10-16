#pragma once

#include <vector>

#include "sound_mind/core/resonant_instrument.h"

namespace sound_mind::core {

/**
 * @brief Joins several paths - each a regular path (one branch) or a
 *        branching path (several) - into one branching path, connected by
 *        the fewest, shortest extra joins (Pre-Beta Plan `v0.Y.77.1`).
 *
 * Each input path is one connected piece. The joins are a minimum spanning
 * tree over the pieces (Kruskal's algorithm): every pair of Path nodes
 * (anchors) lying in two different pieces is a candidate, measured in the
 * same normalized space resonance geometry uses (seconds, and Hz divided by
 * `frequencyToTimeScale`); the shortest candidates that join two
 * still-separate groups are taken until everything is one tree - exactly
 * `paths.size() - 1` joins.
 *
 * Every stroke is kept exactly as drawn. A join becomes a graft: the stroke
 * on one side grafts onto the stroke on the other, with `graftPoint` and
 * `attachPoint` at the two joined nodes. The result is re-ordered
 * (breadth-first from the first path's trunk) so that every branch's
 * parent comes before it, re-rooting a piece's own internal grafts where
 * the join lands somewhere other than its trunk - the shape is unchanged,
 * only which end of each internal graft counts as the parent.
 *
 * @param paths The paths to merge, each as its own branches (trunk first,
 *        `parentIndex` within that path). Strokes with no nodes are dropped.
 * @param frequencyToTimeScale The project's normalization scale - see
 *        `frequencyToTimeScaleFor()`; must be positive.
 * @return The merged branching path, trunk first; empty if no input stroke
 *         has any nodes.
 */
[[nodiscard]] std::vector<BranchGraft> mergeIntoBranchingPath(const std::vector<std::vector<BranchGraft>>& paths,
                                                              double frequencyToTimeScale);

}  // namespace sound_mind::core
