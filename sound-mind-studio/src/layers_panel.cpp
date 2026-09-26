#include "sound_mind/studio/layers_panel.h"

#include <algorithm>

#include <array>
#include <utility>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QVariant>

namespace sound_mind::studio {

namespace {

using sound_mind::core::BlendMode;
using sound_mind::core::LayerId;
using sound_mind::core::LayerType;
using sound_mind::core::MindWaveId;

/// @brief Every `BlendMode` paired with its display name, in the same order
/// `selection_configuration_panel.cpp`'s own `kPasteBlendModes` and
/// `tool_configuration_panel.cpp`'s own `kBlendModes` establish - `Overwrite`
/// first here too, even though a layer's own default is `Normal` (second),
/// for the same "list every option the same way everywhere it appears"
/// consistency those two other combos already follow.
constexpr std::array<std::pair<BlendMode, const char*>, 7> kBlendModes = {{
    {BlendMode::Overwrite, "Overwrite"},
    {BlendMode::Normal, "Normal"},
    {BlendMode::Multiply, "Multiply"},
    {BlendMode::Screen, "Screen"},
    {BlendMode::Overlay, "Overlay"},
    {BlendMode::Difference, "Difference"},
    {BlendMode::Add, "Add"},
}};

/// @brief Whether `type` is one of the fixed-position layer types - no
/// drag handle, no delete button (see the class docs). A thin alias for
/// `sound_mind::core::isLockedLayerType()` - kept as its own name here
/// since every call site in this file already reads naturally as
/// "isLocked", not "isLockedLayerType".
bool isLocked(LayerType type) { return sound_mind::core::isLockedLayerType(type); }

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

/// @brief The visibility button's own glyph for the current (`visible`,
/// `muted`) pair - `v0.Y.46.1` Installment B ("Layers Panel & Editing
/// Enhancements v2"). "Visible"/"Invisible" reuse the exact glyphs the
/// pre-Installment-B binary toggle already used; "Muted" (a new middle
/// state - still shown on the canvas, excluded from audio playback) gets
/// its own, distinct half-filled glyph.
QString visibilityGlyphFor(bool visible, bool muted) {
    if (!visible) {
        return QStringLiteral("○");
    }
    return muted ? QStringLiteral("◐") : QStringLiteral("●");
}

/// @brief The visibility button's own tooltip for the current (`visible`,
/// `muted`) pair - describes what the *next* click does, matching this
/// button's own cycling behavior.
QString visibilityTooltipFor(bool visible, bool muted) {
    if (!visible) {
        return QObject::tr("Hidden - click to show (visible and audible again)");
    }
    if (muted) {
        return QObject::tr("Muted - shown on the canvas, silent in playback - click to hide");
    }
    return QObject::tr("Visible and audible - click to mute (still shown, silent in playback)");
}

/// @brief The row header's own fixed height, in pixels - tall enough for a
/// legible thumbnail strip (`v0.Y.44.1`, Layers Panel Redesign) while
/// staying compact; up from the pre-redesign row's own flat 32px.
constexpr int kRowHeaderHeight = 40;

/// @brief The `QListWidgetItem` data role marking a MindWave child row
/// (see MindWaveChildRowWidget's own docs) - `true` there, unset (so
/// `.toBool()` reads `false`) for every real layer row. `handleRowsMoved()`
/// uses this to skip child rows entirely when reconstructing the dragged
/// order: a child row's own item carries no `Qt::UserRole` layer id at
/// all, and including it as a bogus `LayerId{0}` would corrupt the
/// reconstructed order (no real layer has id `0` - `Project::addLayer()`
/// starts assigning at `1` - so `Project::reorderLayers()`'s own "same set
/// of ids as currently exist" validation would reject *every* reorder
/// while a child row is present, not just an actually-invalid one).
constexpr int kChildRowRole = Qt::UserRole + 1;

/// @brief The plain, theme-matching background color a row without a
/// thumbnail (Filter/Background/Equalizer, or a brand-new empty layer)
/// falls back to - `docs/sound-mind-design.md`'s own "plain background
/// suitable to the overall color theme" - the same secondary-panel color
/// `theme.cpp`'s own stylesheet already uses for input/list backgrounds.
constexpr const char* kPlainRowBackground = "#26262e";

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

/// @brief A QLabel that emits clicked()/doubleClicked() - used for a row's
/// name, to trigger selection/renameRequested() the same way the legacy
/// panel's name label did for rename. A real double-click delivers both a
/// press and a doubleClick per Qt's own event sequence, so clicked() fires
/// once (harmlessly re-selecting an already-selected row) immediately
/// before doubleClicked() does - selecting the row you're about to rename
/// is the natural behavior anyway, not a conflict to guard against.
class ClickableNameLabel : public QLabel {
    Q_OBJECT

public:
    using QLabel::QLabel;

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override {
        emit clicked();
        QLabel::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        emit doubleClicked();
        QLabel::mouseDoubleClickEvent(event);
    }
};

/// @brief One row's worth of widgets - see the class docs on LayersPanel
/// for the row layout this builds. As of `v0.Y.44.1` (Layers Panel
/// Redesign), two visually distinct states: **unselected** shows only the
/// visibility eye alongside the name (overlaid on a rescaled thumbnail of
/// the layer's own content, or a plain background for a Filter/Background/
/// Equalizer row); **selected** additionally reveals the drag handle,
/// opacity slider, MindWave combo, the pre-existing Time Alignment
/// translation/rescale controls, Blend Mode combo, and delete button, all
/// omitted entirely (not just disabled) for the Background row - it's
/// always the floor of the stack, always fully opaque, with nothing
/// beneath it to line up against in time, so none of those concepts apply
/// to it the way they do to every other layer type.
class LayerRowWidget : public QWidget {
    Q_OBJECT

public:
    LayerRowWidget(const LayersPanel::RowData& data, QListWidget* list,
                   const std::vector<std::pair<MindWaveId, QString>>& availableMindWaves, bool disallowedForMindGrain,
                   bool isSelected, QWidget* parent = nullptr)
        : QWidget(parent), id_(data.id) {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(2, 1, 2, 1);
        outer->setSpacing(2);

        const bool locked = isLocked(data.type);

        // ---- Header line: always present, unselected or not ----
        auto* header = new QHBoxLayout();
        header->setSpacing(4);

        if (locked) {
            auto* lock = new QLabel(QStringLiteral("\U0001F512"));
            lock->setObjectName(QStringLiteral("lockLabel"));
            lock->setFixedWidth(18);
            lock->setAlignment(Qt::AlignCenter);
            lock->setToolTip(tr("Position locked - cannot be reordered or deleted"));
            header->addWidget(lock);
        } else if (isSelected) {
            // The drag handle is one of the redesign's own "revealed once
            // selected" controls (see the class docs) - an unselected,
            // unlocked row shows nothing at all in this slot.
            auto* handle = new DragHandleLabel(list);
            handle->setObjectName(QStringLiteral("dragHandle"));
            header->addWidget(handle);
        }

        // Mind Grain ordering-rule guardrail (v0.Y.33.1 Installment B, see
        // LayersPanel::setDisallowedLayers()'s own docs) - a small red "✕"
        // next to any row the currently configured Mind Grain can't paint
        // onto, with a tooltip explaining why. Absent entirely (not merely
        // hidden) when this row isn't disallowed, matching this panel's own
        // "no dead placeholder UI" precedent (the lock icon/drag handle
        // pair above) - purely informational, so shown regardless of
        // selection, unlike the redesign's own interactive controls.
        if (disallowedForMindGrain) {
            auto* disallowedMark = new QLabel(QStringLiteral("✕"));
            disallowedMark->setObjectName(QStringLiteral("mindGrainDisallowedLabel"));
            disallowedMark->setFixedWidth(16);
            disallowedMark->setAlignment(Qt::AlignCenter);
            disallowedMark->setStyleSheet(QStringLiteral("color: #c04040; font-weight: bold;"));
            disallowedMark->setToolTip(
                tr("The currently configured Mind Grain can't paint onto this layer - it must stay above its own "
                   "source layer."));
            header->addWidget(disallowedMark);
        }

        // A 3-way cycle (Visible -> Muted -> Invisible), not a plain
        // on/off toggle - v0.Y.46.1 Installment B ("Layers Panel & Editing
        // Enhancements v2"). Plain (non-checkable) and click-driven, since
        // QPushButton's own checkable/toggled machinery is inherently
        // binary; the button's own glyph/tooltip are derived fresh from
        // data.visible/data.muted on every rebuild (LayersPanel::
        // rebuildRows() already reconstructs every row on any state
        // change), so nothing here needs to track or compute the next
        // state itself.
        auto* visibilityButton = new QPushButton(visibilityGlyphFor(data.visible, data.muted));
        visibilityButton->setObjectName(QStringLiteral("visibilityButton"));
        visibilityButton->setFlat(true);
        visibilityButton->setFixedWidth(22);
        visibilityButton->setToolTip(visibilityTooltipFor(data.visible, data.muted));
        if (data.type == LayerType::Background) {
            // Matches the legacy panel: the Background layer's visibility
            // can't be turned off - it's always the floor of the stack.
            visibilityButton->setEnabled(false);
            visibilityButton->setToolTip(tr("Background layer is always visible"));
        } else {
            connect(visibilityButton, &QPushButton::clicked, this, [this]() { emit visibilityCycleRequested(id_); });
        }
        header->addWidget(visibilityButton);

        // Name overlaid on the layer's own rescaled thumbnail (or a plain
        // background when there isn't one) - docs/sound-mind-design.md's
        // "Layer panel styling". A QStackedLayout with StackAll draws both
        // children on top of each other at the same rect, rather than
        // showing only the topmost one (its own default "which page is
        // current" behavior, meant for wizard-style pages, not an overlay).
        auto* nameArea = new QWidget();
        nameArea->setObjectName(QStringLiteral("nameArea"));
        nameArea->setMinimumHeight(kRowHeaderHeight);
        auto* nameAreaStack = new QStackedLayout(nameArea);
        nameAreaStack->setStackingMode(QStackedLayout::StackAll);
        nameAreaStack->setContentsMargins(0, 0, 0, 0);

        if (!data.thumbnail.isNull()) {
            auto* thumbnailLabel = new QLabel();
            thumbnailLabel->setObjectName(QStringLiteral("thumbnailLabel"));
            thumbnailLabel->setPixmap(QPixmap::fromImage(data.thumbnail));
            thumbnailLabel->setScaledContents(true);  // stretches to nameArea's own current size every resize.
            nameAreaStack->addWidget(thumbnailLabel);
        } else {
            nameArea->setStyleSheet(QStringLiteral("background-color: %1;").arg(QLatin1String(kPlainRowBackground)));
        }

        // White, bold, on a semi-transparent dark backing bar - legible
        // over any thumbnail color underneath (spectrogram content has no
        // predictable palette to contrast against), not just the plain
        // fallback background above.
        auto* nameLabel = new ClickableNameLabel(data.name);
        nameLabel->setObjectName(QStringLiteral("nameLabel"));
        nameLabel->setStyleSheet(
            QStringLiteral("color: #ffffff; font-weight: bold; background-color: rgba(0, 0, 0, 150); padding: 2px;"));
        nameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        connect(nameLabel, &ClickableNameLabel::clicked, this, [this]() { emit selected(id_); });
        connect(nameLabel, &ClickableNameLabel::doubleClicked, this, [this]() { emit renameRequested(id_); });
        nameAreaStack->addWidget(nameLabel);
        // QStackedLayout::StackAll draws every child, but still raises only
        // the *current* one (index 0 by default) in front of the rest - a
        // real, easy-to-miss gotcha, not "all children just overlay in
        // insertion order." Without this, a row with a thumbnail (added at
        // index 0, above) left the thumbnail as the default-current, and
        // therefore frontmost, child - hiding nameLabel entirely behind it.
        // A Filter/Equalizer row never adds a thumbnail, so nameLabel ends
        // up as index 0 - the only, and therefore already-current - child,
        // which is why only those rows ever showed their own name.
        nameAreaStack->setCurrentWidget(nameLabel);

        header->addWidget(nameArea, 1);

        const QString tagText = typeTagText(data.type);
        if (!tagText.isEmpty()) {
            auto* typeTag = new QLabel(tagText);
            typeTag->setObjectName(QStringLiteral("typeTagLabel"));
            typeTag->setStyleSheet(QStringLiteral("color: #7b9fd4; font-size: 9px;"));
            header->addWidget(typeTag);
        }

        if (isSelected && !locked) {
            // Duplicate/Delete are two more of the redesign's own
            // "revealed once selected" controls - see the class docs.
            // Real-world testing pass finding #22.
            auto* duplicateButton = new QPushButton(QStringLiteral("⧉"));
            duplicateButton->setObjectName(QStringLiteral("duplicateButton"));
            duplicateButton->setFlat(true);
            duplicateButton->setFixedWidth(22);
            duplicateButton->setToolTip(tr("Duplicate layer"));
            connect(duplicateButton, &QPushButton::clicked, this, [this]() { emit duplicateRequested(id_); });
            header->addWidget(duplicateButton);

            if (!data.thumbnail.isNull()) {
                // Phase Cleanup only makes sense for a row with real content
                // (a null thumbnail means "no content yet", or a Filter/
                // Equalizer row, which never has stored content at all -
                // see RowData::thumbnail's own docs) - real-world testing
                // pass finding #24.
                auto* cleanUpPhaseButton = new QPushButton(QStringLiteral("✦"));
                cleanUpPhaseButton->setObjectName(QStringLiteral("cleanUpPhaseButton"));
                cleanUpPhaseButton->setFlat(true);
                cleanUpPhaseButton->setFixedWidth(22);
                cleanUpPhaseButton->setToolTip(tr("Clean up phase noise in silent areas"));
                connect(cleanUpPhaseButton, &QPushButton::clicked, this,
                        [this]() { emit cleanUpPhaseRequested(id_); });
                header->addWidget(cleanUpPhaseButton);
            }

            auto* deleteButton = new QPushButton(QStringLiteral("×"));
            deleteButton->setObjectName(QStringLiteral("deleteButton"));
            deleteButton->setFlat(true);
            deleteButton->setFixedWidth(22);
            deleteButton->setStyleSheet(QStringLiteral("color: #c04040;"));
            deleteButton->setToolTip(tr("Delete layer"));
            connect(deleteButton, &QPushButton::clicked, this, [this]() { emit deleteRequested(id_); });
            header->addWidget(deleteButton);
        }

        outer->addLayout(header);

        // ---- Expanded controls: only once selected, per the redesign -
        // see the class docs. Omitted entirely for Background, same as
        // before the redesign (see this constructor's own class docs).
        if (isSelected && data.type != LayerType::Background) {
            auto* controlsRowOne = new QHBoxLayout();
            controlsRowOne->setSpacing(4);

            auto* opacitySlider = new QSlider(Qt::Horizontal);
            opacitySlider->setObjectName(QStringLiteral("opacitySlider"));
            opacitySlider->setRange(0, 100);
            opacitySlider->setValue(static_cast<int>(data.opacity * 100.0f));
            opacitySlider->setToolTip(tr("Layer opacity"));
            connect(opacitySlider, &QSlider::valueChanged, this,
                    [this](int value) { emit opacityChanged(id_, static_cast<float>(value) / 100.0f); });
            controlsRowOne->addWidget(opacitySlider, 1);

            // Per-layer balance (v0.Y.46.1 Installment C, "Layers Panel &
            // Editing Enhancements v2") - alongside opacity: opacity
            // attenuates this layer's overall contribution, balance
            // controls how that contribution splits between channels.
            // `50` (the slider's own midpoint) is a true no-op, matching
            // Layer::balance()'s own default/no-op value of `0.5`.
            auto* balanceSlider = new QSlider(Qt::Horizontal);
            balanceSlider->setObjectName(QStringLiteral("balanceSlider"));
            balanceSlider->setRange(0, 100);
            balanceSlider->setValue(static_cast<int>(data.balance * 100.0f));
            balanceSlider->setToolTip(tr("Layer stereo balance (left/right)"));
            connect(balanceSlider, &QSlider::valueChanged, this,
                    [this](int value) { emit balanceChanged(id_, static_cast<float>(value) / 100.0f); });
            controlsRowOne->addWidget(balanceSlider, 1);

            // MindWave opacity binding (v0.Y.31.1 Installment C2) - "None"
            // (a plain scalar opacity, the default) always first, then
            // every library entry MindWaveController currently knows
            // about. See docs/sound-mind-design.md's "Layer opacity" -
            // this multiplies opacitySlider's own value per cell, it
            // doesn't replace it, so both controls stay meaningful and
            // enabled together.
            auto* mindWaveCombo = new QComboBox();
            mindWaveCombo->setObjectName(QStringLiteral("opacityMindWaveCombo"));
            mindWaveCombo->setToolTip(tr("Bind this layer's opacity to a MindWave"));
            mindWaveCombo->addItem(tr("None"), QVariant::fromValue(qulonglong{0}));
            int selectedIndex = 0;
            for (const auto& [mindWaveId, name] : availableMindWaves) {
                mindWaveCombo->addItem(name, QVariant::fromValue(static_cast<qulonglong>(mindWaveId)));
                if (data.opacityMindWaveId.has_value() && *data.opacityMindWaveId == mindWaveId) {
                    selectedIndex = mindWaveCombo->count() - 1;
                }
            }
            mindWaveCombo->setCurrentIndex(selectedIndex);
            connect(mindWaveCombo, &QComboBox::currentIndexChanged, this, [this, mindWaveCombo](int index) {
                const auto rawId = mindWaveCombo->itemData(index).toULongLong();
                emit opacityMindWaveChanged(
                    id_, rawId == 0 ? std::nullopt : std::optional<MindWaveId>(static_cast<MindWaveId>(rawId)));
            });
            controlsRowOne->addWidget(mindWaveCombo, 1);
            outer->addLayout(controlsRowOne);

            auto* controlsRowTwo = new QHBoxLayout();
            controlsRowTwo->setSpacing(4);

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
            translationSpinBox->setToolTip(
                tr("Shift this layer's content earlier/later in time, in spectrogram columns"));
            connect(translationSpinBox, &QSpinBox::valueChanged, this,
                    [this](int value) { emit translationChanged(id_, static_cast<std::int64_t>(value)); });
            controlsRowTwo->addWidget(translationSpinBox, 1);

            auto* rescaleSpinBox = new QDoubleSpinBox();
            rescaleSpinBox->setObjectName(QStringLiteral("rescaleSpinBox"));
            rescaleSpinBox->setRange(0.1, 10.0);
            rescaleSpinBox->setSingleStep(0.05);
            rescaleSpinBox->setDecimals(2);
            rescaleSpinBox->setSuffix(QStringLiteral("x"));
            rescaleSpinBox->setValue(data.rescaleFactor);
            rescaleSpinBox->setToolTip(tr("Stretch/compress this layer's own timeline"));
            connect(rescaleSpinBox, &QDoubleSpinBox::valueChanged, this,
                    [this](double value) { emit rescaleChanged(id_, value); });
            controlsRowTwo->addWidget(rescaleSpinBox, 1);

            // Blend Mode (v0.Y.37.1) - one of the redesign's own named
            // controls now (see the class docs on LayersPanel), previously
            // a minimal stand-in ahead of it.
            auto* blendModeCombo = new QComboBox();
            blendModeCombo->setObjectName(QStringLiteral("blendModeCombo"));
            blendModeCombo->setToolTip(tr("How this layer combines with what's beneath it"));
            for (const auto& [mode, name] : kBlendModes) {
                blendModeCombo->addItem(tr(name), QVariant::fromValue(static_cast<int>(mode)));
            }
            const int blendModeIndex =
                blendModeCombo->findData(QVariant::fromValue(static_cast<int>(data.blendMode)));
            blendModeCombo->setCurrentIndex(blendModeIndex >= 0 ? blendModeIndex : 0);
            connect(blendModeCombo, &QComboBox::currentIndexChanged, this, [this, blendModeCombo](int index) {
                emit blendModeChanged(id_, static_cast<BlendMode>(blendModeCombo->itemData(index).toInt()));
            });
            controlsRowTwo->addWidget(blendModeCombo, 1);
            outer->addLayout(controlsRowTwo);
        }
    }

signals:
    void visibilityCycleRequested(sound_mind::core::LayerId id);
    void opacityChanged(sound_mind::core::LayerId id, float opacity);
    void balanceChanged(sound_mind::core::LayerId id, float balance);
    void translationChanged(sound_mind::core::LayerId id, std::int64_t translationColumns);
    void rescaleChanged(sound_mind::core::LayerId id, double rescaleFactor);
    void opacityMindWaveChanged(sound_mind::core::LayerId id, std::optional<MindWaveId> mindWaveId);
    void blendModeChanged(sound_mind::core::LayerId id, BlendMode mode);
    void renameRequested(sound_mind::core::LayerId id);
    void deleteRequested(sound_mind::core::LayerId id);
    void duplicateRequested(sound_mind::core::LayerId id);
    void cleanUpPhaseRequested(sound_mind::core::LayerId id);
    void selected(sound_mind::core::LayerId id);

private:
    sound_mind::core::LayerId id_;
};

/// @brief A smaller, non-interactive, visually indented "child" row shown
/// directly beneath a layer whose opacity is bound to a MindWave -
/// `docs/sound-mind-design.md`'s "Layer panel styling" ("Any MindWave
/// applied for opacity is shown as a smaller, tabbed-in, 'child' layer,
/// with the visual contents being the grayscale 'preview'"), `v0.Y.44.1`
/// (Layers Panel Redesign). Purely informational - unlike LayerRowWidget,
/// this has no signals of its own and its own QListWidgetItem is marked
/// unselectable/undraggable (see rebuildRows()'s own construction site),
/// so it never becomes the list's current selection or an independent drag
/// source.
class MindWaveChildRowWidget : public QWidget {
public:
    MindWaveChildRowWidget(const QString& mindWaveName, const QImage& previewImage, QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(2, 0, 2, 0);
        layout->setSpacing(4);

        // A fixed-width indent, visually "tabbing in" this child row
        // beneath its own parent layer's row - roughly the width of the
        // parent's own lock/drag-handle slot plus its visibility eye, so
        // this row's own content lines up starting where the parent's own
        // name area begins.
        auto* indent = new QWidget();
        indent->setFixedWidth(40);
        layout->addWidget(indent);

        auto* previewArea = new QWidget();
        previewArea->setObjectName(QStringLiteral("mindWavePreviewArea"));
        previewArea->setFixedHeight(kRowHeaderHeight * 3 / 4);
        auto* previewStack = new QStackedLayout(previewArea);
        previewStack->setStackingMode(QStackedLayout::StackAll);
        previewStack->setContentsMargins(0, 0, 0, 0);

        if (!previewImage.isNull()) {
            auto* previewLabel = new QLabel();
            previewLabel->setObjectName(QStringLiteral("mindWavePreviewLabel"));
            previewLabel->setPixmap(QPixmap::fromImage(previewImage));
            previewLabel->setScaledContents(true);
            previewStack->addWidget(previewLabel);
        } else {
            previewArea->setStyleSheet(
                QStringLiteral("background-color: %1;").arg(QLatin1String(kPlainRowBackground)));
        }

        auto* nameLabel = new QLabel(mindWaveName);
        nameLabel->setObjectName(QStringLiteral("mindWaveNameLabel"));
        nameLabel->setStyleSheet(QStringLiteral(
            "color: #ffffff; font-style: italic; font-size: 10px; background-color: rgba(0, 0, 0, 150); padding: 1px;"));
        previewStack->addWidget(nameLabel);

        layout->addWidget(previewArea, 1);
    }
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

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* addButtonRow = new QHBoxLayout();
    auto* addLayerButton = new QPushButton(tr("+ Add Layer"));
    addLayerButton->setObjectName(QStringLiteral("addLayerButton"));
    addLayerButton->setToolTip(tr("Add a new, empty layer to paint onto"));
    connect(addLayerButton, &QPushButton::clicked, this, &LayersPanel::addLayerRequested);
    addButtonRow->addWidget(addLayerButton);

