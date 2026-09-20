#pragma once

#include <QDialog>

namespace sound_mind::studio {

/**
 * @brief A simple "About Sound Mind Studio" dialog - name, version,
 *        a one-line description, and a copyright line - per
 *        `docs/sound-mind-design.md`'s "Documentation links" ("there's an
 *        About box and the documentation, which was helpful"),
 *        `v0.0.42.4` (Workflow & Device Polish, Installment D).
 *
 * Deliberately a single, plain panel - no "Open Source Notices" tab
 * enumerating third-party dependency licenses the way the legacy Studio's
 * own About box had, confirmed with the user: a full, accurate notices
 * listing for this rewrite's actual current dependencies (Qt, JUCE,
 * ffmpeg, PocketFFT, libtiff, nlohmann::json) is real license-review work
 * worth its own deliberate pass, not something to fold in silently here.
 *
 * Shown by `MainWindow::showAboutDialog()` via `exec()` - not itself
 * headless-testable (the same real, interactive-gesture-only modal-dialog
 * exception every other `exec()`-based dialog in this codebase already
 * has), but this class's own content is directly testable without
 * `exec()` - see `test_about_dialog.cpp`.
 */
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog - name, `SOUND_MIND_VERSION` (falling back
    ///        to `"unknown"` if undefined, matching `MainWindow`'s own
    ///        identical macro-guard pattern), a short description, and a
    ///        copyright line - plus a single Close button.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit AboutDialog(QWidget* parent = nullptr);
};

}  // namespace sound_mind::studio
