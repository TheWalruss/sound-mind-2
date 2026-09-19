#include "sound_mind/core/sequence_operation.h"

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NoteEvent& note) {
    json = nlohmann::json{{"startTimeSeconds", note.startTimeSeconds},
                          {"durationSeconds", note.durationSeconds},
                          {"frequencyHz", note.frequencyHz}};
}

void from_json(const nlohmann::json& json, NoteEvent& note) {
    json.at("startTimeSeconds").get_to(note.startTimeSeconds);
    json.at("durationSeconds").get_to(note.durationSeconds);
    json.at("frequencyHz").get_to(note.frequencyHz);
}

std::unique_ptr<Operation> SequenceOperation::translatedCopy(
    OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
    const sound_mind::codec::StreamCodecConfig& config) const {
    // Each note's own frequency shifts via its own bin position - the same
    // "deltaFrequencyBins, not a raw Hz offset" reasoning Path::translated()
    // already establishes, so a translated sequence's own notes keep their
    // shape (equal-tempered spacing, in particular) regardless of where in
    // the frequency range they land.
    std::vector<NoteEvent> translatedNotes;
    translatedNotes.reserve(notes_.size());
    for (const NoteEvent& note : notes_) {
        NoteEvent translated = note;
        translated.startTimeSeconds += deltaTimeSeconds;
        translated.frequencyHz =
            translateFrequencyByBins(static_cast<float>(note.frequencyHz), deltaFrequencyBins, config);
        translatedNotes.push_back(translated);
    }
    return std::make_unique<SequenceOperation>(newId, targetLayer_, std::move(translatedNotes), config_->clone(),
                                                id());
}

}  // namespace sound_mind::core
