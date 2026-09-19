#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QObject>

#include "sound_mind/core/gradient.h"
#include "sound_mind/core/mind_grain.h"
#include "sound_mind/core/mind_shot.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/selection_region.h"
#include "sound_mind/core/warp_operation.h"
#include "sound_mind/studio/grid_config.h"

namespace sound_mind::studio {

class PaintController;

/**
 * @brief Which shape a `SelectionController` draws next - see
 *        `docs/sound-mind-design.md`'s "Selection" for all three, and
 *        `SelectionController::setSelectionShape()`'s own docs for how
 *        this is chosen.
 */
enum class SelectionShape {
    Rectangle,  ///< A rubber-band rectangle - every selection before Lasso existed.
    Lasso,      ///< A freehand-drawn closed curve, fit the same way a paint stroke's own Path is.
    Wand,       ///< A flood fill by amplitude similarity from a single clicked point (`v0.Y.35.1` Installment B).
};

/**
 * @brief How a newly-drawn selection combines with whatever selection was
 *        already committed - see `docs/sound-mind-design.md`'s "boolean
 *        combination" and `SelectionController::setSelectionCombineMode()`'s
 *        own docs for how this is chosen.
 */
enum class SelectionCombineMode {
    Replace,    ///< The new selection replaces the old outright - every selection's own behavior before this existed.
    Add,        ///< The new selection is unioned into the old.
    Subtract,   ///< The new selection is carved out of the old.
    Intersect,  ///< Only what both the old and new selections cover survives.
};

/**
 * @brief Owns the current selection (Rectangle, Lasso, or Wand - see
 *        `SelectionShape`), and how it combines with what was already
 *        selected (see `SelectionCombineMode`) - and turns Fill into a
 *        new, non-destructive `FillOperation` - see
 *        `docs/sound-mind-design.md`'s "Selection" and "Fill".
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
 * **Rectangle, Lasso, and Wand (`v0.Y.35.1` Installments A/B)**:
 * `beginSelectionDrag()`/`continueSelectionDrag()`/`endSelectionDrag()`/
 * `cancelSelectionDrag()` are one uniform API for all three shapes - which
 * one a given drag actually produces is decided internally, by
 * `setSelectionShape()`'s own current value, the same "one begin/continue/
 * end trio, config decides the actual behavior" shape `PaintController`'s
 * own tool-type dispatch already established (a `MindGrainConfiguration`
 * branch inside `beginStroke()`, not a separate `beginMindGrainStroke()`).
 * A committed selection's own bounding box (`committedBounds_`, reported
 * by `bounds()`/`displayBounds()`) always exists regardless of shape - a
 * plain `TimeFrequencyRect`, same as before Lasso existed; a non-
 * rectangular selection *additionally* carries its own precise shape
 * (`committedBoundary_`, a `sound_mind::core::SelectionRegion` - `Path`-
 * kind for Lasso, `Mask`-kind for Wand or any boolean-combined result),
 * `std::nullopt` for a plain Rectangle. `Fill`/`Copy`/`Cut`/`Paste` all
 * narrow to that shape when present - see `fill()`'s own docs and
 * `sound_mind::core::FillOperation`/`PasteOperation`'s own `boundary()`
 * docs.
 *
 * **Wand is a single-click gesture, not a drag** - its own flood fill
 * (`sound_mind::core::selectByAmplitudeSimilarity()`) runs once, at
 * `beginSelectionDrag()`'s own anchor point; `continueSelectionDrag()`
 * ignores any subsequent movement entirely before the gesture ends (the
 * classic "click, don't drag" wand convention - a jittery release
 * shouldn't move where the flood fill started from). `setWandTolerance()`/
 * `setWandHarmonicsAware()` set the two parameters it reads.
 *
 * **Boolean combination (`setSelectionCombineMode()`)**: `Add`/`Subtract`/
 * `Intersect` combine a newly-drawn selection (of *any* shape - Rectangle,
 * Lasso, or Wand) with whatever was already committed, producing a new
 * `Mask`-kind `committedBoundary_` (a combined result generally isn't
 * expressible as a single closed curve, even when both operands started
 * out simple - see `sound_mind::core::SelectionRegion::combine()`'s own
 * docs). A combining gesture that produces nothing (a plain click, or a
 * Lasso/Wand gesture that resolves to no area) leaves the existing
 * selection untouched, rather than clearing it the way a `Replace`-mode
 * "drew nothing" does - a combine attempt that "whiffs" shouldn't destroy
 * what it was trying to modify.
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
 *
 * **A known gap, deliberately out of scope**: Mind Shot/Mind Grain capture
 * (`captureMindShot()`/`captureMindGrain()`) still always captures/
 * references the selection's own full bounding box, regardless of a non-
 * rectangular boundary - `docs/sound-mind-design.md`'s "Mind Shots"/"Mind
 * Grains" predate Lasso/Wand and don't describe a non-rectangular capture;
 * extending them the same way Fill/Copy/Cut/Paste were is future work, not
 * resolved here.
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
     * @brief Sets which shape the *next* `beginSelectionDrag()` produces -
     *        the actual work behind the Selection Configuration Panel's
     *        own Selection Type dropdown.
     *
     * Cancels an in-progress drag first (via `cancelSelectionDrag()`) if
     * one is active - switching shape mid-drag would otherwise leave a
     * drag started under one shape's own bookkeeping half-finished under
     * the other's; the previously committed selection (if any) is
     * unaffected either way.
     *
     * @param shape The shape to draw next.
     */
    void setSelectionShape(SelectionShape shape);

