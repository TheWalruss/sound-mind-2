#include "sound_mind/core/operation.h"

namespace sound_mind::core {

// Defined out-of-line (rather than `= default` in the header) so the
// vtable and RTTI for this polymorphic base aren't re-emitted in every
// translation unit that includes operation.h.
Operation::~Operation() = default;

}  // namespace sound_mind::core
