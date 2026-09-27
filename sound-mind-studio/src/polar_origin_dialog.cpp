#include "sound_mind/studio/polar_origin_dialog.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

namespace sound_mind::studio {

namespace {

constexpr double kTwoPi = 6.283185307179586;

/// @brief How close (px) a press needs to land to a handle to grab it -
/// the same constant `ToneCurveEditor`'s/`CanvasWidget`'s own rotate-
/// handle hit-testing already uses.
constexpr double kHitRadiusPixels = 8.0;

/// @brief Drawn radius (px) of the origin/ring handles' own dots.
constexpr double kHandleDotRadius = 5.0;

/// @brief Drawn half-side (px) of each arc handle's own square.
constexpr double kArcHandleHalfSize = 6.0;

/// @brief Draws `shape` twice - a wide black "halo" stroke first, then a
/// narrower colored one on top - so it stays visible over any image
/// content, regardless of the image's own colors. The same technique
/// `CanvasWidget`'s own live paint-stroke preview and path-node handles
/// already use.
template <typename DrawFn>
void drawWithHalo(QPainter& painter, const QColor& color, qreal width, DrawFn draw) {
    painter.setPen(QPen(Qt::black, width + 2.0));
    draw();
    painter.setPen(QPen(color, width));
    draw();
}

}  // namespace

PolarOriginPickerWidget::PolarOriginPickerWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(160, 160);
}

QSize PolarOriginPickerWidget::sizeHint() const { return {240, 240}; }

void PolarOriginPickerWidget::setSourceImage(QImage image) {
    sourceImage_ = std::move(image);
    if (!sourceImage_.isNull() && sourceImage_.width() > 0 && sourceImage_.height() > 0) {
        originX_ = sourceImage_.width() / 2.0;
        originY_ = sourceImage_.height() / 2.0;
        radius_ = std::min(sourceImage_.width(), sourceImage_.height()) / 2.0 * 0.9;
    } else {
        originX_ = 0.0;
        originY_ = 0.0;
        radius_ = 1.0;
    }
    arcStartRadians_ = 0.0;
    arcEndRadians_ = 0.0;
    dragging_ = Handle::None;
    update();
}

void PolarOriginPickerWidget::setOriginX(double x) {
    originX_ = sourceImage_.isNull() ? x : std::clamp(x, 0.0, static_cast<double>(sourceImage_.width()));
    update();
}

void PolarOriginPickerWidget::setOriginY(double y) {
    originY_ = sourceImage_.isNull() ? y : std::clamp(y, 0.0, static_cast<double>(sourceImage_.height()));
    update();
}

void PolarOriginPickerWidget::setRadius(double radius) {
    radius_ = std::max(1.0, radius);
    update();
}

void PolarOriginPickerWidget::setArcStartRadians(double theta) {
    arcStartRadians_ = std::fmod(std::fmod(theta, kTwoPi) + kTwoPi, kTwoPi);
    update();
}

void PolarOriginPickerWidget::setArcEndRadians(double theta) {
    arcEndRadians_ = std::fmod(std::fmod(theta, kTwoPi) + kTwoPi, kTwoPi);
    update();
}

double PolarOriginPickerWidget::imageToWidgetScale() const {
    if (sourceImage_.isNull() || sourceImage_.width() <= 0 || sourceImage_.height() <= 0) {
        return 1.0;
    }
    const double scaleX = static_cast<double>(width()) / sourceImage_.width();
    const double scaleY = static_cast<double>(height()) / sourceImage_.height();
    return std::min(scaleX, scaleY);
}

QPointF PolarOriginPickerWidget::imageOrigin() const {
    if (sourceImage_.isNull()) {
        return QPointF(0.0, 0.0);
    }
    const double scale = imageToWidgetScale();
    return QPointF((width() - sourceImage_.width() * scale) / 2.0, (height() - sourceImage_.height() * scale) / 2.0);
}

QPointF PolarOriginPickerWidget::imageToWidget(QPointF imagePoint) const {
    const double scale = imageToWidgetScale();
    const QPointF origin = imageOrigin();
    return QPointF(origin.x() + imagePoint.x() * scale, origin.y() + imagePoint.y() * scale);
}

