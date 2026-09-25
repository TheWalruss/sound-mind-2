#include "sound_mind/studio/canvas_widget.h"

#include <algorithm>
#include <optional>

#include <QColor>
#include <QFontMetrics>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QWheelEvent>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/core/compositor.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/qt_image_conversion.h"

namespace sound_mind::studio {

namespace {
const QSize kFallbackSize(400, 300);

// Canvas Navigation's own Zoom feature (docs/sound-mind-design.md) - see
// CanvasWidget::enterManualZoom()'s own docs for how these are used.
constexpr double kZoomStepFactor = 1.25;
constexpr double kZoomCoarseStepFactor = 2.0;
constexpr double kMinZoomFactor = 0.05;
constexpr double kMaxZoomFactor = 16.0;

/// @brief The project's own real multi-layer composite (see
/// `sound_mind::core::compositeProject()`'s own docs), converted to
/// displayable pixels - `docs/sound-mind-roadmap.md`'s `v0.Y.27.1`
/// (Multi-layer Compositing), replacing the single-topmost-layer
/// placeholder every render path here used before it.
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> renderComposite(const sound_mind::core::Project& project) {
    const auto composite = sound_mind::core::compositeProject(project);
    if (!composite.has_value()) {
        return std::nullopt;
    }
    return sound_mind::codec::toRgbImage(*composite);
}

/// @brief The topmost visible layer with content - unlike
/// renderComposite() above (which now shows every visible layer's own
/// contribution, blended together), Show bounding boxes/Show path
/// geometry are a per-layer editing aid tied to whichever single layer
/// they'd highlight operations on, so they deliberately keep pointing at
/// the topmost one rather than trying to show every layer's operations
/// overlaid at once - unrelated to (and left unchanged by)
/// `v0.Y.27.1`'s (Multi-layer Compositing) own real composite above.
[[nodiscard]] const sound_mind::core::Layer* findTopmostLayerWithContent(const sound_mind::core::Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->visible() && it->content().has_value()) {
            return &*it;
        }
    }
    return nullptr;
}

}  // namespace

CanvasWidget::CanvasWidget(QWidget* parent) : QWidget(parent) {
    // cursorMoved() needs mouseMoveEvent() to fire on every move, not just
    // while a button is held (QWidget's default) - see that signal's own
    // docs.
    setMouseTracking(true);
}

void CanvasWidget::setProject(const sound_mind::core::Project* project) {
    project_ = project;
    // A previous project's own zoom level means nothing for a different
    // one (a coincidentally-similar canvas size aside) - the same
    // "session-only UI state resets on project switch" precedent
    // UndoStack::clear()/LayersPanel::clearSelection() already establish.
    const bool modeChanged = zoomMode_ != ZoomMode::FitToWindow;
    zoomMode_ = ZoomMode::FitToWindow;
    zoomTime_ = 1.0;
    zoomFrequency_ = 1.0;
    if (modeChanged) {
        emit zoomModeChanged(zoomMode_);
    }
    // Same reasoning as the zoom reset above - a previous project's own
    // MindWave preview (evaluated against its own, possibly different,
    // canvas dimensions) means nothing for a different project.
    mindWavePreview_.reset();
    mindWavePreviewImage_ = QImage();
    updateGeometry();
    update();
}

void CanvasWidget::setPlayheadFraction(std::optional<double> fraction) {
    playheadFraction_ = fraction;
    update();
}

void CanvasWidget::setToolMode(ToolMode mode) {
    toolMode_ = mode;
    paintStrokeActive_ = false;
    pickStrokeActive_ = false;
    selectStrokeActive_ = false;
    rotateHandleDragActive_ = false;
    update();
}

void CanvasWidget::setPaintPreviewPath(sound_mind::core::Path path) {
    paintPreviewPath_ = std::move(path);
    update();
}

void CanvasWidget::setPreviewSelectedNodeIndex(std::optional<std::size_t> index) {
    previewSelectedNodeIndex_ = index;
    update();
}

void CanvasWidget::setPickSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds) {
    pickSelectionBounds_ = bounds;
    update();
}

void CanvasWidget::setSelectionBoundary(std::optional<sound_mind::core::Path> boundary) {
    selectionBoundary_ = std::move(boundary);
    update();
}

void CanvasWidget::setSelectionBounds(std::optional<sound_mind::core::TimeFrequencyRect> bounds) {
    selectionBounds_ = bounds;
    update();
}

void CanvasWidget::setSelectionHasMaskShape(bool hasMaskShape) {
    selectionHasMaskShape_ = hasMaskShape;
    update();
}

