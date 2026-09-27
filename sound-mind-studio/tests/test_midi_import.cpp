#include "test_midi_import.h"

#include <algorithm>
#include <filesystem>

#include <QtTest/QtTest>

#include <juce_audio_basics/juce_audio_basics.h>

#include "sound_mind/core/midi_program_mapping.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/midi_import.h"
#include "sound_mind/studio/paint_controller.h"

using sound_mind::core::BrushTipShape;
using sound_mind::core::MidiProgramMapping;
using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::SequenceOperation;
using sound_mind::core::ToolPresetId;
using sound_mind::core::ToolType;

namespace {

/// @brief Writes a small, hand-built two-channel Standard MIDI File to a
///        fresh temp path and returns it - a local copy of
///        sound-mind-core/tests/test_midi_import.cpp's own identical
///        fixture (a different test binary/translation unit - QTest
///        classes don't share fixtures across files, matching
///        test_import_export.cpp's own established precedent for its WAV
///        fixture helper).
std::filesystem::path writeTestMidiFile() {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence track0;
    track0.addEvent(juce::MidiMessage::programChange(1, 4).withTimeStamp(0.0));
    track0.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)).withTimeStamp(0.0));
    track0.addEvent(juce::MidiMessage::noteOff(1, 69).withTimeStamp(960.0));
    track0.updateMatchedPairs();
    midiFile.addTrack(track0);

    juce::MidiMessageSequence track1;
    track1.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(80)).withTimeStamp(0.0));
    track1.addEvent(juce::MidiMessage::noteOff(2, 60).withTimeStamp(960.0));
    track1.updateMatchedPairs();
    midiFile.addTrack(track1);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-studio-test-midi-import.mid";
    {
        juce::File file(juce::String(path.string()));
        juce::FileOutputStream stream(file);
        midiFile.writeTo(stream);
    }
    return path;
}

/// @brief A small canvasWidth - fast, exact snippet-splitting math, the
///        same precedent test_import_export.cpp's own
///        smallCanvasProjectSettings() already establishes: `100 * 10ms =
///        1.0` real second per project-length snippet, a clean number to
///        build note timings against.
ProjectSettings oneSecondSnippetProjectSettings() {
    ProjectSettings settings;
    settings.canvasWidth = 100;
    return settings;
}

/// @brief Writes a two-channel Standard MIDI File spanning two 1.0-second
///        snippets (see oneSecondSnippetProjectSettings()) to a fresh temp
///        path and returns it. Channel 1 has one note in each snippet
///        (`t=0.0`, `t=1.2`); channel 2 has a note only in the first
///        (`t=0.3`) - so selecting channel 2 + snippet 1 together should
///        yield nothing, the real "combination with nothing left after
///        clipping is silently skipped" case importMidiSelectionInto()'s
///        own docs describe.
std::filesystem::path writeTwoSnippetTestMidiFile() {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    // 120 BPM default: 1 real second == 1920 ticks.

    juce::MidiMessageSequence track0;
    track0.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)).withTimeStamp(0.0));  // t=0.0
    track0.addEvent(juce::MidiMessage::noteOff(1, 69).withTimeStamp(960.0));                              // dur 0.5s
    track0.addEvent(juce::MidiMessage::noteOn(1, 72, static_cast<juce::uint8>(90)).withTimeStamp(2304.0));  // t=1.2
    track0.addEvent(juce::MidiMessage::noteOff(1, 72).withTimeStamp(2880.0));                               // dur 0.3s
    track0.updateMatchedPairs();
    midiFile.addTrack(track0);

    juce::MidiMessageSequence track1;
    track1.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(80)).withTimeStamp(576.0));  // t=0.3
    track1.addEvent(juce::MidiMessage::noteOff(2, 60).withTimeStamp(1344.0));                              // dur 0.4s
    track1.updateMatchedPairs();
    midiFile.addTrack(track1);

    const auto path = std::filesystem::temp_directory_path() / "sound-mind-studio-test-midi-import-snippets.mid";
    {
        juce::File file(juce::String(path.string()));
        juce::FileOutputStream stream(file);
        midiFile.writeTo(stream);
    }
    return path;
}

}  // namespace