    auto* addFilterLayerButton = new QPushButton(tr("+ Add Filter Layer"));
    addFilterLayerButton->setObjectName(QStringLiteral("addFilterLayerButton"));
    addFilterLayerButton->setToolTip(tr("Add a new Filter layer, configured in the Filter Configuration panel"));
    connect(addFilterLayerButton, &QPushButton::clicked, this, &LayersPanel::addFilterLayerRequested);
    addButtonRow->addWidget(addFilterLayerButton);

    auto* generateLayerButton = new QPushButton(tr("+ Generate Layer"));
    generateLayerButton->setObjectName(QStringLiteral("generateLayerButton"));
    generateLayerButton->setToolTip(tr("Generate a new layer's own content algorithmically"));
    connect(generateLayerButton, &QPushButton::clicked, this, &LayersPanel::generateLayerRequested);
    addButtonRow->addWidget(generateLayerButton);
    layout->addLayout(addButtonRow);

    list_ = new QListWidget();
    list_->setObjectName(QStringLiteral("layersList"));
    list_->setSelectionMode(QListWidget::SingleSelection);
    list_->setDragEnabled(true);
    list_->setAcceptDrops(true);
    list_->setDropIndicatorShown(true);
    list_->setDragDropMode(QListWidget::InternalMove);
    connect(list_->model(), &QAbstractItemModel::rowsMoved, this, &LayersPanel::handleRowsMoved);
    layout->addWidget(list_, 1);