    /// @brief The shape the *next* beginSelectionDrag() will produce.
    /// @return The current value set via setSelectionShape() -
    ///         `SelectionShape::Rectangle` for a fresh controller.
    [[nodiscard]] SelectionShape selectionShape() const noexcept { return currentShape_; }

    /**
     * @brief Sets how the *next* `endSelectionDrag()` combines its own
     *        newly-drawn selection with whatever was already committed -
     *        the actual work behind holding Shift (`Add`)/Alt (`Subtract`)/
     *        Shift+Alt (`Intersect`) while starting a new selection
     *        gesture, resolved by the caller (`MainWindow`, from the
     *        canvas's own mouse-press modifiers) before calling
     *        `beginSelectionDrag()`.
     * @param mode The combine mode to use for the next completed drag.
     */
    void setSelectionCombineMode(SelectionCombineMode mode) noexcept { currentCombineMode_ = mode; }

    /// @brief The combine mode the *next* endSelectionDrag() will use.
    /// @return The current value set via setSelectionCombineMode() -
    ///         `SelectionCombineMode::Replace` for a fresh controller.
    [[nodiscard]] SelectionCombineMode selectionCombineMode() const noexcept { return currentCombineMode_; }

    /**
     * @brief Sets Wand's own tolerance - the actual work behind the
     *        Selection Configuration Panel's own Tolerance spin box.
     * @param tolerancePercent See
     *        `sound_mind::core::selectByAmplitudeSimilarity()`'s own
     *        `tolerancePercent` docs.
     */
    void setWandTolerance(double tolerancePercent) noexcept { wandTolerancePercent_ = tolerancePercent; }

    /// @brief Wand's own current tolerance.
    /// @return The value set via setWandTolerance() - `10.0` (a modest,
    ///         generally-useful default) for a fresh controller.
    [[nodiscard]] double wandTolerance() const noexcept { return wandTolerancePercent_; }

    /**
     * @brief Sets whether Wand extends its own selection along the
     *        anchor's harmonic rows - the actual work behind the Selection
     *        Configuration Panel's own Harmonics-aware checkbox.
     * @param harmonicsAware See
     *        `sound_mind::core::selectByAmplitudeSimilarity()`'s own
     *        `harmonicsAware` docs.
     */
    void setWandHarmonicsAware(bool harmonicsAware) noexcept { wandHarmonicsAware_ = harmonicsAware; }

