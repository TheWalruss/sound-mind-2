#pragma once

#include <QDialog>

#include "sound_mind/core/generator_configuration.h"

class QSlider;
class QSpinBox;

namespace sound_mind::studio {

/**
 * @brief Lets the user configure a generator run before it produces a new
 *        layer - `docs/sound-mind-design.md`'s "Generators", `v0.Y.51.1`.
 *
 * Purely presentational, the same division of responsibility
 * `ImageScalePickerDialog`/`AudioSnippetPickerDialog` already establish:
 * `MainWindow` reads `configuration()` back after `exec()` returns
 * `QDialog::Accepted` and calls `LayerController::addGeneratedLayer()`
 * itself - no generation happens in this class.
 *
 * **Only offers `GeneratorFamily::Lattice`, the only family actually
 * implemented this installment** - `Fractal`/`Streaming` aren't shown at
 * all, the same "reserve the enum value, don't expose the non-functional
 * UI option yet" precedent `ToolType::Clone` already establishes (see
 * `ToolConfigurationPanel`'s own docs).
 */
class GeneratorDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog with a fresh, randomly-chosen starting
    ///        seed (via `randomizeSeed()`'s own mechanism) and
    ///        `orderChaos` at `0` (the "rich, complex-but-coherent"
    ///        middle the design doc calls out).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GeneratorDialog(QWidget* parent = nullptr);

    /// @return The currently configured `GeneratorConfiguration` - always
    ///         `GeneratorFamily::Lattice` this installment (see the class
    ///         docs), whatever `orderChaos`/`seed` the controls currently
    ///         show.
    [[nodiscard]] sound_mind::core::GeneratorConfiguration configuration() const;

private:
    /// @brief Assigns a fresh, non-deterministic seed to seedSpinBox_ -
    ///        the "Randomize" button's own slot. The chosen seed itself
    ///        is then used deterministically (see `GeneratorConfiguration::
    ///        seed`'s own docs) - only the *choice* of which seed to
    ///        start from is random, not the generation itself.
    void randomizeSeed();

    QSlider* orderChaosSlider_ = nullptr;
    QSpinBox* seedSpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