QPointF PolarOriginPickerWidget::widgetToImage(QPointF widgetPoint) const {
    const double scale = imageToWidgetScale();
    if (scale <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    const QPointF origin = imageOrigin();
    return QPointF((widgetPoint.x() - origin.x()) / scale, (widgetPoint.y() - origin.y()) / scale);
}

QPointF PolarOriginPickerWidget::arcHandleImagePos(double theta) const {
    return QPointF(originX_ + radius_ * std::sin(theta), originY_ - radius_ * std::cos(theta));
}

PolarOriginPickerWidget::Handle PolarOriginPickerWidget::hitTest(QPointF widgetPoint) const {
    const auto distance = [](QPointF a, QPointF b) {
        const double dx = a.x() - b.x();
        const double dy = a.y() - b.y();
        return std::sqrt(dx * dx + dy * dy);
    };

    if (distance(widgetPoint, imageToWidget(QPointF(originX_, originY_))) <= kHitRadiusPixels) {
        return Handle::Origin;
    }
    if (distance(widgetPoint, imageToWidget(arcHandleImagePos(arcStartRadians_))) <= kHitRadiusPixels) {
        return Handle::ArcStart;
    }
    // A full-circle end handle (arcEndRadians_ == arcStartRadians_) would
    // otherwise sit exactly on top of the start handle, making it
    // impossible to grab independently - nudged fractionally short of a
    // full turn instead, matching the legacy Studio's own identical
    // `2*pi - 1e-6` treatment for this same case.
    const bool fullCircle = std::abs(arcEndRadians_ - arcStartRadians_) < 1e-9;
    const double effectiveArcEnd = fullCircle ? (kTwoPi - 1e-6) : arcEndRadians_;
    if (distance(widgetPoint, imageToWidget(arcHandleImagePos(effectiveArcEnd))) <= kHitRadiusPixels) {
        return Handle::ArcEnd;
    }
    if (distance(widgetPoint, imageToWidget(QPointF(originX_, originY_ - radius_))) <= kHitRadiusPixels) {
        return Handle::Ring;
    }
    return Handle::None;
}

void PolarOriginPickerWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());

    if (!sourceImage_.isNull()) {
        const double scale = imageToWidgetScale();
        const QPointF origin = imageOrigin();
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRectF(origin, QSizeF(sourceImage_.width() * scale, sourceImage_.height() * scale)),
                           sourceImage_);
    } else {
        painter.fillRect(rect().adjusted(4, 4, -4, -4), QColor(60, 60, 60));
    }

    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF originWidget = imageToWidget(QPointF(originX_, originY_));

    // Crosshair - full-width/height lines through the origin, per this
    // class's own docs.
    drawWithHalo(painter, QColor(255, 255, 0), 1.5, [&]() {
        painter.drawLine(QPointF(originWidget.x(), 0), QPointF(originWidget.x(), height()));
        painter.drawLine(QPointF(0, originWidget.y()), QPointF(width(), originWidget.y()));
    });

    // Sampling ring.
    const double scale = imageToWidgetScale();
    const double radiusWidget = radius_ * scale;
    drawWithHalo(painter, QColor(255, 200, 0), 1.5, [&]() {
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(originWidget, radiusWidget, radiusWidget);
    });

    // Arc sector - only drawn when it's not a full circle, per this
    // class's own docs.
    const bool fullCircle = std::abs(arcEndRadians_ - arcStartRadians_) < 1e-9;
    if (!fullCircle) {
        const double arcSpan = std::fmod(arcEndRadians_ - arcStartRadians_ + kTwoPi, kTwoPi);
        // Qt's own drawArc() angles: 1/16th-degree units, 0 = three
        // o'clock, counter-clockwise positive - the opposite winding
        // direction from this class's own clockwise-from-twelve
        // convention, so both the starting angle and the span itself
        // need converting.
        const double startDegrees = 90.0 - qRadiansToDegrees(arcStartRadians_);
        const double spanDegrees = -qRadiansToDegrees(arcSpan);
        painter.setPen(QPen(QColor(255, 120, 0), 2.0));
        painter.drawArc(QRectF(originWidget.x() - radiusWidget, originWidget.y() - radiusWidget, radiusWidget * 2.0,
                                radiusWidget * 2.0),
                         static_cast<int>(startDegrees * 16.0), static_cast<int>(spanDegrees * 16.0));
        painter.drawLine(originWidget, imageToWidget(arcHandleImagePos(arcStartRadians_)));
        painter.drawLine(originWidget, imageToWidget(arcHandleImagePos(arcEndRadians_)));
    }

    // Arc handles - two distinct colors so start and end are always
    // individually identifiable, even where they nearly coincide.
    const auto drawArcHandle = [&](double theta, const QColor& color) {
        const QPointF point = imageToWidget(arcHandleImagePos(theta));
        painter.setPen(QPen(Qt::black, 1.5));
        painter.setBrush(color);
        painter.drawRect(QRectF(point.x() - kArcHandleHalfSize, point.y() - kArcHandleHalfSize,
                                 kArcHandleHalfSize * 2.0, kArcHandleHalfSize * 2.0));
    };
    drawArcHandle(arcStartRadians_, QColor(80, 200, 255));
    drawArcHandle(fullCircle ? (kTwoPi - 1e-6) : arcEndRadians_, QColor(255, 100, 180));

    // Ring handle (top of the ring, at theta=0 - where a radius drag
    // starts from).
    painter.setPen(QPen(Qt::black, 1.5));
    painter.setBrush(QColor(200, 255, 100));
    const QPointF ringHandlePoint = imageToWidget(QPointF(originX_, originY_ - radius_));
    painter.drawEllipse(ringHandlePoint, kHandleDotRadius, kHandleDotRadius);

    // Origin handle - drawn last, on top of everything else, since it's
    // the handle dragged most often.
    painter.setPen(QPen(Qt::black, 1.5));
    painter.setBrush(QColor(255, 255, 0));
    painter.drawEllipse(originWidget, kHandleDotRadius, kHandleDotRadius);
}

void PolarOriginPickerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = hitTest(event->position());
    }
}

void PolarOriginPickerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ == Handle::None) {
        setCursor(hitTest(event->position()) != Handle::None ? Qt::SizeAllCursor : Qt::ArrowCursor);
        return;
    }

    QPointF imagePoint = widgetToImage(event->position());
    if (!sourceImage_.isNull()) {
        imagePoint.setX(std::clamp(imagePoint.x(), 0.0, static_cast<double>(sourceImage_.width())));
        imagePoint.setY(std::clamp(imagePoint.y(), 0.0, static_cast<double>(sourceImage_.height())));
    }

    switch (dragging_) {
        case Handle::Origin:
            originX_ = imagePoint.x();
            originY_ = imagePoint.y();
            break;
        case Handle::Ring:
            // Only the vertical distance from the origin sets the radius
            // - the ring handle is always dragged from directly above it
            // (theta=0) - matching the legacy Studio's own identical
            // "drag straight down/up to resize" interaction.
            radius_ = std::max(1.0, std::abs(imagePoint.y() - originY_));
            break;
        case Handle::ArcStart:
        case Handle::ArcEnd: {
            double theta = std::atan2(imagePoint.x() - originX_, -(imagePoint.y() - originY_));
            if (theta < 0.0) {
                theta += kTwoPi;
            }
            (dragging_ == Handle::ArcStart ? arcStartRadians_ : arcEndRadians_) = theta;
            break;
        }
        case Handle::None:
            break;
    }

    emit paramsChangedByDrag();
    update();
}

void PolarOriginPickerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = Handle::None;
    }
}

