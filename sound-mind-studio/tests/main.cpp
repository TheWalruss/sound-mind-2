/**
 * @file main.cpp
 * @brief Runs every QTest test class in this directory from one binary.
 *
 * QTEST_MAIN() generates a main() for a single test class; combining
 * several into one executable means driving QTest::qExec manually instead,
 * sharing one QApplication (QWidget construction needs one). Run with
 * QT_QPA_PLATFORM=offscreen - see this target's CMakeLists.txt - since CI
 * has no real display.
 *
 * Every class is listed once in kTestClasses below via SM_TEST_CLASS(). The
 * CMakeLists.txt next to this file reads that same list and registers each
 * class as its own CTest test (`--class <Name>`), so each class gets its own
 * timeout, pass/fail and output. With no `--class`, every class runs in
 * order. `--list-classes` prints the registered names.
 */

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QStandardPaths>
#include <QtTest/QtTest>

#include "test_about_dialog.h"
#include "test_command_line.h"
#include "test_audio_snippet_picker_dialog.h"
#include "test_base_recipes.h"
#include "test_axis_labels.h"
#include "test_branch_curve_session.h"
#include "test_canvas_widget.h"
#include "test_checklist_dialog.h"
#include "test_chord_generator_controller.h"
#include "test_chord_generator_panel.h"
#include "test_configure_devices_panel.h"
#include "test_create_project_wizard.h"
#include "test_device_combo_helpers.h"
#include "test_device_configuration_widget.h"
#include "test_equalizer_curve_widget.h"
#include "test_filter_configuration_panel.h"
#include "test_generator_dialog.h"
#include "test_gradient_bar_widget.h"
#include "test_gradient_editor_widget.h"
#include "test_grid_config.h"
#include "test_grid_panel.h"
#include "test_harmonic_series_widget.h"
#include "test_history_panel.h"
#include "test_image_scale_picker_dialog.h"
#include "test_import_export.h"
#include "test_import_helpers.h"
#include "test_landing_page.h"
#include "test_layer_controller.h"
#include "test_layers_panel.h"
#include "test_loop_panel.h"
#include "test_macro_player.h"
#include "test_macro_recorder.h"
#include "test_main_window.h"
#include "test_midi_configuration_panel.h"
#include "test_midi_import.h"
#include "test_midi_import_dialog.h"
#include "test_mind_capture_dialog.h"
#include "test_mind_wave_controller.h"
#include "test_mind_wave_editor.h"
#include "test_preset_indicator_label.h"
#include "test_mind_waves_panel.h"
#include "test_name_collision_dialog.h"
#include "test_numeric_field.h"
#include "test_paint_controller.h"
#include "test_path_controller.h"
#include "test_text_controller.h"
#include "test_analysis_panel.h"
#include "test_text_tool_panel.h"
#include "test_path_edit_session.h"
#include "test_pick_controller.h"
#include "test_playback_controller.h"
#include "test_playback_panel.h"
#include "test_polar_origin_dialog.h"
#include "test_record_panel.h"
#include "test_recent_projects.h"
#include "test_resource_browser_controller.h"
#include "test_resource_browser_panel.h"
#include "test_rotary_dial_widget.h"
#include "test_selection_configuration_panel.h"
#include "test_selection_controller.h"
#include "test_theme.h"
#include "test_tone_curve_editor.h"
#include "test_tool_configuration_panel.h"
#include "test_tool_palette_controller.h"
#include "test_project_history.h"
#include "test_remaster.h"
#include "test_waveform_preview_widget.h"

