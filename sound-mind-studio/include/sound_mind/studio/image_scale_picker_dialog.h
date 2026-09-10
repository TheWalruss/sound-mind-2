#pragma once

#include <QDialog>

namespace sound_mind::studio {

/**
 * @brief Lets the user choose how an imported image is resized to the
 *        project's canvas dimensions, before the import proceeds - see
 *        `docs/sound-mind-roadmap.md`'s Image Import Scaling milestone
 *        (`v0.Y.20.1`).
 *
 * Always shown by `MainWindow::importImage()` - unlike the Audio Import
 * Snippets milestone's picker, there's always a real choice to make here
 * (an image import has no "trivial, only one option" case the way a short
 * audio file does).
 *
 * Purely presentational, the same division of responsibility as
 * `AudioSnippetPickerDialog`/`LayersPanel`: `MainWindow` reads
 * `selectedMode()` back after `exec()` returns `QDialog::Accepted` and
 * does the actual resizing itself - no image I/O happens in this class.
 *
 * As of `v0.Y.22.1` (Image Sequence Import), the dialog can also offer an
 * "Import as sequence" checkbox - see the `allowSequential` constructor
 * parameter and importAsSequence()'s own docs.
 */
class ImageScalePickerDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief The five ways an imported image can be resized to the
     *        project's canvas dimensions - narrower than the legacy
     *        Studio's own set (see the roadmap entry's own "Narrower than
     *        the legacy version" note for what's deliberately not carried
     *        over, and which legacy mode each of these matches, if any).
     */
    enum class Mode {
        /// @brief Both axes stretched to the project's exact width/height,
        /// independent of the source image's own aspect ratio. The
        /// default - matches the legacy Studio's `stretch_fill`.
        RescaleToFitProject,

        /// @brief Height changes to match the project's bin count; width
        /// stays the source image's own native pixel width. Matches
        /// legacy's `height_only`.
        ScaleVerticalKeepHorizontal,

        /// @brief Width changes to match the project's canvas width;
        /// height stays the source image's own native pixel height. Not
        /// present in the legacy Studio.
        ScaleHorizontalKeepVertical,

        /// @brief Height changes to match the project's bin count; width
        /// scales proportionally, preserving the source's aspect ratio.
        /// Matches legacy's `aspect` (its actual default there).
        ScaleVerticalProportional,

        /// @brief No rescaling at all. Matches legacy's `native`.
        KeepNativeResolution,
    };

    /// @brief Builds the dialog with `RescaleToFitProject` pre-selected,
    ///        per this milestone's confirmed default.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    /// @param allowSequential Whether to show the "Import as sequence"
    ///        checkbox at all - `MainWindow::importImage()` passes `true`
    ///        only when more than one file was selected, matching the
    ///        legacy Studio's own "only meaningful for multiple files"
    ///        gating. Defaults to `false` so every pre-`v0.Y.22.1` call
    ///        site (single-file import, and every existing test) keeps
    ///        constructing an identical dialog with no source changes
    ///        needed.
    explicit ImageScalePickerDialog(QWidget* parent = nullptr, bool allowSequential = false);

    /// @brief The currently selected mode.
    ///
    /// Meaningless while importAsSequence() is `true` - the mode radios are
    /// disabled in that state (see the class docs) and the caller should
    /// consult importAsSequence() first, not this.
    ///
    /// @return The mode whose radio button is currently checked.
    [[nodiscard]] Mode selectedMode() const;

    /**
     * @brief Whether "Import as sequence" is checked - see
     *        `docs/sound-mind-roadmap.md`'s Image Sequence Import milestone
     *        (`v0.Y.22.1`).
     *
     * Always `false` when the dialog was built with `allowSequential` set
     * to `false` (the checkbox doesn't exist in that case, so there's
     * nothing to check). When `true`, the caller should ignore
     * selectedMode() entirely - `MainWindow::importImageFiles()` always
     * applies `Mode::ScaleVerticalProportional` to every file in a
     * sequence, regardless of whichever mode radio was last selected
     * before the checkbox disabled them.
     *
     * @return Whether sequential import was requested.
     */
    [[nodiscard]] bool importAsSequence() const;

private:
    Mode selectedMode_ = Mode::RescaleToFitProject;
    bool importAsSequence_ = false;
};

}  // namespace sound_mind::studio