void CanvasWidget::setSelectionRotationHandle(std::optional<sound_mind::core::TimeFrequencyPoint> handlePosition) {
    selectionRotationHandle_ = handlePosition;
    update();
}

void CanvasWidget::setMindWavePreview(std::optional<sound_mind::core::MindWave> wave) {
    mindWavePreview_ = std::move(wave);
    if (!mindWavePreview_.has_value() || project_ == nullptr) {
        mindWavePreviewImage_ = QImage();
        update();
        return;
    }
    const auto& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);
    const auto field = sound_mind::core::evaluateMindWaveField(*mindWavePreview_, config, settings.canvasWidth);
    const auto grayscale = sound_mind::codec::toGrayscaleImage(field, settings.canvasWidth, config.binCount);
    mindWavePreviewImage_ = toQImageView(grayscale).copy();
    update();
}

void CanvasWidget::setChordPreview(std::vector<double> frequenciesHz) {
    chordPreviewFrequenciesHz_ = std::move(frequenciesHz);
    update();
}

void CanvasWidget::setChordGeneratorPanelVisible(bool visible) {
    chordGeneratorPanelVisible_ = visible;
    update();
}

void CanvasWidget::setVerticalAxisLabelMode(VerticalAxisLabelMode mode) {
    verticalAxisLabelMode_ = mode;
    update();
}

void CanvasWidget::setHorizontalAxisLabelMode(HorizontalAxisLabelMode mode) {
    horizontalAxisLabelMode_ = mode;
    update();
}

void CanvasWidget::setFrequencyGridConfig(const FrequencyGridConfig& config) {
    frequencyGridConfig_ = config;
    update();
}

void CanvasWidget::setTimingGridConfig(const TimingGridConfig& config) {
    timingGridConfig_ = config;
    update();
}

void CanvasWidget::setShowBoundingBoxes(bool shown) {
    showBoundingBoxes_ = shown;
    update();
}

void CanvasWidget::setShowPathGeometry(bool shown) {
    showPathGeometry_ = shown;
    update();
}

QSize CanvasWidget::sizeHint() const {
    if (project_ == nullptr) {
        return kFallbackSize;
    }
    return QSize(static_cast<int>(project_->settings().canvasWidth),
                 static_cast<int>(project_->settings().canvasHeight));
}

void CanvasWidget::zoomToFit() {
    const bool modeChanged = zoomMode_ != ZoomMode::FitToWindow;
    zoomMode_ = ZoomMode::FitToWindow;
    if (modeChanged) {
        emit zoomModeChanged(zoomMode_);
    }
    update();
}

void CanvasWidget::zoomToActualSize() { enterManualZoom(1.0, 1.0); }

void CanvasWidget::zoomIn() { enterManualZoom(effectiveZoomTime() * kZoomStepFactor, effectiveZoomFrequency() * kZoomStepFactor); }

void CanvasWidget::zoomOut() { enterManualZoom(effectiveZoomTime() / kZoomStepFactor, effectiveZoomFrequency() / kZoomStepFactor); }

void CanvasWidget::zoomInTimeOnly() { enterManualZoom(effectiveZoomTime() * kZoomStepFactor, effectiveZoomFrequency()); }

void CanvasWidget::zoomOutTimeOnly() { enterManualZoom(effectiveZoomTime() / kZoomStepFactor, effectiveZoomFrequency()); }

void CanvasWidget::zoomInFrequencyOnly() { enterManualZoom(effectiveZoomTime(), effectiveZoomFrequency() * kZoomStepFactor); }

void CanvasWidget::zoomOutFrequencyOnly() { enterManualZoom(effectiveZoomTime(), effectiveZoomFrequency() / kZoomStepFactor); }

void CanvasWidget::zoomInCoarse() {
    enterManualZoom(effectiveZoomTime() * kZoomCoarseStepFactor, effectiveZoomFrequency() * kZoomCoarseStepFactor);
}

void CanvasWidget::zoomOutCoarse() {
    enterManualZoom(effectiveZoomTime() / kZoomCoarseStepFactor, effectiveZoomFrequency() / kZoomCoarseStepFactor);
}

double CanvasWidget::effectiveZoomTime() const {
    if (zoomMode_ == ZoomMode::Manual || project_ == nullptr || project_->settings().canvasWidth == 0) {
        return zoomTime_;
    }
    return static_cast<double>(rect().width()) / static_cast<double>(project_->settings().canvasWidth);
}

