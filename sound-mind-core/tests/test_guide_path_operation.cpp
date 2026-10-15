#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::codec::StreamImage;
using sound_mind::core::BranchGraft;
using sound_mind::core::GuidePathOperation;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::OperationLog;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;

namespace {

Path straightPath(double startTime, double endTime, double frequencyHz) {
    Path path;
    PathNode a;
    a.anchor = TimeFrequencyPoint{startTime, frequencyHz};
    a.type = PathNodeType::Corner;
    PathNode b;
    b.anchor = TimeFrequencyPoint{endTime, frequencyHz};
    b.type = PathNodeType::Corner;
    path.addNode(a);
    path.addNode(b);
    return path;
}

/// @brief A trunk at 500 Hz plus one branch at 1 kHz grafted on at 0.3 s.
std::vector<BranchGraft> twoBranches() {
    BranchGraft trunk{straightPath(0.1, 0.5, 500.0), std::nullopt, TimeFrequencyPoint{}};
    BranchGraft branch{straightPath(0.3, 0.8, 1000.0), std::size_t{0}, TimeFrequencyPoint{0.3, 500.0}};
    return {trunk, branch};
}

}  // namespace

TEST_CASE("A guide path with one branch is a regular path, with several a branching one", "[core][guide_path]") {
    const GuidePathOperation regular(OperationId{1}, LayerId{2}, {twoBranches().front()});
    CHECK_FALSE(regular.isBranching());
    CHECK_FALSE(regular.isEmpty());
    CHECK(regular.targetLayer() == LayerId{2});

    const GuidePathOperation branching(OperationId{2}, LayerId{2}, twoBranches());
    CHECK(branching.isBranching());

    const GuidePathOperation tombstone(OperationId{3}, LayerId{2}, {});
    CHECK(tombstone.isEmpty());
    CHECK_FALSE(tombstone.isBranching());
}

TEST_CASE("A guide path's bounds cover every branch", "[core][guide_path]") {
    const GuidePathOperation operation(OperationId{1}, LayerId{1}, twoBranches());
    const auto bounds = operation.bounds();
    CHECK(bounds.startTimeSeconds == Catch::Approx(0.1));
    CHECK(bounds.endTimeSeconds == Catch::Approx(0.8));
    CHECK(bounds.lowFrequencyHz == Catch::Approx(500.0));
    CHECK(bounds.highFrequencyHz == Catch::Approx(1000.0));
}

TEST_CASE("translatedCopy moves every branch and graft point and supersedes the original", "[core][guide_path]") {
    const GuidePathOperation operation(OperationId{1}, LayerId{4}, twoBranches());
    const auto config = sound_mind::core::streamCodecConfigFor(sound_mind::core::ProjectSettings{});

    const auto copy = operation.translatedCopy(OperationId{9}, 0.25, 0.0, config);
    const auto* moved = dynamic_cast<const GuidePathOperation*>(copy.get());

    REQUIRE(moved != nullptr);
    CHECK(moved->id() == OperationId{9});
    CHECK(moved->supersedes() == OperationId{1});
    CHECK(moved->targetLayer() == LayerId{4});
    REQUIRE(moved->branches().size() == 2);
    CHECK(moved->branches()[0].path.nodes().front().anchor.timeSeconds == Catch::Approx(0.35));
    CHECK(moved->branches()[1].graftPoint.timeSeconds == Catch::Approx(0.55));
    CHECK(moved->branches()[1].graftPoint.frequencyHz == Catch::Approx(500.0).margin(0.5));
    CHECK(moved->branches()[1].parentIndex == std::size_t{0});
}

TEST_CASE("An OperationLog with a branching guide path round-trips through JSON", "[core][guide_path][operation_log]") {
    OperationLog log;
    const OperationId id = log.reserveId();
    log.append(std::make_unique<GuidePathOperation>(id, LayerId{3}, twoBranches()));

    const nlohmann::json json = log;
    const OperationLog restored = json.get<OperationLog>();

    const auto active = restored.activeOperationsTargeting(LayerId{3});
    REQUIRE(active.size() == 1);
    const auto* guide = dynamic_cast<const GuidePathOperation*>(active[0]);
    REQUIRE(guide != nullptr);
    REQUIRE(guide->branches().size() == 2);
    CHECK_FALSE(guide->branches()[0].parentIndex.has_value());
    CHECK(guide->branches()[1].parentIndex == std::size_t{0});
    CHECK(guide->branches()[1].graftPoint.timeSeconds == Catch::Approx(0.3));
    CHECK(guide->branches()[1].path.nodes().size() == 2);
}

TEST_CASE("A guide path is silent - replaying a layer leaves its content untouched", "[core][guide_path]") {
    StreamImage base;
    base.config.binCount = 4;
    base.frameCount = 8;
    base.leftMagnitudeDb.assign(32, -40.0f);
    base.rightMagnitudeDb.assign(32, -40.0f);
    base.sharedPhaseRadians.assign(32, 0.0f);
    const GuidePathOperation operation(OperationId{1}, LayerId{1}, twoBranches());

    const StreamImage result = sound_mind::core::rebuildPaintedContent(base, {&operation}, 1000.0);

    CHECK(result.leftMagnitudeDb == base.leftMagnitudeDb);
    CHECK(result.rightMagnitudeDb == base.rightMagnitudeDb);
}
