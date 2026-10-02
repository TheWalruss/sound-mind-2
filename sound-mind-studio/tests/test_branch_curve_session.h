#pragma once

#include <QObject>

class BranchCurveSessionTest : public QObject {
    Q_OBJECT

private slots:
    void freshSessionIsNotActive();
    void startBeginsWithTheTrunkAndNoPendingGraft();
    void endClearsEverything();
    void noteGraftCandidateArmsAPendingGraftOnlyForAKnownBranch();
    void noteGraftCandidateIsANoOpWhenNotActive();
    void addBranchRefusesWithNoPendingGraft();
    void addBranchAppendsAtThePendingGraftAndClearsIt();
    void addBranchRefusesWhenNotActive();
    void aGraftedBranchCanItselfBeGraftedOnto();
};
