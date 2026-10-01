#include "sound_mind/core/resonant_instrument.h"

#include <algorithm>
#include <cmath>

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief Floor (in `CurveGraph`'s own normalized space) applied to every
/// edge length before it's used as a stiffness/mass weight -
/// `computeWaveKernelSignature()`'s own docs on why: two coincident nodes
/// are a degenerate input, not an error, and should produce a very stiff
/// but still finite, well-posed edge rather than a division by zero.
constexpr double kMinimumEdgeLength = 1e-6;

/// @brief Floor applied to the Gaussian width `computeWaveKernelSignature()`
/// smooths the aggregated spectrum with - guards the degenerate case where
/// a graph has only one usable (non-trivial) eigenvalue, leaving no real
/// energy range to derive a width from.
constexpr double kMinimumSigma = 1e-6;

/// @brief How many widths of Gaussian smoothing `computeWaveKernelSignature()`
/// spreads across the full energy range, divided by the sample count - a
/// best-effort constant (the same kind `kContinuousCharacterAmplitude`,
/// `mind_wave.cpp`, already is for a comparable "looks right" choice),
/// chosen so adjacent discrete eigenvalue spikes blend into a visually
/// continuous curve without smoothing away real spectral structure.
constexpr double kSigmaRangeFraction = 2.0;

/// @brief How many dense Bézier samples curveGraphFromPath() evaluates per
/// Path segment before arc-length-resampling down to targetNodeCount -
/// the same role sampleStrokeDense()'s own per-segment step count
/// (paint_application.cpp) plays for stamp placement, chosen generously
/// here since a CurveGraph's own node count is typically far smaller than
/// a stroke's own dense stamp-placement sampling needs.
constexpr int kDenseStepsPerSegment = 32;

/// @brief `point`, normalized into `CurvePoint`'s own space - the same
/// `frequencyToTimeScale` division `fitPathToPoints()`/brush sizing
/// already use, duplicated here (not shared via `paint_application.h`)
/// since that header's own `normalize()`/`distance()` helpers are
/// anonymous-namespace-private, specific to stamp placement.
CurvePoint normalize(const TimeFrequencyPoint& point, double frequencyToTimeScale) {
    return CurvePoint{point.timeSeconds, point.frequencyHz / frequencyToTimeScale};
}

double distanceBetween(const CurvePoint& a, const CurvePoint& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

CurvePoint lerp(const CurvePoint& a, const CurvePoint& b, double t) {
    return CurvePoint{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

}  // namespace

std::size_t CurveGraph::addNode(CurvePoint position) {
    nodes_.push_back(CurveNode{position, {}});
    return nodes_.size() - 1;
}

bool CurveGraph::addEdge(std::size_t a, std::size_t b) {
    if (a == b || a >= nodes_.size() || b >= nodes_.size()) {
        return false;
    }
    auto& neighborsOfA = nodes_[a].neighbors;
    if (std::find(neighborsOfA.begin(), neighborsOfA.end(), b) != neighborsOfA.end()) {
        return false;
    }
    neighborsOfA.push_back(b);
    nodes_[b].neighbors.push_back(a);
    return true;
}

CurveGraph curveGraphFromPath(const Path& path, double frequencyToTimeScale, std::size_t targetNodeCount) {
    CurveGraph graph;
    const auto& nodes = path.nodes();
    if (nodes.size() < 2) {
        return graph;
    }
    const std::size_t nodeCount = std::max<std::size_t>(2, targetNodeCount);

    // Dense Bézier evaluation - see this function's own docs.
    std::vector<CurvePoint> dense;
    const std::size_t segmentCount = nodes.size() - 1;
    for (std::size_t i = 0; i < segmentCount; ++i) {
        const PathNode& start = nodes[i];
        const PathNode& end = nodes[i + 1];
        const TimeFrequencyPoint p0 = start.anchor;
        const TimeFrequencyPoint p1 = start.handleOut.value_or(start.anchor);
        const TimeFrequencyPoint p2 = end.handleIn.value_or(end.anchor);
        const TimeFrequencyPoint p3 = end.anchor;
        if (i == 0) {
            dense.push_back(normalize(p0, frequencyToTimeScale));
        }
        for (int step = 1; step <= kDenseStepsPerSegment; ++step) {
            const double t = static_cast<double>(step) / static_cast<double>(kDenseStepsPerSegment);
            dense.push_back(normalize(evaluateCubicBezier(p0, p1, p2, p3, t), frequencyToTimeScale));
        }
    }

    // Cumulative arc length, then resample at nodeCount evenly-spaced
    // target lengths - see this function's own docs.
    std::vector<double> cumulative(dense.size(), 0.0);
    for (std::size_t i = 1; i < dense.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + distanceBetween(dense[i - 1], dense[i]);
    }
    const double total = cumulative.back();

    std::size_t segment = 0;
    for (std::size_t j = 0; j < nodeCount; ++j) {
        const double target = total * static_cast<double>(j) / static_cast<double>(nodeCount - 1);
        while (segment + 2 < dense.size() && cumulative[segment + 1] < target) {
            ++segment;
        }
        const double segmentStart = cumulative[segment];
        const double segmentEnd = cumulative[segment + 1];
        const double localT = segmentEnd > segmentStart ? (target - segmentStart) / (segmentEnd - segmentStart) : 0.0;
        graph.addNode(lerp(dense[segment], dense[segment + 1], std::clamp(localT, 0.0, 1.0)));
        if (j > 0) {
            graph.addEdge(j - 1, j);
        }
    }
    return graph;
}

std::vector<float> computeWaveKernelSignature(const CurveGraph& graph, std::size_t spectrumSize) {
    std::vector<float> spectrum(std::max<std::size_t>(1, spectrumSize), 0.0f);
    const auto n = static_cast<Eigen::Index>(graph.nodes().size());
    if (n < 2) {
        return spectrum;
    }

    Eigen::MatrixXd stiffness = Eigen::MatrixXd::Zero(n, n);
    Eigen::MatrixXd mass = Eigen::MatrixXd::Zero(n, n);
    bool anyEdge = false;
    for (Eigen::Index i = 0; i < n; ++i) {
        for (const std::size_t j : graph.nodes()[static_cast<std::size_t>(i)].neighbors) {
            const auto jIndex = static_cast<Eigen::Index>(j);
            if (jIndex <= i) {
                continue;  // Each undirected edge assembled exactly once.
            }
            const CurvePoint& a = graph.nodes()[static_cast<std::size_t>(i)].position;
            const CurvePoint& b = graph.nodes()[j].position;
            const double length = std::max(kMinimumEdgeLength, distanceBetween(a, b));
            const double weight = 1.0 / length;
            stiffness(i, i) += weight;
            stiffness(jIndex, jIndex) += weight;
            stiffness(i, jIndex) -= weight;
            stiffness(jIndex, i) -= weight;
            mass(i, i) += length / 2.0;
            mass(jIndex, jIndex) += length / 2.0;
            anyEdge = true;
        }
    }
    if (!anyEdge) {
        return spectrum;
    }
    // An isolated node (no edges of its own) would leave a zero row in
    // mass, making it singular - floor every diagonal entry so the
    // generalized eigensolver below stays well-posed even then.
    for (Eigen::Index i = 0; i < n; ++i) {
        mass(i, i) = std::max(mass(i, i), kMinimumEdgeLength);
    }

    const Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> solver(stiffness, mass);
    const Eigen::VectorXd& eigenvalues = solver.eigenvalues();
    const Eigen::MatrixXd& eigenvectors = solver.eigenvectors();

    // Index 0's own trivial, constant eigenvector (true for any graph
    // built this way - every row of `stiffness` sums to zero by
    // construction) is always excluded - see this function's own docs on
    // why (log(0) is undefined).
    std::vector<double> logEnergies;
    std::vector<double> weights;
    for (Eigen::Index k = 1; k < n; ++k) {
        const double lambda = eigenvalues(k);
        if (lambda <= 0.0) {
            continue;  // Numerical noise near the trivial mode, not a real second mode.
        }
        logEnergies.push_back(std::log(lambda));
        weights.push_back(eigenvectors.col(k).squaredNorm());
    }
    if (logEnergies.empty()) {
        return spectrum;
    }

    const double energyMin = *std::min_element(logEnergies.begin(), logEnergies.end());
    const double energyMax = *std::max_element(logEnergies.begin(), logEnergies.end());
    const double sigma =
        std::max(kMinimumSigma, (energyMax - energyMin) / static_cast<double>(spectrum.size()) * kSigmaRangeFraction);

    std::vector<double> raw(spectrum.size(), 0.0);
    double peak = 0.0;
    for (std::size_t s = 0; s < spectrum.size(); ++s) {
        const double energy = spectrum.size() > 1
                                   ? energyMin + (energyMax - energyMin) * static_cast<double>(s) /
                                                     static_cast<double>(spectrum.size() - 1)
                                   : energyMin;
        double value = 0.0;
        for (std::size_t k = 0; k < logEnergies.size(); ++k) {
            const double diff = energy - logEnergies[k];
            value += weights[k] * std::exp(-(diff * diff) / (2.0 * sigma * sigma));
        }
        raw[s] = value;
        peak = std::max(peak, value);
    }
    if (peak > 0.0) {
        for (std::size_t s = 0; s < spectrum.size(); ++s) {
            spectrum[s] = static_cast<float>(raw[s] / peak);
        }
    }
    return spectrum;
}

}  // namespace sound_mind::core
