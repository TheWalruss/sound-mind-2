#pragma once

#include <cstdint>
#include <optional>

namespace sound_mind::core {

/// @brief Opaque identifier for an Operation within a Project's OperationLog.
using OperationId = std::uint64_t;

/**
 * @brief The time and frequency extent an Operation's effect covers on the canvas.
 *
 * Used by any view that needs to draw an operation's footprint generically -
 * Composer Mode's per-track timeline boxes and the canvas's "show op
 * geometry" overlay both consume the same Operation::bounds() rather than
 * needing per-operation-type drawing logic. See "Composer Mode Fit" in
 * `docs/sound-mind-architecture.md`.
 */
struct TimeFrequencyRect {
    /// @brief Start of the operation's effect, in seconds from the project's start.
    double startTimeSeconds = 0.0;
    /// @brief End of the operation's effect, in seconds from the project's start.
    double endTimeSeconds = 0.0;
    /// @brief Lowest frequency the operation's effect reaches, in Hz.
    double lowFrequencyHz = 0.0;
    /// @brief Highest frequency the operation's effect reaches, in Hz.
    double highFrequencyHz = 0.0;
};

/**
 * @brief Abstract base for every entry in a Project's OperationLog.
 *
 * Per `docs/sound-mind-architecture.md`'s Core Data Model, an Operation
 * belongs to the Project's single, project-wide OperationLog, not to a
 * Layer: concrete subtypes that affect one layer's raster content (paint,
 * filter, generator, transform) carry their own `targetLayer` reference;
 * structural subtypes (e.g. reordering the layer stack) target the
 * project's layer list itself. This base class only carries what every
 * operation has in common, regardless of which kind it targets.
 *
 * @note `apply(RasterCache)` is deliberately not declared yet: its real
 *       signature depends on the Codec's not-yet-designed `RasterCache`
 *       type (see the architecture doc's Compositing Pipeline section).
 *       Adding it here now would mean designing that type prematurely,
 *       just to satisfy this base class.
 */
class Operation {
public:
    virtual ~Operation();

    Operation(const Operation&) = delete;
    Operation& operator=(const Operation&) = delete;
    Operation(Operation&&) = delete;
    Operation& operator=(Operation&&) = delete;

    /**
     * @brief This operation's own identity within the OperationLog.
     * @return The id this operation was constructed with.
     */
    [[nodiscard]] OperationId id() const noexcept { return id_; }

    /**
     * @brief The prior operation this one replaces, if any.
     *
     * Retiming an operation or moving it to another layer (as Composer
     * Mode allows) never mutates a past log entry - it appends a new
     * operation whose supersedes() points back at the one it replaces,
     * which becomes inactive (hidden, not deleted). This keeps the
     * append-only log's replay determinism intact. See "Composer Mode
     * Fit" in `docs/sound-mind-architecture.md`.
     *
     * @return The superseded operation's id, or `std::nullopt` if this
     *         operation doesn't replace an earlier one.
     */
    [[nodiscard]] std::optional<OperationId> supersedes() const noexcept { return supersedes_; }

    /**
     * @brief This operation's time/frequency footprint on the canvas.
     *
     * Every concrete subtype implements this so that any view - a
     * Composer Mode track box, a canvas overlay - can draw an operation's
     * extent without knowing which specific kind of operation it is.
     *
     * @return This operation's time/frequency footprint.
     */
    [[nodiscard]] virtual TimeFrequencyRect bounds() const = 0;

protected:
    /**
     * @param id Identity to give this operation within its OperationLog.
     * @param supersedes The prior operation this one replaces, if any.
     */
    explicit Operation(OperationId id, std::optional<OperationId> supersedes = std::nullopt) noexcept
        : id_(id), supersedes_(supersedes) {}

private:
    OperationId id_;
    std::optional<OperationId> supersedes_;
};

}  // namespace sound_mind::core
