#pragma once

#include <QObject>

class UndoStackTest : public QObject {
    Q_OBJECT

private slots:
    void freshStackCannotUndoOrRedo();
    void pushMakesCanUndoTrueAndCanRedoFalse();
    void undoInvokesTheCommandsOwnUndoCallback();
    void undoThenRedoInvokesBothCallbacksInOrder();
    void multipleCommandsUndoAndRedoInReverseThenForwardOrder();
    void undoWhenEmptyIsANoOp();
    void redoWhenNothingUndoneIsANoOp();
    void pushingAfterAnUndoDiscardsTheRedoTail();
    void clearDiscardsEveryCommandAndResetsAvailability();
    void countCurrentIndexAndDescriptionAtReflectPushedCommands();
    void jumpToMovesDirectlyInvokingEveryInBetweenCallback();
};
