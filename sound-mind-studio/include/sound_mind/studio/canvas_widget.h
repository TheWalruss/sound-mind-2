#pragma once

#include <QWidget>

#include "sound_mind/core/project.h"

namespace sound_mind::studio {

/**
 * @brief Renders the active project's canvas.
 *
 * Shows the topmost layer with cached content (see
 * `sound_mind::core::renderLayer()`), scaled to fill the widget - real
 * multi-layer compositing (blend modes, opacity, MindWave-bound
 * parameters) doesn't exist yet, so "the composite" is, for now, just
 * whichever layer was rendered most recently. Falls back to a placeholder
 * rectangle, sized to the project's configured canvas dimensions, when no
 * layer has any content yet (e.g. a fresh project with only its empty
 * Background layer).
 */
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructs an empty canvas, with no project set yet.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit CanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief Sets which project's canvas dimensions this widget reflects.
     * @param project The project to reflect, or `nullptr` to show nothing
     *        (no project open). Not owned - the caller must keep it alive
     *        for as long as it's set here, and clear or replace it before
     *        it's destroyed.
     */
    void setProject(const sound_mind::core::Project* project);

    /// @brief The widget's preferred size.
    /// @return The current project's configured canvas dimensions, or a
    ///         fallback size if no project is set.
    [[nodiscard]] QSize sizeHint() const override;

protected:
    /// @brief Repaints the canvas - see the class's own docs for what's
    ///        actually drawn.
    /// @param event Unused; required by QWidget's override signature.
    void paintEvent(QPaintEvent* event) override;

private:
    const sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
