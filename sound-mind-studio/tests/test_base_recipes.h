#pragma once

#include <QObject>

class BaseRecipesTest : public QObject {
    Q_OBJECT

private slots:
    void importedAudioKeepsItsFileAndReplaysExactly();
    void importedImageKeepsItsFileAndReplaysExactly();
    void generatedLayerReplaysFromItsSeed();
    void recordingReplaysFromItsFlacSource();
    void emptyDuplicateAndPhaseCleanupStartFromTheRenderedContentButPoolIsGone();
    void aMissingSourceOrUnknownRecipeFailsWithAReason();
    void undoAfterReopeningRemakesAReplacedBase();
};
