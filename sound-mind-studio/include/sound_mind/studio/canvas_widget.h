#pragma once

#include <optional>
#include <vector>

#include <QImage>
#include <QPainterPath>
#include <QRectF>
#include <QWidget>

#include "sound_mind/core/mind_wave.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/studio/axis_labels.h"
#include "sound_mind/studio/grid_config.h"

class QEvent;
class QMouseEvent;
class QPainter;
class QWheelEvent;

namespace sound_mind::studio {

/**
 * @brief Renders the active project's canvas.
 *
 * **As of `v0.Y.27.1` (Multi-layer Compositing):** shows the project's own
 * real multi-layer composite (see `sound_mind::core::compositeProject()`),
 * scaled to fill the widget - every visible layer's own content mixed
 * together, not just whichever layer happens to be on top. Falls back to
 * a placeholder rectangle, sized to the project's configured canvas
 * dimensions, when no layer has any content yet (e.g. a fresh project
 * with only its empty Background layer). MindWave-bound parameters still
 * don't exist yet (Phase 4) - once they do, they'll bind into this same
 * composite rather than needing a new render path.
 *
 * **As of `v0.0.21.1` (Playback position bar):** also draws a moving
 * playhead line during Playback - see setPlayheadFraction()'s own docs.
 *
 * **As of `v0.Y.24.1` (Basic Painting):** also handles freehand-painting
 * mouse input while toolMode() is `Paint` - see setToolMode()'s own docs.
 * Purely presentational, the same shape as every dock panel: it only ever
 * emits already-converted `TimeFrequencyPoint`s (see
 * `docs/sound-mind-design.md`'s "Freehand Path Capture") - turning those
 * into a real, undoable stroke is `PaintController`'s job, wired up by
 * `MainWindow`, not this widget's own. Also emits cursorMoved()/
 * cursorLeft() on every mouse move/exit, regardless of toolMode() - see
 * their own docs.
 *
 * **As of Canvas Navigation's own Zoom feature:** also owns the current
 * zoom level - see `ZoomMode`/`zoomIn()` and friends. Kept here (not in
 * `MainWindow`) since zoom directly determines this widget's own on-screen
 * size, and every domain/pixel conversion above already lives here too.
 * `MainWindow` only owns the `QScrollArea` that clips and scrolls this
 * widget once it's larger than the visible viewport, toggling that scroll
 * area's own `setWidgetResizable()` in response to zoomModeChanged() (see
 * that signal's own docs for why this widget doesn't just reach into its
 * own parent scroll area directly).
 */
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Which kind of mouse interaction the canvas is currently
    ///        configured for - see setToolMode()'s own docs.
    enum class ToolMode {
        None,  ///< Mouse input does nothing - the default.
        Paint,  ///< Mouse drags/taps paint a freehand stroke.
        Pick,  ///< Mouse press selects a paint object; a drag moves it.
        Select,  ///< Mouse drag draws a rectangular selection.
        Path,  ///< Each mouse press places one more Path node.
        ChordStamp,  ///< Each mouse press stamps the Chord Generator's own current chord/arpeggio.
    };

    /// @brief Whether the canvas's own on-screen size tracks its
    ///        enclosing viewport automatically, or holds a fixed,
    ///        explicitly-chosen zoom level - see zoomToFit()'s/zoomIn()'s
    ///        own docs.
    enum class ZoomMode {
        FitToWindow,  ///< Always exactly fills the visible viewport - the default.
        Manual,  ///< A fixed size, set by a zoom action; scrolls once larger than the viewport.
    };

    /// @brief Constructs an empty canvas, with no project set yet.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit CanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief Sets which project's canvas dimensions this widget reflects.
     * @param project The project to reflect, or `nullptr` to show nothing
     *        (no project open). Not owned - the caller must keep it alive
     *        for as long as it's set here, and clear or replace it before
     *        it's destroyed.
     */
    void setProject(const sound_mind::core::Project* project);

    /**
     * @brief Sets (or clears) the playhead line's horizontal position and
     *        repaints - a vertical white line drawn over whatever the
     *        canvas otherwise shows, matching
     *        `sound_mind::codec::exportVideo()`'s own playhead exactly, so
     *        live playback looks the same as the video it would export to.
     *
     * Independent of setProject()/layer state - drawn over the placeholder
     * or a "no project" blank canvas too, not just real rendered content.
     *
     * @param fraction The playhead's position as a fraction of the
     *        widget's own width, `[0, 1]`; `std::nullopt` draws no
     *        playhead at all (the default).
     */
    void setPlayheadFraction(std::optional<double> fraction);

