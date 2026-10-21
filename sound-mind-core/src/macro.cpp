#include "sound_mind/core/macro.h"

#include "sound_mind/core/project.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const MacroEvent& event) {
    json = nlohmann::json{{"time", event.time}, {"kind", event.kind}};
    if (event.entry.has_value()) {
        json["entry"] = *event.entry;
    }
    if (event.kind == MacroEventKind::Play || event.kind == MacroEventKind::Pause ||
        event.kind == MacroEventKind::Seek) {
        json["position"] = event.position;
    }
}

void from_json(const nlohmann::json& json, MacroEvent& event) {
    event = MacroEvent{};
    json.at("time").get_to(event.time);
    json.at("kind").get_to(event.kind);
    if (json.contains("entry")) {
        event.entry = json.at("entry").get<HistoryEntry>();
    }
    event.position = json.value("position", 0.0);
}

void to_json(nlohmann::json& json, const Macro& macro) {
    json = nlohmann::json{{"id", macro.id},
                          {"name", macro.name},
                          {"settings", macro.settings},
                          {"startAfterEntry", macro.startAfterEntry},
                          {"historyStartsAtCreation", macro.historyStartsAtCreation},
                          {"events", macro.events},
                          {"durationSeconds", macro.durationSeconds}};
    if (macro.prefixRetained) {
        json["retainedPrefix"] = macro.retainedPrefix;
    }
}

void from_json(const nlohmann::json& json, Macro& macro) {
    macro = Macro{};
    json.at("id").get_to(macro.id);
    json.at("name").get_to(macro.name);
    json.at("settings").get_to(macro.settings);
    json.at("startAfterEntry").get_to(macro.startAfterEntry);
    macro.historyStartsAtCreation = json.value("historyStartsAtCreation", false);
    macro.events = json.at("events").get<std::vector<MacroEvent>>();
    macro.durationSeconds = json.value("durationSeconds", 0.0);
    if (json.contains("retainedPrefix")) {
        macro.retainedPrefix = json.at("retainedPrefix").get<std::vector<HistoryEntry>>();
        macro.prefixRetained = true;
    }
    macro.recording = false;  // a saved recording in progress is just a shorter Macro.
}

std::optional<std::vector<HistoryEntry>> macroStartEntries(const Project& project, const Macro& macro) {
    if (!macro.historyStartsAtCreation) {
        return std::nullopt;
    }
    if (macro.prefixRetained) {
        return macro.retainedPrefix;
    }
    std::vector<HistoryEntry> entries;
    if (macro.startAfterEntry == 0) {
        return entries;
    }
    const History& history = project.history();
    for (std::size_t i = 0; i < history.size(); ++i) {
        entries.push_back(history.entry(i));
        if (history.entry(i).id == macro.startAfterEntry) {
            for (HistoryEntry& entry : entries) {
                for (HistoryLayerChange& change : entry.layers) {
                    change.baseBefore.reset();
                    change.baseAfter.reset();
                }
            }
            return entries;
        }
    }
    return std::nullopt;
}

MacroPlayhead macroPlayheadAt(const Macro& macro, double time) {
    MacroPlayhead playhead;
    double since = 0.0;
    for (const MacroEvent& event : macro.events) {
        if (event.time > time) {
            break;
        }
        switch (event.kind) {
            case MacroEventKind::Play:
                playhead = {event.position, true};
                since = event.time;
                break;
            case MacroEventKind::Pause:
                playhead = {event.position, false};
                since = event.time;
                break;
            case MacroEventKind::Seek:
                playhead.position = event.position;
                since = event.time;
                break;
            default:
                break;
        }
    }
    if (playhead.playing) {
        playhead.position += time - since;
    }
    return playhead;
}

}  // namespace sound_mind::core
