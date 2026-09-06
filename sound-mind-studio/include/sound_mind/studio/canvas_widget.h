#pragma once

#include <QWidget>

#include "sound_mind/core/project.h"

namespace sound_mind::studio {

/**
 * @brief Renders the active project's canvas - currently a placeholder.
 *
 * No codec exists yet (see `docs/sound-mind-roadmap.md`'s "Project &
 * Canvas" milestone), so there's no real spectrogram data to show; this
 * paints a solid rectangle sized to the project's configured canvas
 * dimensions, standing in for the (still-empty) Background layer.
 */
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief Sets which project's canvas dimensions this widget reflects.
     * @param project The project to reflect, or `nullptr` to show nothing
     *        (no project open). Not owned - the caller must keep it alive
     *        for as long as it's set here, and clear or replace it before
     *        it's destroyed.
     */
    void setProject(const sound_mind::core::Project* project);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const sound_mind::core::Project* project_ = nullptr;
};

}  // namespace sound_mind::studio
