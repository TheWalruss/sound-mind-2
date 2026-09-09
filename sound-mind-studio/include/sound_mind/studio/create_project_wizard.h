#pragma once

#include <filesystem>

#include <QDialog>

#include "sound_mind/core/project_settings.h"

class QDialogButtonBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace sound_mind::studio {

/**
 * @brief The "Create Project" dialog - see `docs/sound-mind-roadmap.md`'s
 *        Create Project Wizard milestone (`v0.Y.11.1`).
 *
 * Replaces the previous no-dialog `newProject()` (an in-memory default
 * project, nothing asked): Name, Save Location, and Duration are always
 * visible; sample rate, frequency range, bin count, and timestep are
 * hidden behind an "Advanced" disclosure, defaulted to match
 * `sound_mind::core::ProjectSettings{}`'s own defaults for anyone who
 * never opens it.
 *
 * Purely presentational, the same division of responsibility as
 * `LandingPage`: this dialog only collects settings()/path() - it never
 * creates or saves a `Project` itself. `MainWindow::createProjectAt()` is
 * what actually does that once the dialog is accepted.
 *
 * @note Name doesn't correspond to any persisted `ProjectSettings` field
 *       (no such field exists) - its role is *being* the destination
 *       file's name: Location is a folder, not a full file path, so the
 *       project's name and its `.smproj` filename can never drift apart
 *       the way two independently-typed fields could.
 */
class CreateProjectWizard : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog with every field defaulted to match
    ///        `sound_mind::core::ProjectSettings{}` (Advanced section
    ///        collapsed), and Name/Location both empty.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit CreateProjectWizard(QWidget* parent = nullptr);

    /**
     * @brief The settings the dialog's fields currently describe.
     *
     * `canvasWidth` is derived from the Duration field (at whatever
     * timestep is currently set); `canvasHeight` is kept numerically
     * equal to `binCount`, per `sound_mind::core::ProjectSettings`'s own
     * docs on why the two are kept in sync.
     *
     * @return A `ProjectSettings` built from the current field values -
     *         meaningful any time, not just after the dialog is accepted
     *         (useful for tests exercising field collection directly).
     */
    [[nodiscard]] sound_mind::core::ProjectSettings settings() const;

    /**
     * @brief The destination `.smproj` path the Name and Location fields
     *        currently describe together.
     * @return `<Location>/<Name>.smproj` - Name's own text has any
     *         already-present `.smproj` suffix stripped first, so typing
     *         one in doesn't double it up.
     */
    [[nodiscard]] std::filesystem::path path() const;

private slots:
    /// @brief "Browse..." button handler: opens a real folder-choosing
    ///        dialog, and fills the Location field with the chosen
    ///        folder if one was chosen.
    void browseForLocation();

    /// @brief Enables the dialog's OK button only once both Name and
    ///        Location are non-empty - connected to both fields'
    ///        `textChanged`.
    void updateOkEnabled();

private:
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* locationEdit_ = nullptr;
    QDoubleSpinBox* durationSecondsSpin_ = nullptr;

    QWidget* advancedContainer_ = nullptr;
    QSpinBox* sampleRateSpin_ = nullptr;
    QSpinBox* binCountSpin_ = nullptr;
    QSpinBox* minFrequencySpin_ = nullptr;
    QSpinBox* maxFrequencySpin_ = nullptr;
    QDoubleSpinBox* timestepMsSpin_ = nullptr;

    QDialogButtonBox* buttonBox_ = nullptr;
};

}  // namespace sound_mind::studio
