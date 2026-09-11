#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"

using sound_mind::core::Clip;
using sound_mind::core::FillOperation;
using sound_mind::core::Gradient;
using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::OperationLog;
using sound_mind::core::PaintOperation;
using sound_mind::core::PasteOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::core::TimeFrequencyRect;
using sound_mind::core::ToolConfiguration;

namespace {

Path makeTestPath(double startTime = 0.0, double endTime = 1.0) {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{startTime, 100.0};
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{endTime, 500.0};
    path.addNode(end);
    return path;
}

std::unique_ptr<PaintOperation> makePaint(OperationLog& log, LayerId layer,
                                           std::optional<OperationId> supersedes = std::nullopt) {
    const OperationId id = log.reserveId();
    return std::make_unique<PaintOperation>(id, layer, makeTestPath(), ToolConfiguration{}, supersedes);
}

}  // namespace

TEST_CASE("A fresh OperationLog is empty", "[core][operation_log]") {
    const OperationLog log;
    REQUIRE(log.size() == 0);
    REQUIRE_FALSE(log.canUndo());
    REQUIRE_FALSE(log.canRedo());
}

TEST_CASE("reserveId returns unique, increasing ids", "[core][operation_log]") {
    OperationLog log;
    const OperationId first = log.reserveId();
    const OperationId second = log.reserveId();
    REQUIRE(first != second);
    REQUIRE(second > first);
}

TEST_CASE("append adds an operation and it becomes accessible via at()", "[core][operation_log]") {
    OperationLog log;
    const OperationId id = log.reserveId();
    log.append(std::make_unique<PaintOperation>(id, LayerId{1}, makeTestPath(), ToolConfiguration{}));

    REQUIRE(log.size() == 1);
    REQUIRE(log.at(0).id() == id);
}

TEST_CASE("append leaves the log undo-able but not redo-able", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));
    REQUIRE(log.canUndo());
    REQUIRE_FALSE(log.canRedo());
}

TEST_CASE("undo hides the most recent operation from activeOperationsTargeting", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));

    log.undo();

    REQUIRE(log.activeOperationsTargeting(LayerId{1}).empty());
    REQUIRE(log.size() == 1);  // still logged, just inactive.
}

TEST_CASE("redo restores an undone operation", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));
    log.undo();

    log.redo();

    REQUIRE(log.activeOperationsTargeting(LayerId{1}).size() == 1);
}

TEST_CASE("undo is a no-op on an empty log", "[core][operation_log]") {
    OperationLog log;
    log.undo();
    REQUIRE(log.size() == 0);
    REQUIRE_FALSE(log.canUndo());
}

TEST_CASE("redo is a no-op when nothing was undone", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));
    log.redo();
    REQUIRE(log.activeOperationsTargeting(LayerId{1}).size() == 1);
}

TEST_CASE("appending after an undo discards the redo tail", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));
    log.undo();

    log.append(makePaint(log, LayerId{1}));

    // The undone operation is genuinely gone, not just inactive - a fresh
    // append past an undo() replaces it rather than coexisting with it.
    REQUIRE(log.size() == 1);
    REQUIRE_FALSE(log.canRedo());
    REQUIRE(log.activeOperationsTargeting(LayerId{1}).size() == 1);
}

TEST_CASE("activeOperationsTargeting filters to the given layer only", "[core][operation_log]") {
    OperationLog log;
    log.append(makePaint(log, LayerId{1}));
    log.append(makePaint(log, LayerId{2}));

    REQUIRE(log.activeOperationsTargeting(LayerId{1}).size() == 1);
    REQUIRE(log.activeOperationsTargeting(LayerId{2}).size() == 1);
    REQUIRE(log.activeOperationsTargeting(LayerId{3}).empty());
}

TEST_CASE("activeOperationsTargeting returns operations in log order", "[core][operation_log]") {
    OperationLog log;
    const OperationId first = log.reserveId();
    log.append(std::make_unique<PaintOperation>(first, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId second = log.reserveId();
    log.append(std::make_unique<PaintOperation>(second, LayerId{1}, makeTestPath(1.0, 2.0), ToolConfiguration{}));

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 2);
    REQUIRE(active[0]->id() == first);
    REQUIRE(active[1]->id() == second);
}

TEST_CASE("activeOperationsTargeting excludes an operation superseded by a later active one",
          "[core][operation_log]") {
    OperationLog log;
    const OperationId original = log.reserveId();
    log.append(std::make_unique<PaintOperation>(original, LayerId{1}, makeTestPath(), ToolConfiguration{}));
    log.append(makePaint(log, LayerId{1}, original));  // the replacement.

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 1);
    REQUIRE(active[0]->supersedes().has_value());
    REQUIRE(active[0]->supersedes().value() == original);
}

