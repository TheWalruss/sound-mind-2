#include "sound_mind/core/mind_wave.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "sound_mind/core/paint_application.h"

namespace sound_mind::core {

namespace {

/// @brief The smallest period `evaluate()` will actually divide by -
/// defensive only (avoids a literal divide-by-zero/NaN for a period at or
/// below zero), not a musically meaningful minimum cycle length.
constexpr double kMinimumPeriod = 1e-6;

}  // namespace

float MindWave::evaluate(TimeFrequencyPoint point, const sound_mind::codec::StreamCodecConfig& config) const {
    const double positionInPeriodUnits = (axis_ == MindWaveAxis::Time)
                                              ? point.timeSeconds
                                              : static_cast<double>(frequencyToBinIndex(
                                                    static_cast<float>(point.frequencyHz), config));
    const double safePeriod = std::max(kMinimumPeriod, period_);
    const double phase = 2.0 * std::numbers::pi_v<double> * (positionInPeriodUnits / safePeriod) + phaseRadians_;

    switch (periodicWaveform_) {
        case PeriodicWaveform::Sine:
            // sin() ranges [-1, 1] - rescaled to this class's own [0, 1]
            // field convention (see docs/sound-mind-design.md's own "a
            // per-pixel scalar field in [0, 1]").
            return static_cast<float>((std::sin(phase) + 1.0) / 2.0);
    }
    return 0.5f;  // Unreachable while PeriodicWaveform has only one value - defensive only.
}

void to_json(nlohmann::json& json, const MindWave& mindWave) {
    json = nlohmann::json{{"type", mindWave.type()},
                          {"periodicWaveform", mindWave.periodicWaveform()},
                          {"axis", mindWave.axis()},
                          {"period", mindWave.period()},
                          {"phaseRadians", mindWave.phaseRadians()}};
}

void from_json(const nlohmann::json& json, MindWave& mindWave) {
    mindWave.setType(json.at("type").get<GeneratorType>());
    mindWave.setPeriodicWaveform(json.at("periodicWaveform").get<PeriodicWaveform>());
    mindWave.setAxis(json.at("axis").get<MindWaveAxis>());
    mindWave.setPeriod(json.at("period").get<double>());
    mindWave.setPhaseRadians(json.at("phaseRadians").get<double>());
}

}  // namespace sound_mind::core
