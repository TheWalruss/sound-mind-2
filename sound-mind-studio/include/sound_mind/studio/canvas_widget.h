#pragma once

#include <optional>

#include <QPainterPath>
#include <QRectF>
#include <QWidget>

#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"

class QEvent;
class QMouseEvent;
class QPainter;

namespace sound_mind::studio {

/**
 * @brief Renders the active project's canvas.
 *
 * Shows the topmost layer with cached content (see
 * `sound_mind::core::renderLayer()`), scaled to fill the widget - real
 * multi-layer compositing (blend modes, opacity, MindWave-bound
 * parameters) doesn't exist yet, so "the composite" is, for now, just
 * whichever layer was rendered most recently. Falls back to a placeholder
 * rectangle, sized to the project's configured canvas dimensions, when no
 * layer has any content yet (e.g. a fresh project with only its empty
 * Background layer).
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
     * selectStrokeEnded() instead, and `Path` makes each individual left-
     * button press (no drag/release counterpart - see pathNodePlaced()'s
     * own docs) emit pathNodePlaced() instead. Exactly one mode is active
     * at a time. Switching away from `Paint`/`Pick`/`Select` cancels
     * whatever gesture was mid-flight in it (a real mouse-up may never
     * arrive - e.g. the toolbar button was clicked instead); `Path` has no
     * such in-widget state to cancel (see pathNodePlaced()'s own docs) -
     * its own in-progress node sequence lives in `PathController` instead,
     * cancelled from there.
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
     * Always drawn when set, regardless of setShowBoundingBoxes()'s own
     * state - the same "what you're actively interacting with is always
     * visible" precedent setPaintPreviewPath() already established, since
     * there'd otherwise be no visual indication at all of *which* object
     * is selected while Show bounding boxes is off.
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

    /// @brief The widget's preferred size.
    /// @return The current project's configured canvas dimensions, or a
    ///         fallback size if no project is set.
    [[nodiscard]] QSize sizeHint() const override;

signals:
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
    void selectStrokeStarted(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress selection drag continued (`Select` tool
    ///        mode, left button held and moved).
    /// @param point The new position, converted to time/frequency space.
    void selectStrokeContinued(sound_mind::core::TimeFrequencyPoint point);

    /// @brief The in-progress selection drag ended (`Select` tool mode,
    ///        left button released).
    void selectStrokeEnded();

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

private:
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
    bool showBoundingBoxes_ = false;
    bool showPathGeometry_ = false;
};

}  // namespace sound_mind::studio
