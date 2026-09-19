#include "sound_mind/studio/chord_generator_controller.h"

#include <algorithm>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

ChordGeneratorController::ChordGeneratorController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void ChordGeneratorController::setProject(sound_mind::core::Project* project) { project_ = project; }

void ChordGeneratorController::setParams(sound_mind::core::ChordGeneratorParams params) {
    params_ = std::move(params);
    emit previewChanged();
}

std::vector<double> ChordGeneratorController::previewFrequenciesHz() const {
    sound_mind::core::ChordGeneratorParams previewParams = params_;
    previewParams.startTimeSeconds = 0.0;
    const std::vector<sound_mind::core::NoteEvent> notes = sound_mind::core::buildChordNotes(previewParams);

    std::vector<double> frequenciesHz;
    frequenciesHz.reserve(notes.size());
    for (const auto& note : notes) {
        frequenciesHz.push_back(note.frequencyHz);
    }
    std::sort(frequenciesHz.begin(), frequenciesHz.end());
    frequenciesHz.erase(std::unique(frequenciesHz.begin(), frequenciesHz.end()), frequenciesHz.end());
    return frequenciesHz;
}

void ChordGeneratorController::stampAt(sound_mind::core::LayerId targetLayer, double timeSeconds) {
    if (project_ == nullptr) {
        return;
    }

    sound_mind::core::ChordGeneratorParams stampParams = params_;
    stampParams.startTimeSeconds = timeSeconds;
    std::vector<sound_mind::core::NoteEvent> notes = sound_mind::core::buildChordNotes(stampParams);
    if (notes.empty()) {
        return;
    }

    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId newId = log.reserveId();
    log.append(std::make_unique<sound_mind::core::SequenceOperation>(
        newId, targetLayer, std::move(notes), paintController_->toolConfiguration().clone()));

    paintController_->notifyOperationCommitted();
    paintController_->rebuildLayerContent(targetLayer);
    emit contentChanged(targetLayer);
}

}  // namespace sound_mind::studio
