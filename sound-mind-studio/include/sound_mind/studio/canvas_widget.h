#pragma once

#include <optional>

#include <QPainterPath>
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
     * paintStrokeEnded() instead. Exactly one mode is active at a time;
     * future tool modes (Pick, Selection) extend this same enum rather
     * than stacking independent flags.
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

    const sound_mind::core::Project* project_ = nullptr;
    std::optional<double> playheadFraction_;
    ToolMode toolMode_ = ToolMode::None;
    bool paintStrokeActive_ = false;
    sound_mind::core::Path paintPreviewPath_;
    bool showBoundingBoxes_ = false;
    bool showPathGeometry_ = false;
};

}  // namespace sound_mind::studio
