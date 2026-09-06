#include "sound_mind/studio/canvas_widget.h"

#include <QPainter>
#include <QPaintEvent>

namespace sound_mind::studio {

namespace {
const QSize kFallbackSize(400, 300);
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

    const QRect canvasRect(0, 0, static_cast<int>(project_->settings().canvasWidth),
                            static_cast<int>(project_->settings().canvasHeight));
    painter.fillRect(canvasRect.intersected(rect()), QColor(40, 40, 40));
    painter.setPen(Qt::darkGray);
    painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));
}

}  // namespace sound_mind::studio
