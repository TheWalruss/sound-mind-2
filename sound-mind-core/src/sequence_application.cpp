#include "sound_mind/core/sequence_application.h"

#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/path.h"

namespace sound_mind::core {

namespace {

/// @brief `note` as a `Path` - see `applySequenceOperation()`'s own docs
/// for exactly which shape each case produces.
Path notePath(const NoteEvent& note, const Gradient& gradient) {
    Path path;
    if (note.durationSeconds <= 0.0) {
        PathNode tap;
        tap.anchor = TimeFrequencyPoint{note.startTimeSeconds, note.frequencyHz};
        tap.type = PathNodeType::Corner;
        path.addNode(tap);
    } else {
        PathNode start;
        start.anchor = TimeFrequencyPoint{note.startTimeSeconds, note.frequencyHz};
        start.type = PathNodeType::Corner;
        path.addNode(start);
        PathNode end;
        end.anchor = TimeFrequencyPoint{note.startTimeSeconds + note.durationSeconds, note.frequencyHz};
        end.type = PathNodeType::Corner;
        path.addNode(end);
    }
    path.gradient() = gradient;
    return path;
}

}  // namespace

void applySequenceOperation(const SequenceOperation& operation, double frequencyToTimeScale,
                             sound_mind::codec::StreamImage& content, const LayerContentResolver& resolveLayerContent,
                             const MindWaveResolver& resolveMindWave, PrincipalMode principalMode) {
    for (const NoteEvent& note : operation.notes()) {
        // A throwaway id - this PaintOperation is never logged, only used
        // as applyPaintOperation()'s own required argument shape; its own
        // identity is never observed by anything.
        const PaintOperation noteStamp(0, *operation.targetLayer(), notePath(note, operation.config().defaultGradient()),
                                        operation.config().clone());
        applyPaintOperation(noteStamp, frequencyToTimeScale, content, resolveLayerContent, resolveMindWave,
                             principalMode);
    }
}

}  // namespace sound_mind::core
