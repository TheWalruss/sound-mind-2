#include "test_midi_import.h"

#include <filesystem>

#include <QtTest/QtTest>

#include <juce_audio_basics/juce_audio_basics.h>

#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/core/sequence_operation.h"
#include "sound_mind/core/tool_configuration.h"
#include "sound_mind/studio/midi_import.h"

using sound_mind::core::ProceduralConfiguration;
using sound_mind::core::Project;
using sound_mind::core::ProjectSettings;
using sound_mind::core::SequenceOperation;
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