void MidiImportTest::importMidiChannelsIntoCreatesOneLayerPerChannelByDefault() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    const std::size_t layerCountBefore = project.layers().size();

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(2));
    QCOMPARE(project.layers().size(), layerCountBefore + 2);

    const auto* layer0 = project.layerById(newLayerIds[0]);
    const auto* layer1 = project.layerById(newLayerIds[1]);
    QVERIFY(layer0 != nullptr);
    QVERIFY(layer1 != nullptr);
    QVERIFY(layer0->name().find("Ch1") != std::string::npos);
    QVERIFY(layer1->name().find("Ch2") != std::string::npos);

    const auto operations0 = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    QCOMPARE(operations0.size(), static_cast<std::size_t>(1));
    const auto* sequence0 = dynamic_cast<const SequenceOperation*>(operations0.front());
    QVERIFY(sequence0 != nullptr);
    QCOMPARE(sequence0->notes().size(), static_cast<std::size_t>(1));
}

void MidiImportTest::importMidiChannelsIntoMergesEveryChannelIntoOneLayerWhenNotSeparating() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    const std::size_t layerCountBefore = project.layers().size();

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path, /*separateLayerPerChannel=*/false);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(1));
    QCOMPARE(project.layers().size(), layerCountBefore + 1);

    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    QCOMPARE(operations.size(), static_cast<std::size_t>(2));
}

void MidiImportTest::importMidiChannelsIntoFailsGracefullyForAnUnreadableFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-studio-test-midi-import-missing.mid";
    std::filesystem::remove(path);
    Project project = Project::createNew(ProjectSettings{});
    const std::size_t layerCountBefore = project.layers().size();

    QString errorMessage;
    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path, true, &errorMessage);

    QVERIFY(newLayerIds.empty());
    QVERIFY(!errorMessage.isEmpty());
    QCOMPARE(project.layers().size(), layerCountBefore);
}

void MidiImportTest::importMidiChannelsIntoUsesAPlainProceduralConfigurationForEachChannel() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);

    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->config().type(), ToolType::Procedural);
}

void MidiImportTest::importMidiChannelsIntoOnlyIncludesSelectedChannelsWhenGiven() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    const std::size_t layerCountBefore = project.layers().size();

    const auto newLayerIds =
        sound_mind::studio::importMidiChannelsInto(project, path, /*separateLayerPerChannel=*/true, nullptr, {2});
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(1));
    QCOMPARE(project.layers().size(), layerCountBefore + 1);
    const auto* layer = project.layerById(newLayerIds[0]);
    QVERIFY(layer != nullptr);
    QVERIFY(layer->name().find("Ch2") != std::string::npos);
}

void MidiImportTest::rebuildingAMidiImportedLayerProducesRealNonSilentPaintedContent() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);
    QVERIFY(!newLayerIds.empty());

    sound_mind::studio::PaintController controller;
    controller.setProject(&project);
    controller.rebuildLayerContent(newLayerIds[0]);

    const auto* layer = project.layerById(newLayerIds[0]);
    QVERIFY(layer != nullptr);
    QVERIFY(layer->content().has_value());
    // A silent base is uniformly very quiet (see silentContentFor()'s own
    // docs) - a real painted stamp should leave at least one cell audibly
    // louder than that floor.
    const auto& content = *layer->content();
    const bool anyPainted =
        std::any_of(content.leftMagnitudeDb.begin(), content.leftMagnitudeDb.end(), [](float db) { return db > -50.0f; });
    QVERIFY(anyPainted);
}