    // Wrapped in a real QScrollArea, matching every other multi-field dock
    // panel's own established convention (FilterConfigurationPanel,
    // ToolConfigurationPanel, GridPanel, PlaybackPanel, RecordPanel,
    // LoopPanel) - `list_`'s own internal scrollbar already handles
    // overflow of individual rows, but this still lets the dock itself be
    // resized down freely rather than relying on `list_`'s own
    // minimumSizeHint() to stay small forever as this panel's own content
    // grows in a future change.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(container);
    scrollArea->setWidgetResizable(true);
    setWidget(scrollArea);
}

void LayersPanel::setLayers(const std::vector<RowData>& layersBottomToTop) {
    currentRows_ = layersBottomToTop;

    // A selected row that no longer exists in the new rows (deleted, or a
    // stale selection left over from a project clearSelection() should
    // have been called for but wasn't) can't stay selected - see
    // selectedLayerId()'s own docs on paintTargetLayerId() trusting this.
    if (selectedLayerId_.has_value()) {
        const bool stillPresent = std::any_of(layersBottomToTop.begin(), layersBottomToTop.end(),
                                               [this](const RowData& row) { return row.id == *selectedLayerId_; });
        if (!stillPresent) {
            selectedLayerId_.reset();
            emit selectionChanged(std::nullopt);
        }
    }

    rebuildRows();
}

void LayersPanel::setAvailableMindWaves(const std::vector<std::pair<MindWaveId, QString>>& mindWaves) {
    availableMindWaves_ = mindWaves;
    rebuildRows();
}

void LayersPanel::setMindWavePreviewImages(const std::map<MindWaveId, QImage>& previewImages) {
    mindWavePreviewImages_ = previewImages;
    rebuildRows();
}

void LayersPanel::setDisallowedLayers(const std::vector<sound_mind::core::LayerId>& disallowed) {
    disallowedLayers_ = disallowed;
    rebuildRows();
}

void LayersPanel::rebuildRows() {
    // QListWidget::clear() deletes the QListWidgetItems but *not* the
    // LayerRowWidgets set via setItemWidget() on them (a real, easy-to-miss
    // Qt gotcha - an item widget is reparented to the viewport internally,
    // separately from the item itself) - without this, every rebuildRows()
    // call after the first would leak the previous rows' widgets, which
    // would then keep showing up alongside the new ones.
    //
    // deleteLater(), not delete: rebuildRows() is commonly called *from*
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
    for (auto it = currentRows_.rbegin(); it != currentRows_.rend(); ++it) {
        const bool selected = selectedLayerId_.has_value() && *selectedLayerId_ == it->id;

        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(it->id)));
        if (isLocked(it->type)) {
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        }
        list_->addItem(item);
        const bool disallowedForMindGrain =
            std::find(disallowedLayers_.begin(), disallowedLayers_.end(), it->id) != disallowedLayers_.end();
        auto* row = new LayerRowWidget(*it, list_, availableMindWaves_, disallowedForMindGrain, selected);
        // Built from the actual widget's own preferred size (which now
        // varies with selected/collapsed state and locked/Background type -
        // see LayerRowWidget's own docs) rather than a single hardcoded
        // constant every row used before the redesign.
        item->setSizeHint(row->sizeHint());
        list_->setItemWidget(item, row);