double CanvasWidget::effectiveZoomFrequency() const {
    if (zoomMode_ == ZoomMode::Manual || project_ == nullptr || project_->settings().canvasHeight == 0) {
        return zoomFrequency_;
    }
    return static_cast<double>(rect().height()) / static_cast<double>(project_->settings().canvasHeight);
}

void CanvasWidget::enterManualZoom(double newZoomTime, double newZoomFrequency) {
    zoomTime_ = std::clamp(newZoomTime, kMinZoomFactor, kMaxZoomFactor);
    zoomFrequency_ = std::clamp(newZoomFrequency, kMinZoomFactor, kMaxZoomFactor);

    const bool modeChanged = zoomMode_ != ZoomMode::Manual;
    zoomMode_ = ZoomMode::Manual;
    if (modeChanged) {
        // Emitted before resize() below - MainWindow's own reaction
        // (setWidgetResizable(false) on the enclosing QScrollArea) has to
        // land first, or that scroll area would just immediately resize
        // this widget straight back to its own viewport size, undoing the
        // resize() call entirely (see this method's own docs).
        emit zoomModeChanged(zoomMode_);
    }
    resize(contentSizeFor(zoomTime_, zoomFrequency_).toSize());
    update();
}

QSizeF CanvasWidget::contentSizeFor(double zoomTime, double zoomFrequency) const {
    if (project_ == nullptr) {
        return QSizeF(kFallbackSize);
    }
    const auto& settings = project_->settings();
    return QSizeF(static_cast<double>(settings.canvasWidth) * zoomTime,
                  static_cast<double>(settings.canvasHeight) * zoomFrequency);
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    const auto modifiers = event->modifiers();
    if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
        QWidget::wheelEvent(event);
        return;
    }
    // Windows itself remaps a held-Alt wheel scroll onto the *horizontal*
    // delta (angleDelta().x()), the native "Alt+wheel = horizontal scroll"
    // convention other apps also honor - so the meaningful delta isn't
    // reliably in .y() alone. Preferring whichever component is actually
    // nonzero (y first, matching every other modifier's own plain vertical
    // gesture) covers both cases without needing to special-case Alt.
    const QPoint angleDelta = event->angleDelta();
    const int delta = angleDelta.y() != 0 ? angleDelta.y() : angleDelta.x();
    if (delta == 0) {
        event->ignore();
        return;
    }
    const bool zoomingIn = delta > 0;
    if (modifiers & Qt::ControlModifier) {
        zoomingIn ? zoomInCoarse() : zoomOutCoarse();
    } else if (modifiers & Qt::AltModifier) {
        zoomingIn ? zoomInFrequencyOnly() : zoomOutFrequencyOnly();
    } else {  // Qt::ShiftModifier
        zoomingIn ? zoomInTimeOnly() : zoomOutTimeOnly();
    }
    event->accept();
}

void CanvasWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (project_ != nullptr) {
        if (const auto rendered = renderComposite(*project_); rendered.has_value()) {
            painter.drawImage(rect(), toQImageView(*rendered));
        } else {
            // No layer has any content at all yet - fall back to the
            // placeholder that stood in for the whole canvas before
            // Import existed.
            const QRect canvasRect(0, 0, static_cast<int>(project_->settings().canvasWidth),
                                    static_cast<int>(project_->settings().canvasHeight));
            painter.fillRect(canvasRect.intersected(rect()), QColor(40, 40, 40));
            painter.setPen(Qt::darkGray);
            painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));
        }

        if (showBoundingBoxes_ || showPathGeometry_) {
            drawOperationOverlays(painter);
        }

        // Overlay Grids (see setFrequencyGridConfig()'s/
        // setTimingGridConfig()'s own docs) - drawn over the rendered
        // content but under every interactive overlay below (the live
        // paint preview, Pick/Selection highlights), the same "a
        // reference aid the artist paints against, not a foreground
        // element" ordering the design doc's own "purely a display aid"
        // framing implies.
        drawGrid(painter);

        // Chord Overlay (see setChordPreview()'s own docs) - same
        // reference-aid ordering as Overlay Grids above, drawn right after
        // them so both sets of reference lines sit together, under every
        // interactive overlay.
        drawChordPreview(painter);

        // MindWave Preview (see setMindWavePreview()'s own docs) - a
        // semi-transparent grayscale overlay, same ordering reasoning as
        // Overlay Grids above (a display aid over the content, under any
        // interactive overlay). setOpacity() is reset immediately after -
        // nothing else drawn below should inherit it.
        if (!mindWavePreviewImage_.isNull()) {
            painter.setOpacity(0.5);
            painter.drawImage(rect(), mindWavePreviewImage_);
            painter.setOpacity(1.0);
        }
    }

    // The playhead (v0.0.21.1, Playback position bar) is drawn last, over
    // whatever the canvas otherwise shows - independent of project_/layer
    // state, matching video export's own playhead line
    // (sound_mind::codec::exportVideo()) so live playback and an exported
    // video look the same.
    if (playheadFraction_.has_value()) {
        const int x = static_cast<int>(*playheadFraction_ * rect().width());
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(x, 0, x, rect().height());
    }

    // The live paint-stroke preview (v0.Y.24.1, Basic Painting) - see
    // setPaintPreviewPath()'s own docs. Always drawn, regardless of Show
    // path geometry's own setting - see that method's own docs for why.
    //
    // Drawn as a black-outlined white line, not a single fixed color - a
    // path being painted or edited over a matching painted color (yellow
    // over yellow, say) used to disappear entirely. No single fixed color
    // survives an arbitrary painted background, but white-with-a-black-
    // outline does: nothing is simultaneously black and white, so at
    // least one of the two always contrasts, on any background color or
    // pattern - the same "halo" technique drawPreviewPathNodes() below
    // uses for the nodes/handles themselves.
    if (project_ != nullptr && !paintPreviewPath_.nodes().empty()) {
        const QPainterPath previewPath = toPainterPath(paintPreviewPath_);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawPath(previewPath);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawPath(previewPath);
        drawPreviewPathNodes(painter);
    }

    // The Picked object's own selection highlight (Pick) - see
    // setPickSelectionBounds()'s own docs. Drawn when set, regardless of
    // Show bounding boxes' own setting, for the same reason the live
    // paint preview above always draws regardless of Show path geometry -
    // *except* while a live preview is itself showing (a whole-object
    // move, or a path edit session): setPickSelectionBounds() only ever
    // updates on selectionChanged(), which a move/edit-in-progress
    // doesn't emit, so this rectangle would otherwise sit frozen at the
    // picked object's own *pre*-drag position for the entire gesture,
    // visibly out of step with the live preview actually tracking the
    // drag (reported as the bounding box's own top corners "not updated
    // properly when moved" - true of the whole box, but the top edge is
    // the one most visibly wrong while dragging down and to the right).
    // The live preview itself already shows the object's current extent
    // more accurately than this rectangle ever could mid-drag.
    if (project_ != nullptr && pickSelectionBounds_.has_value() && paintPreviewPath_.nodes().empty()) {
        painter.setPen(QPen(Qt::white, 2));
        painter.drawRect(widgetRectFor(*pickSelectionBounds_));
    }

    // The current selection (Select) - see setSelectionBounds()'s/
    // setSelectionBoundary()'s own docs. Real-world testing pass,
    // 2026-09-20, finding #16: only drawn while Pick or Select is the
    // active tool - previously drawn regardless of toolMode(), which in
    // practice meant this box (and, after a Paste, its own "highlight the
    // pasted region" reuse of it - see SelectionController::pasteAt()'s
    // own docs) could linger onscreen through whatever unrelated tool
    // switches followed. A Lasso selection draws its own actual curve
    // (setSelectionBoundary()) in place of the plain rectangle a
    // Rectangle selection draws - the two are mutually exclusive
    // (setSelectionBoundary() is only ever set alongside a Lasso-shaped
    // setSelectionBounds()), so there's never a need to draw both.
    const bool selectionToolActive = toolMode_ == ToolMode::Pick || toolMode_ == ToolMode::Select;
    if (project_ != nullptr && selectionToolActive && selectionBoundary_.has_value()) {
        // closeSubpath() draws the same implicit straight closing edge
        // (last node back to first) sound_mind::core::containsPoint()
        // itself treats the boundary as having - toPainterPath() alone
        // only ever draws an *open* curve (every other caller needs it
        // that way, for an in-progress or already-painted brush stroke).
        QPainterPath boundaryPath = toPainterPath(*selectionBoundary_);
        boundaryPath.closeSubpath();
        painter.setPen(QPen(Qt::green, 2));
        painter.drawPath(boundaryPath);
    } else if (project_ != nullptr && selectionToolActive && selectionBounds_.has_value()) {
        // A Mask-shaped selection (Wand, or any boolean-combined result -
        // see setSelectionHasMaskShape()'s own docs) has no single curve to
        // draw - a dashed pen distinguishes "the real shape is somewhere
        // inside this box, not exactly this box" from a real Rectangle
        // selection's own solid outline.
        painter.setPen(QPen(Qt::green, 2, selectionHasMaskShape_ ? Qt::DashLine : Qt::SolidLine));
        painter.drawRect(widgetRectFor(*selectionBounds_));
    }

    // The current selection's own rotate handle (Rectangle only) - see
    // setSelectionRotationHandle()'s own docs. A small filled dot, the
    // same visual weight kPathHandleRadius-equivalent path-editing
    // handles elsewhere already use, connected back to the selection's
    // own center with a thin line so the handle doesn't read as an
    // unrelated, floating mark.
    if (project_ != nullptr && selectionRotationHandle_.has_value() && selectionBounds_.has_value()) {
        constexpr double kRotationHandleRadius = 4.0;
        const QPointF handlePoint = timeFrequencyToWidgetPoint(*selectionRotationHandle_);
        const QPointF centerPoint = widgetRectFor(*selectionBounds_).center();
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(centerPoint, handlePoint);
        painter.setBrush(Qt::white);
        painter.drawEllipse(handlePoint, kRotationHandleRadius, kRotationHandleRadius);
        painter.setBrush(Qt::NoBrush);
    }

    // Axis Labels (see setVerticalAxisLabelMode()'s/
    // setHorizontalAxisLabelMode()'s own docs) - drawn last, over
    // everything else, the same "always legible" precedent the playhead
    // and every Pick/Selection highlight above already established.
    if (project_ != nullptr) {
        drawAxisLabels(painter);
    }
}

