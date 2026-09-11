#include "sound_mind/core/gradient.h"

#include <algorithm>

namespace sound_mind::core {

namespace {

/// @brief Linearly interpolates between two stops - the shared logic
/// behind both a fresh interior insertStop() (interpolating the existing
/// shape) and evaluate() itself.
GradientStop lerp(const GradientStop& a, const GradientStop& b, float t) {
    GradientStop result;
    result.t = a.t + (b.t - a.t) * t;
    result.leftIntensity = a.leftIntensity + (b.leftIntensity - a.leftIntensity) * t;
    result.rightIntensity = a.rightIntensity + (b.rightIntensity - a.rightIntensity) * t;
    result.leftOpacity = a.leftOpacity + (b.leftOpacity - a.leftOpacity) * t;
    result.rightOpacity = a.rightOpacity + (b.rightOpacity - a.rightOpacity) * t;
    return result;
}

}  // namespace

Gradient::Gradient() {
    GradientStop start;
    start.t = 0.0f;
    GradientStop end;
    end.t = 1.0f;
    stops_ = {start, end};
}

std::size_t Gradient::insertStop(float t) {
    // Nudged just inside the open interval, per this method's own docs -
    // so a caller passing exactly 0 or 1 (or something beyond) still gets
    // a genuine interior stop rather than colliding with an endpoint.
    constexpr float kEpsilon = 1e-6f;
    t = std::clamp(t, kEpsilon, 1.0f - kEpsilon);

    const GradientStop newStop = evaluate(t);

    // Insert just before the first stop whose own t is >= the new one's,
    // keeping stops_ sorted by t at all times.
    const auto insertBefore =
        std::find_if(stops_.begin(), stops_.end(), [t](const GradientStop& stop) { return stop.t >= t; });
    const auto inserted = stops_.insert(insertBefore, newStop);
    return static_cast<std::size_t>(inserted - stops_.begin());
}

bool Gradient::removeStop(std::size_t index) {
    if (index == 0 || index + 1 >= stops_.size()) {
        return false;  // the two endpoints can't be removed - see this method's own docs.
    }
    stops_.erase(stops_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Gradient::setStopValues(std::size_t index, const GradientStop& values) {
    if (index >= stops_.size()) {
        return false;
    }
    const float t = stops_[index].t;  // t is fixed - see this method's own docs.
    stops_[index] = values;
    stops_[index].t = t;
    return true;
}

GradientStop Gradient::evaluate(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);

    // stops_ always has at least two entries (the constructor guarantees
    // it, and removeStop() refuses to drop either endpoint), so this loop
    // always finds a bracketing pair.
    for (std::size_t i = 0; i + 1 < stops_.size(); ++i) {
        const GradientStop& a = stops_[i];
        const GradientStop& b = stops_[i + 1];
        if (t >= a.t && t <= b.t) {
            const float span = b.t - a.t;
            const float localT = span > 0.0f ? (t - a.t) / span : 0.0f;
            return lerp(a, b, localT);
        }
    }
    return stops_.back();
}

void to_json(nlohmann::json& json, const GradientStop& stop) {
    json = nlohmann::json{{"t", stop.t},
                           {"leftIntensity", stop.leftIntensity},
                           {"rightIntensity", stop.rightIntensity},
                           {"leftOpacity", stop.leftOpacity},
                           {"rightOpacity", stop.rightOpacity}};
}

void from_json(const nlohmann::json& json, GradientStop& stop) {
    json.at("t").get_to(stop.t);
    json.at("leftIntensity").get_to(stop.leftIntensity);
    json.at("rightIntensity").get_to(stop.rightIntensity);
    json.at("leftOpacity").get_to(stop.leftOpacity);
    json.at("rightOpacity").get_to(stop.rightOpacity);
}

void to_json(nlohmann::json& json, const Gradient& gradient) {
    json = nlohmann::json{{"stops", gradient.stops_}, {"linkChannels", gradient.linkChannels_}};
}

void from_json(const nlohmann::json& json, Gradient& gradient) {
    json.at("stops").get_to(gradient.stops_);
    json.at("linkChannels").get_to(gradient.linkChannels_);
}

Gradient silenceGradient() {
    constexpr float kSilenceDb = -96.0f;
    Gradient gradient;
    GradientStop stop;
    stop.leftIntensity = kSilenceDb;
    stop.rightIntensity = kSilenceDb;
    stop.leftOpacity = 1.0f;
    stop.rightOpacity = 1.0f;
    gradient.setStopValues(0, stop);
    gradient.setStopValues(1, stop);
    return gradient;
}

}  // namespace sound_mind::core
