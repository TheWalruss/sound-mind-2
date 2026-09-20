#include "sound_mind/studio/device_combo_helpers.h"

#include <QComboBox>
#include <QObject>
#include <QSignalBlocker>

namespace sound_mind::studio {

namespace {

/// @brief The display text for the "use whatever the system default is"
/// combo entry - its itemData() is an empty QString, matching the audio
/// engines' own empty-string-means-default convention.
const QString kSystemDefaultLabel = QObject::tr("(System Default)");

}  // namespace

void populateDeviceCombo(QComboBox* combo, const QStringList& deviceNames) {
    const QString previousSelection = combo->currentData().toString();

    combo->blockSignals(true);
    combo->clear();
    combo->addItem(kSystemDefaultLabel, QString());
    for (const QString& name : deviceNames) {
        combo->addItem(name, name);
    }
    const int previousIndex = combo->findData(previousSelection);
    combo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    combo->blockSignals(false);
}

void setSelectedDeviceInCombo(QComboBox* combo, const QString& deviceName) {
    const QSignalBlocker blocker(combo);
    const int index = combo->findData(deviceName);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

}  // namespace sound_mind::studio
