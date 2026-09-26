#include "sound_mind/studio/composer_panel.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

namespace sound_mind::studio {

namespace {

constexpr int kTrackBodyHeight = 64;

/// @brief The actual painted body of one track row: the selected
/// background image (if any), stretched to fill, plus every operation
/// box on top - separated from `ComposerTrackWidget` below so the header
/// strip (name label, style combo) never gets painted over.
class ComposerTrackBody : public QWidget {
public:
    explicit ComposerTrackBody(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(kTrackBodyHeight);
        setMaximumHeight(kTrackBodyHeight);
    }

    void setBackground(std::optional<QImage> image) {
        background_ = std::move(image);
        update();
    }

    void setOperations(std::vector<ComposerPanel::OperationBox> operations) {
        operations_ = std::move(operations);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.fillRect(rect(), palette().dark().color());

        if (background_.has_value()) {
            painter.drawImage(rect(), *background_);
        }

        // Every active operation targeting this track's own layer, as a
        // translucent cyan-bordered box - the same "operation geometry"
        // color the canvas's own overlays already use (CanvasWidget's
        // Pick/path handle halos), for visual consistency.
        painter.setPen(QPen(Qt::cyan, 1));
        painter.setBrush(QColor(0, 255, 255, 60));
        const int trackWidth = width();
        const int trackHeight = height();
        for (const ComposerPanel::OperationBox& box : operations_) {
            const int left = static_cast<int>(std::lround(box.startFraction * trackWidth));
            const int right = static_cast<int>(std::lround(box.endFraction * trackWidth));
            painter.drawRect(QRect(left, 1, std::max(1, right - left), trackHeight - 2));
        }
    }

private:
    std::optional<QImage> background_;
    std::vector<ComposerPanel::OperationBox> operations_;
};

/// @brief One track row: a name/background-style header strip over a
/// `ComposerTrackBody`. Purely presentational - see `ComposerPanel`'s own
/// class docs on why background-style selection lives entirely here,
/// never round-tripped through `MainWindow`.
class ComposerTrackWidget : public QWidget {
public:
    explicit ComposerTrackWidget(const ComposerPanel::TrackData& data, QWidget* parent = nullptr)
        : QWidget(parent), id_(data.id), amplitudeImage_(data.amplitudeImage), thumbnailImage_(data.thumbnailImage) {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(4, 4, 4, 4);
        outer->setSpacing(2);

        auto* header = new QHBoxLayout();
        header->addWidget(new QLabel(data.name));
        header->addStretch(1);

        styleCombo_ = new QComboBox();
        styleCombo_->setObjectName(QStringLiteral("backgroundStyleCombo"));
        styleCombo_->addItem(QObject::tr("Clean"));
        styleCombo_->addItem(QObject::tr("Amplitude"));
        styleCombo_->addItem(QObject::tr("Thumbnail"));
        styleCombo_->setToolTip(
            QObject::tr("This track's own background - a purely local display choice, not saved with the project"));
        connect(styleCombo_, &QComboBox::currentIndexChanged, this, [this](int) { applyCurrentStyle(); });
        header->addWidget(styleCombo_);
        outer->addLayout(header);

        body_ = new ComposerTrackBody();
        outer->addWidget(body_);

        body_->setOperations(data.operations);
        applyCurrentStyle();
    }

    sound_mind::core::LayerId id() const noexcept { return id_; }

    /// @brief The background style this row's own combo is currently
    ///        showing - `ComposerPanel::setTracks()`'s own way of
    ///        preserving it across a refresh.
    int styleIndex() const { return styleCombo_->currentIndex(); }

    void setStyleIndex(int index) {
        const QSignalBlocker blocker(styleCombo_);
        styleCombo_->setCurrentIndex(index);
        applyCurrentStyle();
    }

private:
    void applyCurrentStyle() {
        switch (styleCombo_->currentIndex()) {
            case 1:
                body_->setBackground(amplitudeImage_);
                break;
            case 2:
                body_->setBackground(thumbnailImage_);
                break;
            case 0:
            default:
                body_->setBackground(std::nullopt);
                break;
        }
    }

    sound_mind::core::LayerId id_;
    std::optional<QImage> amplitudeImage_;
    std::optional<QImage> thumbnailImage_;
    QComboBox* styleCombo_ = nullptr;
    ComposerTrackBody* body_ = nullptr;
};

}  // namespace

ComposerPanel::ComposerPanel(QWidget* parent) : QDockWidget(tr("Composer Mode"), parent) {
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumHeight(160);

    auto* container = new QWidget();
    trackLayout_ = new QVBoxLayout(container);
    trackLayout_->setSpacing(2);
    trackLayout_->addStretch(1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void ComposerPanel::setTracks(const std::vector<TrackData>& tracks) {
    // Every existing row's own currently-selected background style,
    // preserved across the rebuild below by matching on id - see this
    // method's own docs.
    // static_cast, not qobject_cast - ComposerTrackWidget deliberately
    // isn't a Q_OBJECT (it declares no signals/slots of its own), and
    // trackLayout_ only ever holds instances of this one type (plus the
    // trailing addStretch() item below, which has no widget() at all).
    std::vector<std::pair<sound_mind::core::LayerId, int>> previousStyles;
    for (int i = 0; i < trackLayout_->count(); ++i) {
        if (auto* widget = trackLayout_->itemAt(i)->widget()) {
            auto* row = static_cast<ComposerTrackWidget*>(widget);
            previousStyles.emplace_back(row->id(), row->styleIndex());
        }
    }

    // deleteLater(), not delete - this can be called from a signal
    // handler further up a row widget's own call stack (the same
    // LayersPanel::rebuildRows() gotcha this codebase already documents).
    while (trackLayout_->count() > 0) {
        QLayoutItem* item = trackLayout_->takeAt(0);
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (const TrackData& track : tracks) {
        auto* row = new ComposerTrackWidget(track);
        const auto previous = std::find_if(previousStyles.begin(), previousStyles.end(),
                                            [&](const auto& entry) { return entry.first == track.id; });
        if (previous != previousStyles.end()) {
            row->setStyleIndex(previous->second);
        }
        trackLayout_->addWidget(row);
    }
    trackLayout_->addStretch(1);
}

}  // namespace sound_mind::studio
