#pragma once

#include <optional>

#include <QImage>
#include <QObject>
#include <QString>

#include "sound_mind/core/blend_mode.h"
#include "sound_mind/core/operation.h"
#include "sound_mind/core/path.h"
#include "sound_mind/core/project.h"
#include "sound_mind/core/text_paste.h"

namespace sound_mind::studio {

class PaintController;

/// @brief Everything the Text tool's panel sets - see `TextToolPanel`.
struct TextSettings {
    /// @brief The text to write; nothing is written while it's blank.
    QString text;
    /// @brief The font family, e.g. "Arial"; a missing family falls back
    ///        to Qt's default.
    QString fontFamily = QStringLiteral("Arial");
    /// @brief The glyph height in canvas cells (pixel size, not points), so
    ///        text looks the same size whatever the screen.
    int fontSize = 36;
    /// @brief Bold letters.
    bool bold = false;
    /// @brief Italic letters.
    bool italic = false;
    /// @brief The left channel's level where a glyph is fully covered, dBFS.
    float leftIntensityDb = -12.0f;
    /// @brief The right channel's level where a glyph is fully covered, dBFS.
    float rightIntensityDb = -12.0f;
    /// @brief Scales the whole text's amplitude, `[0, 1]`.
    float opacity = 1.0f;
    /// @brief How the text combines with what's already on the layer.
    sound_mind::core::BlendMode blendMode = sound_mind::core::BlendMode::Normal;
    /// @brief Rotation, degrees counterclockwise.
    double rotationDegrees = 0.0;

    /// @brief Field-by-field equality - lets setSettings() skip no-op updates.
    /// @return `true` when every field matches.
    bool operator==(const TextSettings&) const = default;
};

/// @brief Where the text goes, in canvas cells: its bottom-left cell and
///        its size.
struct TextBox {
    /// @brief The layer the text will be written onto.
    sound_mind::core::LayerId layer = 0;
    /// @brief The left edge's frame (column).
    int frameLow = 0;
    /// @brief The bottom edge's bin (row).
    int binLow = 0;
    /// @brief Width in frames.
    int width = 0;
    /// @brief Height in bins.
    int height = 0;
};

/**
 * @brief The Text tool (Pre-Beta Plan `v0.Y.78.1`): places a box on the
 *        canvas, previews the styled text in it live, and on Apply writes
 *        it onto the layer - ported from the legacy Studio's text tool
 *        (behavior, not code).
 *
 * **Workflow:** drag on the canvas to draw the box (the text is stretched
 * to fill it), or just click to place a box sized to the text itself,
 * centred on the click. Edit the text and its style in `TextToolPanel` -
 * the preview follows every change. Apply writes it (one undoable step);
 * Reset (or Esc, or leaving the tool) discards the box.
 *
 * Glyphs are rasterized with Qt (renderGlyphs()), the one part that needs a
 * font engine; turning that coverage into canvas content is
 * `sound_mind::core::makeTextPaste()`, so the written text is an ordinary
 * paste - Pickable, movable and deletable afterwards like any other, though
 * no longer editable as text (as in the legacy tool).
 *
 * @note GUI thread only.
 */
class TextController : public QObject {
    Q_OBJECT

public:
    /// @param paintController Provides undo bookkeeping and layer rebuilds
    ///        on Apply - not owned; must outlive this object.
    /// @param parent The owning object; may be `nullptr`.
    explicit TextController(PaintController* paintController, QObject* parent = nullptr);

    /// @brief Sets the project the text is written into, discarding any box.
    /// @param project The project; may be `nullptr`.
    void setProject(sound_mind::core::Project* project);

    /// @brief Replaces the text and its style; the preview follows. Emits
    ///        previewChanged().
    /// @param settings The new settings.
    void setSettings(const TextSettings& settings);

    /// @brief The current text and style.
    /// @return The settings last given to setSettings().
    [[nodiscard]] const TextSettings& settings() const noexcept { return settings_; }

    /// @brief Starts placing a box at `point` on `layer` (a press).
    /// @param layer The layer the text will be written onto.
    /// @param point The press position, in time/frequency space.
    void beginBox(sound_mind::core::LayerId layer, sound_mind::core::TimeFrequencyPoint point);

    /// @brief Drags the box's opposite corner to `point`. Emits previewChanged().
    /// @param point The cursor's position, in time/frequency space.
    void continueBox(sound_mind::core::TimeFrequencyPoint point);

    /**
     * @brief Finishes placing the box. A drag smaller than
     *        `kMinimumDragCells` on both axes counts as a click: the box is
     *        sized to the text itself (renderGlyphs()), centred on the
     *        press. Emits previewChanged().
     */
    void endBox();

    /// @brief The placed box, if any.
    /// @return The box, or `std::nullopt` before one is placed or after
    ///         apply()/reset().
    [[nodiscard]] const std::optional<TextBox>& box() const noexcept { return box_; }

    /// @brief The preview - white glyphs on transparent, scaled to the box.
    /// @return The image, or a null image with no box or blank text.
    [[nodiscard]] QImage previewImage() const;

    /// @brief Where the preview is drawn, in time/frequency space.
    /// @return The box's extent, or `std::nullopt` with no box.
    [[nodiscard]] std::optional<sound_mind::core::TimeFrequencyRect> previewBounds() const;

    /**
     * @brief Writes the text into the box on its layer as one undoable step
     *        (a masked `PasteOperation` - see this class's docs), then
     *        clears the box. A no-op with no box, blank text, or a
     *        Filter/Equalizer layer.
     * @return The new operation's id, or `std::nullopt` if nothing was written.
     */
    std::optional<sound_mind::core::OperationId> apply();

    /// @brief Discards the box without writing anything. Emits previewChanged().
    void reset();

    /**
     * @brief Renders `settings`' text at its natural size: white glyphs on a
     *        transparent background, with a little padding, rotated by
     *        `rotationDegrees`.
     * @param settings The text and style.
     * @return The image, or a null image for blank text.
     */
    [[nodiscard]] static QImage renderGlyphs(const TextSettings& settings);

    /**
     * @brief `settings`' glyphs stretched to exactly `width` x `height`
     *        cells, as coverage - what apply() hands to `makeTextPaste()`.
     * @param settings The text and style.
     * @param width The box width in frames.
     * @param height The box height in bins.
     * @return The raster; empty (zero size) for blank text.
     */
    [[nodiscard]] static sound_mind::core::GlyphRaster rasterize(const TextSettings& settings, int width, int height);

    /// @brief A drag shorter than this many cells on both axes is a click.
    static constexpr int kMinimumDragCells = 3;

signals:
    /// @brief The preview (box, text or style) changed.
    void previewChanged();

    /// @brief Apply wrote text onto `layer`.
    /// @param layer The layer written to.
    void contentChanged(sound_mind::core::LayerId layer);

private:
    /// @brief `point` as (frame, bin) cell indices.
    [[nodiscard]] std::pair<int, int> cellOf(sound_mind::core::TimeFrequencyPoint point) const;

    PaintController* paintController_;
    sound_mind::core::Project* project_ = nullptr;
    TextSettings settings_;
    std::optional<TextBox> box_;
    std::optional<std::pair<int, int>> dragStart_;
    std::pair<int, int> dragEnd_{0, 0};
    sound_mind::core::LayerId dragLayer_ = 0;
};

}  // namespace sound_mind::studio
