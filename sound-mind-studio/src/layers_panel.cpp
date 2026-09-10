#include "sound_mind/studio/layers_panel.h"

#include <algorithm>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

namespace sound_mind::studio {

namespace {

using sound_mind::core::LayerId;
using sound_mind::core::LayerType;

/// @brief Whether `type` is one of the fixed-position layer types - no
/// drag handle, no delete button (see the class docs).
bool isLocked(LayerType type) { return type == LayerType::Background || type == LayerType::Equalizer; }

QString typeTagText(LayerType type) {
    switch (type) {
        case LayerType::Filter:
            return QObject::tr("Filter");
        case LayerType::Background:
            return QObject::tr("Background");
        case LayerType::Equalizer:
            return QObject::tr("Equalizer");
        case LayerType::Normal:
            break;
    }
    return QString();
}

/// @brief A small "⠿" handle that forwards mouse events to `list`'s
/// viewport, so InternalMove drag-reordering can be initiated from it -
/// the same technique the legacy Studio's own `_DragHandle` used. Without
/// this, a custom item widget (see LayerRowWidget below) swallows mouse
/// events itself and QListWidget never sees them, so nothing would ever
/// be draggable at all.
class DragHandleLabel : public QLabel {
public:
    explicit DragHandleLabel(QListWidget* list, QWidget* parent = nullptr)
        : QLabel(QStringLiteral("⠿"), parent), list_(list) {
        setCursor(Qt::OpenHandCursor);
        setFixedWidth(18);
        setAlignment(Qt::AlignCenter);
        setToolTip(QObject::tr("Drag to reorder"));
    }

protected:
    void mousePressEvent(QMouseEvent* event) override { forward(event); }
    void mouseMoveEvent(QMouseEvent* event) override { forward(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { forward(event); }

private:
    void forward(QMouseEvent* event) {
        QWidget* viewport = list_->viewport();
        // this->mapTo(viewport, ...), not viewport->mapFrom(this, ...) -
        // both take an "other widget" argument that Qt requires to be an
        // *ancestor of the object the method is called on*, and here
        // that's viewport being an ancestor of `this` (true, once
        // setItemWidget() reparents this row under the viewport), not
        // the other way around - the reversed call silently mapped to
        // the wrong point instead of erroring, which is why dragging
        // had no effect at all (the forwarded press/move never landed on
        // any real item, so QListWidget never started a drag).
        const QPoint local = mapTo(viewport, event->pos());
        QMouseEvent forwarded(event->type(), local, viewport->mapToGlobal(local), event->button(), event->buttons(),
                               event->modifiers());
        QApplication::sendEvent(viewport, &forwarded);
    }

    QListWidget* list_;
};

/// @brief A QLabel that emits doubleClicked() - used for a row's name, to
/// trigger renameRequested() the same way the legacy panel's name label did.
class ClickableNameLabel : public QLabel {
    Q_OBJECT

public:
    using QLabel::QLabel;

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        emit doubleClicked();
        QLabel::mouseDoubleClickEvent(event);
    }
};

/// @brief One row's worth of widgets - see the class docs on LayersPanel
/// for the row layout this builds (lock/drag handle, visibility toggle,
/// name, type tag, opacity slider, transform controls, delete) - the
/// opacity/transform controls are omitted entirely for a Background row,
/// see their own construction site below for why.
class LayerRowWidget : public QWidget {
    Q_OBJECT

public:
    LayerRowWidget(const LayersPanel::RowData& data, QListWidget* list, QWidget* parent = nullptr)
        : QWidget(parent), id_(data.id) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(2, 1, 2, 1);
        layout->setSpacing(4);

        const bool locked = isLocked(data.type);

