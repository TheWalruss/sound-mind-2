#pragma once

#include <cstdint>
#include <optional>

#include <QDialog>
#include <QImage>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QSpinBox;

namespace sound_mind::studio {

/**
 * @brief The parameters `PolarOriginDialog` collects for a polar-form
 *        image import - `docs/sound-mind-roadmap.md`'s `v0.Y.53.1`
 *        Installment B, `sound_mind::codec::polarToRect()`'s own
 *        parameter set given a name.
 */
struct PolarImportParams {
    /// @brief The flower's own centre, in the source image's own pixel
    ///        coordinates.
    double originX = 0.0;
    /// @brief The flower's own centre (vertical).
    double originY = 0.0;
    /// @brief The maximum sampling radius, in the source image's own
    ///        pixels.
    double radius = 0.0;
    /// @brief Arc start, in radians (`0` = twelve o'clock, clockwise).
    double arcStartRadians = 0.0;
    /// @brief Arc end, in radians. Equal to `arcStartRadians` (within a
    ///        small epsilon) means a full `2*pi` circle - see
    ///        `sound_mind::codec::polarToRect()`'s own docs.
    double arcEndRadians = 0.0;
    /// @brief The desired output (rectangular) width, in pixels.
    std::uint32_t outputWidth = 0;
};

/**
 * @brief An interactive, draggable-handle picker for `PolarImportParams` -
 *        `docs/sound-mind-roadmap.md`'s `v0.Y.53.1` Installment B.
 *
 * Shows the source image scaled to fit the widget, with four draggable
 * handles: a crosshair (the flower's own centre), a ring (the sampling
 * radius - dragged from its own point at the top, twelve o'clock), and
 * two square handles on the ring (the arc's own start/end angle). The
 * same "purely presentational, every edit emits a signal" division of
 * responsibility every other configuration control in this codebase
 * already follows (see `ToneCurveEditor`'s own docs) - nothing here reads
 * or writes a `PolarImportParams` directly except through this widget's
 * own accessors.
 *
 * A direct, from-scratch reimplementation of the legacy Python Studio's
 * own `PolarOriginDialog._PreviewWidget` (`../sound-mind/packages/
 * sound_mind_studio/src/sound_mind_studio/dialogs/polar_origin_dialog.py`) -
 * the drag/hit-test interactions and the four handles themselves are
 * deliberately identical (that's the whole point of consulting it), but
 * every value is exposed as a real Qt property/signal rather than the
 * legacy widget's own plain public attributes plus a manual
 * `_notify_dialog()` callback, matching this codebase's own established
 * shape instead.
 */
class PolarOriginPickerWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the widget with no source image yet (a plain gray
    ///        placeholder) and every value at its own zero default.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit PolarOriginPickerWidget(QWidget* parent = nullptr);

    /**
     * @brief Sets the image to preview and pick against, resetting every
     *        handle to its own default position: the origin at the
     *        image's own centre, the radius at 45% of the image's own
     *        smaller dimension, and a full circle (arc start/end both
     *        `0`) - matching the legacy Studio's own picker defaults.
     * @param image The source image; a null `QImage` shows a plain gray
     *        placeholder instead (this widget's own state right after
     *        construction).
     */
    void setSourceImage(QImage image);

    /// @brief The flower's own centre, in the source image's own pixel
    ///        coordinates.
    /// @return The current horizontal origin.
    [[nodiscard]] double originX() const noexcept { return originX_; }
    /// @brief The flower's own centre (vertical).
    /// @return The current vertical origin.
    [[nodiscard]] double originY() const noexcept { return originY_; }
    /// @brief The current sampling radius, in the source image's own
    ///        pixels.
    /// @return The current radius.
    [[nodiscard]] double radius() const noexcept { return radius_; }
    /// @brief Arc start, in radians (`0` = twelve o'clock, clockwise).
    /// @return The current arc start.
    [[nodiscard]] double arcStartRadians() const noexcept { return arcStartRadians_; }
    /// @brief Arc end, in radians - `0` means "equal to arc start" (a
    ///        full circle) only when arcStartRadians() is also `0`; see
    ///        `PolarImportParams::arcEndRadians`'s own docs for the
    ///        general "equal to start" rule this is just the default
    ///        case of.
    /// @return The current arc end.
    [[nodiscard]] double arcEndRadians() const noexcept { return arcEndRadians_; }

    /// @brief Sets the origin (horizontal) directly - the actual work
    ///        behind the dialog's own Origin X spinbox - and repaints.
    ///        Clamped to the source image's own width.
    /// @param x The new horizontal origin, in source-image pixels.
    void setOriginX(double x);
    /// @brief The vertical counterpart to setOriginX().
    /// @param y The new vertical origin, in source-image pixels.
    void setOriginY(double y);
    /// @brief Sets the sampling radius directly - clamped to at least `1`
    ///        pixel - and repaints.
    /// @param radius The new radius, in source-image pixels.
    void setRadius(double radius);
    /// @brief Sets the arc start directly, in radians (wrapped into
    ///        `[0, 2*pi)`), and repaints.
    /// @param theta The new arc start, in radians.
    void setArcStartRadians(double theta);
    /// @brief Sets the arc end directly, in radians (wrapped into
    ///        `[0, 2*pi)`), and repaints.
    /// @param theta The new arc end, in radians.
    void setArcEndRadians(double theta);

    /// @return `{240, 240}` - a reasonable default preview size.
    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// @brief Emitted whenever a drag changes any handle - the dialog's
    ///        own cue to re-sync its spinboxes from this widget, the same
    ///        "widget drives dialog" direction `ToneCurveEditor::
    ///        pointsChanged()` already establishes elsewhere. Never
    ///        emitted by the setOriginX()-and-friends setters themselves
    ///        (those are the *dialog* driving *this widget*, the opposite
    ///        direction - emitting here too would loop the two back and
    ///        forth pointlessly).
    void paramsChangedByDrag();

protected:
    /// @brief Draws the source image (or placeholder), the crosshair,
    ///        the sampling ring, the arc sector (if not a full circle),
    ///        and all four handles.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

    /// @brief Begins dragging whichever handle the press landed within a
    ///        small hit radius of, if any.
    /// @param event The press event.
    void mousePressEvent(QMouseEvent* event) override;

    /// @brief Continues an in-progress drag, if any - updates the cursor
    ///        shape over a draggable handle even without one.
    /// @param event The move event.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// @brief Ends an in-progress drag, if any.
    /// @param event The release event.
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// @brief Which handle a press/hit-test landed on, if any.
    enum class Handle {
        None,
        Origin,
        Ring,
        ArcStart,
        ArcEnd,
    };

    /// @brief The image-to-widget scale factor this widget's own current
    ///        size and the source image's own dimensions produce -
    ///        `std::min` of the two axes' own ratios, so the whole image
    ///        fits without distortion (matching `CanvasWidget::
    ///        polarDiskRect()`'s own "largest square/rect that fits,
    ///        centred" precedent, generalized to a non-square image).
    [[nodiscard]] double imageToWidgetScale() const;

    /// @brief The image's own top-left corner, in widget pixels - the
    ///        centering offset imageToWidgetScale()'s own leftover space
    ///        is split into.
    [[nodiscard]] QPointF imageOrigin() const;

    /// @brief Converts a source-image pixel position into a widget-local
    ///        pixel position.
    [[nodiscard]] QPointF imageToWidget(QPointF imagePoint) const;

    /// @brief The inverse of imageToWidget().
    [[nodiscard]] QPointF widgetToImage(QPointF widgetPoint) const;

    /// @brief The arc handle's own image-space position at angle `theta`
    ///        - a point on the ring, `radius_` pixels from the origin.
    [[nodiscard]] QPointF arcHandleImagePos(double theta) const;

    /// @brief Which handle (if any) is within a small hit radius of
    ///        `widgetPoint`.
    [[nodiscard]] Handle hitTest(QPointF widgetPoint) const;

    QImage sourceImage_;
    double originX_ = 0.0;
    double originY_ = 0.0;
    double radius_ = 1.0;
    double arcStartRadians_ = 0.0;
    double arcEndRadians_ = 0.0;
    Handle dragging_ = Handle::None;
};

/**
 * @brief The graphical origin/radius/arc picker for a polar-form image
 *        import - `docs/sound-mind-roadmap.md`'s `v0.Y.53.1` Installment B
 *        ("polar-form image import"), shown when `ImageScalePickerDialog`'s
 *        own `Mode::Polar` is selected.
 *
 * A one-shot `QDialog` around `PolarOriginPickerWidget` plus spinboxes for
 * every value (origin X/Y, radius, arc start/end in degrees, output
 * width) and a read-only duration label - the same one-shot-action shape
 * `GeneratorDialog`'s own docs already establish ("pick settings once,
 * then the action happens and the dialog is done"), not a persistent
 * panel, since choosing where a polar import samples from has no ongoing
 * configuration to keep referencing afterward.
 *
 * The widget and the spinboxes stay in sync both ways: dragging a handle
 * updates its own spinbox (via paramsChangedByDrag()), and editing a
 * spinbox moves the widget's own handle (via the plain setOriginX()-and-
 * friends setters) - the same two-way sync `PolarOriginPickerWidget`'s own
 * docs describe.
 */
class PolarOriginDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog, pre-populated with `sourceImage`'s own
     *        default handle positions (see `PolarOriginPickerWidget::
     *        setSourceImage()`'s own docs).
     * @param sourceImage The image to preview and pick against.
     * @param initialParams If given, every handle starts from these
     *        values instead of `PolarOriginPickerWidget::setSourceImage()`'s
     *        own defaults - `ImageScalePickerDialog`'s own "resume from
     *        the same choice, not the defaults" behavior when its own
     *        "Set origin..." button is clicked more than once. Applied
     *        after the image's own defaults, before the spinboxes' own
     *        first sync, so the dialog opens already showing these values
     *        consistently rather than a caller having to reach in and
     *        mutate the picker widget after the fact.
     * @param defaultOutputWidth The output width spinbox's own starting
     *        value - typically the project's own canvas width. `0` (the
     *        default) instead starts from `floor(2*pi * radius)`, the
     *        width that gives roughly square arc pixels at the picker's
     *        own default radius - recomputed live as the radius changes,
     *        for as long as the output width spinbox itself hasn't been
     *        touched (see the class's own docs on why this stops once it
     *        has).
     * @param timestepMs The project's own column duration, in
     *        milliseconds - `0.0` (the default) shows no duration label
     *        at all, for a caller with no project (or timestep) context
     *        to give one against.
     * @param parent The owning widget, per Qt's normal parent-ownership
     *        convention; may be `nullptr`.
     */
    explicit PolarOriginDialog(QImage sourceImage, std::optional<PolarImportParams> initialParams = std::nullopt,
                                std::uint32_t defaultOutputWidth = 0, double timestepMs = 0.0,
                                QWidget* parent = nullptr);

