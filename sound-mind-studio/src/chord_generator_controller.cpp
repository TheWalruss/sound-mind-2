#include "sound_mind/studio/chord_generator_controller.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "sound_mind/core/chord_generator.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/sequence_notation.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

ChordGeneratorController::ChordGeneratorController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void ChordGeneratorController::setProject(sound_mind::core::Project* project) { project_ = project; }

void ChordGeneratorController::setParams(sound_mind::core::ChordGeneratorParams params) {
    inputSource_ = InputSource::ChordBuilder;
    params_ = std::move(params);
    emit previewChanged();
}

void ChordGeneratorController::setNotation(std::string notation, double referenceHz, double bpm) {
    inputSource_ = InputSource::Notation;
    notation_ = std::move(notation);
    notationReferenceHz_ = referenceHz;
    notationBpm_ = bpm;
    emit previewChanged();
}

std::vector<sound_mind::core::NoteEvent> ChordGeneratorController::resolveNotes(double startTimeSeconds) const {
    if (inputSource_ == InputSource::ChordBuilder) {
        sound_mind::core::ChordGeneratorParams resolvedParams = params_;
        resolvedParams.startTimeSeconds = startTimeSeconds;
        return sound_mind::core::buildChordNotes(resolvedParams);
    }

    try {
        return sound_mind::core::parseSequenceNotation(notation_, notationReferenceHz_, notationBpm_,
                                                         startTimeSeconds);
    } catch (const std::invalid_argument&) {
        return {};
    }
}

std::vector<double> ChordGeneratorController::previewFrequenciesHz() const {
    const std::vector<sound_mind::core::NoteEvent> notes = resolveNotes(0.0);

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

    std::vector<sound_mind::core::NoteEvent> notes = resolveNotes(timeSeconds);
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