namespace {

struct TestClassEntry {
    const char* name;
    std::unique_ptr<QObject> (*make)();
};

// clang-format off
#define SM_TEST_CLASS_ENTRY(TestType) \
    TestClassEntry{#TestType, []() -> std::unique_ptr<QObject> { return std::make_unique<TestType>(); }}

const std::array kTestClasses{
    SM_TEST_CLASS_ENTRY(CanvasWidgetTest),
    SM_TEST_CLASS_ENTRY(CheckListDialogTest),
    SM_TEST_CLASS_ENTRY(ChordGeneratorControllerTest),
    SM_TEST_CLASS_ENTRY(ChordGeneratorPanelTest),
    SM_TEST_CLASS_ENTRY(ConfigureDevicesPanelTest),
    SM_TEST_CLASS_ENTRY(LandingPageTest),
    SM_TEST_CLASS_ENTRY(LayerControllerTest),
    SM_TEST_CLASS_ENTRY(MainWindowTest),
    SM_TEST_CLASS_ENTRY(RecentProjectsTest),
    SM_TEST_CLASS_ENTRY(RotaryDialWidgetTest),
    SM_TEST_CLASS_ENTRY(WaveformPreviewWidgetTest),
    SM_TEST_CLASS_ENTRY(ThemeTest),
    SM_TEST_CLASS_ENTRY(CreateProjectWizardTest),
    SM_TEST_CLASS_ENTRY(LayersPanelTest),
    SM_TEST_CLASS_ENTRY(LoopPanelTest),
    SM_TEST_CLASS_ENTRY(MacroPlayerTest),
    SM_TEST_CLASS_ENTRY(MacroRecorderTest),
    SM_TEST_CLASS_ENTRY(RecordPanelTest),
    SM_TEST_CLASS_ENTRY(PlaybackPanelTest),
    SM_TEST_CLASS_ENTRY(PlaybackControllerTest),
    SM_TEST_CLASS_ENTRY(PaintControllerTest),
    SM_TEST_CLASS_ENTRY(PickControllerTest),
    SM_TEST_CLASS_ENTRY(PathControllerTest),
    SM_TEST_CLASS_ENTRY(TextControllerTest),
    SM_TEST_CLASS_ENTRY(AnalysisPanelTest),
    SM_TEST_CLASS_ENTRY(TextToolPanelTest),
    SM_TEST_CLASS_ENTRY(PathEditSessionTest),
    SM_TEST_CLASS_ENTRY(SelectionControllerTest),
    SM_TEST_CLASS_ENTRY(SelectionConfigurationPanelTest),
    SM_TEST_CLASS_ENTRY(ToolConfigurationPanelTest),
    SM_TEST_CLASS_ENTRY(ToolPaletteControllerTest),
    SM_TEST_CLASS_ENTRY(AudioSnippetPickerDialogTest),
    SM_TEST_CLASS_ENTRY(BaseRecipesTest),
    SM_TEST_CLASS_ENTRY(ImageScalePickerDialogTest),
    SM_TEST_CLASS_ENTRY(PolarOriginDialogTest),
    SM_TEST_CLASS_ENTRY(ImportHelpersTest),
    SM_TEST_CLASS_ENTRY(DeviceComboHelpersTest),
    SM_TEST_CLASS_ENTRY(DeviceConfigurationWidgetTest),
    SM_TEST_CLASS_ENTRY(ImportExportTest),
    SM_TEST_CLASS_ENTRY(AxisLabelsTest),
    SM_TEST_CLASS_ENTRY(BranchCurveSessionTest),
    SM_TEST_CLASS_ENTRY(GridPanelTest),
    SM_TEST_CLASS_ENTRY(HarmonicSeriesWidgetTest),
    SM_TEST_CLASS_ENTRY(HistoryPanelTest),
    SM_TEST_CLASS_ENTRY(GridConfigTest),
    SM_TEST_CLASS_ENTRY(FilterConfigurationPanelTest),
    SM_TEST_CLASS_ENTRY(GeneratorDialogTest),
    SM_TEST_CLASS_ENTRY(GradientBarWidgetTest),
    SM_TEST_CLASS_ENTRY(EqualizerCurveWidgetTest),
    SM_TEST_CLASS_ENTRY(GradientEditorWidgetTest),
    SM_TEST_CLASS_ENTRY(ToneCurveEditorTest),
    SM_TEST_CLASS_ENTRY(MindWaveEditorTest),
    SM_TEST_CLASS_ENTRY(PresetIndicatorLabelTest),
    SM_TEST_CLASS_ENTRY(MindWavesPanelTest),
    SM_TEST_CLASS_ENTRY(NameCollisionDialogTest),
    SM_TEST_CLASS_ENTRY(NumericFieldTest),
    SM_TEST_CLASS_ENTRY(MindCaptureDialogTest),
    SM_TEST_CLASS_ENTRY(MidiImportTest),
    SM_TEST_CLASS_ENTRY(MidiConfigurationPanelTest),
    SM_TEST_CLASS_ENTRY(MidiImportDialogTest),
    SM_TEST_CLASS_ENTRY(MindWaveControllerTest),
    SM_TEST_CLASS_ENTRY(ResourceBrowserPanelTest),
    SM_TEST_CLASS_ENTRY(ResourceBrowserControllerTest),
    SM_TEST_CLASS_ENTRY(ProjectHistoryTest),
    SM_TEST_CLASS_ENTRY(RemasterTest),
    SM_TEST_CLASS_ENTRY(AboutDialogTest),
    SM_TEST_CLASS_ENTRY(CommandLineTest),
};
// clang-format on

}  // namespace

int main(int argc, char** argv) {
    // Unbuffered, so QTest's text output survives even when stdout is a
    // pipe (ctest) and the process exits abnormally before a flush.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const char* onlyClass = nullptr;
    bool listClasses = false;
    std::vector<char*> qtArgs;
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--class") == 0 && i + 1 < argc) {
            onlyClass = argv[++i];
        } else if (std::strcmp(argv[i], "--list-classes") == 0) {
            listClasses = true;
        } else {
            qtArgs.push_back(argv[i]);
        }
    }
    int qtArgc = static_cast<int>(qtArgs.size());
    qtArgs.push_back(nullptr);

    // Redirects ini-format QSettings storage (MainWindow's recentProjects_,
    // in particular - see main_window.h's docs) to a sandboxed test
    // location, so running this suite never touches - or pollutes with
    // throwaway test project paths - the real, installed Studio's own
    // persisted settings on this machine.
    QStandardPaths::setTestModeEnabled(true);
    // ...and gives this process its own ini folder. Test mode alone shares
    // one location between every test process, so two runs at once (a
    // long MainWindowTest run in the background, say) overwrote each
    // other's settings mid-test and broke "remembered across sessions"
    // checks at random.
    const QString settingsDirectory =
        QDir(QDir::tempPath())
            .filePath(QStringLiteral("sound-mind-test-settings-%1").arg(QCoreApplication::applicationPid()));
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory);
    struct RemoveOnExit {
        QString path;
        ~RemoveOnExit() { QDir(path).removeRecursively(); }
    } const removeSettings{settingsDirectory};

    // The offscreen platform looks for fonts only in Qt's own lib/fonts,
    // which Qt no longer ships - so text (the Text tool, v0.Y.78.1) would
    // render as nothing. Point it at the system's fonts unless the caller
    // already chose a directory.
