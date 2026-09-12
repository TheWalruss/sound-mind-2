#pragma once

#include <optional>

#include <QObject>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/grid_config.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief Owns the current rectangular selection and turns Fill into a new,
 *        non-destructive `FillOperation` - see `docs/sound-mind-design.md`'s
 *        "Selection" ("Rectangle") and "Fill".
 *
 * A selection is deliberately *not* itself a logged `Operation` - it's
 * ephemeral, per-session UI state (the same shape `PickController`'s own
 * selection already established) that *scopes* other operations, per the
 * design doc's own framing ("A selection scopes an operation... to a
 * specific region of a layer"). It's tied to whichever layer it was drawn
 * on (captured once, at the drag that created it), not re-derived from
 * whatever the "currently active" layer happens to be later - the same
 * "pinned at creation time" precedent `PickController`'s own selected
 * object already follows.
 *
 * Shares `PaintController`'s own per-layer pre-paint base cache rather
 * than keeping a second one, the same reason (and the same shared
 * `rebuildLayerContent()` call) `PickController` already established.
 *
 * **Deliberately Rectangle-only, for now**: Lasso and Wand (and boolean
 * combination between multiple selections) are real, designed features
 * (`docs/sound-mind-design.md`'s own "Selection" section) not built yet -
 * a plain `TimeFrequencyRect` is all a Rectangle-only selection needs to
 * represent, so that's what this class uses rather than a more general
 * (and, for now, unneeded) region/mask representation.
 *
 * **Cut/Copy/Paste, and cross-layer independence**: `copySelection()`/
 * `cutSelection()` capture the committed selection's own pixels off
 * `selectionLayer_` (whichever layer the selection was drawn on) into an
 * owned `sound_mind::core::Clip` on this controller - the clipboard.
 * `pasteInto()` then writes that clip onto whatever layer its own caller
 * names, which may be a *different* layer than the one it was copied from
 * - a `PasteOperation`'s own `targetLayer` is independent of both the
 * clipboard's origin layer and the selection's own `selectionLayer_`, per
 * `PasteOperation`'s own docs. `cutSelection()` is Copy plus a same-bounds
 * `FillOperation` on `selectionLayer_` written with an opaque, ~-96dB
 * ("silence floor", the same constant `color_conversion.h`'s own display
 * range and Codec's Pool quantization already use) gradient on both
 * channels - reusing `FillOperation` rather than inventing a dedicated
 * "delete" `Operation` subtype, the same way `PickController`'s own delete
 * reuses an empty-effect `PaintOperation`.
 */
class SelectionController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a controller with no project set and no selection.
     * @param paintController The controller whose rebuildLayerContent()
     *        this one calls after a Fill commits - see the class's own
     *        docs on why the pre-paint base cache is shared, not
     *        duplicated. Not owned; must outlive this object.
     * @param parent The owning object, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit SelectionController(PaintController* paintController, QObject* parent = nullptr);

    /**
     * @brief Sets whether Snap to Grid is currently on, and the
     *        Frequency/Timing Grid configurations to snap against while
     *        it is - see `PickController::setGridSnapping()`'s own docs
     *        (this is the same idea, applied to continueSelectionDrag()'s
     *        own dragged corner instead of a Pick move/Path node).
     *
     * @param enabled Whether Snap to Grid is on.
     * @param frequencyGridConfig Which Frequency Grid source(s) are
     *        active, to snap the frequency axis against.
     * @param timingGridConfig The Timing Grid's own current mode, to
     *        snap the time axis against.
     */
    void setGridSnapping(bool enabled, const FrequencyGridConfig& frequencyGridConfig,
                          const TimingGridConfig& timingGridConfig);

    /**
     * @brief Sets which project selection/fill targets.
     *
     * Clears the current selection, any in-progress drag, and the
     * clipboard - all three are meaningless once the project they refer to
     * is gone (a clipped clip's own pixel dimensions are tied to the
     * project's own Stream config, which a different project need not
     * share).
     *
     * @param project The project to select within; may be `nullptr`
     *        (nothing selectable until a real one is set again).
     */
    void setProject(sound_mind::core::Project* project);

    /**
     * @brief Starts a new rectangular selection drag on `layer`, anchored
     *        at `point`. Replaces (visually, until committed - see
     *        endSelection()'s own docs) whatever selection already
     *        existed.
     * @param layer Which layer this selection will scope operations on.
     * @param point The drag's own anchor corner, already converted to
     *        time/frequency space.
     */
    void beginSelectionDrag(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Continues an in-progress selection drag, live-updating the
     *        rectangle between the original anchor and `point`. A no-op
     *        if no drag is in progress.
     *
     * Emits boundsChanged() so the caller can redraw the live rectangle.
     *
     * @param point The cursor's current position, in time/frequency
     *        space.
     */
    void continueSelectionDrag(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Ends an in-progress selection drag, committing its own
     *        rectangle as the current selection - unless the drag never
     *        really moved (a plain click, not a drag), in which case this
     *        clears the selection instead (the same "click empty space to
     *        deselect" convention `docs/sound-mind-design.md`'s "Pick"
     *        already established, applied here to "drew nothing" rather
     *        than "clicked nothing").
     *
     * A no-op if no drag is in progress. Emits boundsChanged(), and
     * selectionChanged() if the committed selection actually changed.
     */
    void endSelectionDrag();

    /// @brief Abandons an in-progress selection drag without committing
    ///        or clearing anything - the previous selection (if any)
    ///        reappears exactly as it was. A no-op if no drag is in
    ///        progress. Emits boundsChanged() if a live preview was
    ///        showing.
    void cancelSelectionDrag();

    /// @brief Clears the current selection ("Deselect") - a no-op if
    ///        there isn't one. Emits boundsChanged() and
    ///        selectionChanged().
    void clearSelection();

    /// @brief Whether a selection currently exists (committed - not
    ///        mid-drag).
    /// @return `true` if a selection is committed; `false` otherwise.
    [[nodiscard]] bool hasSelection() const noexcept { return committedBounds_.has_value(); }

    /**
     * @brief What to actually draw as the selection overlay right now -
     *        the in-progress drag's own live rectangle while one is
     *        active, otherwise the committed selection.
     * @return The bounds to display, or `std::nullopt` if there's
     *         neither a drag in progress nor a committed selection.
     */
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyRect> displayBounds() const;

    /**
     * @brief Fills the current committed selection with `gradient` - the
     *        actual work behind Edit → Fill Selection. A no-op if there's
     *        no committed selection (mid-drag doesn't count).
     *
     * Appends a new `FillOperation` (not superseding anything - a fill is
     * a fresh, additive edit, the same as a new paint stroke, not a
     * revision of an existing one) and rebuilds the target layer's
     * content via `paintController_`.
     *
     * Emits contentChanged() for the affected layer.
     *
     * @param gradient The color (or gradient) to fill with.
     */
    void fill(const sound_mind::core::Gradient& gradient);

    /// @brief Whether a clip is currently on the clipboard (from a prior
    ///        copySelection()/cutSelection()).
    /// @return `true` if a clip is available to paste.
    [[nodiscard]] bool hasClipboard() const noexcept { return clipboard_.has_value(); }

    /**
     * @brief Copies the current committed selection's own pixels off
     *        `selectionLayer_` onto the clipboard - the actual work behind
     *        Edit → Copy. A no-op if there's no committed selection.
     *
     * Captures both the pixel data (via `captureClip()`) and the
     * selection's own bounds (so a later pasteInto() lands exactly where
     * the selection was, regardless of what's selected by then) - neither
     * is re-derived from whatever the selection happens to be at paste
     * time.
     */
    void copySelection();

    /**
     * @brief Copies the current committed selection (see copySelection())
     *        and then clears its own source pixels on `selectionLayer_` -
     *        the actual work behind Edit → Cut. A no-op if there's no
     *        committed selection.
     *
     * The "clear" is a same-bounds `FillOperation` with an opaque,
     * ~silence-floor gradient - see this class's own docs - appended and
     * replayed the same way Fill's own `fill()` already is; it targets
     * `selectionLayer_` specifically, independent of wherever a later
     * pasteInto() writes to.
     *
     * Emits contentChanged() for `selectionLayer_`.
     */
    void cutSelection();

    /**
     * @brief Pastes the current clipboard onto `targetLayer`, at the
     *        bounds it was originally copied from - the actual work behind
     *        Edit → Paste. A no-op if hasClipboard() is `false`.
     *
     * `targetLayer` is resolved by the caller (typically "whichever layer
     * is currently active"), independently of the clipboard's own source
     * layer - the same "may not be the same layer" independence
     * `PasteOperation`'s own docs describe. After pasting, the committed
     * selection is updated to the pasted region on `targetLayer`, so the
     * result is visibly highlighted the same way a fresh selection would
     * be.
     *
     * Emits contentChanged() for `targetLayer`, boundsChanged(), and
     * selectionChanged().
     *
     * @param targetLayer Which layer to paste onto.
     * @return The newly appended `PasteOperation`'s own id - so a caller
     *         (`MainWindow`, in particular - see its own paste()) can
     *         hand it straight to `PickController::selectOperation()`,
     *         making the pasted result immediately Pickable without a
     *         separate click to find it again. `std::nullopt` if this
     *         was a no-op (no clipboard).
     */
    std::optional<sound_mind::core::OperationId> pasteInto(sound_mind::core::LayerId targetLayer);

signals:
    /// @brief Emitted whenever displayBounds() would return something
    ///        different - a drag updating live, a selection committed,
    ///        a drag cancelled back to the prior selection, or a clear.
    void boundsChanged();

    /// @brief Emitted whenever the *committed* selection changes (a new
    ///        one replacing the old, or a clear) - unlike boundsChanged(),
    ///        not emitted for a drag's own live, not-yet-committed
    ///        updates.
    void selectionChanged();

    /// @brief Emitted whenever a layer's own rendered content changes as
    ///        a result of a committed Fill.
    /// @param layer Which layer's content changed.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;

    std::optional<sound_mind::core::TimeFrequencyRect> committedBounds_;
    sound_mind::core::LayerId selectionLayer_ = 0;

    std::optional<sound_mind::core::Clip> clipboard_;
    std::optional<sound_mind::core::TimeFrequencyRect> clipboardBounds_;

    bool dragActive_ = false;
    bool dragMoved_ = false;
    sound_mind::core::TimeFrequencyPoint dragAnchor_;
    sound_mind::core::TimeFrequencyRect dragPreviewBounds_;

    /// @brief Snap to Grid's own current state - see setGridSnapping()'s
    /// own docs.
    bool gridSnappingEnabled_ = false;
    FrequencyGridConfig frequencyGridConfig_;
    TimingGridConfig timingGridConfig_;
};

}  // namespace sound_mind::studio
