#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "sound_mind/core/macro.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"

using sound_mind::core::Macro;
using sound_mind::core::MacroEvent;
using sound_mind::core::MacroEventKind;
using sound_mind::core::Project;

namespace {

Project smallProject() {
    sound_mind::core::ProjectSettings settings;
    settings.canvasWidth = 16;
    settings.canvasHeight = 8;
    return Project::createNew(settings);
}

void change(Project& project, float opacity) {
    project.layers().front().setOpacity(opacity);
    project.history().commit(project, "Opacity");
}

}  // namespace

TEST_CASE("A Macro round-trips through the project file", "[core][macro]") {
    Project project = smallProject();
    change(project, 0.5f);
    Macro macro;
    macro.name = "Take";
    macro.settings = project.settings();
    macro.startAfterEntry = project.history().entry(0).id;
    macro.historyStartsAtCreation = true;
    macro.events.push_back({0.0, MacroEventKind::Play, std::nullopt, 2.0});
    macro.events.push_back({1.0, MacroEventKind::Entry, project.history().entry(0), 0.0});
    macro.durationSeconds = 3.0;
    const auto id = project.addMacro(macro);
    REQUIRE(id == 1);

    const Project parsed = nlohmann::json(project).get<Project>();
    REQUIRE(parsed.macros().size() == 1);
    const Macro& back = parsed.macros().front();
    REQUIRE(back.name == "Take");
    REQUIRE(back.events.size() == 2);
    REQUIRE(back.events[0].kind == MacroEventKind::Play);
    REQUIRE(back.events[0].position == 2.0);
    REQUIRE(back.events[1].entry->description == "Opacity");
    REQUIRE(back.durationSeconds == 3.0);
    REQUIRE_FALSE(back.recording);
}

TEST_CASE("macroStartEntries gives the project's entries up to the start", "[core][macro]") {
    Project project = smallProject();
    change(project, 0.9f);
    change(project, 0.8f);
    Macro macro;
    macro.historyStartsAtCreation = true;
    macro.startAfterEntry = project.history().entry(0).id;
    const auto entries = sound_mind::core::macroStartEntries(project, macro);
    REQUIRE(entries.has_value());
    REQUIRE(entries->size() == 1);

    macro.startAfterEntry = 0;
    REQUIRE(sound_mind::core::macroStartEntries(project, macro)->empty());
    macro.historyStartsAtCreation = false;
    REQUIRE_FALSE(sound_mind::core::macroStartEntries(project, macro).has_value());
}

TEST_CASE("A Macro keeps the entries it starts from when later editing drops them", "[core][macro]") {
    Project project = smallProject();
    change(project, 0.9f);
    change(project, 0.8f);
    Macro macro;
    macro.historyStartsAtCreation = true;
    macro.startAfterEntry = project.history().entry(1).id;
    project.addMacro(macro);

    project.history().undo(project);  // the start entry is now undone...
    change(project, 0.1f);            // ...and this drops it.
    const Macro& kept = project.macros().front();
    REQUIRE(kept.prefixRetained);
    REQUIRE(kept.retainedPrefix.size() == 2);
    const auto entries = sound_mind::core::macroStartEntries(project, kept);
    REQUIRE(entries->back().layers.front().after.at("opacity") == 0.8f);
}

TEST_CASE("macroPlayheadAt follows Play, Seek and Pause", "[core][macro]") {
    Macro macro;
    macro.events = {{0.0, MacroEventKind::Play, std::nullopt, 10.0},
                    {2.0, MacroEventKind::Seek, std::nullopt, 0.0},
                    {5.0, MacroEventKind::Pause, std::nullopt, 3.0},
                    {5.0, MacroEventKind::Play, std::nullopt, 3.0}};
    REQUIRE(sound_mind::core::macroPlayheadAt(macro, 1.0).position == 11.0);
    REQUIRE(sound_mind::core::macroPlayheadAt(macro, 1.0).playing);
    REQUIRE(sound_mind::core::macroPlayheadAt(macro, 3.0).position == 1.0);
    REQUIRE(sound_mind::core::macroPlayheadAt(macro, 6.5).position == 4.5);

    Macro paused;
    paused.events = {{0.0, MacroEventKind::Play, std::nullopt, 0.0}, {1.0, MacroEventKind::Pause, std::nullopt, 1.0}};
    REQUIRE(sound_mind::core::macroPlayheadAt(paused, 4.0).position == 1.0);
    REQUIRE_FALSE(sound_mind::core::macroPlayheadAt(paused, 4.0).playing);
}
