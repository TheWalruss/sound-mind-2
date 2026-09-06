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
#include <QtTest/QtTest>

#include "test_canvas_widget.h"
#include "test_main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    int status = 0;

    CanvasWidgetTest canvasWidgetTest;
    status |= QTest::qExec(&canvasWidgetTest, argc, argv);

    MainWindowTest mainWindowTest;
    status |= QTest::qExec(&mainWindowTest, argc, argv);

    return status;
}
