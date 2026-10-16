#pragma once

#include <memory>

#include "sound_mind/core/operation.h"

namespace sound_mind::core {

/**
 * @brief An independent duplicate of `operation` - same geometry and
 *        settings, a new identity, on `targetLayer`, superseding nothing -
 *        for copying and pasting Picked objects (Pre-Beta Plan `v0.Y.77.1`).
 *
 * Unlike `Operation::translatedCopy()`, which *replaces* the original (its
 * copy supersedes it), the clone sits alongside the original, so the two
 * render independently.
 *
 * @param operation The operation to duplicate.
 * @param newId The clone's identity.
 * @param targetLayer The layer the clone belongs to.
 * @return The clone, or `nullptr` for a kind that has no target layer to
 *         duplicate onto (none of today's concrete kinds).
 */
[[nodiscard]] std::unique_ptr<Operation> cloneOperation(const Operation& operation, OperationId newId,
                                                        LayerId targetLayer);

}  // namespace sound_mind::core