        if (locked) {
            auto* lock = new QLabel(QStringLiteral("\U0001F512"));
            lock->setObjectName(QStringLiteral("lockLabel"));
            lock->setFixedWidth(18);
            lock->setAlignment(Qt::AlignCenter);
            lock->setToolTip(tr("Position locked - cannot be reordered or deleted"));
            layout->addWidget(lock);
        } else {
            auto* handle = new DragHandleLabel(list);
            handle->setObjectName(QStringLiteral("dragHandle"));
            layout->addWidget(handle);
        }

        auto* visibilityButton = new QPushButton(data.visible ? QStringLiteral("●") : QStringLiteral("○"));
        visibilityButton->setObjectName(QStringLiteral("visibilityButton"));
        visibilityButton->setFlat(true);
        visibilityButton->setCheckable(true);
        visibilityButton->setChecked(data.visible);
        visibilityButton->setFixedWidth(22);
        visibilityButton->setToolTip(tr("Toggle layer visibility"));
        if (data.type == LayerType::Background) {
            // Matches the legacy panel: the Background layer's visibility
            // can't be turned off - it's always the floor of the stack.
            visibilityButton->setEnabled(false);
            visibilityButton->setToolTip(tr("Background layer is always visible"));
        } else {
            connect(visibilityButton, &QPushButton::toggled, this, [this, visibilityButton](bool checked) {
                visibilityButton->setText(checked ? QStringLiteral("●") : QStringLiteral("○"));
                emit visibilityToggled(id_, checked);
            });
        }
        layout->addWidget(visibilityButton);

        auto* nameLabel = new ClickableNameLabel(data.name);
        nameLabel->setObjectName(QStringLiteral("nameLabel"));
        nameLabel->setMinimumWidth(60);
        connect(nameLabel, &ClickableNameLabel::doubleClicked, this, [this]() { emit renameRequested(id_); });
        layout->addWidget(nameLabel, 1);

        const QString tagText = typeTagText(data.type);
        if (!tagText.isEmpty()) {
            auto* typeTag = new QLabel(tagText);
            typeTag->setObjectName(QStringLiteral("typeTagLabel"));
            typeTag->setStyleSheet(QStringLiteral("color: #7b9fd4; font-size: 9px;"));
            layout->addWidget(typeTag);
        }

        // Opacity and the Layer Time Alignment transform controls below are
        // omitted entirely (not just disabled) for the Background layer -
        // confirmed with the user: it's always the floor of the stack,
        // always fully opaque, with nothing beneath it to line up against
        // in time, so neither concept applies to it the way it does to
        // every other layer type.
        if (data.type != LayerType::Background) {
            auto* opacitySlider = new QSlider(Qt::Horizontal);
            opacitySlider->setObjectName(QStringLiteral("opacitySlider"));
            opacitySlider->setRange(0, 100);
            opacitySlider->setValue(static_cast<int>(data.opacity * 100.0f));
            opacitySlider->setFixedWidth(60);
            opacitySlider->setToolTip(tr("Layer opacity"));
            connect(opacitySlider, &QSlider::valueChanged, this,
                    [this](int value) { emit opacityChanged(id_, static_cast<float>(value) / 100.0f); });
            layout->addWidget(opacitySlider);

            // Time Alignment (v0.Y.21.1): two per-layer horizontal transform
            // controls - see sound_mind::core::Layer::translationColumns()/
            // rescaleFactor()'s own docs for what each does. QSpinBox's range is
            // a plain `int`, not Layer's `std::int64_t` - a UI-level limit, the
            // same shape as opacitySlider's own float-via-0..100-int range
            // above; a shift of ±2^31 columns (millions of seconds at any
            // realistic hop length) is far beyond anything this control needs
            // to reach.
            auto* translationSpinBox = new QSpinBox();
            translationSpinBox->setObjectName(QStringLiteral("translationSpinBox"));
            translationSpinBox->setRange(-1'000'000, 1'000'000);
            translationSpinBox->setValue(static_cast<int>(data.translationColumns));
            translationSpinBox->setFixedWidth(70);
            translationSpinBox->setToolTip(
                tr("Shift this layer's content earlier/later in time, in spectrogram columns"));
            connect(translationSpinBox, &QSpinBox::valueChanged, this,
                    [this](int value) { emit translationChanged(id_, static_cast<std::int64_t>(value)); });
            layout->addWidget(translationSpinBox);

            auto* rescaleSpinBox = new QDoubleSpinBox();
            rescaleSpinBox->setObjectName(QStringLiteral("rescaleSpinBox"));
            rescaleSpinBox->setRange(0.1, 10.0);
            rescaleSpinBox->setSingleStep(0.05);
            rescaleSpinBox->setDecimals(2);
            rescaleSpinBox->setSuffix(QStringLiteral("x"));
            rescaleSpinBox->setValue(data.rescaleFactor);
            rescaleSpinBox->setFixedWidth(60);
            rescaleSpinBox->setToolTip(tr("Stretch/compress this layer's own timeline"));
            connect(rescaleSpinBox, &QDoubleSpinBox::valueChanged, this,
                    [this](double value) { emit rescaleChanged(id_, value); });
            layout->addWidget(rescaleSpinBox);
        }

