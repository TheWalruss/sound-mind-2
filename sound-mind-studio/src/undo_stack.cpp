#include "sound_mind/studio/undo_stack.h"

#include <utility>

namespace sound_mind::studio {

void UndoStack::push(UndoCommand command) {
    // Discard any redo tail - the same "a fresh edit abandons the old
    // future" rule OperationLog::append() already follows for its own,
    // independent history (see this class's own docs).
    commands_.resize(index_);
    commands_.push_back(std::move(command));
    ++index_;
}

void UndoStack::undo() {
    if (!canUndo()) {
        return;
    }
    --index_;
    commands_[index_].undo();
}

void UndoStack::redo() {
    if (!canRedo()) {
        return;
    }
    commands_[index_].redo();
    ++index_;
}

void UndoStack::clear() {
    commands_.clear();
    index_ = 0;
}

void UndoStack::jumpTo(std::size_t index) {
    while (index_ > index && canUndo()) {
        undo();
    }
    while (index_ < index && canRedo()) {
        redo();
    }
}

}  // namespace sound_mind::studio
