#include "test_branch_curve_session.h"

#include <QtTest/QtTest>

#include "sound_mind/core/path.h"
#include "sound_mind/studio/branch_curve_session.h"

using sound_mind::core::OperationId;
using sound_mind::core::Path;
using sound_mind::core::PathNode;
using sound_mind::core::PathNodeType;
using sound_mind::core::TimeFrequencyPoint;
using sound_mind::studio::BranchCurveSession;

namespace {

Path straightLinePath(double startTime, double endTime) {
    Path path;
    PathNode start;
    start.anchor = TimeFrequencyPoint{startTime, 0.0};
    start.type = PathNodeType::Corner;
    path.addNode(start);
    PathNode end;
    end.anchor = TimeFrequencyPoint{endTime, 0.0};
    end.type = PathNodeType::Corner;
    path.addNode(end);
    return path;
}

}  // namespace

void BranchCurveSessionTest::freshSessionIsNotActive() {
    const BranchCurveSession session;
    QVERIFY(!session.isActive());
    QVERIFY(session.branches().empty());
}

void BranchCurveSessionTest::startBeginsWithTheTrunkAndNoPendingGraft() {
    BranchCurveSession session;
    const Path trunk = straightLinePath(0.0, 1.0);

    session.start(trunk, OperationId{1});

    QVERIFY(session.isActive());
    QCOMPARE(session.branches().size(), std::size_t{1});
    QVERIFY(!session.branches().front().parentIndex.has_value());
    QVERIFY(!session.hasPendingGraft());
}

void BranchCurveSessionTest::endClearsEverything() {
    BranchCurveSession session;
    session.start(straightLinePath(0.0, 1.0), OperationId{1});
    session.noteGraftCandidate(OperationId{1}, TimeFrequencyPoint{0.5, 0.0});

    session.end();

    QVERIFY(!session.isActive());
    QVERIFY(session.branches().empty());
    QVERIFY(!session.hasPendingGraft());
}

void BranchCurveSessionTest::noteGraftCandidateArmsAPendingGraftOnlyForAKnownBranch() {
    BranchCurveSession session;
    session.start(straightLinePath(0.0, 1.0), OperationId{1});

    session.noteGraftCandidate(OperationId{99}, TimeFrequencyPoint{0.5, 0.0});
    QVERIFY(!session.hasPendingGraft());

    session.noteGraftCandidate(OperationId{1}, TimeFrequencyPoint{0.5, 0.0});
    QVERIFY(session.hasPendingGraft());
}

void BranchCurveSessionTest::noteGraftCandidateIsANoOpWhenNotActive() {
    BranchCurveSession session;

    session.noteGraftCandidate(OperationId{1}, TimeFrequencyPoint{0.5, 0.0});

    QVERIFY(!session.hasPendingGraft());
}

void BranchCurveSessionTest::addBranchRefusesWithNoPendingGraft() {
    BranchCurveSession session;
    session.start(straightLinePath(0.0, 1.0), OperationId{1});

    const bool added = session.addBranch(straightLinePath(2.0, 3.0), OperationId{2});

    QVERIFY(!added);
    QCOMPARE(session.branches().size(), std::size_t{1});
}

void BranchCurveSessionTest::addBranchAppendsAtThePendingGraftAndClearsIt() {
    BranchCurveSession session;
    session.start(straightLinePath(0.0, 1.0), OperationId{1});
    session.noteGraftCandidate(OperationId{1}, TimeFrequencyPoint{0.5, 0.0});

    const bool added = session.addBranch(straightLinePath(2.0, 3.0), OperationId{2});

    QVERIFY(added);
    QCOMPARE(session.branches().size(), std::size_t{2});
    QVERIFY(session.branches()[1].parentIndex.has_value());
    QCOMPARE(*session.branches()[1].parentIndex, std::size_t{0});
    QCOMPARE(session.branches()[1].graftPoint.timeSeconds, 0.5);
    QVERIFY(!session.hasPendingGraft());  // Consumed - the next branch needs its own fresh graft click.
}

void BranchCurveSessionTest::addBranchRefusesWhenNotActive() {
    BranchCurveSession session;

    const bool added = session.addBranch(straightLinePath(0.0, 1.0), OperationId{1});

    QVERIFY(!added);
}

void BranchCurveSessionTest::aGraftedBranchCanItselfBeGraftedOnto() {
    BranchCurveSession session;
    session.start(straightLinePath(0.0, 1.0), OperationId{1});
    session.noteGraftCandidate(OperationId{1}, TimeFrequencyPoint{0.5, 0.0});
    session.addBranch(straightLinePath(2.0, 3.0), OperationId{2});

    session.noteGraftCandidate(OperationId{2}, TimeFrequencyPoint{2.5, 0.0});
    const bool added = session.addBranch(straightLinePath(4.0, 5.0), OperationId{3});

    QVERIFY(added);
    QCOMPARE(session.branches().size(), std::size_t{3});
    QCOMPARE(*session.branches()[2].parentIndex, std::size_t{1});
}