        if (!locked) {
            auto* deleteButton = new QPushButton(QStringLiteral("×"));
            deleteButton->setObjectName(QStringLiteral("deleteButton"));
            deleteButton->setFlat(true);
            deleteButton->setFixedWidth(22);
            deleteButton->setStyleSheet(QStringLiteral("color: #c04040;"));
            deleteButton->setToolTip(tr("Delete layer"));
            connect(deleteButton, &QPushButton::clicked, this, [this]() { emit deleteRequested(id_); });
            layout->addWidget(deleteButton);
        }
    }

signals:
    void visibilityToggled(sound_mind::core::LayerId id, bool visible);
    void opacityChanged(sound_mind::core::LayerId id, float opacity);
    void translationChanged(sound_mind::core::LayerId id, std::int64_t translationColumns);
    void rescaleChanged(sound_mind::core::LayerId id, double rescaleFactor);
    void renameRequested(sound_mind::core::LayerId id);
    void deleteRequested(sound_mind::core::LayerId id);

private:
    sound_mind::core::LayerId id_;
};

}  // namespace

LayersPanel::LayersPanel(QWidget* parent) : QDockWidget(tr("Layers"), parent) {
    // DockWidgetClosable included, unlike this dock's original feature set
    // (Movable | Floatable only) - a real bug, found via manual testing:
    // QDockWidget's toggleViewAction() can flip its own checked state
    // freely either way, but only actually hides the dock (calls hide())
    // when DockWidgetClosable is one of its features - without it, the
    // toolbar's Layers toggle button visibly changes state but the panel
    // itself never responds. Playback/Record/Loop's own panels never call
    // setFeatures() at all, keeping Qt's default full feature set
    // (Closable included), which is exactly why their own toolbar toggles
    // already worked correctly before this fix.
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(260);

    list_ = new QListWidget();
    list_->setObjectName(QStringLiteral("layersList"));
    list_->setSelectionMode(QListWidget::SingleSelection);
    list_->setDragEnabled(true);
    list_->setAcceptDrops(true);
    list_->setDropIndicatorShown(true);
    list_->setDragDropMode(QListWidget::InternalMove);
    connect(list_->model(), &QAbstractItemModel::rowsMoved, this, &LayersPanel::handleRowsMoved);

    setWidget(list_);
}

void LayersPanel::setLayers(const std::vector<RowData>& layersBottomToTop) {
    currentRows_ = layersBottomToTop;

    // QListWidget::clear() deletes the QListWidgetItems but *not* the
    // LayerRowWidgets set via setItemWidget() on them (a real, easy-to-miss
    // Qt gotcha - an item widget is reparented to the viewport internally,
    // separately from the item itself) - without this, every setLayers()
    // call after the first would leak the previous rows' widgets, which
    // would then keep showing up alongside the new ones.
    //
    // deleteLater(), not delete: setLayers() is commonly called *from*
    // one of these very rows' own signal handlers (MainWindow's
    // renameLayerTo()/setLayerOpacity()/etc. all call it via
    // refreshLayersPanel(), reached via that row's own emitted signal) -
    // a plain delete would destroy the widget still further up the very
    // call stack currently running one of its own event handlers
    // (QSlider::valueChanged's internal bookkeeping, or this file's own
    // ClickableNameLabel::mouseDoubleClickEvent() touching `this` again
    // right after emitting doubleClicked()) - a real, reproduced crash,
    // not a theoretical one. deleteLater() defers the actual destruction
    // to the next trip through the event loop, once nothing is still
    // executing on top of it.
    for (int i = 0; i < list_->count(); ++i) {
        if (QWidget* rowWidget = list_->itemWidget(list_->item(i))) {
            rowWidget->deleteLater();
        }
    }
    list_->clear();
    for (auto it = layersBottomToTop.rbegin(); it != layersBottomToTop.rend(); ++it) {
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(it->id)));
        item->setSizeHint(QSize(0, 32));
        if (isLocked(it->type)) {
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        }
        list_->addItem(item);
        list_->setItemWidget(item, new LayerRowWidget(*it, list_));