TEST_CASE("undoing a superseding operation makes the original visible again", "[core][operation_log]") {
    OperationLog log;
    const OperationId original = log.reserveId();
    log.append(std::make_unique<PaintOperation>(original, LayerId{1}, makeTestPath(), ToolConfiguration{}));
    log.append(makePaint(log, LayerId{1}, original));

    log.undo();  // hides the replacement, not the original.

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 1);
    REQUIRE(active[0]->id() == original);
}

TEST_CASE("a superseding append preserves the superseded operation's own position in the stack, "
          "not just its own append position",
          "[core][operation_log]") {
    // The actual bug this mechanism exists to fix: A, B, C painted in that
    // order (bottom to top); superseding B (a Pick move/modify, say) used
    // to always append the replacement at the very end of the log, which
    // activeOperationsTargeting() then read back as "B' is now on top of
    // C" - silently reordering the stack. B' must stay exactly where B
    // was: between A and C.
    OperationLog log;
    const OperationId a = log.reserveId();
    log.append(std::make_unique<PaintOperation>(a, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId b = log.reserveId();
    log.append(std::make_unique<PaintOperation>(b, LayerId{1}, makeTestPath(1.0, 2.0), ToolConfiguration{}));
    const OperationId c = log.reserveId();
    log.append(std::make_unique<PaintOperation>(c, LayerId{1}, makeTestPath(2.0, 3.0), ToolConfiguration{}));

    log.append(makePaint(log, LayerId{1}, b));  // supersedes B - "moving" it, in effect.

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 3);
    REQUIRE(active[0]->id() == a);
    REQUIRE(active[1]->supersedes().has_value());
    REQUIRE(*active[1]->supersedes() == b);  // B's replacement, in B's own old slot.
    REQUIRE(active[2]->id() == c);
}

TEST_CASE("a chain of several supersedes on the same original all preserve its own stack position",
          "[core][operation_log]") {
    OperationLog log;
    const OperationId a = log.reserveId();
    log.append(std::make_unique<PaintOperation>(a, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId b = log.reserveId();
    log.append(std::make_unique<PaintOperation>(b, LayerId{1}, makeTestPath(1.0, 2.0), ToolConfiguration{}));
    const OperationId c = log.reserveId();
    log.append(std::make_unique<PaintOperation>(c, LayerId{1}, makeTestPath(2.0, 3.0), ToolConfiguration{}));

    log.append(makePaint(log, LayerId{1}, b));   // move B once.
    log.append(makePaint(log, LayerId{1}, log.at(3).id()));  // move it again.

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 3);
    REQUIRE(active[0]->id() == a);
    REQUIRE(active[1]->id() == log.at(4).id());  // the second (latest) move.
    REQUIRE(active[2]->id() == c);
}

TEST_CASE("a fresh, non-superseding append always places the new operation on top of the stack",
          "[core][operation_log]") {
    OperationLog log;
    const OperationId a = log.reserveId();
    log.append(std::make_unique<PaintOperation>(a, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId b = log.reserveId();
    log.append(std::make_unique<PaintOperation>(b, LayerId{1}, makeTestPath(1.0, 2.0), ToolConfiguration{}));

    log.append(makePaint(log, LayerId{1}, a));  // moves A - must NOT jump above B.
    const OperationId freshOnTop = log.reserveId();
    log.append(std::make_unique<PaintOperation>(freshOnTop, LayerId{1}, makeTestPath(3.0, 4.0), ToolConfiguration{}));

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 3);
    REQUIRE(active.back()->id() == freshOnTop);  // a genuinely new object always goes on top.
}

TEST_CASE("An OperationLog's stack order round-trips through JSON", "[core][operation_log]") {
    OperationLog log;
    const OperationId a = log.reserveId();
    log.append(std::make_unique<PaintOperation>(a, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId b = log.reserveId();
    log.append(std::make_unique<PaintOperation>(b, LayerId{1}, makeTestPath(1.0, 2.0), ToolConfiguration{}));
    const OperationId c = log.reserveId();
    log.append(std::make_unique<PaintOperation>(c, LayerId{1}, makeTestPath(2.0, 3.0), ToolConfiguration{}));
    log.append(makePaint(log, LayerId{1}, b));

    const nlohmann::json json = log;
    const OperationLog roundTripped = json.get<OperationLog>();

    const auto active = roundTripped.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 3);
    REQUIRE(active[0]->id() == a);
    REQUIRE(active[1]->supersedes().has_value());
    REQUIRE(*active[1]->supersedes() == b);
    REQUIRE(active[2]->id() == c);
}

TEST_CASE("An OperationLog's pre-stack-order JSON shape (no \"stackOrder\" field) falls back to append order",
          "[core][operation_log]") {
    const nlohmann::json legacy = nlohmann::json{
        {"operations",
         nlohmann::json::array({
             nlohmann::json{{"kind", "paint"},
                             {"id", 1},
                             {"targetLayer", 1},
                             {"path", makeTestPath(0.0, 1.0)},
                             {"config", ToolConfiguration{}}},
             nlohmann::json{{"kind", "paint"},
                             {"id", 2},
                             {"targetLayer", 1},
                             {"path", makeTestPath(1.0, 2.0)},
                             {"config", ToolConfiguration{}}},
         })},
        {"activeCount", 2},
        {"nextId", 3},
    };

    const OperationLog log = legacy.get<OperationLog>();

    const auto active = log.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 2);
    REQUIRE(active[0]->id() == OperationId{1});
    REQUIRE(active[1]->id() == OperationId{2});
}

TEST_CASE("An OperationLog with real PaintOperations round-trips through JSON", "[core][operation_log]") {
    OperationLog log;
    const OperationId original = log.reserveId();
    log.append(std::make_unique<PaintOperation>(original, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    log.append(makePaint(log, LayerId{2}, original));
    log.undo();  // the second operation is now inactive.

    const nlohmann::json json = log;
    const OperationLog roundTripped = json.get<OperationLog>();

    REQUIRE(roundTripped.size() == 2);
    REQUIRE(roundTripped.canRedo());
    REQUIRE(roundTripped.activeOperationsTargeting(LayerId{1}).size() == 1);
    REQUIRE(roundTripped.activeOperationsTargeting(LayerId{2}).empty());

    // reserveId() must not collide with what was already logged.
    const OperationId fresh = const_cast<OperationLog&>(roundTripped).reserveId();
    REQUIRE(fresh != original);
}

TEST_CASE("An OperationLog with a mix of PaintOperations and FillOperations round-trips through JSON",
          "[core][operation_log]") {
    OperationLog log;
    const OperationId paintId = log.reserveId();
    log.append(std::make_unique<PaintOperation>(paintId, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));
    const OperationId fillId = log.reserveId();
    log.append(std::make_unique<FillOperation>(fillId, LayerId{1}, TimeFrequencyRect{}, Gradient{}));

    const nlohmann::json json = log;
    const OperationLog roundTripped = json.get<OperationLog>();

    REQUIRE(roundTripped.size() == 2);
    const auto active = roundTripped.activeOperationsTargeting(LayerId{1});
    REQUIRE(active.size() == 2);
    REQUIRE(dynamic_cast<const PaintOperation*>(active[0]) != nullptr);
    REQUIRE(dynamic_cast<const FillOperation*>(active[1]) != nullptr);
}

TEST_CASE("An OperationLog with a PasteOperation round-trips through JSON, targeting a different layer than "
          "the clip's own bounds might suggest",
          "[core][operation_log]") {
    OperationLog log;
    const OperationId paintId = log.reserveId();
    log.append(std::make_unique<PaintOperation>(paintId, LayerId{1}, makeTestPath(0.0, 1.0), ToolConfiguration{}));

    Clip clip;
    clip.frameCount = 2;
    clip.binCount = 2;
    clip.leftMagnitudeDb = {-1.0f, -2.0f, -3.0f, -4.0f};
    clip.rightMagnitudeDb = {-5.0f, -6.0f, -7.0f, -8.0f};
    clip.sharedPhaseRadians = {0.1f, 0.2f, 0.3f, 0.4f};
    const OperationId pasteId = log.reserveId();
    // Deliberately targets a *different* layer than the paint above - a
    // paste's own target layer is independent of anything else in the log,
    // per this installment's own "not necessarily the same layer" design.
    log.append(std::make_unique<PasteOperation>(pasteId, LayerId{2}, TimeFrequencyRect{}, clip));

    const nlohmann::json json = log;
    const OperationLog roundTripped = json.get<OperationLog>();

    REQUIRE(roundTripped.size() == 2);
    const auto layerOneOps = roundTripped.activeOperationsTargeting(LayerId{1});
    const auto layerTwoOps = roundTripped.activeOperationsTargeting(LayerId{2});
    REQUIRE(layerOneOps.size() == 1);
    REQUIRE(layerTwoOps.size() == 1);
    REQUIRE(dynamic_cast<const PaintOperation*>(layerOneOps[0]) != nullptr);
    const auto* restoredPaste = dynamic_cast<const PasteOperation*>(layerTwoOps[0]);
    REQUIRE(restoredPaste != nullptr);
    REQUIRE(restoredPaste->clip().leftMagnitudeDb == clip.leftMagnitudeDb);
}

TEST_CASE("An OperationLog fails to load JSON with an unrecognized operation kind", "[core][operation_log]") {
    const nlohmann::json malformed = nlohmann::json{
        {"operations", nlohmann::json::array({nlohmann::json{{"kind", "not-a-real-kind"}, {"id", 1}}})},
        {"activeCount", 1},
        {"nextId", 2},
    };
    REQUIRE_THROWS_AS(malformed.get<OperationLog>(), std::invalid_argument);
}

TEST_CASE("An empty OperationLog's pre-v0.0.24.1 JSON shape (a bare empty array) still parses",
          "[core][operation_log]") {
    const nlohmann::json legacyEmpty = nlohmann::json::array();
    const OperationLog log = legacyEmpty.get<OperationLog>();
    REQUIRE(log.size() == 0);
}