        connect(row, &LayerRowWidget::visibilityCycleRequested, this, &LayersPanel::visibilityCycleRequested);
        connect(row, &LayerRowWidget::opacityChanged, this, &LayersPanel::opacityChanged);
        connect(row, &LayerRowWidget::balanceChanged, this, &LayersPanel::balanceChanged);
        connect(row, &LayerRowWidget::translationChanged, this, &LayersPanel::translationChanged);
        connect(row, &LayerRowWidget::rescaleChanged, this, &LayersPanel::rescaleChanged);
        connect(row, &LayerRowWidget::opacityMindWaveChanged, this, &LayersPanel::opacityMindWaveChanged);
        connect(row, &LayerRowWidget::blendModeChanged, this, &LayersPanel::blendModeChanged);
        connect(row, &LayerRowWidget::renameRequested, this, &LayersPanel::renameRequested);
        connect(row, &LayerRowWidget::deleteRequested, this, &LayersPanel::deleteRequested);
        connect(row, &LayerRowWidget::duplicateRequested, this, &LayersPanel::duplicateRequested);
        connect(row, &LayerRowWidget::cleanUpPhaseRequested, this, &LayersPanel::cleanUpPhaseRequested);
        connect(row, &LayerRowWidget::selected, this, &LayersPanel::selectLayer);

