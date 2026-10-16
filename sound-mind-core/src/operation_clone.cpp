#include "sound_mind/core/operation_clone.h"

#include "sound_mind/core/fill_operation.h"
#include "sound_mind/core/filter_operation.h"
#include "sound_mind/core/guide_path_operation.h"
#include "sound_mind/core/paint_operation.h"
#include "sound_mind/core/paste_operation.h"
#include "sound_mind/core/sequence_operation.h"

namespace sound_mind::core {

std::unique_ptr<Operation> cloneOperation(const Operation& operation, OperationId newId, LayerId targetLayer) {
    if (const auto* paint = dynamic_cast<const PaintOperation*>(&operation)) {
        return std::make_unique<PaintOperation>(newId, targetLayer, paint->path(), paint->config().clone());
    }
    if (const auto* fill = dynamic_cast<const FillOperation*>(&operation)) {
        return std::make_unique<FillOperation>(newId, targetLayer, fill->bounds(), fill->gradient(), std::nullopt,
                                               fill->boundary());
    }
    if (const auto* paste = dynamic_cast<const PasteOperation*>(&operation)) {
        return std::make_unique<PasteOperation>(newId, targetLayer, paste->bounds(), paste->clip(), std::nullopt,
                                                paste->boundary(), paste->blendMode(), paste->textSource());
    }
    if (const auto* sequence = dynamic_cast<const SequenceOperation*>(&operation)) {
        return std::make_unique<SequenceOperation>(newId, targetLayer, sequence->notes(), sequence->config().clone());
    }
    if (const auto* filter = dynamic_cast<const FilterOperation*>(&operation)) {
        return std::make_unique<FilterOperation>(newId, targetLayer, filter->bounds(), filter->config(), std::nullopt,
                                                 filter->boundary());
    }
    if (const auto* guide = dynamic_cast<const GuidePathOperation*>(&operation)) {
        return std::make_unique<GuidePathOperation>(newId, targetLayer, guide->branches());
    }
    return nullptr;
}

}  // namespace sound_mind::core