PolarOriginDialog::PolarOriginDialog(QImage sourceImage, std::optional<PolarImportParams> initialParams,
                                     std::uint32_t defaultOutputWidth, double timestepMs, QWidget* parent)
    : QDialog(parent), timestepMs_(timestepMs) {
    setWindowTitle(tr("Set Polar Origin"));
    setMinimumSize(560, 480);

    auto* root = new QVBoxLayout(this);

    picker_ = new PolarOriginPickerWidget(this);
    picker_->setObjectName(QStringLiteral("picker"));
    picker_->setSourceImage(std::move(sourceImage));
    if (initialParams.has_value()) {
        picker_->setOriginX(initialParams->originX);
        picker_->setOriginY(initialParams->originY);
        picker_->setRadius(initialParams->radius);
        picker_->setArcStartRadians(initialParams->arcStartRadians);
        picker_->setArcEndRadians(initialParams->arcEndRadians);
        // Resumes from the previously-chosen output width, not
        // `defaultOutputWidth` (the project's own canvas width) - a user
        // who deliberately changed it away from that default shouldn't
        // have their own choice silently reset just for reopening this
        // dialog to tweak something else.
        defaultOutputWidth = initialParams->outputWidth;
    }
    outputWidthTracksRadius_ = defaultOutputWidth == 0;
    root->addWidget(picker_, 1);
    connect(picker_, &PolarOriginPickerWidget::paramsChangedByDrag, this, &PolarOriginDialog::syncSpinboxesFromWidget);

    auto* formRow = new QHBoxLayout();
    root->addLayout(formRow);

    auto* leftForm = new QFormLayout();
    leftForm->setLabelAlignment(Qt::AlignRight);

    originXSpinBox_ = new QDoubleSpinBox(this);
    originXSpinBox_->setObjectName(QStringLiteral("originXSpinBox"));
    originXSpinBox_->setRange(0.0, 99999.0);
    originXSpinBox_->setDecimals(1);
    originXSpinBox_->setSuffix(tr(" px"));
    connect(originXSpinBox_, &QDoubleSpinBox::valueChanged, this, &PolarOriginDialog::syncWidgetFromSpinboxes);
    leftForm->addRow(tr("Origin X:"), originXSpinBox_);

    originYSpinBox_ = new QDoubleSpinBox(this);
    originYSpinBox_->setObjectName(QStringLiteral("originYSpinBox"));
    originYSpinBox_->setRange(0.0, 99999.0);
    originYSpinBox_->setDecimals(1);
    originYSpinBox_->setSuffix(tr(" px"));
    connect(originYSpinBox_, &QDoubleSpinBox::valueChanged, this, &PolarOriginDialog::syncWidgetFromSpinboxes);
    leftForm->addRow(tr("Origin Y:"), originYSpinBox_);

    radiusSpinBox_ = new QDoubleSpinBox(this);
    radiusSpinBox_->setObjectName(QStringLiteral("radiusSpinBox"));
    radiusSpinBox_->setRange(1.0, 99999.0);
    radiusSpinBox_->setDecimals(1);
    radiusSpinBox_->setSuffix(tr(" px"));
    connect(radiusSpinBox_, &QDoubleSpinBox::valueChanged, this, &PolarOriginDialog::syncWidgetFromSpinboxes);
    leftForm->addRow(tr("Radius:"), radiusSpinBox_);

    arcStartSpinBox_ = new QDoubleSpinBox(this);
    arcStartSpinBox_->setObjectName(QStringLiteral("arcStartSpinBox"));
    arcStartSpinBox_->setRange(0.0, 359.9);
    arcStartSpinBox_->setDecimals(1);
    arcStartSpinBox_->setSuffix(QStringLiteral("°"));
    arcStartSpinBox_->setToolTip(tr("Arc start angle (0 = 12 o'clock, clockwise)"));
    connect(arcStartSpinBox_, &QDoubleSpinBox::valueChanged, this, &PolarOriginDialog::syncWidgetFromSpinboxes);
    leftForm->addRow(tr("Arc start:"), arcStartSpinBox_);

    arcEndSpinBox_ = new QDoubleSpinBox(this);
    arcEndSpinBox_->setObjectName(QStringLiteral("arcEndSpinBox"));
    arcEndSpinBox_->setRange(0.0, 360.0);
    arcEndSpinBox_->setDecimals(1);
    arcEndSpinBox_->setSuffix(QStringLiteral("°"));
    arcEndSpinBox_->setToolTip(tr("Arc end angle (360 = full circle)"));
    arcEndSpinBox_->setValue(360.0);
    connect(arcEndSpinBox_, &QDoubleSpinBox::valueChanged, this, &PolarOriginDialog::syncWidgetFromSpinboxes);
    leftForm->addRow(tr("Arc end:"), arcEndSpinBox_);

    formRow->addLayout(leftForm);
    formRow->addSpacing(20);

    auto* rightForm = new QFormLayout();
    rightForm->setLabelAlignment(Qt::AlignRight);

    outputWidthSpinBox_ = new QSpinBox(this);
    outputWidthSpinBox_->setObjectName(QStringLiteral("outputWidthSpinBox"));
    outputWidthSpinBox_->setRange(1, 65535);
    outputWidthSpinBox_->setSuffix(tr(" px"));
    outputWidthSpinBox_->setToolTip(
        tr("Width of the resulting rectangular layer (default: the project's own canvas width, else "
           "approximately 2*pi times the radius)"));
    connect(outputWidthSpinBox_, &QSpinBox::valueChanged, this, [this](int) {
        outputWidthTracksRadius_ = false;
        updateDurationLabel();
    });
    rightForm->addRow(tr("Output width:"), outputWidthSpinBox_);

    durationLabel_ = new QLabel(QStringLiteral("—"), this);
    durationLabel_->setObjectName(QStringLiteral("durationLabel"));
    durationLabel_->setStyleSheet(QStringLiteral("color: gray;"));
    rightForm->addRow(QString(), durationLabel_);

    formRow->addLayout(rightForm);
    formRow->addStretch();

    const std::uint32_t initialOutputWidth =
        defaultOutputWidth > 0 ? defaultOutputWidth
                                : static_cast<std::uint32_t>(std::max(1.0, kTwoPi * picker_->radius()));
    {
        // Blocked - this is this constructor's own initial value, not a
        // real user edit, so it must not trip the "stop tracking the
        // radius" lambda above the same way a genuine typed-in value
        // would (see outputWidthTracksRadius_'s own docs).
        const QSignalBlocker blockWidth(outputWidthSpinBox_);
        outputWidthSpinBox_->setValue(static_cast<int>(initialOutputWidth));
    }

    syncSpinboxesFromWidget();
    updateDurationLabel();

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);
}