    /// @brief The chosen parameters - call after `exec()` returns
    ///        `QDialog::Accepted`.
    /// @return The current origin/radius/arc/output-width values.
    [[nodiscard]] PolarImportParams params() const;

private:
    /// @brief Copies the picker widget's own current values into every
    ///        spinbox (signals blocked, so this never loops back into
    ///        syncWidgetFromSpinboxes()) - paramsChangedByDrag()'s own
    ///        slot.
    void syncSpinboxesFromWidget();

    /// @brief Copies every spinbox's own current value into the picker
    ///        widget - each spinbox's own valueChanged() slot.
    void syncWidgetFromSpinboxes();

    /// @brief Refreshes the duration label from the output width
    ///        spinbox's own current value and timestepMs_ - a no-op
    ///        (label stays hidden) if timestepMs_ is `0`.
    void updateDurationLabel();

    /// @brief Re-derives the output width spinbox's own value from the
    ///        picker's own current radius, if outputWidthTracksRadius_ is
    ///        still `true` - the shared body both syncSpinboxesFromWidget()
    ///        (a drag) and syncWidgetFromSpinboxes() (a direct edit of any
    ///        spinbox, including the radius one itself) need, so either
    ///        direction of edit keeps the output width hint live for as
    ///        long as it's still tracking.
    void updateOutputWidthFromRadius();

    PolarOriginPickerWidget* picker_ = nullptr;
    QDoubleSpinBox* originXSpinBox_ = nullptr;
    QDoubleSpinBox* originYSpinBox_ = nullptr;
    QDoubleSpinBox* radiusSpinBox_ = nullptr;
    QDoubleSpinBox* arcStartSpinBox_ = nullptr;
    QDoubleSpinBox* arcEndSpinBox_ = nullptr;
    QSpinBox* outputWidthSpinBox_ = nullptr;
    QLabel* durationLabel_ = nullptr;

    /// @brief See the constructor's own docs.
    double timestepMs_ = 0.0;

    /// @brief Whether outputWidthSpinBox_'s own value should still track
    ///        the radius live - `true` until the user edits it directly
    ///        (or the dialog's own constructor already gave an explicit
    ///        `defaultOutputWidth`, in which case this starts `false`) -
    ///        matching the legacy Studio's own "auto-update output width
    ///        hint... only when no project width is set" precedent.
    bool outputWidthTracksRadius_ = true;
};

}  // namespace sound_mind::studio
