#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sound_mind/core/layer.h"
#include "sound_mind/core/loudness_meter.h"
#include "sound_mind/core/pitch_detection.h"
#include "sound_mind/core/stereo_analysis.h"
#include "sound_mind/core/project.h"

namespace sound_mind::core {

/// @brief One layer's own contribution, heard on its own - see
///        `ProjectLoudnessReport`.
struct LayerLoudness {
    /// @brief The layer measured.
    LayerId layer = 0;
    /// @brief Its name when measured.
    std::string name;
    /// @brief Its loudness, peaks and clipping, soloed.
    LoudnessMeasurement measurement;
};

/**
 * @brief The Analysis panel's full report on a project (Pre-Beta Plan
 *        `v0.Y.80.1`): the mix as played - loudness, stereo image, per-band
 *        views, and the decoded mix itself for the goniometer - and each
 *        audible layer's own loudness contribution.
 */
struct ProjectLoudnessReport {
    /// @brief The mix, exactly as playback hears it (respecting mute).
    LoudnessMeasurement mix;
    /// @brief The mix's per-band levels - see `bandLevelsOverTime()`.
    std::vector<float> mixBandLevels;
    /// @brief Frames in the mix's composite (the band levels' row length).
    std::uint32_t frameCount = 0;
    /// @brief Seconds per frame - maps a band-level column to a time.
    double secondsPerFrame = 0.0;
    /// @brief The mix's stereo image - see `measureStereo()`.
    StereoMeasurement mixStereo;
    /// @brief The mix's per-band stereo widths - see `bandStereoWidths()`.
    std::vector<float> mixBandWidths;
    /// @brief The mix's pitch tracks, highest voice first, each one
    ///        fundamental per frame in Hz (`0` where silent) - see
    ///        `detectPitches()`. As many tracks as the analysis asked for.
    std::vector<std::vector<float>> mixPitchTracks;
    /// @brief The decoded mix (shared, never modified) - the goniometer draws
    ///        the samples around the playhead from it. Null if nothing is
    ///        audible.
    std::shared_ptr<const sound_mind::codec::AudioBuffer> mixAudio;
    /// @brief Each visible, unmuted, non-Filter layer with content, soloed
    ///        (every other layer muted), in stack order.
    std::vector<LayerLoudness> layers;
};

/**
 * @brief Composites, decodes and measures `project`: the whole mix, then
 *        each audible layer on its own - a layer's contribution includes
 *        its own opacity, gain, balance and timing, exactly as heard in the
 *        mix. Filter and Equalizer layers are left out of the per-layer list
 *        (on their own they contribute nothing); muted or hidden layers too.
 *
 * @param project The project to analyze; not modified.
 * @param shouldCancel Polled between stages and layers.
 * @return The report; an empty mix measurement if nothing is audible.
 * @throws CompositeCancelled or std::runtime_error if cancelled.
 * @note Costs one composite and decode for the mix plus one per layer; run
 *       it off the GUI thread.
 * @param pitchVoices How many simultaneous pitches to track (`v0.1.40.11`)
 *        - see `detectPitches()`.
 */
[[nodiscard]] ProjectLoudnessReport analyzeProjectLoudness(const Project& project,
                                                           const std::function<bool()>& shouldCancel = {},
                                                           int pitchVoices = 1);

}  // namespace sound_mind::core