    /// @brief Whether Wand currently extends along harmonic rows.
    /// @return The value set via setWandHarmonicsAware() - `false` for a
    ///         fresh controller.
    [[nodiscard]] bool wandHarmonicsAware() const noexcept { return wandHarmonicsAware_; }

    /// @brief Whether the selection currently on display (the live Wand
    ///        preview while one is active, otherwise the committed
    ///        selection - the same "live-or-committed" split
    ///        displayBounds() itself follows) is Mask-shaped (Wand, or any
    ///        boolean-combined result) - `true` means displayBoundary()
    ///        can't return a curve for it (there may not be a single one),
    ///        so a caller (`CanvasWidget`, in particular) should indicate
    ///        the selection some other way (a dashed bounding box, in
    ///        `CanvasWidget`'s own case) rather than drawing nothing at all.
    /// @return `true` if the currently-displayed selection is Mask-kind.
    [[nodiscard]] bool hasMaskShapedSelection() const noexcept;

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
     * @brief Starts a new selection drag on `layer`, anchored at `point` -
     *        a rectangle's own first corner, or a Lasso's own first raw
     *        point, per setSelectionShape()'s own current value. Replaces
     *        (visually, until committed - see endSelectionDrag()'s own
     *        docs) whatever selection already existed.
     * @param layer Which layer this selection will scope operations on.
     * @param point The drag's own anchor/first point, already converted to
     *        time/frequency space.
     */
    void beginSelectionDrag(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Continues an in-progress selection drag, live-updating the
     *        rectangle between the original anchor and `point` (Rectangle),
     *        or appending `point` to the freehand curve being fit
     *        (Lasso - the same fitPathToPoints()-per-sample refit
     *        `PaintController::continueStroke()` already does). A no-op
     *        if no drag is in progress.
     *
     * Emits boundsChanged() so the caller can redraw the live shape.
     *
     * @param point The cursor's current position, in time/frequency
     *        space.
     */
    void continueSelectionDrag(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Ends an in-progress selection drag, committing its own shape
     *        as the current selection - unless the drag never really
     *        moved, or (Lasso only) never gathered enough points to
     *        enclose any area, in which case this clears the selection
     *        instead (the same "click empty space to deselect" convention
     *        `docs/sound-mind-design.md`'s "Pick" already established,
     *        applied here to "drew nothing meaningful" rather than
     *        "clicked nothing").
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

    /// @brief Whether the *committed* selection currently accepts a
    ///        rotate-handle drag - `true` exactly when it originated from
    ///        a Rectangle drag (a Lasso/Wand/boolean-combined selection
    ///        never does, regardless of what `selectionShape()` is
    ///        currently set to for the *next* drag).
    /// @return `true` if `beginRotateDrag()` would do anything.
    [[nodiscard]] bool canRotateSelection() const noexcept { return committedRotationRadians_.has_value(); }

    /**
     * @brief Starts dragging the committed selection's own rotate handle -
     *        `v0.Y.35.1` Installment C - a no-op unless `canRotateSelection()`.
     *
     * Modeled directly on `PickController`'s own `pick()`/`continueMove()`/
     * `endMove()` shape (the closest existing precedent for "drag a handle
     * on an already-committed selection", though Pick's own handle is the
     * whole object, not a dedicated point) - a *separate* method quartet
     * from `beginSelectionDrag()`'s own, since this transforms an
     * *existing* committed selection rather than drawing a new one.
     *
     * @param point Where the drag starts, in time/frequency space -
     *        typically wherever the rotate handle itself is, though any
     *        point works (only the drag's own angular *change* from this
     *        anchor matters, not its exact distance from the selection's
     *        own center).
     */
    void beginRotateDrag(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Continues an in-progress rotate drag, live-updating the
     *        committed selection's own rotation angle by however far
     *        `point` has swept around the selection's own center since
     *        `beginRotateDrag()`. A no-op if no rotate drag is in
     *        progress.
     *
     * Unlike `continueSelectionDrag()` (which only live-previews an
     * as-yet-uncommitted shape), this directly updates the *committed*
     * selection on every call - there's no separate "commit" step,
     * since a rotate drag only ever acts on a selection that's already
     * committed. Emits both `boundsChanged()` and `selectionChanged()`.
     *
     * @param point The cursor's current position, in time/frequency
     *        space.
     */
    void continueRotateDrag(sound_mind::core::TimeFrequencyPoint point);

    /// @brief Ends an in-progress rotate drag - the rotation applied by
    ///        continueRotateDrag() along the way is already fully
    ///        committed, so this only clears the drag-active bookkeeping.
    ///        A no-op if no rotate drag is in progress.
    void endRotateDrag();

    /// @brief Abandons an in-progress rotate drag, reverting the
    ///        committed selection's own rotation back to whatever it was
    ///        when `beginRotateDrag()` started. A no-op if no rotate drag
    ///        is in progress. Emits `boundsChanged()`/`selectionChanged()`
    ///        if the rotation actually reverted to something different.
    void cancelRotateDrag();

    /**
     * @brief The rotate handle's own current position, to draw and hit-
     *        test against - `docs/sound-mind-design.md`'s "Selection"
     *        ("Rectangle" - "resize and rotate handles"; only the rotate
     *        handle is built so far).
     *
     * A fixed proportion of the (unrotated) rectangle's own normalized
     * height above its own top edge, at its own horizontal midpoint,
     * carried along with the shape's own current rotation - a simple,
     * zoom-proportionate placement rather than a fixed on-screen pixel
     * offset, confirmed as reasonable by not needing `CanvasWidget` to
     * hand this class any screen-space geometry at all.
     *
     * @return The handle's own position, or `std::nullopt` if
     *         `canRotateSelection()` is `false` (no committed Rectangle-
     *         origin selection to rotate).
     */
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyPoint> displayRotationHandle() const;

    /// @brief Clears the current selection ("Deselect") - a no-op if
    ///        there isn't one. Emits boundsChanged() and
    ///        selectionChanged().
    void clearSelection();

    /// @brief Whether a selection currently exists (committed - not
    ///        mid-drag).
    /// @return `true` if a selection is committed; `false` otherwise.
    [[nodiscard]] bool hasSelection() const noexcept { return committedBounds_.has_value(); }

    /**
     * @brief What to actually draw as the selection overlay's own bounding
     *        box right now - the in-progress drag's own live rectangle (or
     *        the live Lasso curve's own bounding box) while a drag is
     *        active, otherwise the committed selection's. Always present
     *        for a Lasso selection too - see this class's own docs on why
     *        a bounding box is tracked regardless of shape.
     * @return The bounds to display, or `std::nullopt` if there's neither
     *         a drag in progress nor a committed selection, or (Lasso
     *         only) the drag hasn't gathered enough points yet to have a
     *         meaningful bounding box.
     */
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyRect> displayBounds() const;

    /**
     * @brief The Lasso curve to actually draw as the selection overlay
     *        right now, if the current selection is Lasso-*shaped* - the
     *        in-progress drag's own live-fit curve while one is active,
     *        otherwise the committed selection's own boundary (only when
     *        it's `Path`-kind). A caller draws this curve *instead of* a
     *        plain rectangle outline whenever it's present (`CanvasWidget`'s
     *        own convention).
     * @return The curve to display, or `std::nullopt` for a Rectangle
     *         selection, a Mask-shaped one (Wand or boolean-combined -
     *         see `hasMaskShapedSelection()`), no selection at all, or a
     *         Lasso drag that hasn't gathered enough points yet to have a
     *         meaningful curve.
     */
    [[nodiscard]] std::optional<sound_mind::core::Path> displayBoundary() const;

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

    /**
     * @brief Warps the current committed selection's own bounding box
     *        along `curve` - the actual work behind Edit → Warp Selection
     *        (`docs/sound-mind-design.md`'s "Selection" ("Warp"),
     *        `v0.Y.35.1` Installment C). A no-op if there's no committed
     *        selection.
     *
     * Appends a new `WarpOperation` (not superseding anything, the same
     * "a fresh, additive edit" reasoning `fill()`'s own docs give) and
     * rebuilds the target layer's content via `paintController_`. Always
     * scoped to `committedBounds_` - the selection's own plain bounding
     * box - regardless of its actual shape (Rectangle, rotated Rectangle,
     * Lasso, Wand, or a boolean-combined result); see
     * `sound_mind::core::WarpOperation`'s own docs for why.
     *
     * Emits contentChanged() for the affected layer.
     *
     * @param curve The warp curve - typically `PickController::
     *        selectedPath()`'s own result (see `MainWindow::
     *        warpSelection()`'s own docs for the full workflow).
     * @param axis Which direction content is displaced.
     * @param mode How far along each column/row the deflection carries.
     */
    void warpSelection(sound_mind::core::Path curve, sound_mind::core::WarpAxis axis, sound_mind::core::WarpMode mode);

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
     * @param blendMode How the pasted clip combines with what's already at
     *        the target - `v0.Y.37.1` (Deferred Blend Modes), forwarded
     *        straight to the new `PasteOperation`'s own constructor.
     *        `sound_mind::core::BlendMode::Overwrite` (the default)
     *        reproduces this method's own pre-`v0.Y.37.1` hard-overwrite
     *        behavior exactly.
     * @return The newly appended `PasteOperation`'s own id - so a caller
     *         (`MainWindow`, in particular - see its own paste()) can
     *         hand it straight to `PickController::selectOperation()`,
     *         making the pasted result immediately Pickable without a
     *         separate click to find it again. `std::nullopt` if this
     *         was a no-op (no clipboard).
     */
    std::optional<sound_mind::core::OperationId> pasteInto(
        sound_mind::core::LayerId targetLayer,
        sound_mind::core::BlendMode blendMode = sound_mind::core::BlendMode::Overwrite);

    /**
     * @brief Captures the current committed selection's own pixels off
     *        `selectionLayer_` into a new, permanently-stored, named entry
     *        in the project's Mind Shot library - the actual work behind
     *        "Capture as Mind Shot" (`docs/sound-mind-design.md`'s "Mind
     *        Shots"). A no-op if there's no committed selection.
     *
     * Shares `copySelection()`'s own `captureClip()`-from-selection
     * plumbing (see its own docs) - capturing a Mind Shot is
     * architecturally the same operation as Copy, just stored permanently
     * and named in `Project::mindShots()` instead of held anonymously,
     * transiently, on this controller's own clipboard. Does not touch the
     * clipboard itself, or `selectionLayer_`'s own content - unlike Cut,
     * a capture never clears its own source pixels.
     *
     * Emits mindShotCaptured() with the new entry's own id, so a listener
     * (the Tool Configuration Panel's own Mind Shot picker, in particular)
     * can refresh itself.
     *
     * @param name Display name for the new library entry.
     * @return The new entry's own id, or `std::nullopt` if this was a
     *         no-op (no committed selection).
     */
    std::optional<sound_mind::core::MindShotId> captureMindShot(const std::string& name);

    /**
     * @brief Captures the current committed selection's own `selectionLayer_`
     *        and bounds - not any pixel data - into a new, permanently-
     *        stored, named entry in the project's Mind Grain library - the
     *        actual work behind "Capture as Mind Grain"
     *        (`docs/sound-mind-design.md`'s "Mind Grains"). A no-op if
     *        there's no committed selection.
     *
     * The deliberate opposite of captureMindShot(): a Mind Grain never
     * touches `captureClip()`/the layer's own content at all - only the
     * reference `{selectionLayer_, *committedBounds_}` is stored, since a
     * Mind Grain's whole point is to re-read its source *live*, at whatever
     * content it holds when each stroke painted with it is (re)applied -
     * see `sound_mind::core::LayerContentResolver`'s own docs. Does not
     * touch the clipboard, or `selectionLayer_`'s own content.
     *
     * Emits mindGrainCaptured() with the new entry's own id, so a listener
     * (the Tool Configuration Panel's own Mind Grain picker, in particular)
     * can refresh itself.
     *
     * @param name Display name for the new library entry.
     * @return The new entry's own id, or `std::nullopt` if this was a
     *         no-op (no committed selection).
     */
    std::optional<sound_mind::core::MindGrainId> captureMindGrain(const std::string& name);

signals:
    /// @brief Emitted whenever displayBounds()/displayBoundary() would
    ///        return something different - a drag updating live, a
    ///        selection committed, a drag cancelled back to the prior
    ///        selection, or a clear.
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

    /// @brief Emitted whenever captureMindShot() actually adds a new entry
    ///        to the project's Mind Shot library.
    /// @param id The new entry's own id.
    void mindShotCaptured(sound_mind::core::MindShotId id);

    /// @brief Emitted whenever captureMindGrain() actually adds a new entry
    ///        to the project's Mind Grain library.
    /// @param id The new entry's own id.
    void mindGrainCaptured(sound_mind::core::MindGrainId id);

private:
    /**
     * @brief Combines `newBounds`/`newBoundary` (a just-finished drag's own
     *        result) into the already-committed selection per
     *        `currentCombineMode_`, replacing `committedBounds_`/
     *        `committedBoundary_` with the combined result - the shared
     *        tail end of `endSelectionDrag()`'s own Add/Subtract/Intersect
     *        handling. A no-op (existing selection left exactly as it was)
     *        if `selectionLayer_` has no real content to rasterize
     *        against.
     * @param newBounds The new selection's own bounding box.
     * @param newBoundary The new selection's own precise shape, or
     *        `std::nullopt` for a plain rectangle over `newBounds`.
     */
    void combineIntoCommittedSelection(const sound_mind::core::TimeFrequencyRect& newBounds,
                                        const std::optional<sound_mind::core::SelectionRegion>& newBoundary);

    /// @brief Applies committedRotationRadians_ to committedUnrotatedRect_,
    /// updating committedBounds_/committedBoundary_ to match - shared by
    /// continueRotateDrag() and cancelRotateDrag()'s own revert. Sets
    /// committedBoundary_ back to std::nullopt (a plain rectangle) rather
    /// than a degenerate zero-rotation Path when the angle is close enough
    /// to zero, the same "nullopt is the well-understood default" economy
    /// every other shape already follows.
    void refreshRotatedSelection();

    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;

    /// @brief Which shape beginSelectionDrag() produces next - see
    /// setSelectionShape()'s own docs.
    SelectionShape currentShape_ = SelectionShape::Rectangle;
    /// @brief How the next endSelectionDrag() combines its own result with
    /// what's already committed - see setSelectionCombineMode()'s own docs.
    SelectionCombineMode currentCombineMode_ = SelectionCombineMode::Replace;
    /// @brief Wand's own tolerance/harmonics-aware settings - see
    /// setWandTolerance()'s/setWandHarmonicsAware()'s own docs.
    double wandTolerancePercent_ = 10.0;
    bool wandHarmonicsAware_ = false;

    std::optional<sound_mind::core::TimeFrequencyRect> committedBounds_;
    /// @brief The committed selection's own precise shape, if it's not a
    /// plain Rectangle - `std::nullopt` for one. Always kept consistent
    /// with committedBounds_ (which is always this shape's own bounding
    /// box, when present) - see this class's own docs.
    std::optional<sound_mind::core::SelectionRegion> committedBoundary_;
    sound_mind::core::LayerId selectionLayer_ = 0;

    /// @brief The committed selection's own current rotation, in radians -
    /// present (starting at 0.0) exactly when the committed selection
    /// came from a Rectangle drag (rotatable); std::nullopt for Lasso/
    /// Wand/boolean-combined selections - see canRotateSelection()'s own
    /// docs.
    std::optional<double> committedRotationRadians_;
    /// @brief The committed selection's own *original*, never-rotated
    /// bounding rectangle - rotatedRectangle() is always computed fresh
    /// from this (never compounded from an already-rotated shape, which
    /// would accumulate floating-point drift over repeated small drags).
    /// Only meaningful alongside committedRotationRadians_.
    sound_mind::core::TimeFrequencyRect committedUnrotatedRect_;

    bool rotateDragActive_ = false;
    /// @brief The angle (frequencyToTimeScale-normalized radians, from
    /// committedUnrotatedRect_'s own center) beginRotateDrag()'s own
    /// anchor point sat at - continueRotateDrag() compares each new
    /// point's own angle against this to derive how far the drag has
    /// swept.
    double rotateDragStartAngleRadians_ = 0.0;
    /// @brief committedRotationRadians_'s own value when the rotate drag
    /// started - continueRotateDrag() adds the drag's own angular sweep
    /// to this, and cancelRotateDrag() reverts straight back to it.
    double rotateDragStartRotation_ = 0.0;

    std::optional<sound_mind::core::Clip> clipboard_;
    std::optional<sound_mind::core::TimeFrequencyRect> clipboardBounds_;
    /// @brief committedBoundary_'s own value at the moment copySelection()/
    /// cutSelection() last ran - carried alongside clipboard_/
    /// clipboardBounds_ so pasteInto() can narrow the paste the same way
    /// the original selection was shaped.
    std::optional<sound_mind::core::SelectionRegion> clipboardBoundary_;

    bool dragActive_ = false;
    bool dragMoved_ = false;
    sound_mind::core::TimeFrequencyPoint dragAnchor_;
    sound_mind::core::TimeFrequencyRect dragPreviewBounds_;

    /// @brief A Lasso drag's own raw, unfitted sample points, gathered by
    /// continueSelectionDrag() exactly the way PaintController's own
    /// strokePoints_ are - only meaningful while dragActive_ and
    /// currentShape_ == SelectionShape::Lasso.
    std::vector<sound_mind::core::TimeFrequencyPoint> lassoRawPoints_;
    /// @brief The live, curve-fit preview of lassoRawPoints_ - refit on
    /// every continueSelectionDrag() call, the same "cheap enough to refit
    /// every sample" precedent PaintController::continueStroke() already
    /// established. Empty (no nodes) until at least 2 raw points exist.
    sound_mind::core::Path lassoPreviewPath_;

    /// @brief A Wand click's own flood-fill result, computed once at
    /// beginSelectionDrag() (see this class's own docs on why Wand ignores
    /// any subsequent drag movement) and held here until endSelectionDrag()
    /// commits it - `std::nullopt` if the anchor missed the layer's own
    /// content entirely. pendingWandBounds_ is that same result's own
    /// bounding box, computed alongside it (once), so displayBounds()/
    /// endSelectionDrag() never need to re-derive it from the mask later.
    std::optional<sound_mind::core::SelectionRegion> pendingWandRegion_;
    std::optional<sound_mind::core::TimeFrequencyRect> pendingWandBounds_;

    /// @brief Snap to Grid's own current state - see setGridSnapping()'s
    /// own docs.
    bool gridSnappingEnabled_ = false;
    FrequencyGridConfig frequencyGridConfig_;
    TimingGridConfig timingGridConfig_;
};

}  // namespace sound_mind::studio
