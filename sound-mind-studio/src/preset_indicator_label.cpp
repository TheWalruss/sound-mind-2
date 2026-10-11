#include "sound_mind/studio/preset_indicator_label.h"

namespace sound_mind::studio {

PresetIndicatorLabel::PresetIndicatorLabel(QWidget* parent) : QLabel(parent) {
    setObjectName(QStringLiteral("presetIndicatorLabel"));
    setTextFormat(Qt::PlainText);
    refresh();
}

void PresetIndicatorLabel::setActivePreset(const QString& name) {
    hasActive_ = true;
    modified_ = false;
    name_ = name;
    refresh();
}

void PresetIndicatorLabel::clearActivePreset() {
    hasActive_ = false;
    modified_ = false;
    name_.clear();
    refresh();
}

void PresetIndicatorLabel::markModified() {
    if (!hasActive_ || modified_) {
        return;
    }
    modified_ = true;
    refresh();
}

void PresetIndicatorLabel::refresh() {
    if (!hasActive_) {
        setText(tr("No preset active"));
        setStyleSheet(QStringLiteral("color: palette(mid);"));
    } else if (modified_) {
        setText(tr("Preset: %1 * (modified)").arg(name_));
        setStyleSheet(QStringLiteral("color: #e08a1e; font-weight: bold;"));
    } else {
        setText(tr("Preset: %1").arg(name_));
        setStyleSheet(QStringLiteral("font-weight: bold;"));
    }
}

}  // namespace sound_mind::studio
