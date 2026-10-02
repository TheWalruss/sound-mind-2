#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "sound_mind/core/path.h"

namespace sound_mind::core {

/**
 * @brief A plain 2D point in a `CurveGraph`'s own normalized embedding
 *        space - `docs/sound-mind-roadmap.md`'s "Resonant Instruments"
 *        own `v0.Y.59.1`.
 *
 * Deliberately *not* a `TimeFrequencyPoint` - a `CurveGraph` has no
 * inherent notion of "seconds" or "Hz" of its own; `curveGraphFromPath()`
 * is the one place that converts a `Path`'s own `TimeFrequencyPoint`
 * anchors into this normalized space (via `frequencyToTimeScale`, the
 * same yardstick `fitPathToPoints()`/brush sizing already use to make
 * time and frequency comparable), so that a curve's own *visual* shape -
 * not raw seconds-vs-Hz, which are wildly different-scaled units - is
 * what the Laplace-Beltrami discretization actually measures.
 */
struct CurvePoint {
    /// @brief Position along the curve's own normalized time-like axis -
    ///        a `TimeFrequencyPoint::timeSeconds` value, unchanged by
    ///        `curveGraphFromPath()`'s own normalization (only frequency
    ///        needs rescaling against it - see this struct's own docs).
    double x = 0.0;

    /// @brief Position along the curve's own normalized frequency-like
    ///        axis - a `TimeFrequencyPoint::frequencyHz` value divided by
    ///        `frequencyToTimeScale`, so it's directly comparable to `x`.
    double y = 0.0;
};

/**
 * @brief One discretized point of a `CurveGraph` - see that class's own
 *        docs.
 */
struct CurveNode {
    /// @brief This node's own position, in its `CurveGraph`'s own
    ///        normalized space.
    CurvePoint position;

    /// @brief Indices (into the owning `CurveGraph::nodes()`) of every
    ///        node this one is directly connected to. A plain list, not a
    ///        fixed next/prev pair - what actually lets this structure
    ///        support branching once something populates more than two
    ///        entries here (see `CurveGraph`'s own docs on why nothing
    ///        does yet).
    std::vector<std::size_t> neighbors;
};

/**
 * @brief A branching-capable graph of connected points - the Laplace-
 *        Beltrami discretization `docs/sound-mind-roadmap.md`'s
 *        "Resonant Instruments" (`v0.Y.59.1`) own item 1 describes
 *        ("extend the Curve structure to support branching, so the user
 *        can draw curves, trees, and graphs").
 *
 * **A new, separate structure, not a generalized `Path`** - confirmed
 * with the user specifically so `Path` itself, and every one of its
 * existing linear-sequence consumers (freehand capture, the Path tool,
 * stamp-along-curve spacing, Drawn MindWave sampling - all of them
 * written against "an ordered `std::vector<PathNode>`, addressed by a
 * single `t` in `[0, 1]`"), stays completely untouched by this feature.
 * `curveGraphFromPath()` below is the bridge: any already-drawn `Path`
 * (freehand-captured or placed with the Path tool, it makes no
 * difference) can be used as a Resonant Instrument's own source curve,
 * no branching-curve editor required. `curveGraphFromBranches()` is the
 * genuinely branching entry point - see its own docs - built on exactly
 * the same per-branch resampling, with one extra cross-branch edge per
 * graft point. `CurveNode::neighbors` needed no change at all to support
 * either one: a node could already carry any number of neighbors, not
 * just two.
 *
 * Purely a plain data structure - no Bézier curvature, no gradient, no
 * paint semantics of any kind, unlike `Path`. Every edge is a straight
 * line between its own two endpoints; the "curve" a `CurveGraph` actually
 * represents is the piecewise-linear shape its own edges trace, which is
 * exactly what the discrete Laplace-Beltrami assembly `computeWaveKernelSignature()`
 * needs - no smoother an approximation is justified for a hand-drawn
 * input in the first place.
 */
class CurveGraph {
public:
    /// @brief This graph's own nodes, in the order they were added.
    /// @return The current nodes; may be empty.
    [[nodiscard]] const std::vector<CurveNode>& nodes() const noexcept { return nodes_; }

    /**
     * @brief Appends a new, as-yet-unconnected node.
     * @param position The new node's own position.
     * @return The new node's index in nodes().
     */
    std::size_t addNode(CurvePoint position);