void CanvasWidget::drawAxisLabels(QPainter& painter) const {
    // How far a tick mark extends from the canvas's own edge, and how
    // much breathing room its own label text gets around it - small
    // enough not to eat into the canvas itself, big enough to read.
    constexpr int kTickLength = 4;
    constexpr int kTextPadding = 2;
    // A translucent backing behind each label - like the halo technique
    // drawPreviewPathNodes() uses for node dots, but simpler for text:
    // a dark backing plate reads clearly over any painted color or
    // pattern, without needing a second color to cover the opposite
    // case.
    const QColor kLabelBackground(0, 0, 0, 170);

    const auto& settings = project_->settings();
    const QFontMetrics metrics(painter.font());

    if (verticalAxisLabelMode_ != VerticalAxisLabelMode::Off) {
        const auto ticks = verticalAxisTicks(verticalAxisLabelMode_, settings, rect().height());
        for (const auto& tick : ticks) {
            const double y =
                timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{0.0, tick.domainValue}).y();
            painter.setPen(QPen(Qt::lightGray, 1));
            painter.drawLine(QPointF(0.0, y), QPointF(kTickLength, y));

            const QRect textBounds = metrics.boundingRect(tick.label);
            const QRectF backing(kTickLength + kTextPadding, y - textBounds.height() / 2.0 - kTextPadding,
                                   textBounds.width() + 2 * kTextPadding, textBounds.height() + 2 * kTextPadding);
            painter.fillRect(backing, kLabelBackground);
            painter.setPen(Qt::lightGray);
            painter.drawText(QPointF(backing.left() + kTextPadding, y + textBounds.height() / 2.0 - metrics.descent()),
                              tick.label);
        }
    }

    if (horizontalAxisLabelMode_ != HorizontalAxisLabelMode::Off) {
        const auto ticks = horizontalAxisTicks(horizontalAxisLabelMode_, settings, rect().width());
        const double bottom = rect().height();
        for (const auto& tick : ticks) {
            const double x =
                timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{tick.domainValue, 0.0}).x();
            painter.setPen(QPen(Qt::lightGray, 1));
            painter.drawLine(QPointF(x, bottom), QPointF(x, bottom - kTickLength));

            const QRect textBounds = metrics.boundingRect(tick.label);
            const QRectF backing(x + kTextPadding, bottom - kTickLength - kTextPadding - textBounds.height(),
                                   textBounds.width() + 2 * kTextPadding, textBounds.height() + 2 * kTextPadding);
            painter.fillRect(backing, kLabelBackground);
            painter.setPen(Qt::lightGray);
            painter.drawText(QPointF(backing.left() + kTextPadding, backing.bottom() - kTextPadding - metrics.descent()),
                              tick.label);
        }
    }
}

