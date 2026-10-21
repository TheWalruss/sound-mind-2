#pragma once

#include <QDialog>

namespace sound_mind::studio {

/**
 * @brief The "About Sound Mind Studio" dialog: name, version, a one-line
 *        description, the copyright and licence, and the third-party notices
 *        (`v0.0.42.4`; licence and notices since `v0.1.42.1`).
 *
 * **Licence...** and **Open-source notices...** each open a scrollable,
 * non-modal viewer (`licenceViewer` / `noticesViewer`) showing `LICENSE.md` /
 * `THIRD_PARTY_NOTICES.md` rendered from Markdown. Both documents are
 * embedded as Qt resources (`:/legal/`), so they're always available; the
 * same files, plus every licence text, also ship next to the program.
 *
 * Shown by `MainWindow::showAboutDialog()` via `exec()`; its content and the
 * viewers are testable without `exec()` - see `test_about_dialog.cpp`.
 */
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog - name, `SOUND_MIND_VERSION` (`"unknown"` if
    ///        undefined), a description, the copyright and licence summary,
    ///        the two document buttons and a Close button.
    /// @param parent The owning widget; may be `nullptr`.
    explicit AboutDialog(QWidget* parent = nullptr);

    /// @brief Sound Mind Studio's licence, as shipped (Markdown).
    /// @return The text of the embedded `LICENSE.md`; empty if missing.
    [[nodiscard]] static QString licenceText();

    /// @brief The third-party notices, as shipped (Markdown).
    /// @return The text of the embedded `THIRD_PARTY_NOTICES.md`; empty if
    ///         missing.
    [[nodiscard]] static QString noticesText();

    /// @brief The copyright line shown in the dialog.
    /// @return E.g. "Copyright © 2026 Andreas Lundberg. All rights reserved."
    [[nodiscard]] static QString copyrightLine();

private:
    /// @brief Opens (or raises) a viewer for `markdown`, titled `title`.
    void showDocument(const QString& objectName, const QString& title, const QString& markdown);
};

}  // namespace sound_mind::studio
