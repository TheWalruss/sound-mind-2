/**
 * @file main.cpp
 * @brief Runs every QTest test class in this directory from one binary.
 *
 * QTEST_MAIN() generates a main() for a single test class; combining
 * several into one CTest-registered executable means driving QTest::qExec
 * manually instead, sharing one QApplication across all of them (QWidget
 * construction needs one). Run with QT_QPA_PLATFORM=offscreen - see this
 * target's CMakeLists.txt - since CI has no real display.
 */

#include <QApplication>
#include <QStandardPaths>
#include <QtTest/QtTest>

#include "test_audio_snippet_picker_dialog.h"
#include "test_canvas_widget.h"
#include "test_create_project_wizard.h"
#include "test_image_scale_picker_dialog.h"
#include "test_import_export.h"
#include "test_import_helpers.h"
#include "test_landing_page.h"
#include "test_layers_panel.h"
#include "test_loop_panel.h"
#include "test_main_window.h"
#include "test_paint_controller.h"
#include "test_playback_controller.h"
#include "test_playback_panel.h"
#include "test_record_panel.h"
#include "test_recent_projects.h"
#include "test_theme.h"
#include "test_tool_configuration_panel.h"

int main(int argc, char** argv) {
    // Redirects ini-format QSettings storage (MainWindow's recentProjects_,
    // in particular - see main_window.h's docs) to a sandboxed test
    // location, so running this suite never touches - or pollutes with
    // throwaway test project paths - the real, installed Studio's own
    // persisted settings on this machine.
    QStandardPaths::setTestModeEnabled(true);

    QApplication app(argc, argv);

    // See main.cpp's own comment - the same static-library resource
    // gotcha applies here too: LandingPageTest/ThemeTest need
    // ":/ChooseAgainLarge.png" to actually resolve.
    Q_INIT_RESOURCE(app);

    int status = 0;

    CanvasWidgetTest canvasWidgetTest;
    status |= QTest::qExec(&canvasWidgetTest, argc, argv);

    LandingPageTest landingPageTest;
    status |= QTest::qExec(&landingPageTest, argc, argv);

    MainWindowTest mainWindowTest;
    status |= QTest::qExec(&mainWindowTest, argc, argv);

    RecentProjectsTest recentProjectsTest;
    status |= QTest::qExec(&recentProjectsTest, argc, argv);

    ThemeTest themeTest;
    status |= QTest::qExec(&themeTest, argc, argv);

    CreateProjectWizardTest createProjectWizardTest;
    status |= QTest::qExec(&createProjectWizardTest, argc, argv);

    LayersPanelTest layersPanelTest;
    status |= QTest::qExec(&layersPanelTest, argc, argv);

    LoopPanelTest loopPanelTest;
    status |= QTest::qExec(&loopPanelTest, argc, argv);

    RecordPanelTest recordPanelTest;
    status |= QTest::qExec(&recordPanelTest, argc, argv);

    PlaybackPanelTest playbackPanelTest;
    status |= QTest::qExec(&playbackPanelTest, argc, argv);

    PlaybackControllerTest playbackControllerTest;
    status |= QTest::qExec(&playbackControllerTest, argc, argv);

    PaintControllerTest paintControllerTest;
    status |= QTest::qExec(&paintControllerTest, argc, argv);

    ToolConfigurationPanelTest toolConfigurationPanelTest;
    status |= QTest::qExec(&toolConfigurationPanelTest, argc, argv);

    AudioSnippetPickerDialogTest audioSnippetPickerDialogTest;
    status |= QTest::qExec(&audioSnippetPickerDialogTest, argc, argv);

    ImageScalePickerDialogTest imageScalePickerDialogTest;
    status |= QTest::qExec(&imageScalePickerDialogTest, argc, argv);

    ImportHelpersTest importHelpersTest;
    status |= QTest::qExec(&importHelpersTest, argc, argv);

    ImportExportTest importExportTest;
    status |= QTest::qExec(&importExportTest, argc, argv);

    return status;
}
