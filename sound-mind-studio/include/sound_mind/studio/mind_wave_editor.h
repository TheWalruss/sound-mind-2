#pragma once

#include <QWidget>

#include "sound_mind/core/mind_wave.h"

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QSpinBox;

namespace sound_mind::studio {

/**
 * @brief A plain-widget editor for a single `sound_mind::core::MindWave`'s
 *        own generator type and parameters - `v0.Y.31.1` Installment C2's
 *        own answer to "a bare-bones functional panel" (confirmed with the
 *        user during the milestone's own planning pass; Deferred Decision
 *        #7's real interaction-design pass for MindWaves is still open).
 *
 * Purely presentational, the same division of responsibility
 * `FilterConfigurationPanel` already established for `FilterConfiguration`:
 * every edit emits mindWaveChanged() with this editor's own current,
 * complete `MindWave`; nothing here knows about `Project`, `NamedMindWave`,
 * or identity of any kind - `MindWavesPanel` is what threads an edited
 * value back into the right library entry (or superposition-stack slot).
 *
 * **One `QGroupBox` per `GeneratorType`, mirroring `FilterConfigurationPanel`'s
 * own "show only the active type's own group" pattern exactly.** A few
 * fields genuinely shared by more than one generator type/sub-shape
 * (`period()`/`phaseRadians()`, `seed()`/`noiseScale()`/`noiseOctaves()`/
 * `noisePersistence()` - see `MindWave`'s own docs for exactly which) are
 * shown once, above every group, rather than duplicated inside each one
 * that uses them - matching `MindWave`'s own field-sharing design directly
 * in the UI. This means a few of these shared fields are visible-but-inert
 * for some sub-shapes (e.g. `Seed` shown while editing a `Periodic` wave,
 * which never reads it) - an accepted "bare-bones" simplification, the
 * same kind `FilterConfigurationPanel`'s own always-visible `Pulse`-only
 * duty cycle field already accepts within its own `Periodic` group.
 *
 * **No superposition-stack editing here** - this widget edits exactly one
 * generator's own parameters. `MindWavesPanel` reuses this same class
 * (once for the top-level `MindWave`, once for whichever stack member is
 * currently selected) rather than teaching this widget to recurse into
 * `superpositionStack()` itself.
 */
class MindWaveEditor : public QWidget {
    Q_OBJECT

public:
    /// @brief Builds the editor with a fresh, default `MindWave` - see
    ///        `MindWave`'s own docs for what that default is.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit MindWaveEditor(QWidget* parent = nullptr);

    /// @brief The MindWave this editor's controls currently describe.
    /// @return The current value.
    [[nodiscard]] const sound_mind::core::MindWave& mindWave() const noexcept { return wave_; }

    /**
     * @brief Loads a MindWave into the editor's own controls.
     *
     * Deliberately does *not* emit mindWaveChanged() - the same "loading
     * is a sync from some other source, not a user edit" reasoning
     * `FilterConfigurationPanel::setFilterConfiguration()`'s own docs
     * give; emitting here would make `MindWavesPanel`'s own wiring
     * indistinguishable from a real edit and re-apply the unchanged value
     * right back onto whatever was just selected.
     *
     * @param wave The MindWave to display. Its own `superpositionStack()`/
     *        `superpositionBlendMode()` are preserved internally (round-
     *        tripped through mindWave()) but have no controls of their
     *        own here - see this class's own docs.
     */
    void setMindWave(const sound_mind::core::MindWave& wave);

signals:
    /// @brief Emitted whenever any control changes.
    /// @param wave This editor's own new, complete MindWave - including
    ///        whatever `superpositionStack()`/`superpositionBlendMode()`
    ///        `setMindWave()` last loaded, untouched by this editor's own
    ///        controls.
    void mindWaveChanged(const sound_mind::core::MindWave& wave);

private:
    /// @brief Emits mindWaveChanged() with the current wave_.
    void emitChanged();

    /// @brief Shows the one per-type group matching `wave_.type()` and
    ///        hides every other one; hides the axis combo entirely for
    ///        `GeneratorType::Spatial` - see this class's own docs.
    void updateVisibleGroup();

    sound_mind::core::MindWave wave_;

    QComboBox* generatorTypeCombo_ = nullptr;
    QLabel* axisLabel_ = nullptr;
    QComboBox* axisCombo_ = nullptr;
    QDoubleSpinBox* periodSpinBox_ = nullptr;
    QDoubleSpinBox* phaseSpinBox_ = nullptr;
    QSpinBox* seedSpinBox_ = nullptr;
    QDoubleSpinBox* noiseScaleSpinBox_ = nullptr;
    QSpinBox* noiseOctavesSpinBox_ = nullptr;
    QDoubleSpinBox* noisePersistenceSpinBox_ = nullptr;

    QGroupBox* periodicGroup_ = nullptr;
    QComboBox* periodicWaveformCombo_ = nullptr;
    QDoubleSpinBox* dutyCycleSpinBox_ = nullptr;

    QGroupBox* envelopeGroup_ = nullptr;
    QComboBox* envelopeShapeCombo_ = nullptr;
    QDoubleSpinBox* envelopeCenterSpinBox_ = nullptr;
    QDoubleSpinBox* envelopeSteepnessSpinBox_ = nullptr;
    QDoubleSpinBox* decayRateSpinBox_ = nullptr;

    QGroupBox* steppedNoiseGroup_ = nullptr;
    QComboBox* steppedNoiseShapeCombo_ = nullptr;
    QSpinBox* stepCountSpinBox_ = nullptr;

    QGroupBox* spatialGroup_ = nullptr;
    QComboBox* spatialPatternCombo_ = nullptr;
    QDoubleSpinBox* spatialCenterXSpinBox_ = nullptr;
    QDoubleSpinBox* spatialCenterYSpinBox_ = nullptr;
    QDoubleSpinBox* domainWarpStrengthSpinBox_ = nullptr;

    QGroupBox* fractalGroup_ = nullptr;
    QDoubleSpinBox* fractalRoughnessSpinBox_ = nullptr;
    QSpinBox* fractalIterationsSpinBox_ = nullptr;
};

}  // namespace sound_mind::studio