void CanvasWidget::drawGrid(QPainter& painter) const {
    if (project_ == nullptr) {
        return;
    }
    const auto& settings = project_->settings();

    if (frequencyGridConfig_.isActive()) {
        painter.setPen(QPen(frequencyGridConfig_.lineColor, frequencyGridConfig_.lineWidthPixels,
                              frequencyGridConfig_.lineStyle));
        for (const double frequencyHz : frequencyGridLinesHz(frequencyGridConfig_, settings)) {
            const double y = timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{0.0, frequencyHz}).y();
            painter.drawLine(QPointF(0.0, y), QPointF(rect().width(), y));
        }
    }

    if (timingGridConfig_.isActive()) {
        const double durationSeconds = static_cast<double>(settings.canvasWidth) * settings.timestepMs / 1000.0;
        painter.setPen(
            QPen(timingGridConfig_.lineColor, timingGridConfig_.lineWidthPixels, timingGridConfig_.lineStyle));
        for (const double seconds : timingGridLinesSeconds(timingGridConfig_, settings, durationSeconds)) {
            const double x = timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{seconds, 0.0}).x();
            painter.drawLine(QPointF(x, 0.0), QPointF(x, rect().height()));
        }
    }
}

void CanvasWidget::drawChordPreview(QPainter& painter) const {
    // Real-world testing pass, 2026-09-20, finding #15: only drawn while
    // the Chord Generator panel is open or the Chord tool is active -
    // either is enough - not unconditionally just because there's data.
    if (project_ == nullptr || chordPreviewFrequenciesHz_.empty() ||
        !(chordGeneratorPanelVisible_ || toolMode_ == ToolMode::ChordStamp)) {
        return;
    }
    // A distinct amber/dashed style from drawGrid()'s own Frequency Grid
    // lines (plain lightGray/solid by default) - see setChordPreview()'s
    // own docs on why this stays a visually separate overlay.
    painter.setPen(QPen(QColor(255, 180, 0), 2.0, Qt::DashLine));
    for (const double frequencyHz : chordPreviewFrequenciesHz_) {
        const double y = timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint{0.0, frequencyHz}).y();
        painter.drawLine(QPointF(0.0, y), QPointF(rect().width(), y));
    }
}

void CanvasWidget::drawOperationOverlays(QPainter& painter) const {
    const sound_mind::core::Layer* layer = findTopmostLayerWithContent(*project_);
    if (layer == nullptr) {
        return;
    }
    const auto operations = project_->operationLog().activeOperationsTargeting(layer->id());

    if (showBoundingBoxes_) {
        painter.setPen(QPen(Qt::cyan, 1));
        for (const sound_mind::core::Operation* operation : operations) {
            painter.drawRect(widgetRectFor(operation->bounds()));
        }
    }

    if (showPathGeometry_) {
        painter.setPen(QPen(Qt::magenta, 1));
        for (const sound_mind::core::Operation* operation : operations) {
            if (const auto* paint = dynamic_cast<const sound_mind::core::PaintOperation*>(operation)) {
                painter.drawPath(toPainterPath(paint->path()));
            }
        }
    }
}

