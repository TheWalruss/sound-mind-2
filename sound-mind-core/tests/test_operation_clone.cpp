#include <catch2/catch_test_macros.hpp>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/gradient.h"
#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/operation_clone.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/tool_configuration.h"

using sound_mind::core::BranchGraft;
using sound_mind::core::cloneOperation;
using sound_mind::core::FillOperation;
using sound_mind::core::GuidePathOperation;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;

namespace {

Path twoNodePath() {
    Path path;
    PathNode a;
    a.anchor = TimeFrequencyPoint{0.1, 300.0};
    a.type = PathNodeType::Corner;
    PathNode b;
    b.anchor = TimeFrequencyPoint{0.4, 600.0};
    b.type = PathNodeType::Corner;
    path.addNode(a);
    path.addNode(b);
    return path;
}

}  // namespace

TEST_CASE("cloneOperation duplicates a stroke onto a layer, superseding nothing", "[core][operation_clone]") {
    sound_mind::core::ProceduralConfiguration config;
    config.setSize(0.03);
    const PaintOperation original(OperationId{1}, LayerId{2}, twoNodePath(), config.clone(), OperationId{7});

    const auto clone = cloneOperation(original, OperationId{9}, LayerId{3});
    const auto* paint = dynamic_cast<const PaintOperation*>(clone.get());

    REQUIRE(paint != nullptr);
    CHECK(paint->id() == OperationId{9});
    CHECK(paint->targetLayer() == LayerId{3});
    CHECK_FALSE(paint->supersedes().has_value());
    CHECK(paint->path().nodes().size() == 2);
    CHECK(paint->config().size() == 0.03);
}

TEST_CASE("cloneOperation duplicates a fill and a guide path", "[core][operation_clone]") {
    const FillOperation fill(OperationId{1}, LayerId{2}, TimeFrequencyRect{0.1, 0.2, 100.0, 200.0},
                             sound_mind::core::silenceGradient());
    const auto fillClone = cloneOperation(fill, OperationId{5}, LayerId{2});
    const auto* clonedFill = dynamic_cast<const FillOperation*>(fillClone.get());
    REQUIRE(clonedFill != nullptr);
    CHECK(clonedFill->bounds().endTimeSeconds == 0.2);
    CHECK_FALSE(clonedFill->supersedes().has_value());

    const GuidePathOperation guide(OperationId{1}, LayerId{2},
                                   {BranchGraft{twoNodePath(), std::nullopt, TimeFrequencyPoint{}},
                                    BranchGraft{twoNodePath(), std::size_t{0}, TimeFrequencyPoint{0.2, 400.0}}});
    const auto guideClone = cloneOperation(guide, OperationId{6}, LayerId{4});
    const auto* clonedGuide = dynamic_cast<const GuidePathOperation*>(guideClone.get());
    REQUIRE(clonedGuide != nullptr);
    CHECK(clonedGuide->isBranching());
    CHECK(clonedGuide->targetLayer() == LayerId{4});
}
