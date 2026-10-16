#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <QDialog>
#include <QString>

#include "sound_mind/core/generator_configuration.h"
#include "sound_mind/studio/numeric_field.h"

class QComboBox;
class QFormLayout;
class QGroupBox;
class QSlider;

namespace sound_mind::studio {

class GradientEditorWidget;

/**
 * @brief The one-shot "Generate Layer" dialog - `docs/sound-mind-design.md`'s
 *        "Generators": pick a family and its settings, and a new layer is
 *        generated from them (`sound_mind::core::generateContent()`).
 *
 * **Families offered:** Lattice (`v0.Y.51.1`) and, as of Pre-Beta Plan
 * `v0.Y.79.1`, Noise. The Order/Chaos slider applies to the order/chaos
 * families and is hidden for Noise; the seed applies to every family.
 *
 * **Noise group** (shown only for Noise): the color; the level (dB, with a
 * dB/value toggle) for the named colors; horizontal/vertical resolution,
 * each with a MindWave-binding combo; and, for Custom only, a spectrum
 * gradient editor with its own MindWave-binding combo - see
 * `sound_mind::core::NoiseConfiguration` for what each does.
 * setAvailableMindWaves() fills the binding combos.
 */
class GeneratorDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Builds the dialog with a fresh, randomly-chosen starting
    ///        seed (via `randomizeSeed()`'s own mechanism), the Lattice
    ///        family, and `orderChaos` at `0` (the "rich, complex-but-
    ///        coherent" middle the design doc calls out).
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit GeneratorDialog(QWidget* parent = nullptr);

    /// @brief Lists the project's MindWaves in every binding combo (after a
    ///        leading "None").
    /// @param mindWaves Each MindWave's id and display name.
    void setAvailableMindWaves(const std::vector<std::pair<sound_mind::core::MindWaveId, QString>>& mindWaves);

    /// @return The configuration the controls currently describe.
    [[nodiscard]] sound_mind::core::GeneratorConfiguration configuration() const;

private:
    /// @brief Assigns a fresh, non-deterministic seed to seedSpinBox_ -
    ///        the "Randomize" button's own slot. The chosen seed itself
    ///        is then used deterministically (see `GeneratorConfiguration::
    ///        seed`'s own docs) - only the *choice* of which seed to
    ///        start from is random, not the generation itself.
    void randomizeSeed();

    /// @brief Shows only the controls the chosen family and color use.
    void updateVisibleControls();

    /// @brief The MindWave a binding combo selects, if any.
    [[nodiscard]] static std::optional<sound_mind::core::MindWaveId> boundMindWave(const QComboBox* combo);

    QFormLayout* mainForm_ = nullptr;
    QComboBox* familyCombo_ = nullptr;
    QSlider* orderChaosSlider_ = nullptr;
    IntegerField* seedSpinBox_ = nullptr;
    QGroupBox* noiseGroup_ = nullptr;
    QComboBox* noiseColorCombo_ = nullptr;
    NumericField* noiseLevelField_ = nullptr;
    NumericField* horizontalResolutionField_ = nullptr;
    QComboBox* horizontalResolutionMindWaveCombo_ = nullptr;
    NumericField* verticalResolutionField_ = nullptr;
    QComboBox* verticalResolutionMindWaveCombo_ = nullptr;
    QWidget* customSpectrumSection_ = nullptr;
    GradientEditorWidget* customSpectrumEditor_ = nullptr;
    QComboBox* customSpectrumMindWaveCombo_ = nullptr;
};

}  // namespace sound_mind::studio
