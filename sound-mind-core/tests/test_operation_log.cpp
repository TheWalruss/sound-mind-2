#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_operation.h"

using sound_mind::core::LayerId;
using sound_mind::core::OperationId;
using sound_mind::core::OperationLog;
using sound_mind::core::PaintOperation;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
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

TEST_CASE("An empty OperationLog's pre-v0.0.24.1 JSON shape (a bare empty array) still parses",
          "[core][operation_log]") {
    const nlohmann::json legacyEmpty = nlohmann::json::array();
    const OperationLog log = legacyEmpty.get<OperationLog>();
    REQUIRE(log.size() == 0);
}
