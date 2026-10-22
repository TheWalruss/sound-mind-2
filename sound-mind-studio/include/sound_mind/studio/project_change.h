#pragma once

#include <cstddef>
#include <optional>

#include "sound_mind/core/layer.h"

namespace sound_mind::studio {

/**
 * @brief What an edit changed (`v0.1.48.1`) - handed to
 *        `MainWindow::applyProjectChange()`, the one place that brings
 *        everything showing or playing the project up to date: the unsaved
 *        flag, the canvas, prepared playback and the analysis, the Layers,
 *        History and Composer panels, and the Record panel's targets.
 *
 * Before this, every edit site called its own selection of those, and a
 * forgotten one was a stale-canvas or stale-audio bug. Build one with the
 * named constructors below (adjusting fields for an unusual edit), never by
 * listing the reactions at the edit site.
 *
 * Every change marks the project unsaved.
 */
struct ProjectChange {
    /// @brief How much of the canvas composite is out of date.
    enum class Canvas {
        Unchanged,  ///< Nothing shown on the canvas changed.
        FromLayer,  ///< From `fromLayerIndex` up (everything above may filter it).
        All,        ///< The whole stack.
    };

    Canvas canvas = Canvas::Unchanged;                     ///< See Canvas.
    std::size_t fromLayerIndex = 0;                        ///< With `Canvas::FromLayer`: the lowest changed stack position.
    bool audible = false;                                  ///< What's heard changed: prepared playback, and so the analysis, are out of date.
    bool analysis = false;                                 ///< Only the analysis is out of date (playback already handled).
    bool layerList = false;                                ///< The Layers panel's rows changed (count, order, names, bindings).
    std::optional<sound_mind::core::LayerId> thumbnail;    ///< Only this layer's row thumbnail changed.
    bool history = false;                                  ///< A History entry was committed: refresh the History panel.
    bool composer = false;                                 ///< Composer Mode's tracks or boxes changed.
    bool recordTargets = false;                            ///< The set of recorded layers may have changed.

    /// @brief A setting saved with the project that nothing on screen or in
    ///        the audio depends on (decimal separator, principal mode, a
    ///        Macro, a preset).
    /// @return The change: unsaved only.
    [[nodiscard]] static ProjectChange settingsOnly() { return {}; }

    /// @brief A resource library changed (MindWaves, kernels, presets, ...)
    ///        with a History entry; what uses it is handled by its own path.
    /// @return The change.
    [[nodiscard]] static ProjectChange resources() {
        ProjectChange change;
        change.history = true;
        return change;
    }

    /// @brief Layers were added (an import, a recording, a generated layer):
    ///        the whole canvas, the audio, and every panel listing layers.
    /// @return The change.
    [[nodiscard]] static ProjectChange layersAdded() {
        ProjectChange change;
        change.canvas = Canvas::All;
        change.audible = true;
        change.layerList = true;
        change.history = true;
        change.composer = true;
        change.recordTargets = true;
        return change;
    }

    /// @brief One layer's content changed (a stroke, a fill, a paste, a
    ///        replay): the canvas from that layer up, the audio, and its
    ///        thumbnail.
    /// @param layerIndex Its stack position.
    /// @param layer Its id.
    /// @return The change.
    [[nodiscard]] static ProjectChange layerContent(std::size_t layerIndex, sound_mind::core::LayerId layer) {
        ProjectChange change;
        change.canvas = Canvas::FromLayer;
        change.fromLayerIndex = layerIndex;
        change.audible = true;
        change.thumbnail = layer;
        change.history = true;
        change.composer = true;
        return change;
    }

    /// @brief A layer property or the layer stack changed through the
    ///        Layers panel (LayerController already refreshed the canvas,
    ///        playback and the panel itself).
    /// @return The change.
    [[nodiscard]] static ProjectChange layerProperties() {
        ProjectChange change;
        change.analysis = true;
        change.history = true;
        change.composer = true;
        change.recordTargets = true;
        return change;
    }
};

}  // namespace sound_mind::studio
