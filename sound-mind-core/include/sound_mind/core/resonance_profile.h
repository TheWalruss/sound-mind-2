#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sound_mind/core/resonant_instrument.h"

namespace sound_mind::core {

/// @brief Opaque identifier for a `NamedResonanceProfile` within a Project -
/// see `MindShotId`'s own docs for the same pattern, applied here instead
/// to a Resonance's own computed spectrum.
using ResonanceProfileId = std::uint64_t;

/**
 * @brief A named, permanently-stored Wave Kernel Signature spectrum in a
 *        Project's own library - `docs/sound-mind-roadmap.md`'s "Resonant
 *        Instruments" (`v0.Y.59.1`) own items 2-4 ("With a Curve selected,
 *        the user selects the option to create a 'Resonance'...
 *        store the resulting global geometrical spectrum").
 *
 * Holds only the already-computed `spectrum` itself
 * (`computeWaveKernelSignature()`'s own result, `resonant_instrument.h`) -
 * not the source `CurveGraph`/`Path` it was computed from, and not a live
 * reference back to either. The same "permanent snapshot, independent of
 * whatever created it" shape `NamedMindShot::clip`'s own docs already
 * establish, deliberately chosen over `NamedMindGrain`'s own live-
 * reference alternative: recomputing an eigendecomposition every time a
 * long-since-painted stroke's source curve happens to change has no good
 * answer anyway (unlike a Mind Grain's cheap live re-sample), so there is
 * no "stay in sync with the source" behavior worth keeping a reference
 * for. A later edit to the curve that produced this entry simply has no
 * effect on it - creating a new, updated Resonance from the
 * edited curve is its own fresh `addResonanceProfile()` call, the same way
 * a changed Mind Shot source requires a fresh capture, not an in-place
 * update of the old one.
 *
 * Fed by *two* separate downstream consumers - a `ResonanceConfiguration`
 * brush tip and a MindWave generator type (both still to come, in a later
 * installment) - each referencing one of these library entries by id
 * rather than independently computing and storing their own copy, the
 * same "peer resource library" shape `Project::mindShots()`/`toolPresets()`/
 * `mindWaves()` already establish for exactly this "more than one feature
 * needs the same computed/captured thing" situation.
 */
struct NamedResonanceProfile {
    /// @brief This entry's identity within its Project - assigned by
    ///        `Project::addResonanceProfile()`, not meant to be picked by
    ///        hand.
    ResonanceProfileId id = 0;

    /// @brief Display name. `Project` is responsible for keeping names
    ///        unique within itself, the same division `Layer::name()`'s
    ///        own docs already draw for layer names.
    std::string name;

    /// @brief The computed Wave Kernel Signature spectrum itself - see
    ///        `computeWaveKernelSignature()`'s own docs
    ///        (`resonant_instrument.h`) for exactly how this was produced
    ///        and why every value is already in `[0, 1]`. Permanent once
    ///        stored - see this struct's own docs on why nothing in this
    ///        codebase ever mutates it in place.
    std::vector<float> spectrum;

    /**
     * @brief The `CurveGraph` this entry's own `spectrum` was computed
     *        from, if any - added so the Resource Browser panel can show
     *        the source curve rendered, next to the spectrum plot
     *        (confirmed with the user). A permanent snapshot, the same
     *        "copied in, never a live reference" reasoning this struct's
     *        own class docs already give for `spectrum` itself - editing
     *        or deleting the original `Path`/branching curve afterward
     *        has no effect on this copy. Empty (no nodes) for any entry
     *        created before this field existed, or via a path this
     *        codebase doesn't retain the curve for.
     */
    CurveGraph sourceCurve;
};

/// @brief Serializes a named resonant profile to its JSON representation.
void to_json(nlohmann::json& json, const NamedResonanceProfile& namedProfile);

/// @brief Parses a named resonant profile from its JSON representation.
/// @throws nlohmann::json::exception on malformed or missing required data.
void from_json(const nlohmann::json& json, NamedResonanceProfile& namedProfile);

}  // namespace sound_mind::core
