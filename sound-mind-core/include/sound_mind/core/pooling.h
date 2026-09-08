#pragma once

#include "sound_mind/core/layer.h"

namespace sound_mind::core {

/**
 * @brief Pools a layer: decodes its current Stream content, re-encodes it
 *        through the near-lossless Pool codec, and stores the result on
 *        the layer (`Layer::poolContent()`) - also re-deriving a fresh
 *        Stream copy from the pooled result and replacing the layer's
 *        `Layer::content()` with it, per `docs/sound-mind-design.md`'s
 *        "the resulting Pool file converted to a light-weight Stream
 *        copy."
 *
 * A first, minimal Pool action per `docs/sound-mind-roadmap.md`'s Pool
 * Codec milestone (`v0.0.5.1`): replaces the layer's content in place,
 * rather than the design doc's eventual "hide-not-delete, recorded as an
 * undoable operation" mechanics - those need the first concrete Operation
 * subtype, which doesn't exist yet (arrives with the Basic Painting
 * milestone, per `OperationLog`'s own docs).
 *
 * @param layer The layer to pool.
 * @return `true` if pooling happened; `false` if the layer had no content
 *         to pool (nothing to do).
 */
bool poolLayer(Layer& layer);

}  // namespace sound_mind::core
