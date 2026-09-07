#include "sound_mind/studio/canvas_widget.h"

#include <optional>

#include <QImage>
#include <QPainter>
#include <QPaintEvent>

#include "sound_mind/core/compositor.h"

namespace sound_mind::studio {

namespace {
const QSize kFallbackSize(400, 300);

/// @brief The last layer (top of the stack) with cached content, if any -
/// see CanvasWidget's docs for why "last with content" stands in for a
/// real composite for now.
[[nodiscard]] std::optional<sound_mind::codec::RgbImage> findTopmostRender(const sound_mind::core::Project& project) {
    const auto& layers = project.layers();
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (auto rendered = sound_mind::core::renderLayer(*it); rendered.has_value()) {
            return rendered;
        }
    }
    return std::nullopt;
}

/// @brief Wraps an RgbImage's pixel data as a QImage, valid only as long as
/// the RgbImage itself stays alive - no copy is made.
[[nodiscard]] QImage toQImage(const sound_mind::codec::RgbImage& image) {
    return QImage(image.pixels.data(), static_cast<int>(image.width), static_cast<int>(image.height),
                  static_cast<int>(image.width) * 3, QImage::Format_RGB888);
}

}  // namespace

CanvasWidget::CanvasWidget(QWidget* parent) : QWidget(parent) {}

void CanvasWidget::setProject(const sound_mind::core::Project* project) {
    project_ = project;
    updateGeometry();
    update();
}

QSize CanvasWidget::sizeHint() const {
    if (project_ == nullptr) {
        return kFallbackSize;
    }
    return QSize(static_cast<int>(project_->settings().canvasWidth),
                 static_cast<int>(project_->settings().canvasHeight));
}

void CanvasWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (project_ == nullptr) {
        return;
    }

    if (const auto rendered = findTopmostRender(*project_); rendered.has_value()) {
        painter.drawImage(rect(), toQImage(*rendered));
        return;
    }

    // No layer has any content yet - fall back to the placeholder that
    // stood in for the whole canvas before Import existed.
    const QRect canvasRect(0, 0, static_cast<int>(project_->settings().canvasWidth),
                            static_cast<int>(project_->settings().canvasHeight));
    painter.fillRect(canvasRect.intersected(rect()), QColor(40, 40, 40));
    painter.setPen(Qt::darkGray);
    painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));
}

}  // namespace sound_mind::studio
