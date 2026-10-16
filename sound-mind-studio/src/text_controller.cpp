#include "sound_mind/studio/text_controller.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTransform>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/operation_log.h"
#include "sound_mind/core/paint_application.h"
#include "sound_mind/core/project_settings.h"
#include "sound_mind/studio/paint_controller.h"

namespace sound_mind::studio {

TextController::TextController(PaintController* paintController, QObject* parent)
    : QObject(parent), paintController_(paintController) {}

void TextController::setProject(sound_mind::core::Project* project) {
    project_ = project;
    dragStart_.reset();
    box_.reset();
    emit previewChanged();
}

void TextController::setSettings(const TextSettings& settings) {
    if (settings == settings_) {
        return;
    }
    settings_ = settings;
    emit previewChanged();
}

std::pair<int, int> TextController::cellOf(sound_mind::core::TimeFrequencyPoint point) const {
    const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
    return {static_cast<int>(std::lround(sound_mind::core::timeToFrameIndex(point.timeSeconds, config))),
            static_cast<int>(std::lround(sound_mind::core::frequencyToBinIndex(static_cast<float>(point.frequencyHz), config)))};
}

void TextController::beginBox(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point) {
    if (project_ == nullptr) {
        return;
    }
    dragLayer_ = layer;
    dragStart_ = cellOf(point);
    dragEnd_ = *dragStart_;
}

void TextController::continueBox(sound_mind::core::TimeFrequencyPoint point) {
    if (!dragStart_.has_value()) {
        return;
    }
    dragEnd_ = cellOf(point);
    const auto [startFrame, startBin] = *dragStart_;
    const auto [endFrame, endBin] = dragEnd_;
    box_ = TextBox{dragLayer_, std::min(startFrame, endFrame), std::min(startBin, endBin),
                   std::max(kMinimumDragCells, std::abs(endFrame - startFrame) + 1),
                   std::max(kMinimumDragCells, std::abs(endBin - startBin) + 1)};
    emit previewChanged();
}

void TextController::endBox() {
    if (!dragStart_.has_value()) {
        return;
    }
    const auto [startFrame, startBin] = *dragStart_;
    const auto [endFrame, endBin] = dragEnd_;
    dragStart_.reset();
    if (std::abs(endFrame - startFrame) < kMinimumDragCells && std::abs(endBin - startBin) < kMinimumDragCells) {
        // A click: a box the text's own size, centred on it (as the legacy
        // tool did); a sensible placeholder size while the text is blank.
        const QImage glyphs = renderGlyphs(settings_);
        const int width = glyphs.isNull() ? std::max(80, settings_.fontSize * 6) : glyphs.width();
        const int height = glyphs.isNull() ? settings_.fontSize + 16 : glyphs.height();
        box_ = TextBox{dragLayer_, startFrame - width / 2, startBin - height / 2, width, height};
    }
    emit previewChanged();
}

QImage TextController::previewImage() const {
    if (!box_.has_value()) {
        return {};
    }
    const QImage glyphs = renderGlyphs(settings_);
    if (glyphs.isNull()) {
        return {};
    }
    return glyphs.scaled(box_->width, box_->height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

std::optional<sound_mind::core::TimeFrequencyRect> TextController::previewBounds() const {
    if (!box_.has_value() || project_ == nullptr) {
        return std::nullopt;
    }
    const auto config = sound_mind::core::streamCodecConfigFor(project_->settings());
    sound_mind::core::TimeFrequencyRect rect;
    rect.startTimeSeconds = sound_mind::core::frameIndexToTime(static_cast<double>(box_->frameLow), config);
    rect.endTimeSeconds = sound_mind::core::frameIndexToTime(static_cast<double>(box_->frameLow + box_->width - 1), config);
    rect.lowFrequencyHz = sound_mind::core::binIndexToFrequency(static_cast<float>(box_->binLow), config);
    rect.highFrequencyHz =
        sound_mind::core::binIndexToFrequency(static_cast<float>(box_->binLow + box_->height - 1), config);
    return rect;
}

std::optional<sound_mind::core::OperationId> TextController::apply() {
    if (project_ == nullptr || !box_.has_value()) {
        return std::nullopt;
    }
    const sound_mind::core::Layer* layer = project_->layerById(box_->layer);
    if (layer == nullptr || sound_mind::core::isFilterLayerType(layer->type())) {
        return std::nullopt;
    }
    const sound_mind::core::GlyphRaster raster = rasterize(settings_, box_->width, box_->height);
    sound_mind::core::OperationLog& log = project_->operationLog();
    const sound_mind::core::OperationId id = log.reserveId();
    auto paste = sound_mind::core::makeTextPaste(
        id, box_->layer, raster, box_->frameLow, box_->binLow, settings_.leftIntensityDb, settings_.rightIntensityDb,
        settings_.opacity, settings_.blendMode, sound_mind::core::streamCodecConfigFor(project_->settings()));
    if (paste == nullptr) {
        return std::nullopt;
    }
    const sound_mind::core::LayerId target = box_->layer;
    log.append(std::move(paste));
    paintController_->notifyOperationCommitted(tr("Added text"));
    box_.reset();
    paintController_->rebuildLayerContent(target);
    emit contentChanged(target);
    emit previewChanged();
    return id;
}

void TextController::reset() {
    dragStart_.reset();
    if (box_.has_value()) {
        box_.reset();
        emit previewChanged();
    }
}

QImage TextController::renderGlyphs(const TextSettings& settings) {
    if (settings.text.trimmed().isEmpty()) {
        return {};
    }
    QFont font(settings.fontFamily);
    font.setPixelSize(std::max(1, settings.fontSize));
    font.setBold(settings.bold);
    font.setItalic(settings.italic);
    const QFontMetrics metrics(font);
    // The one-argument boundingRect() is relative to the baseline, so its
    // top is negative above it - see the legacy tool's own note on this.
    const QRect bounds = metrics.boundingRect(settings.text);
    const int pad = std::max(4, settings.fontSize / 8);
    QImage image(std::max(1, bounds.width() + 2 * pad), std::max(1, bounds.height() + 2 * pad),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(pad - bounds.left(), pad - bounds.top(), settings.text);
    }
    if (std::abs(settings.rotationDegrees) > 1e-9) {
        // Counterclockwise on screen; Qt's y axis points down.
        image = image.transformed(QTransform().rotate(-settings.rotationDegrees), Qt::SmoothTransformation);
    }
    return image;
}

sound_mind::core::GlyphRaster TextController::rasterize(const TextSettings& settings, int width, int height) {
    sound_mind::core::GlyphRaster raster;
    const QImage glyphs = renderGlyphs(settings);
    if (glyphs.isNull() || width <= 0 || height <= 0) {
        return raster;
    }
    const QImage scaled = glyphs.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_ARGB32);
    raster.width = static_cast<std::uint32_t>(width);
    raster.height = static_cast<std::uint32_t>(height);
    raster.coverage.resize(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(scaled.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            raster.coverage[static_cast<std::size_t>(y) * width + x] = static_cast<float>(qAlpha(line[x])) / 255.0f;
        }
    }
    return raster;
}

}  // namespace sound_mind::studio