    /**
     * @brief Sets which kind of mouse interaction the canvas currently
     *        accepts.
     *
     * `None` leaves ordinary mouse events unhandled (the pre-`v0.Y.24.1`
     * behavior, and still the default) - `Paint` makes a left-button
     * press/drag/release emit paintStrokeStarted()/paintStrokeContinued()/
     * paintStrokeEnded() instead, `Pick` makes the same gesture emit
     * pickStrokeStarted()/pickStrokeContinued()/pickStrokeEnded() instead,
     * `Select` makes it emit selectStrokeStarted()/selectStrokeContinued()/
     * selectStrokeEnded() instead, `Path` makes each individual left-
     * button press (no drag/release counterpart - see pathNodePlaced()'s
     * own docs) emit pathNodePlaced() instead, and `ChordStamp` makes each
     * individual left-button press (the same single-press-is-the-whole-
     * gesture shape as `Path`, not a drag) emit chordStampRequested()
     * instead. Exactly one mode is active at a time. Switching away from
     * `Paint`/`Pick`/`Select` cancels whatever gesture was mid-flight in it
     * (a real mouse-up may never arrive - e.g. the toolbar button was
     * clicked instead); `Path`/`ChordStamp` have no such in-widget state to
     * cancel (see pathNodePlaced()'s own docs) - `Path`'s own in-progress
     * node sequence lives in `PathController` instead, cancelled from
     * there, and `ChordStamp`'s own single-press gesture has no
     * in-progress state at all to begin with.
     *
     * Also repaints - `ChordStamp` specifically gates the Chord Overlay
     * (see `setChordGeneratorPanelVisible()`'s own docs), so switching
     * into or out of it needs to show/hide that overlay immediately,
     * not just on whatever unrelated repaint happens to come next.
     *
     * @param mode The new tool mode.
     */
    void setToolMode(ToolMode mode);

    /// @brief The canvas's current mouse-interaction mode.
    /// @return The mode set via setToolMode(); `None` by default.
    [[nodiscard]] ToolMode toolMode() const noexcept { return toolMode_; }

    /**
     * @brief Sets whether every paintable operation's bounding box is
     *        drawn over the canvas - see `docs/sound-mind-design.md`'s
     *        "Tool Configuration" ("Show bounding boxes").
     *
     * Drawn for the same layer the canvas otherwise renders (its topmost
     * visible layer with content) - every one of that layer's own active
     * `PaintOperation`s, via `Operation::bounds()`.
     *
     * @param shown Whether to draw them; `false` by default.
     */
    void setShowBoundingBoxes(bool shown);

    /**
     * @brief Sets whether every paintable operation's own Path geometry
     *        (nodes and handles) is drawn over the canvas - see
     *        `docs/sound-mind-design.md`'s "Tool Configuration" ("Show
     *        path geometry").
     *
     * Independent of the in-progress live preview (setPaintPreviewPath()),
     * which is always drawn regardless of this setting - see that
     * method's own docs for why.
     *
     * @param shown Whether to draw them; `false` by default.
     */
    void setShowPathGeometry(bool shown);

    /**
     * @brief Sets the live paint-stroke preview to draw, and repaints.
     *
     * Drawn as an overlay - its nodes converted from time/frequency space
     * back into widget pixels the same way a mouse click converts the
     * other direction - regardless of any future "Show path geometry"
     * setting's own state (see `docs/sound-mind-design.md`'s "Tool
     * Configuration"): a stroke actively being drawn always shows its own
     * geometry live.
     *
     * @param path The preview path to draw; an empty (no-node) Path draws
     *        nothing, clearing any previous preview.
     */
    void setPaintPreviewPath(sound_mind::core::Path path);

    /**
     * @brief Sets (or clears) which node of the current live preview path
     *        is highlighted as "selected" - during Pick's own node/handle
     *        editing (`PickController::beginPathEdit()`), the node
     *        `PickController::selectedPathNodeIndex()` currently reports.
     *
     * Every node in the current preview is always drawn as a small dot
     * (see setPaintPreviewPath()'s own docs); the *selected* one is drawn
     * larger and in a distinct color, and - if it's a `Smooth` node -
     * with its own two handles (and the thin lines connecting them to
     * the anchor) also drawn, in yet another distinct color. Handles are
     * deliberately only ever shown for the selected node, not every node
     * at once, to keep an in-progress edit legible rather than cluttering
     * the whole path with every handle simultaneously.
     *
     * @param index The node to highlight, by its own index into the
     *        current preview path's `nodes()`; `std::nullopt` highlights
     *        none (still drawing every node as a plain dot).
     */
    void setPreviewSelectedNodeIndex(std::optional<std::size_t> index);

