#pragma once

#include <functional>
#include <stdexcept>

#include "sound_mind/codec/pool_codec.h"
#include "sound_mind/codec/stream_codec.h"
#include "sound_mind/core/layer.h"

namespace sound_mind::core {

/**
 * @brief Thrown by computePooledContent() when its own `shouldCancel`
 *        callback starts returning `true` between phases -
 *        `docs/sound-mind-roadmap.md`'s "real, non-blocking cancel
 *        affordance for long operations" milestone (finding #12,
 *        Installment G).
 *
 * A distinct type, not a plain `std::runtime_error`, specifically so a
 * caller can tell "cancelled on request" apart from a genuine encode
 * failure. A separate type from both `sound_mind::codec::ExportCancelled`
 * and `sound_mind::studio::ImportCancelled` - each names the operation it
 * actually belongs to, and this one lives in `sound-mind-core` (where
 * `poolLayer()`/`computePooledContent()` themselves do), not Codec or
 * Studio.
 */
class PoolCancelled : public std::runtime_error {
public:
    PoolCancelled() : std::runtime_error("pool cancelled") {}
};

/**
 * @brief What poolLayer() would set on a layer - the Pool image plus a
 *        fresh Stream re-encode derived from it.
 */
struct PooledContent {
    sound_mind::codec::PoolImage poolImage;        ///< The layer's own Pool-encoded image - `Layer::setPoolContent()`'s own argument.
    sound_mind::codec::StreamImage streamContent;  ///< The fresh Stream re-encode derived from it - `Layer::setContent()`'s own argument.
};

/**
 * @brief Computes `content`'s own pooled result - poolLayer()'s encode-only
 *        half, split out so the actual (potentially much longer than other
 *        operations - see this file's own docs) codec work can run
 *        somewhere that isn't safe to mutate a live `Layer` from directly,
 *        such as a background thread - `docs/sound-mind-roadmap.md`'s
 *        "real, non-blocking cancel affordance for long operations"
 *        milestone (finding #12, Installment G).
 *
 * Takes `content` (a plain value) rather than a `Layer&` for exactly this
 * reason: nothing here touches anything shared or mutable. Runs the same
 * four phases `poolLayer()` always has - decode, Pool-encode, Pool-decode,
 * Stream-re-encode - checking `shouldCancel` once between each. This is a
 * coarser checkpoint granularity than `exportVideo()`'s own per-frame check
 * or `encodeAudioSnippets()`'s own per-snippet one (there's no natural
 * finer-grained chunking point inside any single one of these four codec
 * passes without restructuring their own internals, which is out of scope
 * here), but still a real, meaningful improvement over no checkpoint at
 * all for what the architecture doc already calls out as the operation
 * most likely to need one.
 *
 * @param content The layer content to pool - typically a live layer's own
 *        `Layer::content()`, copied out before calling this.
 * @param shouldCancel Consulted once before each of the four phases
 *        begins. Once it returns `true`, throws `PoolCancelled`
 *        immediately rather than running that phase or any subsequent
 *        one. `nullptr` (the default) never cancels - the exact prior
 *        behavior, unchanged for every existing caller.
 * @return The computed Pool image and fresh Stream re-encode.
 * @throws PoolCancelled if `shouldCancel` returns `true` - see its own
 *         docs.
 */
[[nodiscard]] PooledContent computePooledContent(const sound_mind::codec::StreamImage& content,
                                                  const std::function<bool()>& shouldCancel = nullptr);

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
 * A thin wrapper around computePooledContent() (with no `shouldCancel`, so
 * it never throws `PoolCancelled`) plus the two `layer.set*()` calls - kept
 * as its own synchronous, non-cancellable entry point for every existing
 * caller that doesn't need cancellation.
 *
 * @param layer The layer to pool.
 * @return `true` if pooling happened; `false` if the layer had no content
 *         to pool (nothing to do).
 */
bool poolLayer(Layer& layer);

}  // namespace sound_mind::core
