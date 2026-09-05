/**
 * @file main.cpp
 * @brief Entry point for the Sound Mind Studio GUI application.
 *
 * Currently opens an empty window - this is a toolchain/design bring-up
 * milestone (see CHANGELOG.md, v0.0.0.1), not the real Studio UI yet. See
 * `docs/sound-mind-design.md` for what the Studio is meant to become, and
 * `docs/sound-mind-architecture.md` for how this executable fits together
 * with sound-mind-core and sound-mind-codec.
 */

#include <QApplication>
#include <QMainWindow>
#include <QString>

#ifndef SOUND_MIND_VERSION
#define SOUND_MIND_VERSION "unknown"
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Sound Mind Studio v" SOUND_MIND_VERSION));
    window.resize(800, 600);
    window.show();

    return app.exec();
}