void PolarOriginDialog::syncSpinboxesFromWidget() {
    {
        const QSignalBlocker blockX(originXSpinBox_);
        originXSpinBox_->setValue(picker_->originX());
    }
    {
        const QSignalBlocker blockY(originYSpinBox_);
        originYSpinBox_->setValue(picker_->originY());
    }
    {
        const QSignalBlocker blockR(radiusSpinBox_);
        radiusSpinBox_->setValue(picker_->radius());
    }
    {
        const QSignalBlocker blockStart(arcStartSpinBox_);
        arcStartSpinBox_->setValue(qRadiansToDegrees(picker_->arcStartRadians()));
    }
    {
        const QSignalBlocker blockEnd(arcEndSpinBox_);
        const bool fullCircle = std::abs(picker_->arcEndRadians() - picker_->arcStartRadians()) < 1e-9;
        arcEndSpinBox_->setValue(fullCircle ? 360.0 : qRadiansToDegrees(picker_->arcEndRadians()));
    }

    updateOutputWidthFromRadius();
    updateDurationLabel();
}

void PolarOriginDialog::syncWidgetFromSpinboxes() {
    picker_->setOriginX(originXSpinBox_->value());
    picker_->setOriginY(originYSpinBox_->value());
    picker_->setRadius(radiusSpinBox_->value());
    picker_->setArcStartRadians(qDegreesToRadians(arcStartSpinBox_->value()));
    const double arcEndDegrees = arcEndSpinBox_->value();
    picker_->setArcEndRadians(arcEndDegrees >= 360.0 ? 0.0 : qDegreesToRadians(arcEndDegrees));
    // The radius spinbox is one of the ones just applied above - a direct
    // edit of it needs the same "still tracking?" re-check
    // syncSpinboxesFromWidget()'s own drag-driven path already gets, or
    // typing a new radius directly would never re-derive the output width
    // hint at all (only a drag on the picker widget would).
    updateOutputWidthFromRadius();
    updateDurationLabel();
}

void PolarOriginDialog::updateOutputWidthFromRadius() {
    // Auto-updates the output width hint as the radius changes, for as
    // long as the user hasn't edited it directly - matching the legacy
    // Studio's own identical "only when no project width is set"
    // precedent (here: only until the spinbox's own valueChanged() fires
    // from a real edit, which clears outputWidthTracksRadius_).
    if (!outputWidthTracksRadius_) {
        return;
    }
    const QSignalBlocker blockWidth(outputWidthSpinBox_);
    outputWidthSpinBox_->setValue(static_cast<int>(std::max(1.0, kTwoPi * picker_->radius())));
}

void PolarOriginDialog::updateDurationLabel() {
    if (timestepMs_ <= 0.0) {
        durationLabel_->setText(QStringLiteral("—"));
        return;
    }
    const double totalMs = outputWidthSpinBox_->value() * timestepMs_;
    const double totalSeconds = totalMs / 1000.0;
    const int minutes = static_cast<int>(totalSeconds) / 60;
    const double seconds = totalSeconds - minutes * 60.0;
    durationLabel_->setText(QStringLiteral("%1:%2")
                                 .arg(minutes)
                                 .arg(seconds, 4, 'f', 1, QChar('0')));
}

PolarImportParams PolarOriginDialog::params() const {
    PolarImportParams result;
    result.originX = picker_->originX();
    result.originY = picker_->originY();
    result.radius = picker_->radius();
    result.arcStartRadians = picker_->arcStartRadians();
    result.arcEndRadians = picker_->arcEndRadians();
    result.outputWidth = static_cast<std::uint32_t>(outputWidthSpinBox_->value());
    return result;
}

}  // namespace sound_mind::studio
