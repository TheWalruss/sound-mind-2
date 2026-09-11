#include "sound_mind/core/path.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief A point in the simplify/smoothing algorithm's own normalized
/// space - `frequencyHz / frequencyToTimeScale`, so both axes carry the
/// same weight (see fitPathToPoints()'s own docs).
struct NormalizedPoint {
    double timeSeconds = 0.0;
    double normalizedFrequency = 0.0;
};

double distance(const NormalizedPoint& a, const NormalizedPoint& b) {
    const double dt = a.timeSeconds - b.timeSeconds;
    const double df = a.normalizedFrequency - b.normalizedFrequency;
    return std::sqrt(dt * dt + df * df);
}

/// @brief Perpendicular distance from `point` to the infinite line through
/// `lineStart`/`lineEnd` - falls back to the plain distance to `lineStart`
/// when the two coincide, so a degenerate (zero-length) segment doesn't
/// divide by zero.
double perpendicularDistance(const NormalizedPoint& point, const NormalizedPoint& lineStart,
                              const NormalizedPoint& lineEnd) {
    const double lineLength = distance(lineStart, lineEnd);
    if (lineLength <= 0.0) {
        return distance(point, lineStart);
    }
    // |cross product| / |line vector| - the standard point-to-line distance formula.
    const double cross = (lineEnd.timeSeconds - lineStart.timeSeconds) * (lineStart.normalizedFrequency - point.normalizedFrequency) -
                          (lineStart.timeSeconds - point.timeSeconds) * (lineEnd.normalizedFrequency - lineStart.normalizedFrequency);
    return std::abs(cross) / lineLength;
}

/// @brief Ramer-Douglas-Peucker simplification: recursively keeps only
/// the points that matter to the curve's own shape, discarding any point
/// within `tolerance` of the straight line already implied by its
/// surviving neighbors. `keep` is marked `true` for every surviving
/// index; `points[first]`/`points[last]` are always kept (the recursion's
/// own base case).
void simplifyRange(const std::vector<NormalizedPoint>& points, std::size_t first, std::size_t last, double tolerance,
                    std::vector<bool>& keep) {
    if (last <= first + 1) {
        return;
    }

    double maxDistance = 0.0;
    std::size_t maxIndex = first;
    for (std::size_t i = first + 1; i < last; ++i) {
        const double d = perpendicularDistance(points[i], points[first], points[last]);
        if (d > maxDistance) {
            maxDistance = d;
            maxIndex = i;
        }
    }

    if (maxDistance > tolerance) {
        keep[maxIndex] = true;
        simplifyRange(points, first, maxIndex, tolerance, keep);
        simplifyRange(points, maxIndex, last, tolerance, keep);
    }
}

TimeFrequencyPoint denormalize(const NormalizedPoint& point, double frequencyToTimeScale) {
    return TimeFrequencyPoint{point.timeSeconds, point.normalizedFrequency * frequencyToTimeScale};
}

}  // namespace

std::size_t Path::addNode(PathNode node) {
    nodes_.push_back(std::move(node));
    return nodes_.size() - 1;
}

bool Path::insertNode(std::size_t index, PathNode node) {
    if (index > nodes_.size()) {
        return false;
    }
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(index), std::move(node));
    return true;
}

