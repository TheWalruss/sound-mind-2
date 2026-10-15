#include "sound_mind/core/guide_path_operation.h"

#include <algorithm>

namespace sound_mind::core {

TimeFrequencyRect GuidePathOperation::bounds() const {
    if (branches_.empty()) {
        return {};
    }
    TimeFrequencyRect result = branches_.front().path.bounds();
    for (std::size_t i = 1; i < branches_.size(); ++i) {
        const TimeFrequencyRect branch = branches_[i].path.bounds();
        result.startTimeSeconds = std::min(result.startTimeSeconds, branch.startTimeSeconds);
        result.endTimeSeconds = std::max(result.endTimeSeconds, branch.endTimeSeconds);
        result.lowFrequencyHz = std::min(result.lowFrequencyHz, branch.lowFrequencyHz);
        result.highFrequencyHz = std::max(result.highFrequencyHz, branch.highFrequencyHz);
    }
    return result;
}

std::unique_ptr<Operation> GuidePathOperation::translatedCopy(OperationId newId, double deltaTimeSeconds,
                                                              double deltaFrequencyBins,
                                                              const sound_mind::codec::StreamCodecConfig& config) const {
    std::vector<BranchGraft> moved = branches_;
    for (BranchGraft& branch : moved) {
        branch.path = branch.path.translated(deltaTimeSeconds, deltaFrequencyBins, config);
        // A one-node path moves a point exactly the way the branch's own
        // nodes just moved, bin-space frequency shift included.
        Path graft;
        PathNode node;
        node.anchor = branch.graftPoint;
        node.type = PathNodeType::Corner;
        graft.addNode(node);
        branch.graftPoint = graft.translated(deltaTimeSeconds, deltaFrequencyBins, config).nodes().front().anchor;
    }
    return std::make_unique<GuidePathOperation>(newId, targetLayer_, std::move(moved), id());
}

void to_json(nlohmann::json& json, const BranchGraft& branch) {
    json = nlohmann::json{{"path", branch.path}, {"graftPoint", branch.graftPoint}};
    if (branch.parentIndex.has_value()) {
        json["parentIndex"] = *branch.parentIndex;
    }
}

void from_json(const nlohmann::json& json, BranchGraft& branch) {
    branch.path = json.at("path").get<Path>();
    branch.graftPoint = json.at("graftPoint").get<TimeFrequencyPoint>();
    branch.parentIndex =
        json.contains("parentIndex") ? std::optional(json.at("parentIndex").get<std::size_t>()) : std::nullopt;
}

}  // namespace sound_mind::core
