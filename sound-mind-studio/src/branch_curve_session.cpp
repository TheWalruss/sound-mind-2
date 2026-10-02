#include "sound_mind/studio/branch_curve_session.h"

#include <algorithm>

namespace sound_mind::studio {

using sound_mind::core::BranchGraft;
using sound_mind::core::OperationId;
using sound_mind::core::Path;
using sound_mind::core::TimeFrequencyPoint;

void BranchCurveSession::start(const Path& trunkPath, OperationId trunkOperationId) {
    active_ = true;
    branches_.clear();
    branchOperationIds_.clear();
    pendingParentIndex_.reset();

    BranchGraft trunk;
    trunk.path = trunkPath;
    branches_.push_back(std::move(trunk));
    branchOperationIds_.push_back(trunkOperationId);
}

void BranchCurveSession::end() {
    active_ = false;
    branches_.clear();
    branchOperationIds_.clear();
    pendingParentIndex_.reset();
}

void BranchCurveSession::noteGraftCandidate(OperationId operationId, TimeFrequencyPoint point) {
    if (!active_) {
        return;
    }
    const auto it = std::find(branchOperationIds_.begin(), branchOperationIds_.end(), operationId);
    if (it == branchOperationIds_.end()) {
        return;
    }
    pendingParentIndex_ = static_cast<std::size_t>(std::distance(branchOperationIds_.begin(), it));
    pendingGraftPoint_ = point;
}

bool BranchCurveSession::addBranch(const Path& path, OperationId operationId) {
    if (!active_ || !pendingParentIndex_.has_value()) {
        return false;
    }
    BranchGraft branch;
    branch.path = path;
    branch.parentIndex = pendingParentIndex_;
    branch.graftPoint = pendingGraftPoint_;
    branches_.push_back(std::move(branch));
    branchOperationIds_.push_back(operationId);
    pendingParentIndex_.reset();
    return true;
}

}  // namespace sound_mind::studio
