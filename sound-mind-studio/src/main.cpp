/**
 * @file main.cpp
 * @brief Entry point for the Sound Mind Studio GUI application.
 *
 * See `docs/sound-mind-design.md` for what the Studio is meant to become,
 * `docs/sound-mind-architecture.md` for how this executable fits together
 * with sound-mind-core and sound-mind-codec, and
 * `docs/sound-mind-roadmap.md` for which milestone this build represents.
 */

#include <QApplication>

#include "sound_mind/studio/main_window.h"

/**
 * @brief Constructs and shows the main window, then runs the Qt event loop.
 * @param argc Argument count, forwarded to QApplication.
 * @param argv Argument values, forwarded to QApplication.
 * @return The application's exit code, from QApplication::exec().
 */
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    sound_mind::studio::MainWindow window;
    window.show();

    return app.exec();
}
