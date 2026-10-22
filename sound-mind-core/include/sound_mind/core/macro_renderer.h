#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sound_mind/codec/audio_buffer.h"
#include "sound_mind/codec/rgb_image.h"
#include "sound_mind/codec/video_options.h"
#include "sound_mind/core/history.h"
#include "sound_mind/core/history_replay.h"
#include "sound_mind/core/macro.h"
#include "sound_mind/core/master_limiter.h"
#include "sound_mind/core/project_settings.h"

namespace sound_mind::core {

/**
 * @brief One stretch of a rendered Macro (`v0.1.44.2`): the project as it
 *        stood between two changes, and what the playhead played meanwhile.
 */
struct MacroSegment {
    /// Where it starts, on the Macro's clock (seconds) - also the video
    /// time.
    double startSeconds = 0.0;
    /// How long it lasts (seconds).
    double durationSeconds = 0.0;
    /// The canvas: the composite as `renderProjectForExport()` renders it.
    sound_mind::codec::RgbImage canvas;
    /// The audio, exactly `durationSeconds` long: the composite's audio at
    /// the positions the playhead was at (following its seeks and wraps).
    sound_mind::codec::AudioBuffer audio;
};

/// @brief How a Macro is rendered.
struct MacroRenderOptions {
    /// Remakes bases from recipes during the replay (the Studio's).
    ReplayRecipeRunner runRecipe;
    /// The master limiter's ceiling, if it's on.
    std::optional<float> limiterCeilingDb = kDefaultLimiterCeilingDb;
    /// Polled during rendering; `true` stops it.
    std::function<bool()> shouldCancel;
};

/**
 * @brief Renders a Macro, segment by segment (`v0.1.44.2`,
 *        `docs/sound-mind-design.md`'s *Macros*) - for Export Macro Video
 *        and Play Macro.
 *
 * Rebuilds the project as it was when recording began by replaying its
 * start entries into a project of its own (`HistoryReplayer`), so the
 * project being edited is never touched. Then it walks the events: every
 * change (an entry applied, undone or redone) ends one segment and starts
 * the next, and appears complete at the moment it was made. The audio of a
 * segment is what the playhead played meanwhile: `macroPlayheadAt()`'s
 * position, through seeks, Repeat wraps and Loop passes, with silence
 * beyond the project's end. The master limiter runs across segments
 * without look-ahead, so they join seamlessly.
 *
 * Changes made at the same moment (while paused) land together. Segments
 * shorter than a sample still apply their changes but produce nothing.
 *
 * Not thread-safe; owns its project, so it can run on a background thread.
 */
class MacroRenderer {
public:
    /**
     * @param settings The project settings the Macro was recorded at.
     * @param startEntries `macroStartEntries()` for the Macro.
     * @param macro The Macro (copied).
     * @param options How to render.
     */
    MacroRenderer(const ProjectSettings& settings, std::vector<HistoryEntry> startEntries, Macro macro,
                  MacroRenderOptions options);
    ~MacroRenderer();

    MacroRenderer(const MacroRenderer&) = delete;
    MacroRenderer& operator=(const MacroRenderer&) = delete;

    /**
     * @brief Rebuilds the starting state.
     * @param error Receives the reason on failure; may be `nullptr`.
     * @return `true` if ready to render.
     */
    bool prepare(std::string* error = nullptr);

    /**
     * @brief Renders the next segment.
     * @param error Receives the reason on failure (cancellation included);
     *        empty when the Macro simply ended.
     * @return The segment, or `std::nullopt` at the end or on failure.
     */
    std::optional<MacroSegment> next(std::string* error = nullptr);

    /// @brief The Macro's length (seconds).
    /// @return Its duration.
    [[nodiscard]] double durationSeconds() const noexcept { return macro_.durationSeconds; }

    /// @brief The project's length (seconds) - what the playhead's position
    ///        is a fraction of.
    /// @return Seconds.
    [[nodiscard]] double projectSeconds() const noexcept;

    /// @brief The Macro being rendered.
    /// @return It.
    [[nodiscard]] const Macro& macro() const noexcept { return macro_; }

    /// @brief Replaces the cancellation check - for a renderer built on one
    ///        thread and run on another's task. Call before prepare().
    /// @param shouldCancel Returns `true` to stop.
    void setShouldCancel(std::function<bool()> shouldCancel) { options_.shouldCancel = std::move(shouldCancel); }

private:
    bool applyEventsAt(double time, std::string* error);

    ProjectSettings settings_;
    std::vector<HistoryEntry> startEntries_;
    Macro macro_;
    MacroRenderOptions options_;
    std::unique_ptr<HistoryReplayer> replayer_;
    std::unique_ptr<MasterLimiter> limiter_;
    std::vector<double> boundaries_;
    std::size_t segment_ = 0;
    std::size_t nextEvent_ = 0;
};

/**
 * @brief Renders a whole Macro to a video (`v0.1.44.2`; format, size and
 *        animation since `v0.1.45.1`): the canvas cutting to each new state,
 *        the playhead following the recorded playhead, and the audio as
 *        played - via `codec::StreamedVideoWriter`, one segment in memory at
 *        a time. A scrolling window follows the playhead too.
 * @param renderer A prepared renderer.
 * @param path Destination path.
 * @param options Format, size, frame rate, animation, playhead, polar.
 * @param progress Told the Macro seconds rendered so far; may be empty.
 * @param error Receives the reason on failure.
 * @return `true` if written.
 */
bool exportMacroVideo(MacroRenderer& renderer, const std::filesystem::path& path,
                      const sound_mind::codec::VideoExportOptions& options, const std::function<void(double)>& progress,
                      std::string* error);

}  // namespace sound_mind::core
