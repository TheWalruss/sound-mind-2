#pragma once

#include <cstdint>
#include <optional>

#include <QDialog>
#include <QImage>

#include "sound_mind/studio/polar_origin_dialog.h"

class QPushButton;

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

        /// @brief Un-warps a polar/flower-shaped source image back to a
        /// rectangular one, via `PolarOriginDialog`'s own graphical
        /// origin/radius/arc picker - `docs/sound-mind-roadmap.md`'s
        /// `v0.Y.53.1` Installment B ("polar-form image import"). Only
        /// ever offered when this dialog was built with a real
        /// `polarSourceImage` (see the constructor's own docs) - matches
        /// the legacy Studio's own "Polar" size mode, which is likewise
        /// hidden whenever there's no single, real source image to
        /// preview it against (a multi-file import, in particular).
        Polar,
    };

    /**
     * @brief Builds the dialog with `RescaleToFitProject` pre-selected,
     *        per this milestone's confirmed default.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     * @param allowSequential Whether to show the "Import as sequence"
     *        checkbox at all - `MainWindow::importImage()` passes `true`
     *        only when more than one file was selected, matching the
     *        legacy Studio's own "only meaningful for multiple files"
     *        gating. Defaults to `false` so every pre-`v0.Y.22.1` call
     *        site (single-file import, and every existing test) keeps
     *        constructing an identical dialog with no source changes
     *        needed.
     * @param polarSourceImage The single file's own already-loaded image,
     *        so the "Polar" radio (and, once selected, its own "Set
     *        origin..." button) can be offered at all - a null `QImage`
     *        (the default, and every pre-`v0.Y.53.1` call site) hides
     *        "Polar" entirely, matching `allowSequential`'s own "only
     *        offered when it's actually usable" precedent. Never passed
     *        alongside `allowSequential == true` in practice (a real,
     *        multi-file import has no single image to preview) - not
     *        itself enforced here, since a caller that somehow did would
     *        just get a "Polar" option a sequence import silently ignores
     *        anyway (see the class's own docs on what a sequence import
     *        does with `mode`).
     * @param polarDefaultOutputWidth The polar picker's own default output
     *        width - see `PolarOriginDialog`'s own constructor docs.
     *        Ignored if `polarSourceImage` is null.
     * @param polarTimestepMs The polar picker's own duration-label basis -
     *        see `PolarOriginDialog`'s own constructor docs. Ignored if
     *        `polarSourceImage` is null.
     */
    explicit ImageScalePickerDialog(QWidget* parent = nullptr, bool allowSequential = false,
                                     QImage polarSourceImage = QImage(), std::uint32_t polarDefaultOutputWidth = 0,
                                     double polarTimestepMs = 0.0);

    /// @brief The currently selected mode.
    ///
    /// Meaningless while importAsSequence() is `true` - the mode radios are
    /// disabled in that state (see the class docs) and the caller should
    /// consult importAsSequence() first, not this.
    ///
    /// @return The mode whose radio button is currently checked.
    [[nodiscard]] Mode selectedMode() const;

    /**
     * @brief The parameters chosen for a `Mode::Polar` import - call after
     *        `exec()` returns `QDialog::Accepted` and selectedMode() is
     *        `Mode::Polar`.
     *
     * Reflects the *last* `PolarOriginDialog` this dialog's own "Set
     * origin..." button opened and was accepted from - the picker
     * defaults (image centre, 45% radius, full circle, the constructor's
     * own `polarDefaultOutputWidth`) if that button was never clicked at
     * all, so accepting this dialog with `Mode::Polar` selected but the
     * picker never opened still gives a real, usable result rather than
     * an empty one.
     *
     * @return The chosen parameters, or `std::nullopt` if this dialog was
     *         built with a null `polarSourceImage` (there is no picker to
     *         have chosen anything from).
     */
    [[nodiscard]] std::optional<PolarImportParams> polarParams() const;

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
    /// @brief Opens `PolarOriginDialog` (seeded from polarParams_'s own
    ///        current value) and, if accepted, stores its result into
    ///        polarParams_ - the "Set origin..." button's own slot.
    void openPolarOriginDialog();

    /// @brief The default `PolarImportParams` for polarSourceImage_ - the
    ///        exact same centre/45%-radius/full-circle defaults a fresh
    ///        `PolarOriginPickerWidget` starts with, computed directly
    ///        (without actually constructing/showing a `PolarOriginDialog`
    ///        just to read them back) so `polarParams()` has a real result
    ///        even if the "Set origin..." button is never clicked - see
    ///        its own docs.
    [[nodiscard]] PolarImportParams defaultPolarParams() const;

    Mode selectedMode_ = Mode::RescaleToFitProject;
    bool importAsSequence_ = false;

    QImage polarSourceImage_;
    std::uint32_t polarDefaultOutputWidth_ = 0;
    double polarTimestepMs_ = 0.0;
    std::optional<PolarImportParams> polarParams_;
    QPushButton* polarOriginButton_ = nullptr;
};

}  // namespace sound_mind::studio