    /**
     * @brief Sets (or clears) the Picked object's own selection highlight
     *        and repaints - a distinct-colored (white) rectangle, drawn
     *        over whatever the canvas otherwise shows.
     *
     * Drawn when set, regardless of setShowBoundingBoxes()'s own state -
     * the same "what you're actively interacting with is always visible"
     * precedent setPaintPreviewPath() already established, since there'd
     * otherwise be no visual indication at all of *which* object is
     * selected while Show bounding boxes is off - *except* while
     * setPaintPreviewPath() itself currently has a live preview showing
     * (a whole-object move or an active path edit session in progress):
     * this rectangle only ever updates when the picked object's own
     * selection genuinely changes, not on every drag step, so it would
     * otherwise sit frozen at the pre-drag position for the entire
     * gesture - visibly wrong once the live preview itself has moved on.
     *
     * @param bounds The selected object's own current bounds, per
     *        `sound_mind::core::Operation::bounds()`; `std::nullopt`
     *        draws nothing, clearing any previous highlight.
     */
    void setPickSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds);

    /**
     * @brief Sets (or clears) the current rectangular selection's own
     *        overlay and repaints - a distinct-colored (green) rectangle,
     *        drawn over whatever the canvas otherwise shows, independent
     *        of `toolMode()`: a selection scopes other operations (Fill,
     *        in particular - see `docs/sound-mind-design.md`'s
     *        "Selection") and so stays visible/usable even after
     *        switching to a different tool, the same way it would in any
     *        other image editor.
     *
     * @param bounds The selection to display - either a `SelectionController`'s
     *        own in-progress drag preview or its committed selection (see
     *        `SelectionController::displayBounds()`'s own docs for which);
     *        `std::nullopt` draws nothing, clearing any previous overlay.
     */
    void setSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds);

    /**
     * @brief Sets (or clears) a Lasso selection's own curve overlay and
     *        repaints, drawn *instead of* the plain rectangle
     *        `setSelectionBounds()` would otherwise draw - see
     *        `docs/sound-mind-design.md`'s "Lasso" and
     *        `SelectionController::displayBoundary()`'s own docs.
     *
     * Drawn closed (a straight line back to its own first node, even
     * though the underlying `Path` never stores that closing segment
     * itself) - the same implicit closure
     * `sound_mind::core::containsPoint()` already treats every boundary
     * as having.
     *
     * @param boundary The curve to display - either a `SelectionController`'s
     *        own in-progress Lasso drag preview or its committed
     *        boundary (see `SelectionController::displayBoundary()`'s own
     *        docs for which); `std::nullopt` for a Rectangle selection, no
     *        selection at all, or an in-progress Lasso drag too short to
     *        have a meaningful curve yet - `setSelectionBounds()`'s own
     *        rectangle (if any) draws instead.
     */
    void setSelectionBoundary(std::optional<sound_mind::core::Path> boundary);

    /**
     * @brief Sets whether the current selection is Mask-shaped (a Wand
     *        selection, or any boolean-combined result - `v0.Y.35.1`
     *        Installment B) and repaints - draws `setSelectionBounds()`'s
     *        own rectangle dashed instead of solid when `true`, since a
     *        mask generally has no single curve `setSelectionBoundary()`
     *        could draw instead (see
     *        `SelectionController::hasMaskShapedSelection()`'s own docs) -
     *        a plain bounding-box indicator rather than nothing at all.
     * @param hasMaskShape Whether to draw the dashed indicator.
     */
    void setSelectionHasMaskShape(bool hasMaskShape);

    /**
     * @brief Sets (or clears) the current Rectangle selection's own
     *        rotate handle overlay and repaints - a small circle drawn
     *        over whatever the canvas otherwise shows, `v0.Y.35.1`
     *        Installment C. Also the handle's own hit-test target - a
     *        press within its own hit radius starts a rotate drag
     *        (`selectionRotateStarted()`) instead of a new selection.
     * @param handlePosition The handle's own position - either
     *        `SelectionController::displayRotationHandle()`'s own live
     *        or committed value; `std::nullopt` for a non-rotatable
     *        selection (Lasso, Wand, a combined result, or none at all)
     *        draws (and hit-tests) nothing.
     */
    void setSelectionRotationHandle(std::optional<sound_mind::core::TimeFrequencyPoint> handlePosition);

    /**
     * @brief Sets (or clears) the MindWave being live-previewed, and
     *        repaints - a semi-transparent (50% opacity) grayscale
     *        rendering of `wave`'s own `[0, 1]` field
     *        (`sound_mind::core::evaluateMindWaveField()`), laid over
     *        whatever the canvas otherwise shows, per
     *        `docs/sound-mind-design.md`'s "Low Frequency Oscillations" -
     *        `MindWaveController`'s own answer to the MindWaves panel's
     *        Preview toggle.
     *
     * The field is evaluated once here, not on every paintEvent() - a
     * plain cached `QImage`, scaled to fill the widget the same way the
     * main composite already is (`drawImage(rect(), ...)`), gets redrawn
     * as many times as needed for free. A no-op (clears any existing
     * preview instead) if no project is set - there's no canvas geometry
     * to evaluate `wave` against yet.
     *
     * @param wave The MindWave to preview, by value (no lifetime tie to
     *        wherever the caller's own copy lives - see
     *        `MindWaveController`'s own docs on why); `std::nullopt`
     *        clears the preview.
     */
    void setMindWavePreview(std::optional<sound_mind::core::MindWave> wave);

    /**
     * @brief Sets (or clears) the Chord Overlay - the Chord Generator's own
     *        currently-configured chord/arpeggio's note pitches, drawn live
     *        on the frequency axis, and repaints - see
     *        `docs/sound-mind-design.md`'s "Chord Overlay" ("its notes are
     *        drawn live on the frequency axis... so the notes are visible
     *        on the canvas before or while they're stamped").
     *
     * Drawn as horizontal reference lines, the same visual language
     * `drawGrid()`'s own Frequency Grid lines use (see
     * `setFrequencyGridConfig()`'s own docs) but in a distinct color and
     * kept **independent of it** - a deliberately separate overlay, not a
     * temporary addition to `FrequencyGridConfig`'s own state, per the
     * design doc's own "independent of, and in addition to, the general
     * frequency grid" wording: toggling or reconfiguring the user's actual
     * Frequency Grid never affects this, and vice versa.
     *
     * @param frequenciesHz Each note pitch to draw a line at, in Hz -
     *        ordinarily `ChordGeneratorController::previewNotes()`'s own
     *        distinct `frequencyHz` values; an empty list draws nothing,
     *        clearing any previous overlay.
     */
    void setChordPreview(std::vector<double> frequenciesHz);

    /**
     * @brief Sets whether the Chord Generator panel is currently visible -
     *        real-world testing pass, 2026-09-20, finding #15 ("stamped
     *        chords [the Chord Overlay] should only be visible... while
     *        the Chord Generator panel is open, or the Chord tool is the
     *        active tool - not unconditionally").
     *
     * A separate flag from `setChordPreview()`'s own data, the same
     * "what to show" vs. "whether to show it right now" split
     * `setShowBoundingBoxes()`/`showBoundingBoxes_` already establish for
     * the bounding-box overlay - so toggling the panel's own visibility
     * back and forth never loses or needs to recompute the actual preview
     * data. The overlay itself only ever draws while this is `true` *or*
     * `toolMode() == ToolMode::ChordStamp` (see `setToolMode()`'s own
     * docs) - either one alone is enough.
     *
     * @param visible The panel's own current visibility.
     */
    void setChordGeneratorPanelVisible(bool visible);

    /**
     * @brief Sets what the frequency (vertical) axis's own labels show,
     *        and repaints - see `docs/sound-mind-design.md`'s "Axis
     *        Labels".
     *
     * Drawn along the canvas's own left edge, independent of every other
     * overlay this widget draws - a pure display aid, like every Overlay
     * Grid line, with no effect on encoding, decoding, or any stored
     * pixel data.
     *
     * @param mode Which labeling scheme to draw; `Off` (the default)
     *        draws nothing.
     */
    void setVerticalAxisLabelMode(VerticalAxisLabelMode mode);

    /**
     * @brief Sets what the time (horizontal) axis's own labels show, and
     *        repaints - see `docs/sound-mind-design.md`'s "Axis Labels".
     *
     * Drawn along the canvas's own bottom edge - see
     * setVerticalAxisLabelMode()'s own docs for the rest.
     *
     * @param mode Which labeling scheme to draw; `Off` (the default)
     *        draws nothing.
     */
    void setHorizontalAxisLabelMode(HorizontalAxisLabelMode mode);

    /**
     * @brief Sets the Frequency Grid's own current configuration, and
     *        repaints - see `docs/sound-mind-design.md`'s "Overlay
     *        Grids" > "Frequency Grid".
     *
     * Drawn as horizontal reference lines (this canvas's own time-
     * horizontal/frequency-vertical convention - see
     * `docs/sound-mind-architecture.md`'s Decision #56), independent of
     * every other overlay this widget draws - a pure display aid, never
     * affecting encoding, decoding, or any stored pixel data.
     *
     * @param config The configuration to draw; a default-constructed one
     *        (no source enabled) draws nothing.
     */
    void setFrequencyGridConfig(const FrequencyGridConfig& config);

    /**
     * @brief Sets the Timing Grid's own current configuration, and
     *        repaints - see `docs/sound-mind-design.md`'s "Overlay
     *        Grids" > "Timing Grid".
     *
     * Drawn as vertical reference lines - see setFrequencyGridConfig()'s
     * own docs for the rest.
     *
     * @param config The configuration to draw; `TimingGridMode::Off`
     *        (the default) draws nothing.
     */
    void setTimingGridConfig(const TimingGridConfig& config);

    /// @brief The widget's preferred size.
    /// @return The current project's configured canvas dimensions, or a
    ///         fallback size if no project is set.
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief Whether the canvas is auto-fitting the viewport or holding a
    ///        fixed zoom level.
    /// @return The current mode; `FitToWindow` by default.
    [[nodiscard]] ZoomMode zoomMode() const noexcept { return zoomMode_; }

    /**
     * @brief Switches to `ZoomMode::FitToWindow` - the canvas always
     *        exactly fills its enclosing viewport from here on (until the
     *        next zoom-in/out/actual-size call switches back to `Manual`).
     *
     * Emits zoomModeChanged() if the mode actually changed, so
     * `MainWindow` can put its own enclosing `QScrollArea` back into
     * `setWidgetResizable(true)` mode - that's what actually makes Qt
     * resize this widget to the viewport; this method itself only ever
     * *repaints* (the resize follows from the scroll area's own reaction,
     * not from anything this method does directly).
     */
    void zoomToFit();

    /// @brief Switches to `ZoomMode::Manual` at exactly 100% - one screen
    ///        pixel per encoded pixel (one column, one bin) - regardless
    ///        of the current zoom level or mode.
    void zoomToActualSize();

    /// @brief Zooms in proportionally (time and frequency scale together)
    ///        by the normal step - `]` with no modifier. Exits
    ///        `FitToWindow` if that's the current mode, using whatever
    ///        it's currently displaying as the new zoom level's own
    ///        starting point, so the zoomed-in result looks like a
    ///        continuation of what was already on screen rather than an
    ///        unrelated jump.
    void zoomIn();

    /// @brief The inverse of zoomIn() - `[` with no modifier.
    void zoomOut();

    /// @brief Zooms in the **time axis only** (frequency-invariant) by
    ///        the normal step - `Shift+]`. See zoomIn()'s own docs on
    ///        exiting `FitToWindow`.
    void zoomInTimeOnly();

    /// @brief The inverse of zoomInTimeOnly() - `Shift+[`.
    void zoomOutTimeOnly();

    /// @brief Zooms in the **frequency axis only** (time-invariant) by
    ///        the normal step - `Alt+]`. See zoomIn()'s own docs on
    ///        exiting `FitToWindow`.
    void zoomInFrequencyOnly();

    /// @brief The inverse of zoomInFrequencyOnly() - `Alt+[`.
    void zoomOutFrequencyOnly();

    /// @brief Zooms in proportionally by a coarser step than zoomIn()'s
    ///        own - `Ctrl+]`. Proportional is already `]`'s own default,
    ///        so Ctrl's usual "proportional" meaning would be redundant
    ///        here - this is the one zoom control where the modifier
    ///        mnemonic bends, toward a bigger step instead (confirmed
    ///        with the user - see `docs/sound-mind-design.md`'s "Canvas
    ///        Navigation" > "Zoom").
    void zoomInCoarse();

    /// @brief The inverse of zoomInCoarse() - `Ctrl+[`.
    void zoomOutCoarse();

