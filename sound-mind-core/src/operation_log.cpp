#include "sound_mind/core/operation_log.h"

#include <stdexcept>

namespace sound_mind::core {

void to_json(nlohmann::json& json, const OperationLog& /*log*/) {
    json = nlohmann::json::array();
}

void from_json(const nlohmann::json& json, OperationLog& /*log*/) {
    if (!json.is_array()) {
        throw std::invalid_argument("OperationLog JSON must be an array");
    }
    // Nothing to populate yet - see the class doc comment. A non-empty
    // array here (from a newer file) is silently accepted rather than
    // rejected, consistent with the forward-compatibility approach the
    // Pool TIFF format already established.
}

}  // namespace sound_mind::core