void MidiImportTest::midiImportPreviewForFileReturnsChannelsAndComputedSnippets() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    const auto preview = sound_mind::studio::midiImportPreviewForFile(project, path);
    std::filesystem::remove(path);

    QVERIFY(preview.has_value());
    QCOMPARE(preview->channels.size(), static_cast<std::size_t>(2));
    // Latest note end is channel 1's second note: 1.2 + 0.3 = 1.5s: two
    // 1.0-second snippets, the second one shorter than a full second.
    QCOMPARE(preview->snippets.size(), static_cast<std::size_t>(2));
    QCOMPARE(preview->snippets[0].startSeconds, 0.0);
    QCOMPARE(preview->snippets[0].endSeconds, 1.0);
    QCOMPARE(preview->snippets[1].startSeconds, 1.0);
    QCOMPARE(preview->snippets[1].endSeconds, 1.5);
}

void MidiImportTest::midiImportPreviewForFileFailsGracefullyForAnUnreadableFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-studio-test-midi-preview-missing.mid";
    std::filesystem::remove(path);
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    QString errorMessage;
    const auto preview = sound_mind::studio::midiImportPreviewForFile(project, path, &errorMessage);

    QVERIFY(!preview.has_value());
    QVERIFY(!errorMessage.isEmpty());
}

void MidiImportTest::importMidiSelectionIntoOnlyIncludesSelectedChannels() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    // Channel 2 has no note at all in snippet 1 - that combination should
    // simply be skipped, not produce an empty layer.
    const auto newLayerIds =
        sound_mind::studio::importMidiSelectionInto(project, path, {2}, {0, 1}, /*separateLayerPerChannel=*/true);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(1));
    const auto* layer = project.layerById(newLayerIds[0]);
    QVERIFY(layer != nullptr);
    QVERIFY(layer->name().find("Ch2") != std::string::npos);
}

void MidiImportTest::importMidiSelectionIntoClipsAndRebasesNotesToTheSelectedSnippetWindow() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    const auto newLayerIds =
        sound_mind::studio::importMidiSelectionInto(project, path, {1}, {1}, /*separateLayerPerChannel=*/true);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(1));
    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    QCOMPARE(operations.size(), static_cast<std::size_t>(1));
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->notes().size(), static_cast<std::size_t>(1));
    // The note started at t=1.2 in the source file; snippet 1 starts at
    // t=1.0, so it should be rebased to 0.2 within this new layer.
    QVERIFY(qAbs(sequence->notes().front().startTimeSeconds - 0.2) < 0.001);
}

void MidiImportTest::importMidiSelectionIntoMergesChannelsIntoOneLayerPerSnippetWhenNotSeparating() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    const auto newLayerIds = sound_mind::studio::importMidiSelectionInto(project, path, {1, 2}, {0, 1},
                                                                           /*separateLayerPerChannel=*/false);
    std::filesystem::remove(path);

    // Snippet 0: both channels have a note (2 operations, 1 layer).
    // Snippet 1: only channel 1 has a note (1 operation, 1 layer).
    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(2));
    const auto snippet0Operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto snippet1Operations = project.operationLog().activeOperationsTargeting(newLayerIds[1]);
    QCOMPARE(snippet0Operations.size(), static_cast<std::size_t>(2));
    QCOMPARE(snippet1Operations.size(), static_cast<std::size_t>(1));
}

void MidiImportTest::importMidiSelectionIntoAppendsSnippetSuffixToLayerNamesWhenMoreThanOneSnippet() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());

    const auto newLayerIds =
        sound_mind::studio::importMidiSelectionInto(project, path, {1}, {0, 1}, /*separateLayerPerChannel=*/true);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(2));
    const auto* layer0 = project.layerById(newLayerIds[0]);
    const auto* layer1 = project.layerById(newLayerIds[1]);
    QVERIFY(layer0 != nullptr);
    QVERIFY(layer1 != nullptr);
    QVERIFY(layer0->name().find("_0000") != std::string::npos);
    QVERIFY(layer1->name().find("_0001") != std::string::npos);
}