#ifdef _WIN32
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
        const QString windowsDirectory = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
        qputenv("QT_QPA_FONTDIR", QDir(windowsDirectory).filePath(QStringLiteral("Fonts")).toLocal8Bit());
    }
#endif

    QApplication app(qtArgc, qtArgs.data());

    // See main.cpp's own comment - the same static-library resource
    // gotcha applies here too: LandingPageTest/ThemeTest need
    // ":/ChooseAgainLarge.png" to actually resolve.
    Q_INIT_RESOURCE(app);

    if (listClasses) {
        for (const auto& entry : kTestClasses) {
            std::printf("%s\n", entry.name);
        }
        return 0;
    }

    // QTest's text logger doesn't write to a piped stdout on Windows when
    // no console is attached (it targets the debugger output instead), so
    // ctest would see nothing. Unless the caller chose its own -o, log each
    // class to a temporary file and copy that to stdout afterwards.
    QStringList baseArgs;
    bool callerChoseOutput = false;
    for (int i = 0; i < qtArgc; ++i) {
        baseArgs << QString::fromLocal8Bit(qtArgs[static_cast<std::size_t>(i)]);
        callerChoseOutput = callerChoseOutput || std::strcmp(qtArgs[static_cast<std::size_t>(i)], "-o") == 0;
    }

    bool matched = false;
    int failedClasses = 0;
    for (const auto& entry : kTestClasses) {
        if (onlyClass != nullptr && std::strcmp(onlyClass, entry.name) != 0) {
            continue;
        }
        matched = true;
        const auto testObject = entry.make();
        if (callerChoseOutput) {
            failedClasses += QTest::qExec(testObject.get(), baseArgs) != 0 ? 1 : 0;
            continue;
        }
        const QString logPath = QDir::temp().filePath(QStringLiteral("sound-mind-studio-tests-%1-%2.txt")
                                                          .arg(QCoreApplication::applicationPid())
                                                          .arg(QString::fromLatin1(entry.name)));
        QStringList args = baseArgs;
        args << QStringLiteral("-o") << logPath + QStringLiteral(",txt");
        failedClasses += QTest::qExec(testObject.get(), args) != 0 ? 1 : 0;
        QFile log(logPath);
        if (log.open(QIODevice::ReadOnly)) {
            const QByteArray text = log.readAll();
            std::fwrite(text.constData(), 1, static_cast<std::size_t>(text.size()), stdout);
            log.close();
            log.remove();
        }
    }
    if (onlyClass != nullptr && !matched) {
        std::fprintf(stderr, "Unknown test class: %s\n", onlyClass);
        return 2;
    }
    return failedClasses == 0 ? 0 : 1;
}
