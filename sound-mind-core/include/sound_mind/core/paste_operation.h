#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/operation.h"

namespace sound_mind::core {

/**
 * @brief A captured rectangular snapshot of a `StreamImage`'s amplitude/
 *        phase planes - what Copy/Cut actually places on the clipboard, and
 *        what a `PasteOperation` writes back out.
 *
 * Deliberately a plain Core struct of raw arrays, not an embedded
 * `sound_mind::codec::StreamImage`: Codec is a deliberately JSON-free module
 * (see `docs/sound-mind-architecture.md`'s Build & Module Layout - confirmed
 * via `sound-mind-codec/CMakeLists.txt` carrying no `nlohmann_json`
 * dependency at all), and every other `Operation` subtype so far
 * (`PaintOperation`, `FillOperation`) only ever stores plain Core types for
 * exactly that reason. A `Clip` needs no `StreamCodecConfig` of its own - it
 * is always captured from, and pasted back into, a layer using the current
 * project's own config, the same assumption `applyFillOperation()` already
 * makes about `bounds()`.
 */
struct Clip {
    /// @brief Number of time-axis columns this clip spans.
    std::uint32_t frameCount = 0;

    /// @brief Number of frequency-axis rows this clip spans.
    std::uint32_t binCount = 0;

    /// @brief Left channel amplitude, in dB, row-major `[bin][frame]` -
    ///        exactly `binCount * frameCount` values.
    std::vector<float> leftMagnitudeDb;

    /// @brief Right channel amplitude, in dB, row-major `[bin][frame]` -
    ///        exactly `binCount * frameCount` values.
    std::vector<float> rightMagnitudeDb;

    /// @brief The shared phase channel, in radians, row-major
    ///        `[bin][frame]` - exactly `binCount * frameCount` values.
    std::vector<float> sharedPhaseRadians;
};

/// @brief Serializes a clip to its JSON representation.
void to_json(nlohmann::json& json, const Clip& clip);

/// @brief Parses a clip from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, Clip& clip);

/**
 * @brief Pastes a previously captured `Clip` onto a layer's content, logged
 *        non-destructively - see `docs/sound-mind-design.md`'s "Cut / Copy /
 *        Paste" and `docs/sound-mind-architecture.md`'s Core Data Model.
 *
 * The third concrete `Operation` subtype (after `PaintOperation` and
 * `FillOperation`). Deliberately independent of both the selection it was
 * copied from *and* of "the currently active layer" at construction time:
 * `targetLayer()` names whichever layer this specific paste writes into,
 * which may differ from the layer the clip was originally captured on - a
 * copy from one layer can be pasted onto a completely different one. Unlike
 * `FillOperation`/`PaintOperation`, applying a paste is a direct overwrite
 * of `bounds()`'s own cells from the clip's own pixel grid, not a gradient-
 * blended edit - see `applyPasteOperation()`'s own docs.
 */
class PasteOperation : public Operation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer this paste writes into - independent
     *        of whichever layer `clip` was originally captured from.
     * @param placement Where on `targetLayer` the clip lands - its own
     *        time/frequency extent, an owned snapshot (same "owned, not
     *        shared" reasoning `FillOperation::bounds` already
     *        established).
     * @param clip The captured pixel data to paste - an owned copy.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     */
    PasteOperation(OperationId id, LayerId targetLayer, TimeFrequencyRect placement, Clip clip,
                    std::optional<OperationId> supersedes = std::nullopt) noexcept
        : Operation(id, supersedes),
          targetLayer_(targetLayer),
          placement_(placement),
          clip_(std::move(clip)) {}

    /// @brief This operation's own time/frequency footprint - exactly
    ///        where the clip was placed.
    /// @return This operation's own bounds.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return placement_; }

    /// @brief Which layer this paste wrote into.
    /// @return This operation's own target layer id.
    [[nodiscard]] std::optional<LayerId> targetLayer() const noexcept override { return targetLayer_; }

    /// @brief The captured pixel data this operation pastes.
    /// @return This operation's own clip.
    [[nodiscard]] const Clip& clip() const noexcept { return clip_; }

private:
    LayerId targetLayer_;
    TimeFrequencyRect placement_;
    Clip clip_;
};

}  // namespace sound_mind::core
