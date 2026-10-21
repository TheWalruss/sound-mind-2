#include "sound_mind/core/macro_renderer.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "sound_mind/codec/color_mapping.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/codec/video_export.h"
#include "sound_mind/core/compositor.h"

namespace sound_mind::core {

namespace {

bool changesState(MacroEventKind kind) {
    return kind == MacroEventKind::Entry || kind == MacroEventKind::Undo || kind == MacroEventKind::Redo;
}

}  // namespace

MacroRenderer::MacroRenderer(const ProjectSettings& settings, std::vector<HistoryEntry> startEntries, Macro macro,
                             MacroRenderOptions options)
    : settings_(settings),
      startEntries_(std::move(startEntries)),
      macro_(std::move(macro)),
      options_(std::move(options)) {
    std::stable_sort(macro_.events.begin(), macro_.events.end(),
                     [](const MacroEvent& a, const MacroEvent& b) { return a.time < b.time; });
    // A segment starts at 0, at each moment something changed, and ends at
    // the Macro's end.
    std::set<double> times{0.0};
    for (const MacroEvent& event : macro_.events) {
        if (changesState(event.kind) && event.time < macro_.durationSeconds) {
            times.insert(event.time);
        }
    }
    boundaries_.assign(times.begin(), times.end());
    boundaries_.push_back(std::max(macro_.durationSeconds, boundaries_.back()));
}

MacroRenderer::~MacroRenderer() = default;

double MacroRenderer::projectSeconds() const noexcept {
    return static_cast<double>(settings_.canvasWidth) * settings_.timestepMs / 1000.0;
}

bool MacroRenderer::prepare(std::string* error) {
    ReplayOptions replay;
    replay.runRecipe = options_.runRecipe;
    replay.keepSteppedEntries = true;
    replay.shouldCancel = options_.shouldCancel;
    replayer_ = std::make_unique<HistoryReplayer>(settings_, std::move(replay));
    for (const HistoryEntry& entry : startEntries_) {
        if (options_.shouldCancel && options_.shouldCancel()) {
            if (error != nullptr) {
                *error = "Cancelled.";
            }
            return false;
        }
        std::string failure;
        if (!replayer_->apply(entry, &failure)) {
            if (error != nullptr) {
                *error = "\"" + entry.description + "\": " + failure;
            }
            return false;
        }
    }
    if (options_.limiterCeilingDb.has_value()) {
        limiter_ = std::make_unique<MasterLimiter>(MasterLimiter::LookAhead::Off);
        limiter_->setSampleRate(settings_.sampleRateHz);
        limiter_->setCeilingDb(*options_.limiterCeilingDb);
    }
    segment_ = 0;
    nextEvent_ = 0;
    return true;
}

bool MacroRenderer::applyEventsAt(double time, std::string* error) {
    while (nextEvent_ < macro_.events.size() && macro_.events[nextEvent_].time <= time) {
        const MacroEvent& event = macro_.events[nextEvent_++];
        if (!changesState(event.kind) || !event.entry.has_value()) {
            continue;
        }
        std::string failure;
        const bool done = event.kind == MacroEventKind::Undo ? replayer_->revert(*event.entry, &failure)
                                                              : replayer_->apply(*event.entry, &failure);
        if (!done) {
            if (error != nullptr) {
                *error = "\"" + event.entry->description + "\": " + failure;
            }
            return false;
        }
    }
    return true;
}

std::optional<MacroSegment> MacroRenderer::next(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    if (!replayer_ || segment_ + 1 >= boundaries_.size()) {
        return std::nullopt;
    }
    if (options_.shouldCancel && options_.shouldCancel()) {
        if (error != nullptr) {
            *error = "Cancelled.";
        }
        return std::nullopt;
    }
    // Changes made at the same moment land together: a segment too short
    // to hold a sample only applies its changes.
    double start = 0.0;
    double end = 0.0;
    while (true) {
        start = boundaries_[segment_];
        end = boundaries_[segment_ + 1];
        ++segment_;
        if (!applyEventsAt(start, error)) {
            return std::nullopt;
        }
        if (end - start >= 1.0 / settings_.sampleRateHz) {
            break;
        }
        if (segment_ + 1 >= boundaries_.size()) {
            return std::nullopt;
        }
    }

    replayer_->renderAll();
    const Project& project = replayer_->project();
    const auto composite = compositeProject(project, options_.shouldCancel, /*respectMute=*/true);
    const auto config = streamCodecConfigFor(settings_);
    const std::uint32_t rate = config.sampleRateHz;

    MacroSegment segment;
    segment.startSeconds = start;
    segment.durationSeconds = end - start;
    sound_mind::codec::AudioBuffer played;
    if (composite.has_value()) {
        segment.canvas = sound_mind::codec::toRgbImage(*composite);
        played = sound_mind::codec::decode(*composite);
    } else {
        segment.canvas = sound_mind::codec::toRgbImage(silentContentFor(settings_));
    }

    // What the playhead played: walk the segment in pieces between
    // playhead events.
    const auto firstSample = static_cast<std::int64_t>(std::llround(start * rate));
    const auto lastSample = static_cast<std::int64_t>(std::llround(end * rate));
    const auto samples = static_cast<std::size_t>(std::max<std::int64_t>(0, lastSample - firstSample));
    segment.audio.sampleRateHz = rate;
    segment.audio.left.assign(samples, 0.0f);
    segment.audio.right.assign(samples, 0.0f);
    std::vector<double> cuts{start};
    for (const MacroEvent& event : macro_.events) {
        if (!changesState(event.kind) && event.time > start && event.time < end) {
            cuts.push_back(event.time);
        }
    }
    cuts.push_back(end);
    for (std::size_t piece = 0; piece + 1 < cuts.size(); ++piece) {
        const MacroPlayhead playhead = macroPlayheadAt(macro_, cuts[piece]);
        if (!playhead.playing) {
            continue;
        }
        const auto from = static_cast<std::int64_t>(std::llround(cuts[piece] * rate)) - firstSample;
        const auto to = static_cast<std::int64_t>(std::llround(cuts[piece + 1] * rate)) - firstSample;
        const auto source = static_cast<std::int64_t>(std::llround(playhead.position * rate));
        for (std::int64_t i = std::max<std::int64_t>(0, from); i < std::min<std::int64_t>(to, samples); ++i) {
            const std::int64_t at = source + (i - from);
            if (at >= 0 && at < static_cast<std::int64_t>(played.frameCount())) {
                segment.audio.left[static_cast<std::size_t>(i)] = played.left[static_cast<std::size_t>(at)];
                segment.audio.right[static_cast<std::size_t>(i)] = played.right[static_cast<std::size_t>(at)];
            }
        }
    }
    if (limiter_ && samples > 0) {
        limiter_->process(segment.audio.left.data(), segment.audio.right.data(), samples);
    }
    return segment;
}

bool exportMacroVideo(MacroRenderer& renderer, const std::filesystem::path& path, int frameRate,
                      const std::function<void(double)>& progress, std::string* error) {
    std::unique_ptr<sound_mind::codec::StreamedVideoWriter> writer;
    const double projectSeconds = renderer.projectSeconds();
    const Macro& macro = renderer.macro();
    const auto playheadAt = [&macro, projectSeconds](double seconds) -> std::optional<double> {
        if (projectSeconds <= 0.0) {
            return std::nullopt;
        }
        return macroPlayheadAt(macro, seconds).position / projectSeconds;
    };
    try {
        while (auto segment = renderer.next(error)) {
            if (!writer) {
                writer = std::make_unique<sound_mind::codec::StreamedVideoWriter>(
                    path, segment->canvas.width, segment->canvas.height, segment->audio.sampleRateHz, frameRate);
            }
            writer->addVideo(segment->canvas, segment->durationSeconds, playheadAt);
            writer->addAudio(segment->audio);
            if (progress) {
                progress(segment->startSeconds + segment->durationSeconds);
            }
        }
        if (error != nullptr && !error->empty()) {
            return false;
        }
        if (!writer) {
            if (error != nullptr) {
                *error = "The macro is empty - nothing was played while it recorded.";
            }
            return false;
        }
        writer->finish();
        return true;
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    }
}

}  // namespace sound_mind::core