signals:
    /// @brief zoomMode() actually changed - either a fresh zoom-in/out/
    ///        actual-size call left `FitToWindow` for `Manual`, or
    ///        zoomToFit() returned to `FitToWindow` from `Manual`.
    ///        `MainWindow` listens for this to toggle its own enclosing
    ///        `QScrollArea::setWidgetResizable()` in lockstep - `true`
    ///        (Qt keeps this widget exactly matching the viewport) for
    ///        `FitToWindow`, `false` (Qt leaves this widget's own explicit
    ///        size alone, scrolling instead of stretching it) for
    ///        `Manual`. Not emitted for a zoom action that changes the
    ///        zoom *level* without changing which of these two modes is
    ///        active (e.g. two zoomIn() calls in a row, both in `Manual`).
    /// @param mode The mode now in effect.
    void zoomModeChanged(sound_mind::studio::CanvasWidget::ZoomMode mode);
    /// @brief A paint stroke started (`Paint` tool mode, left button
    ///        pressed).
    /// @param point The press position, converted to time/frequency space.
    void paintStrokeStarted(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress paint stroke continued (`Paint` tool mode,
    ///        left button held and moved).
    /// @param point The new position, converted to time/frequency space.
    void paintStrokeContinued(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress paint stroke ended (`Paint` tool mode, left
    ///        button released).
    void paintStrokeEnded();

    /// @brief A pick gesture started (`Pick` tool mode, left button
    ///        pressed) - also the anchor point for a potential drag, if
    ///        the press continues into one.
    /// @param point The press position, converted to time/frequency space.
    void pickStrokeStarted(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress pick gesture continued (`Pick` tool mode,
    ///        left button held and moved) - a drag, moving whatever was
    ///        selected at pickStrokeStarted().
    /// @param point The new position, converted to time/frequency space.
    void pickStrokeContinued(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress pick gesture ended (`Pick` tool mode, left
    ///        button released).
    void pickStrokeEnded();

    /// @brief A selection drag started (`Select` tool mode, left button
    ///        pressed) - the drag's own anchor corner.
    /// @param point The press position, converted to time/frequency space.
    /// @param modifiers The keyboard modifiers held at press time - Shift/
    ///        Alt/Shift+Alt choose Add/Subtract/Intersect combination
    ///        instead of the default Replace (`v0.Y.35.1` Installment B);
    ///        resolved by the caller (`MainWindow`), not by this class.
    void selectStrokeStarted(sound_mind::core::TimeFrequencyPoint point, Qt::KeyboardModifiers modifiers);

    /// @brief The in-progress selection drag continued (`Select` tool
    ///        mode, left button held and moved).
    /// @param point The new position, converted to time/frequency space.
    void selectStrokeContinued(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress selection drag ended (`Select` tool mode,
    ///        left button released).
    void selectStrokeEnded();

    /// @brief A rotate-handle drag started (`Select` tool mode, left
    ///        button pressed within the handle's own hit-test radius -
    ///        `docs/sound-mind-design.md`'s "Selection" ("Rectangle"),
    ///        `v0.Y.35.1` Installment C). Takes priority over starting a
    ///        new `selectStrokeStarted()` gesture whenever the press
    ///        lands on the handle.
    /// @param point The press position, converted to time/frequency space.
    void selectionRotateStarted(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress rotate-handle drag continued (`Select` tool
    ///        mode, left button held and moved).
    /// @param point The new position, converted to time/frequency space.
    void selectionRotateContinued(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress rotate-handle drag ended (`Select` tool
    ///        mode, left button released).
    void selectionRotateEnded();

    /**
     * @brief A new Path node was placed (`Path` tool mode, left button
     *        pressed).
     *
     * Unlike Paint/Pick/Select's own Started/Continued/Ended triples, a
     * single press is the *whole* gesture - dragging before release, or
     * the eventual release itself, changes nothing (`mouseMoveEvent()`/
     * `mouseReleaseEvent()` do nothing extra for `Path` mode beyond the
     * unconditional cursorMoved() every mode already gets). Placing many
     * nodes across many separate presses, and deciding when the whole
     * path is finished or cancelled, is `PathController`'s own job, not
     * this widget's - the same "purely presentational, already-converted
     * points only" split every other tool mode already follows.
     *
     * @param point The press position, converted to time/frequency space.
     */
    void pathNodePlaced(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief A chord/arpeggio stamp was requested (`ChordStamp` tool mode,
     *        left button pressed) - the same single-press-is-the-whole-
     *        gesture shape as pathNodePlaced()'s own docs describe, for the
     *        same reason (no drag/release counterpart).
     *
     * `point`'s own `frequencyHz` is deliberately **not** used by
     * `ChordGeneratorController::stampAt()` - only `timeSeconds` is: a
     * chord's own pitches are already fully determined by the Chord
     * Generator panel's own Root/Octave controls (see
     * `docs/sound-mind-architecture.md`'s Decision on this installment),
     * so a click only ever places *when* a chord starts, never *where* on
     * the frequency axis. Still carries the full point (matching every
     * other tool mode's own signal shape) rather than a bare `double`, so
     * callers don't need a second, differently-shaped conversion path just
     * for this one tool mode.
     *
     * @param point The press position, converted to time/frequency space -
     *        see this method's own docs on which half of it actually gets
     *        used.
     */
    void chordStampRequested(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief The mouse moved over the canvas - independent of toolMode(),
     *        unlike paintStrokeContinued() (which only fires while
     *        actively painting). For a status-bar-style "where's the
     *        cursor" readout - see `MainWindow`'s own status bar label.
     *
     * Requires `setMouseTracking(true)` (set in the constructor) to fire
     * without a button held, unlike every mousePressEvent()-gated signal
     * above.
     *
     * @param widgetPixel The cursor's widget-local pixel position.
     * @param domainPoint The same position converted to time/frequency
     *        space, or `std::nullopt` if no project is set (matching
     *        widgetPointToTimeFrequency()'s own contract).
     */
    void cursorMoved(QPointF widgetPixel, std::optional<sound_mind::core::TimeFrequencyPoint> domainPoint);

    /// @brief The mouse left the canvas entirely - pairs with
    ///        cursorMoved() so a caller can clear its own "where's the
    ///        cursor" display rather than leaving it showing a stale
    ///        position.
    void cursorLeft();

protected:
    /// @brief Repaints the canvas - see the class's own docs for what's
    ///        actually drawn.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Starts a paint stroke - see setToolMode()'s own docs.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress paint stroke - see setToolMode()'s
    ///        own docs.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress paint stroke - see setToolMode()'s own
    ///        docs.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

    /// @brief Emits cursorLeft() - see its own docs.
    /// @param event Unused; required by QWidget's override signature.
    void leaveEvent(QEvent* event) override;

    /**
     * @brief Zooms via the scroll wheel - `Ctrl`/`Alt`/`Shift`+wheel,
     *        matching the same modifier mnemonic as the `[`/`]` shortcuts
     *        (see zoomInCoarse()'s own docs on why `Ctrl` means "coarse"
     *        here specifically, not "proportional"). An unmodified wheel
     *        is left to `QWidget::wheelEvent()` - ordinary scrolling, once
     *        the enclosing `QScrollArea` has something to scroll.
     *
     * Prefers the wheel's own vertical delta, falling back to the
     * horizontal one if that's zero - a real, reported bug: Windows
     * itself remaps a held-`Alt` wheel scroll onto the *horizontal* delta
     * (`angleDelta().x()`), the same native "Alt+wheel = horizontal
     * scroll" convention other apps honor, so `Alt`+wheel's own
     * meaningful delta isn't reliably in `.y()` alone the way `Ctrl`'s/
     * `Shift`'s own always are. Only the resulting delta's own *sign* is
     * used (one zoom step per event, forward or backward) - not its
     * magnitude, which varies too much across mice/trackpads/OSes to map
     * onto a specific zoom multiplier meaningfully.
     *
     * @param event The wheel event.
     */
    void wheelEvent(QWheelEvent* event) override;

private:
    /// @brief This widget's own current effective pixels-per-column scale
    ///        - `zoomTime_` in `Manual` mode; derived from the widget's
    ///        own current width divided by the project's `canvasWidth`
    ///        while `FitToWindow` (i.e. whatever the enclosing scroll
    ///        area actually gave it), so a subsequent zoomIn()/zoomOut()/
    ///        etc. call has a real starting point to multiply from rather
    ///        than an arbitrary stale one.
    /// @return The effective scale; `1.0` (falls back to `zoomTime_`'s
    ///         own default) if no project is set.
    [[nodiscard]] double effectiveZoomTime() const;

    /// @brief The frequency-axis counterpart to effectiveZoomTime() - this
    ///        widget's own current height divided by `canvasHeight` while
    ///        `FitToWindow`.
    [[nodiscard]] double effectiveZoomFrequency() const;

    /// @brief The shared implementation behind every zoom-in/out/actual-
    ///        size method: clamps both factors to a sane range, stores
    ///        them, switches to `ZoomMode::Manual` (emitting
    ///        zoomModeChanged() first if that's an actual mode change, so
    ///        `MainWindow` has already put the enclosing `QScrollArea`
    ///        into `setWidgetResizable(false)` mode by the time this
    ///        method's own resize() call below actually runs - otherwise
    ///        the scroll area would just immediately resize this widget
    ///        straight back to its own viewport size), resizes this
    ///        widget to the new zoom level's own content size, and repaints.
    /// @param newZoomTime The new pixels-per-column scale, pre-clamping.
    /// @param newZoomFrequency The new pixels-per-bin scale, pre-clamping.
    void enterManualZoom(double newZoomTime, double newZoomFrequency);

    /// @brief The on-screen size the canvas would have at the given zoom
    ///        factors, given the current project's own canvas dimensions.
    /// @param zoomTime Pixels-per-column scale.
    /// @param zoomFrequency Pixels-per-bin scale.
    /// @return The computed size, or `kFallbackSize` (see the .cpp) if no
    ///         project is set.
    [[nodiscard]] QSizeF contentSizeFor(double zoomTime, double zoomFrequency) const;

    /// @brief Converts a widget-local pixel position into time/frequency
    ///        space, using the current project's own canvas geometry.
    ///        Accounts for the rendered image's own top-is-highest-
    ///        frequency convention (`color_mapping.cpp`'s `toRgbImage()`),
    ///        so a point over a visible feature on screen converts to the
    ///        same bin that feature actually lives in.
    /// @param point The widget-local pixel position.
    /// @return The converted point, or `std::nullopt` if no project is set.
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyPoint> widgetPointToTimeFrequency(
        QPointF point) const;

    /// @brief The inverse of widgetPointToTimeFrequency() - converts a
    ///        time/frequency point back into widget-local pixels, for
    ///        drawing the live preview path.
    [[nodiscard]] QPointF timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint point) const;

    /// @brief Draws every active `PaintOperation` targeting the displayed
    ///        layer's own bounding box and/or Path geometry, per
    ///        showBoundingBoxes_/showPathGeometry_ - the shared logic
    ///        behind both overlay settings, since both need the same
    ///        "which layer, which operations" lookup.
    /// @param painter The painter to draw with - already set up by
    ///        paintEvent().
    void drawOperationOverlays(QPainter& painter) const;

    /// @brief Converts a Path into a `QPainterPath` in widget-pixel space,
    ///        via timeFrequencyToWidgetPoint() - the shared geometry
    ///        behind both the live preview and Show path geometry's own
    ///        overlay.
    /// @param path The path to convert; an empty (no-node) Path converts
    ///        to an empty QPainterPath.
    [[nodiscard]] QPainterPath toPainterPath(const sound_mind::core::Path& path) const;

    /// @brief Converts a `TimeFrequencyRect` into a normalized, widget-
    ///        pixel-space `QRectF`, via timeFrequencyToWidgetPoint() - the
    ///        shared geometry behind the "Show bounding boxes" overlay and
    ///        the Pick selection highlight. `.normalized()` guards against
    ///        the rendered image's own top-is-highest-frequency corner
    ///        order - see widgetPointToTimeFrequency()'s own docs.
    /// @param bounds The rectangle to convert.
    [[nodiscard]] QRectF widgetRectFor(const sound_mind::core::TimeFrequencyRect& bounds) const;

    /// @brief Draws a small dot at every node of `paintPreviewPath_`, and
    ///        - for whichever one `previewSelectedNodeIndex_` names - a
    ///        larger, distinctly-colored dot plus (for a `Smooth` node)
    ///        its own two handles - the shared drawing behind
    ///        setPreviewSelectedNodeIndex()'s own docs.
    /// @param painter The painter to draw with - already set up by
    ///        paintEvent().
    void drawPreviewPathNodes(QPainter& painter) const;

    /// @brief Draws the frequency/time axis labels (see
    ///        setVerticalAxisLabelMode()'s/setHorizontalAxisLabelMode()'s
    ///        own docs) along the canvas's own left/bottom edges - a
    ///        small tick plus its own text at each of verticalAxisTicks()'/
    ///        horizontalAxisTicks()'s own returned positions, converted
    ///        to widget pixels via timeFrequencyToWidgetPoint(). A no-op
    ///        for whichever axis is currently `Off`, or if no project is
    ///        set.
    /// @param painter The painter to draw with - already set up by
    ///        paintEvent().
    void drawAxisLabels(QPainter& painter) const;

    /// @brief Draws the Frequency/Timing Grid's own active lines (see
    ///        setFrequencyGridConfig()'s/setTimingGridConfig()'s own
    ///        docs) - a horizontal line per `frequencyGridLinesHz()`
    ///        entry, spanning the full canvas width, and a vertical line
    ///        per `timingGridLinesSeconds()` entry, spanning the full
    ///        canvas height, each drawn with its own config's line
    ///        color/width/style. A no-op for whichever grid isn't
    ///        currently active, or if no project is set.
    /// @param painter The painter to draw with - already set up by
    ///        paintEvent().
    void drawGrid(QPainter& painter) const;

    /// @brief Draws the Chord Overlay's own active lines (see
    ///        setChordPreview()'s own docs) - a horizontal line per
    ///        `chordPreviewFrequenciesHz_` entry, spanning the full canvas
    ///        width, in a distinct color/style from `drawGrid()`'s own
    ///        Frequency Grid lines. A no-op if `chordPreviewFrequenciesHz_`
    ///        is empty, or if no project is set.
    /// @param painter The painter to draw with - already set up by
    ///        paintEvent().
    void drawChordPreview(QPainter& painter) const;

    const sound_mind::core::Project* project_ = nullptr;
    std::optional<double> playheadFraction_;
    ToolMode toolMode_ = ToolMode::None;
    bool paintStrokeActive_ = false;
    bool pickStrokeActive_ = false;
    bool selectStrokeActive_ = false;
    sound_mind::core::Path paintPreviewPath_;
    std::optional<std::size_t> previewSelectedNodeIndex_;
    std::optional<sound_mind::core::TimeFrequencyRect> pickSelectionBounds_;
    std::optional<sound_mind::core::TimeFrequencyRect> selectionBounds_;
    std::optional<sound_mind::core::Path> selectionBoundary_;
    bool selectionHasMaskShape_ = false;
    std::optional<sound_mind::core::TimeFrequencyPoint> selectionRotationHandle_;
    bool rotateHandleDragActive_ = false;

    /// @brief The MindWave currently being previewed, if any - kept only
    ///        so setProject() can tell whether there's actually a preview
    ///        to clear; mindWavePreviewImage_ (already evaluated) is what
    ///        paintEvent() actually draws.
    std::optional<sound_mind::core::MindWave> mindWavePreview_;

    /// @brief The cached grayscale rendering of mindWavePreview_'s own
    ///        field, evaluated once in setMindWavePreview() rather than on
    ///        every repaint - a default-constructed (null) QImage draws
    ///        nothing.
    QImage mindWavePreviewImage_;
    bool showBoundingBoxes_ = false;
    bool showPathGeometry_ = false;
    VerticalAxisLabelMode verticalAxisLabelMode_ = VerticalAxisLabelMode::Off;
    HorizontalAxisLabelMode horizontalAxisLabelMode_ = HorizontalAxisLabelMode::Off;
    FrequencyGridConfig frequencyGridConfig_;
    TimingGridConfig timingGridConfig_;

    /// @brief The Chord Overlay's own current note pitches, in Hz - see
    ///        setChordPreview()'s own docs; empty draws nothing.
    std::vector<double> chordPreviewFrequenciesHz_;

    /// @brief See setChordGeneratorPanelVisible()'s own docs.
    bool chordGeneratorPanelVisible_ = false;
    ZoomMode zoomMode_ = ZoomMode::FitToWindow;
    /// @brief `Manual` mode's own stored pixels-per-column scale - `1.0`
    ///        is "Actual Size" (one screen pixel per column). Unused
    ///        while `FitToWindow` (see effectiveZoomTime()'s own docs).
    double zoomTime_ = 1.0;
    /// @brief The frequency-axis counterpart to zoomTime_ - pixels per bin.
    double zoomFrequency_ = 1.0;
};

}  // namespace sound_mind::studio
