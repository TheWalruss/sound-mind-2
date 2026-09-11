#pragma once

#include <memory>
#include <optional>

#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/tool_configuration.h"

namespace sound_mind::core {

/**
 * @brief A single paint stroke, logged non-destructively - see
 *        `docs/sound-mind-design.md`'s "Painting" and "Non-destructive
 *        Processing", and `docs/sound-mind-architecture.md`'s Core Data
 *        Model.
 *
 * The first concrete `Operation` subtype (per `OperationLog`'s own docs) -
 * carries everything needed to both replay itself (which layer, what
 * Path, what tool configuration) and be Picked back up later (see the
 * design doc's "Pick"): its own `Path` and `ToolConfiguration` are
 * whatever the artist actually painted with, not references into a
 * shared, mutable list, so re-editing this operation later can't be
 * affected by unrelated later changes to a saved preset of the same name.
 */
class PaintOperation : public Operation {
public:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param targetLayer Which layer's content this stroke paints into.
     * @param path The Path this stroke was painted along - see path.h.
     * @param config The tool configuration this stroke was painted with -
     *        an owned snapshot, not a shared reference.
     * @param supersedes The prior operation this one replaces, if any -
     *        see Operation::supersedes()'s own docs.
     */
    PaintOperation(OperationId id, LayerId targetLayer, Path path, ToolConfiguration config,
                   std::optional<OperationId> supersedes = std::nullopt) noexcept
        : Operation(id, supersedes), targetLayer_(targetLayer), path_(std::move(path)), config_(std::move(config)) {}

    /// @brief This operation's own time/frequency footprint - directly
    ///        this path's own bounds(), since a paint stroke never
    ///        affects anything outside the Path it was painted along.
    /// @return This operation's Path's own bounding rectangle.
    [[nodiscard]] TimeFrequencyRect bounds() const override { return path_.bounds(); }

    /// @brief Which layer this stroke painted into.
    /// @return This operation's own target layer id.
    [[nodiscard]] std::optional<LayerId> targetLayer() const noexcept override { return targetLayer_; }

    /// @brief The Path this stroke was painted along.
    /// @return This operation's own Path.
    [[nodiscard]] const Path& path() const noexcept { return path_; }

    /// @brief The tool configuration this stroke was painted with.
    /// @return This operation's own tool configuration.
    [[nodiscard]] const ToolConfiguration& config() const noexcept { return config_; }

    /// @copydoc Operation::translatedCopy()
    [[nodiscard]] std::unique_ptr<Operation> translatedCopy(
        OperationId newId, double deltaTimeSeconds, double deltaFrequencyBins,
        const sound_mind::codec::StreamCodecConfig& config) const override {
        return std::make_unique<PaintOperation>(
            newId, targetLayer_, path_.translated(deltaTimeSeconds, deltaFrequencyBins, config), config_, id());
    }

private:
    LayerId targetLayer_;
    Path path_;
    ToolConfiguration config_;
};

}  // namespace sound_mind::core