void MidiImportTest::importMidiSelectionIntoFailsGracefullyForAnUnreadableFile() {
    const auto path = std::filesystem::temp_directory_path() / "sound-mind-studio-test-midi-selection-missing.mid";
    std::filesystem::remove(path);
    Project project = Project::createNew(oneSecondSnippetProjectSettings());
    const std::size_t layerCountBefore = project.layers().size();

    QString errorMessage;
    const auto newLayerIds =
        sound_mind::studio::importMidiSelectionInto(project, path, {1}, {0}, true, &errorMessage);

    QVERIFY(newLayerIds.empty());
    QVERIFY(!errorMessage.isEmpty());
    QCOMPARE(project.layers().size(), layerCountBefore);
}

void MidiImportTest::importMidiChannelsIntoUsesTheMappedToolPresetWhenOneExists() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    ProceduralConfiguration diamondConfig;
    diamondConfig.setTipShape(BrushTipShape::Diamond);
    const auto presetId = project.addToolPreset("My Diamond", diamondConfig);
    MidiProgramMapping mapping;
    mapping.programNumber = 4;  // Channel 1's own program in writeTestMidiFile()'s fixture.
    mapping.toolPresetId = presetId;
    project.setMidiProgramMapping(mapping);

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);

    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(dynamic_cast<const ProceduralConfiguration&>(sequence->config()).tipShape(), BrushTipShape::Diamond);
}

void MidiImportTest::importMidiChannelsIntoAppliesDurationScaleAndPitchOffset() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    mapping.durationScale = 2.0;
    mapping.pitchOffsetSemitones = 12.0;  // One octave up - frequency doubles.
    project.setMidiProgramMapping(mapping);

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);

    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->notes().size(), static_cast<std::size_t>(1));
    const auto& note = sequence->notes().front();
    QVERIFY(qAbs(note.durationSeconds - 1.0) < 0.001);  // Original 0.5s * 2.0.
    QVERIFY(qAbs(note.frequencyHz - 880.0) < 0.5);       // Original 440Hz (A4), one octave up.
}

void MidiImportTest::importMidiChannelsIntoFallsBackToDefaultWhenTheMappedPresetNoLongerExists() {
    const auto path = writeTestMidiFile();
    Project project = Project::createNew(ProjectSettings{});
    MidiProgramMapping mapping;
    mapping.programNumber = 4;
    mapping.toolPresetId = ToolPresetId{999999};  // Never existed.
    project.setMidiProgramMapping(mapping);

    const auto newLayerIds = sound_mind::studio::importMidiChannelsInto(project, path);
    std::filesystem::remove(path);

    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    QCOMPARE(sequence->config().type(), ToolType::Procedural);
    // Confirm it's the genuinely opaque default (see makeOpaqueDefaultConfiguration()'s
    // own docs), not just any Procedural configuration.
    QCOMPARE(sequence->config().defaultGradient().stops().front().leftOpacity, 1.0f);
}

void MidiImportTest::importMidiSelectionIntoAppliesTheProgramMappingToo() {
    const auto path = writeTwoSnippetTestMidiFile();
    Project project = Project::createNew(oneSecondSnippetProjectSettings());
    MidiProgramMapping mapping;
    mapping.programNumber = 0;  // Channel 1 has no Program Change in this fixture - defaults to 0.
    mapping.durationScale = 2.0;
    project.setMidiProgramMapping(mapping);

    const auto newLayerIds =
        sound_mind::studio::importMidiSelectionInto(project, path, {1}, {0}, /*separateLayerPerChannel=*/true);
    std::filesystem::remove(path);

    QCOMPARE(newLayerIds.size(), static_cast<std::size_t>(1));
    const auto operations = project.operationLog().activeOperationsTargeting(newLayerIds[0]);
    const auto* sequence = dynamic_cast<const SequenceOperation*>(operations.front());
    QVERIFY(sequence != nullptr);
    // Original note in snippet 0: duration 0.5s, scaled by 2.0.
    QVERIFY(qAbs(sequence->notes().front().durationSeconds - 1.0) < 0.001);
}