void CanvasWidget::drawPreviewPathNodes(QPainter& painter) const {
    constexpr double kNodeRadius = 3.0;
    constexpr double kSelectedNodeRadius = 5.0;
    constexpr double kHandleRadius = 3.0;
    // How much wider/larger the black halo is drawn than the colored
    // shape it sits behind - see this method's own docs, and
    // paintEvent()'s matching treatment of the path line itself.
    constexpr double kHaloExtra = 1.5;

    // Draws `radius`-sized dot at `point` in `color`, behind a slightly
    // larger solid black one - so it stays visible even directly over a
    // painted region the same color as the dot itself.
    const auto drawHaloDot = [&painter](QPointF point, double radius, const QColor& color) {
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(Qt::black);
        painter.drawEllipse(point, radius + kHaloExtra, radius + kHaloExtra);
        painter.setPen(QPen(color, 1));
        painter.setBrush(color);
        painter.drawEllipse(point, radius, radius);
        painter.setBrush(Qt::NoBrush);
    };
    // Draws a line from `from` to `to` in `color`, behind a wider solid
    // black one, for the same reason.
    const auto drawHaloLine = [&painter](QPointF from, QPointF to, const QColor& color) {
        painter.setPen(QPen(Qt::black, 3));
        painter.drawLine(from, to);
        painter.setPen(QPen(color, 1));
        painter.drawLine(from, to);
    };

    const auto& nodes = paintPreviewPath_.nodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const bool selected = previewSelectedNodeIndex_.has_value() && *previewSelectedNodeIndex_ == i;
        const QPointF anchorPoint = timeFrequencyToWidgetPoint(nodes[i].anchor);

        // The node's own anchor - drawn *before* its own handles below,
        // not after: a freshly-smoothed node's handles start collapsed
        // exactly onto its own anchor (see PathNode's own docs) and stay
        // there until dragged out, so if the anchor drew on top it would
        // fully hide them - the only visible sign Toggle Node Type did
        // anything would be a Corner node's already-absent handles simply
        // staying absent, indistinguishable from nothing having happened.
        drawHaloDot(anchorPoint, selected ? kSelectedNodeRadius : kNodeRadius, selected ? Qt::white : Qt::yellow);

        // Handles - only for the selected node, and only if it's Smooth -
        // see setPreviewSelectedNodeIndex()'s own docs for why.
        if (selected && nodes[i].type == sound_mind::core::PathNodeType::Smooth) {
            if (nodes[i].handleOut.has_value()) {
                const QPointF handlePoint = timeFrequencyToWidgetPoint(*nodes[i].handleOut);
                drawHaloLine(anchorPoint, handlePoint, Qt::cyan);
                drawHaloDot(handlePoint, kHandleRadius, Qt::cyan);
            }
            if (nodes[i].handleIn.has_value()) {
                const QPointF handlePoint = timeFrequencyToWidgetPoint(*nodes[i].handleIn);
                drawHaloLine(anchorPoint, handlePoint, Qt::cyan);
                drawHaloDot(handlePoint, kHandleRadius, Qt::cyan);
            }
        }
    }
}

QPainterPath CanvasWidget::toPainterPath(const sound_mind::core::Path& path) const {
    QPainterPath qPath;
    const auto& nodes = path.nodes();
    if (nodes.empty()) {
        return qPath;
    }
    qPath.moveTo(timeFrequencyToWidgetPoint(nodes.front().anchor));
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
        const auto& start = nodes[i];
        const auto& end = nodes[i + 1];
        const QPointF p1 = timeFrequencyToWidgetPoint(start.handleOut.value_or(start.anchor));
        const QPointF p2 = timeFrequencyToWidgetPoint(end.handleIn.value_or(end.anchor));
        qPath.cubicTo(p1, p2, timeFrequencyToWidgetPoint(end.anchor));
    }
    return qPath;
}

