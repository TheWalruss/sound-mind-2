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
#include "sound_mind/studio/theme.h"

/**
 * @brief Constructs and shows the main window, then runs the Qt event loop.
 *
 * Applies the app-wide brand stylesheet and default window icon (see
 * `theme.h` and `docs/sound-mind-roadmap.md`'s Visual Identity milestone,
 * `v0.Y.14.1`) at the `QApplication` level, before the window is built, so
 * every widget - including any dialog `MainWindow` opens later - picks
 * them up automatically rather than needing them set individually.
 *
 * @param argc Argument count, forwarded to QApplication.
 * @param argv Argument values, forwarded to QApplication.
 * @return The application's exit code, from QApplication::exec().
 */
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // assets/app.qrc's resources are compiled into sound-mind-studio-lib, a
    // static library - the linker drops a static library's object files
    // that nothing else references, and (unlike a .cpp file with ordinary
    // code) nothing calls into rcc-generated qrc_app.cpp on its own, so
    // without this explicit call the embedded logo silently never
    // registers and every ":/ChooseAgainLarge.png" lookup returns null.
    // Standard Qt pattern for exactly this - see Q_INIT_RESOURCE's own
    // docs. Must run before theme.cpp/landing_page.cpp's first resource
    // access, which "before MainWindow is constructed" guarantees here.
    Q_INIT_RESOURCE(app);

    app.setStyleSheet(sound_mind::studio::studioStyleSheet());
    app.setWindowIcon(sound_mind::studio::studioWindowIcon());

    sound_mind::studio::MainWindow window;
    window.show();

    return app.exec();
}
