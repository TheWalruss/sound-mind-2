#pragma once

#include <QIcon>
#include <QString>

namespace sound_mind::studio {

/**
 * @brief The Studio's app-wide visual identity - see
 *        `docs/sound-mind-roadmap.md`'s Visual Identity milestone
 *        (`v0.Y.14.1`).
 *
 * Carries the legacy documentation's brand palette (deep orange `#DD4B00`
 * to amber gold `#FEC100`, on a dark ground - see `extra.css` in the
 * legacy repo's `docs/stylesheets/`) into the Studio itself, for a
 * cohesive look between the app and its own documentation (see
 * `docs/doxygen/sound-mind-theme.css` for the Doxygen-side half of that).
 *
 * @return The complete Qt stylesheet (QSS) to apply app-wide via
 *         `QApplication::setStyleSheet()` - see `main.cpp`.
 *
 * @note Deliberately a single, fixed dark-first look, not a light/dark
 *       toggle. `docs/sound-mind-design.md`'s Export section already names
 *       a future "light/dark mode" as a Studio-wide setting that affects
 *       canvas color mapping and export - this milestone doesn't build
 *       that toggle (no scope creep beyond what the roadmap entry calls
 *       for); when it exists, this function's fixed QSS will need
 *       revisiting to become theme-aware rather than one constant string.
 */
[[nodiscard]] QString studioStyleSheet();

/**
 * @brief The Studio's application/window icon.
 *
 * Loaded from the embedded `ChooseAgainLarge.png` resource (see
 * `assets/app.qrc`) - a plain, natively-supported image format, rather
 * than the `.ico` file also carried over from the legacy Studio (that one
 * is used only as the executable's native Win32 resource icon, via
 * `resources/app.rc`, read directly by the RC compiler at build time - see
 * that file's own docs for why loading it through Qt at runtime too would
 * just add an avoidable `qico` plugin dependency for no real benefit).
 *
 * @return The icon, suitable for `QApplication::setWindowIcon()` and
 *         `QWidget::setWindowIcon()` alike - Qt scales it down as needed
 *         for the taskbar, title bar, and Alt+Tab.
 */
[[nodiscard]] QIcon studioWindowIcon();

}  // namespace sound_mind::studio