    /**
     * @brief Connects two existing nodes with an undirected edge - a no-op
     *        if either index is invalid, `a == b`, or the two are already
     *        connected (no duplicate neighbor entries).
     * @param a One endpoint's index.
     * @param b The other endpoint's index.
     * @return `true` if a new edge was actually added; `false` otherwise.
     */
    bool addEdge(std::size_t a, std::size_t b);

private:
    std::vector<CurveNode> nodes_;
};

/**
 * @brief Resamples `path` into a linear (non-branching) `CurveGraph` of
 *        `targetNodeCount` evenly arc-length-spaced points - Resonant
 *        Instruments' own entry point for "any already-drawn `Path` can
 *        be used, no separate branching-curve tool required yet" (see
 *        `CurveGraph`'s own docs).
 *
 * Evaluates `path`'s own Bézier segments densely (the same per-segment
 * subdivision technique `sampleStrokeDense()` (`paint_application.cpp`)
 * already uses for stamp placement, written again in miniature here
 * rather than shared - that function's own `StrokeSample`/`pathT` shape
 * is specific to stamp placement, not this module's own needs), then
 * walks the resulting dense polyline's own cumulative arc length (in the
 * same `frequencyToTimeScale`-normalized space `CurvePoint`'s own docs
 * describe) to place `targetNodeCount` nodes at evenly-spaced arc-length
 * fractions, linearly interpolating between whichever dense hop each one
 * falls in - the same technique `sampleStrokeAlongCurve()`
 * (`paint_application.cpp`) uses for `StampMode::AlongCurve`, just
 * targeting a fixed node count instead of a repeating interval pattern.
 * The resulting nodes are connected into a single open chain (node `i` to
 * node `i + 1`), the simplest possible `CurveGraph` - nothing here
 * invents a branch.
 *
 * @param path The source curve. Returns an empty graph (no nodes) if it
 *        has fewer than 2 nodes of its own - too degenerate to resample.
 * @param frequencyToTimeScale The same per-project normalization scale
 *        `fitPathToPoints()`'s own docs describe; must be positive.
 * @param targetNodeCount How many nodes to place; clamped up to `2` if
 *        given less (a `CurveGraph` needs at least one edge to have any
 *        geometric structure for `computeWaveKernelSignature()` to read).
 * @return The resampled graph.
 */
[[nodiscard]] CurveGraph curveGraphFromPath(const Path& path, double frequencyToTimeScale,
                                             std::size_t targetNodeCount);

/**
 * @brief One stroke of a hand-drawn branching curve, plus where it grafts
 *        onto whichever earlier stroke it grew from - `curveGraphFromBranches()`'s
 *        own input, and the Studio-side "Branching Curve" session's own
 *        accumulated state (`docs/sound-mind-roadmap.md`'s "Resonant
 *        Instruments" item 1, the deferred branching-curve editor).
 */
struct BranchGraft {
    /// @brief This stroke's own curve, in the same `Path` form every other
    ///        freehand/Path-tool stroke already uses.
    Path path;

    /// @brief Index, into whichever `std::vector<BranchGraft>` this entry
    ///        belongs to, of the earlier stroke this one grows out of -
    ///        `std::nullopt` for the trunk (the one stroke with nothing to
    ///        graft onto). Must refer to an earlier entry (a smaller
    ///        index) if present; `curveGraphFromBranches()` silently skips
    ///        a graft that doesn't, the same "malformed input degrades
    ///        gracefully, never throws" convention `addEdge()`'s own docs
    ///        already establish.
    std::optional<std::size_t> parentIndex;