        auto* row = qobject_cast<LayerRowWidget*>(list_->itemWidget(item));
        connect(row, &LayerRowWidget::visibilityToggled, this, &LayersPanel::visibilityToggled);
        connect(row, &LayerRowWidget::opacityChanged, this, &LayersPanel::opacityChanged);
        connect(row, &LayerRowWidget::translationChanged, this, &LayersPanel::translationChanged);
        connect(row, &LayerRowWidget::rescaleChanged, this, &LayersPanel::rescaleChanged);
        connect(row, &LayerRowWidget::renameRequested, this, &LayersPanel::renameRequested);
        connect(row, &LayerRowWidget::deleteRequested, this, &LayersPanel::deleteRequested);
    }
}

void LayersPanel::handleRowsMoved() {
    std::vector<sound_mind::core::LayerId> newOrderTopToBottom;
    newOrderTopToBottom.reserve(static_cast<std::size_t>(list_->count()));
    for (int i = 0; i < list_->count(); ++i) {
        newOrderTopToBottom.push_back(static_cast<sound_mind::core::LayerId>(list_->item(i)->data(Qt::UserRole).toULongLong()));
    }

    // Bottom-to-top, matching setLayers()'s/reorderRequested()'s own
    // convention.
    std::vector<sound_mind::core::LayerId> newOrderBottomToTop(newOrderTopToBottom.rbegin(),
                                                                 newOrderTopToBottom.rend());

    // Reject the drag if it moved a locked layer away from the fixed
    // position it had in currentRows_ - see reorderRequested()'s docs.
    bool valid = true;
    for (std::size_t i = 0; i < currentRows_.size() && valid; ++i) {
        if (!isLocked(currentRows_[i].type)) {
            continue;
        }
        if (i >= newOrderBottomToTop.size() || newOrderBottomToTop[i] != currentRows_[i].id) {
            valid = false;
        }
    }

    if (!valid) {
        setLayers(currentRows_);  // snap back to the last known-good order.
        return;
    }

    // Update currentRows_ to the new order (same RowData, reordered) so a
    // *subsequent* invalid drag still has the right snap-back target,
    // without waiting for the caller's own setLayers() round-trip.
    std::vector<RowData> reordered;
    reordered.reserve(currentRows_.size());
    for (const sound_mind::core::LayerId id : newOrderBottomToTop) {
        auto found = std::find_if(currentRows_.begin(), currentRows_.end(), [id](const RowData& row) { return row.id == id; });
        if (found != currentRows_.end()) {
            reordered.push_back(*found);
        }
    }
    currentRows_ = std::move(reordered);

    emit reorderRequested(newOrderBottomToTop);
}

}  // namespace sound_mind::studio

#include "layers_panel.moc"