        // Restores the selection highlight across this refresh, for the
        // (already-verified-still-present, above) previously-selected id.
        if (selected) {
            list_->setCurrentItem(item);
        }

        // MindWave-bound-opacity child row (see setMindWavePreviewImages()'s
        // own docs) - a separate, smaller, non-selectable/non-draggable
        // QListWidgetItem placed directly beneath this layer's own, only
        // when a preview image is actually available for the bound id (a
        // dangling id - the MindWave was since deleted - simply has no
        // entry, so no child row, matching Layer::opacityMindWave()'s own
        // graceful-dangling-reference contract).
        if (it->opacityMindWaveId.has_value()) {
            const auto previewIt = mindWavePreviewImages_.find(*it->opacityMindWaveId);
            if (previewIt != mindWavePreviewImages_.end()) {
                QString mindWaveName;
                for (const auto& [mindWaveId, name] : availableMindWaves_) {
                    if (mindWaveId == *it->opacityMindWaveId) {
                        mindWaveName = name;
                        break;
                    }
                }
                auto* childItem = new QListWidgetItem();
                childItem->setFlags(childItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsDragEnabled);
                childItem->setData(kChildRowRole, true);
                list_->addItem(childItem);
                auto* childRow = new MindWaveChildRowWidget(mindWaveName, previewIt->second);
                childItem->setSizeHint(childRow->sizeHint());
                list_->setItemWidget(childItem, childRow);
            }
        }
    }
}

