#pragma once

#include <QDialog>
#include <QString>

class QDoubleSpinBox;
class QLineEdit;

namespace sound_mind::studio {

/**
 * @brief A small modal dialog collecting the details a fresh Mind Shot/
 *        Mind Grain capture needs beyond its own captured content/reference -
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.55.1` (MIDI track import) own
 *        stated prerequisite: "both need a stored fundamental frequency and
 *        a start-time offset."
 *
 * Replaces the previous instant, no-dialog capture flow (`MainWindow::
 * captureMindShot()`/`captureMindGrain()` used to call straight through to
 * `ToolPaletteController`/`SelectionController` with only an auto-generated
 * name, no user input at all) - a real value is needed for
 * `fundamentalFrequencyHz()`/`startTimeOffsetSeconds()` to be useful for
 * MIDI import's own pitch-shifting (see `sound_mind::core::
 * MindShotConfiguration::fundamentalFrequencyHz()`'s own docs), and there is
 * no other library-management UI yet to set them after the fact (see
 * `NamedMindGrain`'s own "no Mind Grain library management UI yet" note) -
 * so this dialog is the only place either field can ever be set.
 *
 * One dialog class serves both Mind Shot and Mind Grain capture (identical
 * fields either way - only the window title/auto-suggested name differ,
 * both supplied by the caller) rather than two near-identical copies.
 *
 * Both numeric fields default to `0.0` - "not set," the same meaning
 * `fundamentalFrequencyHz()`/`startTimeOffsetSeconds()` themselves give that
 * value - so accepting the dialog without touching either field reproduces
 * the exact pre-`v0.Y.55.1` behavior (a verbatim, centered stamp) for anyone
 * who doesn't need pitch-shifting/onset alignment.
 */
class MindCaptureDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the dialog, pre-filled with `defaultName`.
     * @param windowTitle This dialog's own window title (e.g. "Capture Mind
     *        Shot"/"Capture Mind Grain") - the only difference callers
     *        actually need between the two capture flows.
     * @param defaultName The name field's own initial text - typically an
     *        auto-generated `"Mind Shot N"`/`"Mind Grain N"`, still freely
     *        editable before accepting.
     * @param parent The owning widget, or `nullptr`.
     */
    explicit MindCaptureDialog(const QString& windowTitle, const QString& defaultName, QWidget* parent = nullptr);

    /// @brief The name field's own current text, trimmed of leading/
    ///        trailing whitespace.
    /// @return The name to capture under.
    [[nodiscard]] QString name() const;

    /// @brief The fundamental frequency field's own current value.
    /// @return The value to store as `fundamentalFrequencyHz()` - `0.0`
    ///         means "not set."
    [[nodiscard]] double fundamentalFrequencyHz() const;

    /// @brief The start-time offset field's own current value.
    /// @return The value to store as `startTimeOffsetSeconds()` - `0.0`
    ///         means "not set."
    [[nodiscard]] double startTimeOffsetSeconds() const;

private:
    QLineEdit* nameLineEdit_ = nullptr;
    QDoubleSpinBox* fundamentalFrequencySpinBox_ = nullptr;
    QDoubleSpinBox* startTimeOffsetSpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