    /// @brief Where, along `parentIndex`'s own curve, this stroke grafts
    ///        on - the point the user clicked while Picking the parent
    ///        stroke, in the same raw `TimeFrequencyPoint` space `path`'s
    ///        own anchors already use (normalized the same way `path` is,
    ///        via `frequencyToTimeScale`, once `curveGraphFromBranches()`
    ///        locates the parent's own nearest resampled node to it).
    ///        Ignored when `parentIndex` is `std::nullopt`.
    TimeFrequencyPoint graftPoint;
};

/**
 * @brief Resamples every stroke in `branches` into its own linear chain of
 *        `targetNodeCountPerBranch` nodes (exactly `curveGraphFromPath()`'s
 *        own resampling, run once per stroke), then welds each non-trunk
 *        stroke onto its own parent with one extra edge at the closest
 *        resampled node to its own `graftPoint` - the genuinely branching
 *        counterpart to `curveGraphFromPath()`, assembling a real tree (or,
 *        if a later installment ever lets a stroke graft onto more than
 *        one parent, a general graph) out of individually-drawn strokes
 *        rather than resampling a single existing `Path`.
 *
 * Each stroke keeps its own independent internal resampling - grafting
 * never merges two nodes into one or re-threads a stroke's own interior
 * nodes - so the result is `branches.size()` separate open chains, plus
 * one additional cross-chain edge per graft. This is already exactly the
 * shape `computeWaveKernelSignature()` needs: that function's own
 * discretization is purely edge-local (no special case for how many edges
 * meet at a node), so a node gaining one extra neighbor from a graft is no
 * different, as far as the Laplace-Beltrami assembly is concerned, from
 * any other node along a chain.
 *
 * @param branches Every stroke, trunk first - see `BranchGraft`'s own docs
 *        on ordering (`parentIndex` must point to an earlier entry).
 *        A stroke with fewer than 2 `Path` nodes of its own contributes no
 *        nodes at all (the same too-degenerate-to-resample case
 *        `curveGraphFromPath()` already handles), and anything that was
 *        meant to graft onto it is silently skipped instead of grafting
 *        onto nothing.
 * @param frequencyToTimeScale The same per-project normalization scale
 *        `curveGraphFromPath()` already takes.
 * @param targetNodeCountPerBranch How many nodes to resample *each* stroke
 *        down to; clamped up to `2` if given less, same as
 *        `curveGraphFromPath()`.
 * @return The assembled branching graph; empty if `branches` is empty or
 *         every entry was too degenerate to include.
 */
[[nodiscard]] CurveGraph curveGraphFromBranches(const std::vector<BranchGraft>& branches, double frequencyToTimeScale,
                                                 std::size_t targetNodeCountPerBranch);

/**
 * @brief Computes `graph`'s own aggregated, whole-shape Wave Kernel
 *        Signature - `docs/sound-mind-roadmap.md`'s "Resonant
 *        Instruments" own items 3-4 ("Calculate the Laplace-Beltrami
 *        operator (LBO) for the selected curve. Integrate Wave Kernel
 *        Signature over the whole shape and store the resulting global
 *        geometrical spectrum (the 'WKS curve')").
 *
 * **Discretization**: each edge `(i, j)` of length `h` (Euclidean, in
 * `graph`'s own normalized space) contributes the standard piecewise-
 * linear finite-element assembly for the continuum 1D Laplace-Beltrami
 * operator `-d²/ds²` on a metric graph - purely edge-local, so it
 * generalizes to a branching `graph` exactly as written, with no special
 * case for how many edges meet at a node:
 * - **Stiffness** (the operator itself): `L(i,i) += 1/h`, `L(j,j) += 1/h`,
 *   `L(i,j) -= 1/h`, `L(j,i) -= 1/h`.
 * - **Mass** (lumped, standard 1D FEM practice): `M(i,i) += h/2`,
 *   `M(j,j) += h/2`.
 *
 * Edge lengths are floored to a small positive epsilon before use, so two
 * coincident nodes (a degenerate input, not an error) produce a very
 * stiff but still finite, well-posed edge rather than a division by zero.
 *
 * **Eigendecomposition**: solves the generalized eigenproblem `L·v = λ·M·v`
 * (via Eigen's `GeneralizedSelfAdjointEigenSolver`, this codebase's one
 * new dependency added specifically for this - see `docs/tech-stack-
 * decisions.md`), giving ascending eigenvalues `λ_0 = 0 < λ_1 ≤ ... ≤
 * λ_{n-1}` and `M`-orthonormal eigenvectors `φ_k` (`φ_kᵀ·M·φ_k = 1`) -
 * `λ_0`'s own trivial constant eigenvector (true for *any* graph built
 * this way, branching or not - every row of `L` sums to zero by
 * construction) is always excluded from what follows, since the Wave
 * Kernel Signature's own energy axis is `log(λ)`, undefined at `0`.
 *
 * **The aggregated "WKS curve"**: the classic per-point Wave Kernel
 * Signature (Aubry, Schlickewei, Cremers 2011) is
 * `WKS(x, e) = Σ_k φ_k(x)² · exp(-(e - log λ_k)² / (2σ²))`; *integrating*
 * it "over the whole shape" (summing over every node `x`) collapses the
 * per-node term `Σ_x φ_k(x)²` into a single per-eigenmode weight (its own
 * eigenvector's squared norm, already `1` only in the non-generalized
 * case - `M`-orthonormality alone doesn't guarantee that here, so it's
 * computed directly rather than assumed), giving the whole-shape spectrum
 * this function actually returns: `spectrum(e) = Σ_k weight_k ·
 * exp(-(e - log λ_k)² / (2σ²))`, sampled at `spectrumSize` evenly-spaced
 * `e` across `[log λ_1, log λ_{n-1}]` and finally normalized to `[0, 1]`
 * (dividing by its own peak) - the same range every other `MindWave`
 * field already promises, so this result is directly usable as one
 * without any further rescaling at either call site (a brush tip or a
 * MindWave generator).
 *
 * @param graph The curve graph to analyze.
 * @param spectrumSize How many evenly-spaced samples to return; floored
 *        at `1`.
 * @return A spectrum of exactly `std::max<std::size_t>(1, spectrumSize)`
 *         values, each in `[0, 1]` - all-zero if `graph` has fewer than 2
 *         nodes, no edges at all, or (after excluding `λ_0`) no further
 *         usable eigenvalue to build a spectrum from.
 */
[[nodiscard]] std::vector<float> computeWaveKernelSignature(const CurveGraph& graph, std::size_t spectrumSize);

}  // namespace sound_mind::core
