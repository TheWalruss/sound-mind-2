#pragma once

#include <QString>
#include <QStringList>

class QComboBox;

namespace sound_mind::studio {

/**
 * @brief Replaces `combo`'s items with a "(System Default)" entry followed
 *        by `deviceNames`, preserving the current selection by device name
 *        where possible (falling back to the default entry otherwise).
 *
 * Shared by every panel with a device-selection combo
 * (`ConfigureDevicesPanel`/`LoopPanel`/`RecordPanel`/`PlaybackPanel`) -
 * previously four independent, near-verbatim copies of this same function,
 * unified here as part of the `v0.Y.45.1` (Refactor & Clean Up) milestone's
 * own Installment A. Blocks `combo`'s own signals for the duration, so a
 * refresh never re-emits that combo's own selection-changed signal.
 *
 * @param combo The combo box to repopulate. Must not be `nullptr`.
 * @param deviceNames The real device names to list after the default entry,
 *        in the order given.
 */
void populateDeviceCombo(QComboBox* combo, const QStringList& deviceNames);

/**
 * @brief Selects `deviceName` in `combo` (as populated by
 *        populateDeviceCombo()), falling back to the "(System Default)"
 *        entry if `deviceName` isn't present among its items.
 *
 * Shared by every panel that syncs its own device combo from externally-
 * owned state (`LoopPanel`/`RecordPanel`/`PlaybackPanel`) - previously
 * three independent, near-verbatim copies of this same function, unified
 * here alongside populateDeviceCombo() (`v0.Y.45.1` Installment A). Blocks
 * `combo`'s own signals for the duration, so calling this to sync a combo
 * from external state never re-emits that combo's own selection-changed
 * signal.
 *
 * @param combo The combo box to update. Must not be `nullptr`.
 * @param deviceName The device name to select - an empty string, or one not
 *        currently among `combo`'s items, selects the default entry instead.
 */
void setSelectedDeviceInCombo(QComboBox* combo, const QString& deviceName);

}  // namespace sound_mind::studio