void LayersPanel::selectLayer(sound_mind::core::LayerId id) {
    const bool exists =
        std::any_of(currentRows_.begin(), currentRows_.end(), [id](const RowData& row) { return row.id == id; });
    if (!exists) {
        // No row has this id - a no-op, per this method's own docs, rather
        // than setting selectedLayerId_ to an id with no matching row.
        return;
    }
    selectedLayerId_ = id;
    // Rebuilds every row, not just moves the list's own native highlight -
    // since the Layers Panel Redesign (v0.Y.44.1), which controls a row's
    // own widget actually contains depends on whether it's the selected
    // one (see LayerRowWidget's own docs), so a *different* row becoming
    // selected needs its own widget rebuilt too, not just this one's.
    rebuildRows();
    emit selectionChanged(selectedLayerId_);
}

void LayersPanel::clearSelection() {
    selectedLayerId_.reset();
    rebuildRows();  // see selectLayer()'s own docs on why a rebuild, not just setCurrentItem(nullptr), is needed now.
    emit selectionChanged(std::nullopt);
}

void LayersPanel::handleRowsMoved() {
    std::vector<sound_mind::core::LayerId> newOrderTopToBottom;
    newOrderTopToBottom.reserve(static_cast<std::size_t>(list_->count()));
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item->data(kChildRowRole).toBool()) {
            continue;  // A MindWave child row, not a real layer - see kChildRowRole's own docs.
        }
        newOrderTopToBottom.push_back(static_cast<sound_mind::core::LayerId>(item->data(Qt::UserRole).toULongLong()));
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