QRectF CanvasWidget::widgetRectFor(const sound_mind::core::TimeFrequencyRect& bounds) const {
    // .normalized() guards against bin 0 mapping to the *top* of the
    // widget (see widgetPointToTimeFrequency()'s own docs) -
    // highFrequencyHz's own corner lands at a numerically *larger* y than
    // lowFrequencyHz's, the opposite of what a plain QRectF(topLeft,
    // bottomRight) construction assumes.
    const QPointF corner1 = timeFrequencyToWidgetPoint(
        sound_mind::core::TimeFrequencyPoint{bounds.startTimeSeconds, bounds.highFrequencyHz});
    const QPointF corner2 = timeFrequencyToWidgetPoint(
        sound_mind::core::TimeFrequencyPoint{bounds.endTimeSeconds, bounds.lowFrequencyHz});
    return QRectF(corner1, corner2).normalized();
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton ||
        (toolMode_ != ToolMode::Paint && toolMode_ != ToolMode::Pick && toolMode_ != ToolMode::Select &&
         toolMode_ != ToolMode::Path && toolMode_ != ToolMode::ChordStamp)) {
        QWidget::mousePressEvent(event);
        return;
    }
    const auto point = widgetPointToTimeFrequency(event->position());
    if (!point.has_value()) {
        return;
    }
    if (toolMode_ == ToolMode::Paint) {
        paintStrokeActive_ = true;
        emit paintStrokeStarted(*point);
    } else if (toolMode_ == ToolMode::Pick) {
        pickStrokeActive_ = true;
        emit pickStrokeStarted(*point);
    } else if (toolMode_ == ToolMode::Select) {
        // A press within the rotate handle's own hit radius takes
        // priority over starting a brand-new selection - the same
        // kHitRadiusPixels convention ToneCurveEditor's own handle
        // hit-testing already established.
        constexpr double kHitRadiusPixels = 8.0;
        if (selectionRotationHandle_.has_value()) {
            const QPointF handleScreen = timeFrequencyToWidgetPoint(*selectionRotationHandle_);
            const QPointF delta = event->position() - handleScreen;
            if (delta.x() * delta.x() + delta.y() * delta.y() <= kHitRadiusPixels * kHitRadiusPixels) {
                rotateHandleDragActive_ = true;
                emit selectionRotateStarted(*point);
                return;
            }
        }
        selectStrokeActive_ = true;
        emit selectStrokeStarted(*point, event->modifiers());
    } else if (toolMode_ == ToolMode::Path) {
        // Path mode: no "active" bookkeeping - see pathNodePlaced()'s own
        // docs for why a single press is the whole gesture.
        emit pathNodePlaced(*point);
    } else {
        // ChordStamp mode: same single-press-is-the-whole-gesture shape as
        // Path above - see chordStampRequested()'s own docs.
        emit chordStampRequested(*point);
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    // Independent of toolMode()/paintStrokeActive_/pickStrokeActive_/
    // selectStrokeActive_ - see cursorMoved()'s own docs; every move gets
    // a position readout, not just ones that also continue an in-progress
    // stroke/drag.
    emit cursorMoved(event->position(), widgetPointToTimeFrequency(event->position()));

    if (toolMode_ == ToolMode::Paint && paintStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit paintStrokeContinued(*point);
        }
        return;
    }
    if (toolMode_ == ToolMode::Pick && pickStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit pickStrokeContinued(*point);
        }
        return;
    }
    if (toolMode_ == ToolMode::Select && rotateHandleDragActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit selectionRotateContinued(*point);
        }
        return;
    }
    if (toolMode_ == ToolMode::Select && selectStrokeActive_) {
        if (const auto point = widgetPointToTimeFrequency(event->position()); point.has_value()) {
            emit selectStrokeContinued(*point);
        }
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CanvasWidget::leaveEvent(QEvent* /*event*/) { emit cursorLeft(); }

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (toolMode_ == ToolMode::Paint && paintStrokeActive_) {
        paintStrokeActive_ = false;
        emit paintStrokeEnded();
        return;
    }
    if (toolMode_ == ToolMode::Pick && pickStrokeActive_) {
        pickStrokeActive_ = false;
        emit pickStrokeEnded();
        return;
    }
    if (toolMode_ == ToolMode::Select && rotateHandleDragActive_) {
        rotateHandleDragActive_ = false;
        emit selectionRotateEnded();
        return;
    }
    if (toolMode_ == ToolMode::Select && selectStrokeActive_) {
        selectStrokeActive_ = false;
        emit selectStrokeEnded();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

std::optional<sound_mind::core::TimeFrequencyPoint> CanvasWidget::widgetPointToTimeFrequency(QPointF point) const {
    if (project_ == nullptr || rect().width() <= 0 || rect().height() <= 0) {
        return std::nullopt;
    }
    const auto& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    // Bin index rises bottom-to-top on screen, not top-to-bottom - see
    // this method's own docs (and color_mapping.cpp's toRgbImage(), the
    // actual rendered image's "row 0 = highest frequency"/highest bin
    // convention this has to match). Flipping here, rather than changing
    // frequencyToBinIndex()/binIndexToFrequency() themselves, keeps those
    // shared with paint_application.cpp's own frequency-domain math,
    // which has no notion of screen pixels at all.
    const double frameIndex = point.x() * static_cast<double>(settings.canvasWidth) / rect().width();
    const double binIndex =
        static_cast<double>(settings.binCount) - (point.y() * static_cast<double>(settings.binCount) / rect().height());

    sound_mind::core::TimeFrequencyPoint result;
    result.timeSeconds = sound_mind::core::frameIndexToTime(frameIndex, config);
    result.frequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(binIndex), config);
    return result;
}

QPointF CanvasWidget::timeFrequencyToWidgetPoint(sound_mind::core::TimeFrequencyPoint point) const {
    if (project_ == nullptr) {
        return QPointF(0.0, 0.0);
    }
    const auto& settings = project_->settings();
    const auto config = sound_mind::core::streamCodecConfigFor(settings);

    const double frameIndex = sound_mind::core::timeToFrameIndex(point.timeSeconds, config);
    const double binIndex = sound_mind::core::frequencyToBinIndex(static_cast<float>(point.frequencyHz), config);

    // The exact inverse of widgetPointToTimeFrequency()'s own flip - see
    // its comment above.
    const double x = frameIndex * rect().width() / static_cast<double>(settings.canvasWidth);
    const double y =
        (static_cast<double>(settings.binCount) - binIndex) * rect().height() / static_cast<double>(settings.binCount);
    return QPointF(x, y);
}

}  // namespace sound_mind::studio
