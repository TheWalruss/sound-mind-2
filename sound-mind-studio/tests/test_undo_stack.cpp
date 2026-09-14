#include "test_undo_stack.h"

#include <vector>

#include <QtTest/QtTest>

#include "sound_mind/studio/undo_stack.h"

using sound_mind::studio::UndoCommand;
using sound_mind::studio::UndoStack;

namespace {

/// @brief Pushes a command that appends `label` to `log` on redo() (or on
/// the implicit "already happened" push itself, matching how a real
/// caller applies its own change before push()ing) and removes it again
/// on undo() - a small, order-observable stand-in for a real property/
/// content mutation, shared by every test below.
void pushLabeledCommand(UndoStack& stack, std::vector<int>& log, int label) {
    log.push_back(label);
    stack.push(UndoCommand{/*undo=*/[&log, label]() {
                                // The most recently applied label is always
                                // the log's own last element, matching a
                                // real undo reverting the most recent change.
                                Q_ASSERT(!log.empty() && log.back() == label);
                                log.pop_back();
                            },
                            /*redo=*/[&log, label]() { log.push_back(label); }});
}

}  // namespace

void UndoStackTest::freshStackCannotUndoOrRedo() {
    UndoStack stack;
    QVERIFY(!stack.canUndo());
    QVERIFY(!stack.canRedo());
}

void UndoStackTest::pushMakesCanUndoTrueAndCanRedoFalse() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);

    QVERIFY(stack.canUndo());
    QVERIFY(!stack.canRedo());
}

void UndoStackTest::undoInvokesTheCommandsOwnUndoCallback() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);

    stack.undo();

    QVERIFY(log.empty());
    QVERIFY(!stack.canUndo());
    QVERIFY(stack.canRedo());
}

void UndoStackTest::undoThenRedoInvokesBothCallbacksInOrder() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);

    stack.undo();
    stack.redo();

    QCOMPARE(log, std::vector<int>({1}));
    QVERIFY(stack.canUndo());
    QVERIFY(!stack.canRedo());
}

void UndoStackTest::multipleCommandsUndoAndRedoInReverseThenForwardOrder() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);
    pushLabeledCommand(stack, log, 2);
    pushLabeledCommand(stack, log, 3);
    QCOMPARE(log, std::vector<int>({1, 2, 3}));

    stack.undo();
    QCOMPARE(log, std::vector<int>({1, 2}));
    stack.undo();
    QCOMPARE(log, std::vector<int>({1}));

    stack.redo();
    QCOMPARE(log, std::vector<int>({1, 2}));
    stack.redo();
    QCOMPARE(log, std::vector<int>({1, 2, 3}));
    QVERIFY(!stack.canRedo());
}

void UndoStackTest::undoWhenEmptyIsANoOp() {
    UndoStack stack;
    stack.undo();  // Must not crash/assert with nothing pushed yet.
    QVERIFY(!stack.canUndo());
}

void UndoStackTest::redoWhenNothingUndoneIsANoOp() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);

    stack.redo();  // Nothing undone yet - must not touch log at all.

    QCOMPARE(log, std::vector<int>({1}));
}

void UndoStackTest::pushingAfterAnUndoDiscardsTheRedoTail() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);
    pushLabeledCommand(stack, log, 2);

    stack.undo();  // log == {1}; command 2 is now a pending redo.
    QVERIFY(stack.canRedo());

    pushLabeledCommand(stack, log, 3);  // A fresh edit abandons that redo tail.

    QVERIFY(!stack.canRedo());
    QCOMPARE(log, std::vector<int>({1, 3}));

    // Undoing back through the fresh history must never reach command 2's
    // own undo() (it was discarded, not merely hidden) - reaching {} then
    // back to {1} confirms only commands 1 and 3 remain live.
    stack.undo();
    QCOMPARE(log, std::vector<int>({1}));
    stack.undo();
    QVERIFY(log.empty());
    QVERIFY(!stack.canUndo());
}

void UndoStackTest::clearDiscardsEveryCommandAndResetsAvailability() {
    UndoStack stack;
    std::vector<int> log;
    pushLabeledCommand(stack, log, 1);
    pushLabeledCommand(stack, log, 2);
    stack.undo();

    stack.clear();

    QVERIFY(!stack.canUndo());
    QVERIFY(!stack.canRedo());
    // clear() only forgets the history - it never itself invokes any
    // callback, so log (already at {1} from the undo() above) is untouched.
    QCOMPARE(log, std::vector<int>({1}));
}