bool Path::removeNode(std::size_t index) {
    if (index >= nodes_.size()) {
        return false;
    }
    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Path::setNode(std::size_t index, PathNode node) {
    if (index >= nodes_.size()) {
        return false;
    }
    nodes_[index] = std::move(node);
    return true;
}

Path Path::translated(double deltaTimeSeconds, double deltaFrequencyBins,
                        const sound_mind::codec::StreamCodecConfig& config) const {
    Path result = *this;
    // Each point's own frequency shifts via its own bin position - see
    // translateFrequencyByBins()'s own docs for why that, not a shared
    // Hz offset, is what keeps the path's own shape intact.
    const auto shift = [&](TimeFrequencyPoint& point) {
        point.timeSeconds += deltaTimeSeconds;
        point.frequencyHz = translateFrequencyByBins(static_cast<float>(point.frequencyHz), deltaFrequencyBins, config);
    };
    for (PathNode& node : result.nodes_) {
        shift(node.anchor);
        if (node.handleIn) {
            shift(*node.handleIn);
        }
        if (node.handleOut) {
            shift(*node.handleOut);
        }
    }
    return result;
}

TimeFrequencyRect Path::bounds() const noexcept {
    if (nodes_.empty()) {
        return TimeFrequencyRect{};
    }

    double minTime = std::numeric_limits<double>::max();
    double maxTime = std::numeric_limits<double>::lowest();
    double minFrequency = std::numeric_limits<double>::max();
    double maxFrequency = std::numeric_limits<double>::lowest();

    const auto include = [&](const TimeFrequencyPoint& point) {
        minTime = std::min(minTime, point.timeSeconds);
        maxTime = std::max(maxTime, point.timeSeconds);
        minFrequency = std::min(minFrequency, point.frequencyHz);
        maxFrequency = std::max(maxFrequency, point.frequencyHz);
    };

    for (const PathNode& node : nodes_) {
        include(node.anchor);
        if (node.handleIn) {
            include(*node.handleIn);
        }
        if (node.handleOut) {
            include(*node.handleOut);
        }
    }

    TimeFrequencyRect rect;
    rect.startTimeSeconds = minTime;
    rect.endTimeSeconds = maxTime;
    rect.lowFrequencyHz = minFrequency;
    rect.highFrequencyHz = maxFrequency;
    return rect;
}

void to_json(nlohmann::json& json, const TimeFrequencyPoint& point) {
    json = nlohmann::json{{"timeSeconds", point.timeSeconds}, {"frequencyHz", point.frequencyHz}};
}

void from_json(const nlohmann::json& json, TimeFrequencyPoint& point) {
    json.at("timeSeconds").get_to(point.timeSeconds);
    json.at("frequencyHz").get_to(point.frequencyHz);
}

void to_json(nlohmann::json& json, const PathNode& node) {
    json = nlohmann::json{{"anchor", node.anchor}, {"type", node.type}};
    if (node.handleIn) {
        json["handleIn"] = *node.handleIn;
    }
    if (node.handleOut) {
        json["handleOut"] = *node.handleOut;
    }
}

void from_json(const nlohmann::json& json, PathNode& node) {
    json.at("anchor").get_to(node.anchor);
    json.at("type").get_to(node.type);
    node.handleIn = json.contains("handleIn") ? std::optional(json.at("handleIn").get<TimeFrequencyPoint>())
                                               : std::nullopt;
    node.handleOut = json.contains("handleOut") ? std::optional(json.at("handleOut").get<TimeFrequencyPoint>())
                                                 : std::nullopt;
}

void to_json(nlohmann::json& json, const Path& path) {
    json = nlohmann::json{{"nodes", path.nodes_}, {"gradient", path.gradient_}};
}

void from_json(const nlohmann::json& json, Path& path) {
    json.at("nodes").get_to(path.nodes_);
    json.at("gradient").get_to(path.gradient_);
}

Path fitPathToPoints(const std::vector<TimeFrequencyPoint>& rawPoints, double frequencyToTimeScale,
                      double simplifyToleranceSeconds) {
    Path path;
    if (rawPoints.size() < 2 || frequencyToTimeScale <= 0.0) {
        return path;
    }

    std::vector<NormalizedPoint> normalized;
    normalized.reserve(rawPoints.size());
    for (const TimeFrequencyPoint& point : rawPoints) {
        normalized.push_back(NormalizedPoint{point.timeSeconds, point.frequencyHz / frequencyToTimeScale});
    }

    std::vector<bool> keep(normalized.size(), false);
    keep.front() = true;
    keep.back() = true;
    simplifyRange(normalized, 0, normalized.size() - 1, simplifyToleranceSeconds, keep);

    std::vector<NormalizedPoint> anchors;
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        if (keep[i]) {
            anchors.push_back(normalized[i]);
        }
    }

    // Fewer than 2 surviving anchors can't happen (the endpoints are
    // always kept), but exactly 2 means a straight line - no interior
    // point to derive a tangent from, so both ends stay plain corners.
    if (anchors.size() < 3) {
        for (const NormalizedPoint& anchor : anchors) {
            PathNode node;
            node.anchor = denormalize(anchor, frequencyToTimeScale);
            node.type = PathNodeType::Corner;
            path.addNode(node);
        }
        return path;
    }

    for (std::size_t i = 0; i < anchors.size(); ++i) {
        PathNode node;
        node.anchor = denormalize(anchors[i], frequencyToTimeScale);

        if (i == 0 || i + 1 == anchors.size()) {
            node.type = PathNodeType::Corner;
            path.addNode(node);
            continue;
        }

        // Catmull-Rom-style tangent through this anchor's now-sparse
        // neighbors, converted to a pair of Bézier handles one third of
        // the way to each neighbor - the standard, well-behaved way to
        // turn a central-difference tangent into a smooth cubic segment.
        const NormalizedPoint& prev = anchors[i - 1];
        const NormalizedPoint& next = anchors[i + 1];
        const double tangentTime = (next.timeSeconds - prev.timeSeconds) / 2.0;
        const double tangentFrequency = (next.normalizedFrequency - prev.normalizedFrequency) / 2.0;

        NormalizedPoint handleInNorm{anchors[i].timeSeconds - tangentTime / 3.0,
                                      anchors[i].normalizedFrequency - tangentFrequency / 3.0};
        NormalizedPoint handleOutNorm{anchors[i].timeSeconds + tangentTime / 3.0,
                                       anchors[i].normalizedFrequency + tangentFrequency / 3.0};

        node.type = PathNodeType::Smooth;
        node.handleIn = denormalize(handleInNorm, frequencyToTimeScale);
        node.handleOut = denormalize(handleOutNorm, frequencyToTimeScale);
        path.addNode(node);
    }

    return path;
}

}  // namespace sound_mind::core
