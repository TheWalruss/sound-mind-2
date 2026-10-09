#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QDockWidget>
#include <QImage>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace sound_mind::studio {

/**
 * @brief Which of a project's "peer resource library" lists the panel is
 *        currently showing - `docs/sound-mind-roadmap.md`'s `v0.Y.57.1`
 *        Installment C ("pick a category... see every named entry in
 *        it").
 *
 * Layer is included even though it has no portable file of its own
 * (`docs/sound-mind-architecture.md`'s "Portable Resource Files" table) -
 * a layer travels between projects by being browsed and copied in
 * directly, which is exactly this category's own reason to exist here.
 * MindGrain likewise has no portable file (that same table), but *is*
 * meaningfully inspectable within the current project - its own
 * `sourceLayerId()`/`bounds()` resolve against a layer that's actually
 * open right now.
 */
enum class ResourceCategory {
    MindWave,
    ToolPreset,
    MindShot,
    ResonanceProfile,
    ConvolutionKernel,
    MindGrain,
    Layer,
};

/**
 * @brief A dockable panel browsing one project's resource libraries at a
 *        time - either the Studio's own current project, or a second
 *        `.smproj` opened read-only alongside it (`docs/sound-mind-
 *        roadmap.md`'s `v0.Y.57.1` Installment C, folding in the roadmap
 *        backlog's own "resource browser panel" entry, and serving that
 *        same installment's "cross-project import" line - browsing
 *        another project *is* how an entry gets copied into this one).
 *
 * Purely presentational, the same division of responsibility
 * `MindWavesPanel`/`LayersPanel` already establish: every button click and
 * selection is a signal `ResourceBrowserController` connects to its own
 * handlers, which do the actual `Project` reads/mutations and build this
 * panel's displayed rows/inspector content. Unlike `MindWavesPanel` (which
 * embeds a real `MindWaveEditor` knowing `MindWave`'s own full shape),
 * this panel deliberately stays generic across six different resource
 * types - rows are plain `id`/`name` pairs, and the inspector is
 * pre-rendered text/images handed to it by the controller, rather than
 * six different bespoke editors.
 *
 * **Simpler first pass, confirmed scope**: Tool Presets of every `ToolType`
 * are listed together, not grouped under their own sub-filter - each
 * row's own name includes its tool type (e.g. "My Brush (Procedural)"),
 * matching this codebase's general "ship a flat list first, revisit if it
 * turns out to need grouping" precedent.
 */
class ResourceBrowserPanel : public QDockWidget {
    Q_OBJECT

public:
    /// @brief One library entry's worth of row data - deliberately just an
    /// id/display name pair, not the full resource (see the class's own
    /// docs on why).
    struct RowData {
        /// @brief This entry's id within whichever project is currently
        ///        shown - `sound_mind::core::MindWaveId`/`MindShotId`/etc.,
        ///        type-erased to a plain integer since this panel treats
        ///        every category uniformly.
        std::uint64_t id = 0;

        /// @brief Display name, already including a type badge where
        ///        relevant (e.g. a Tool Preset's own `ToolType`).
        QString name;
    };

    /// @brief Builds the panel with every category empty and no project
    ///        being browsed.
    /// @param parent The owning widget, per Qt's normal parent-ownership
    ///        convention; may be `nullptr`.
    explicit ResourceBrowserPanel(QWidget* parent = nullptr);

    /// @brief Replaces the currently selected category's displayed rows.
    ///        A no-op on the panel's own category selection - the
    ///        controller is responsible for calling this again whenever
    ///        the selected category, or the underlying library, changes.
    /// @param rows The entries to display.
    void setEntries(const std::vector<RowData>& rows);

    /// @brief Shows or hides the "Import from File..." button - hidden for
    ///        a category with no portable file format (Mind Grain, Layer).
    /// @param enabled `true` to show the button.
    void setImportFromFileEnabled(bool enabled);

    /// @brief Clears the inspector pane entirely (no entry selected, or
    ///        the previously-selected one no longer exists).
    void clearInspector();

    /**
     * @brief Fills the inspector pane for the currently selected entry.
     * @param name The entry's own display name.
     * @param parameterText A human-readable dump of its own parameters -
     *        typically its JSON representation, pretty-printed.
     * @param raster A rendered pixel raster (a Mind Shot's own spectrogram,
     *        or a Resonance Profile's own spectrum plotted as a small
     *        line chart) - a null `QImage` hides the raster area entirely
     *        (every other category has no pixel content to show).
     * @param canPlay Whether a playable decoded preview exists for this
     *        entry (Mind Shot only) - shows/hides the Play/Stop buttons.
     * @param canExport Whether this category has a portable file format at
     *        all (`false` for Mind Grain and Layer) - hides the Export
     *        button when `false`.
     * @param canImportEntry Whether an "Import" button should appear for
     *        this one entry - `true` only while browsing another project.
     */
    void setInspector(const QString& name, const QString& parameterText, const QImage& raster, bool canPlay,
                       bool canExport, bool canImportEntry);

    /// @brief Sets the Play/Stop buttons' own state - `playing` toggles
    ///        which of the two is shown, independent of setInspector()'s
    ///        own canPlay (which only governs whether either is visible
    ///        at all).
    /// @param playing `true` to show Stop in place of Play.
    void setPlaying(bool playing);

    /**
     * @brief Shows which project is currently being browsed.
     * @param otherProjectPath Empty while browsing the Studio's own
     *        current project; otherwise the second project's own path,
     *        shown in the panel's own header label, and this panel
     *        offers "Return to This Project" in place of "Browse Other
     *        Project...".
     */
    void setBrowsingOtherProject(const QString& otherProjectPath);

    /// @brief The currently selected category.
    /// @return The category last selected via the category combo, or
    ///         `ResourceCategory::MindWave` before any selection (the
    ///         combo's own first entry).
    [[nodiscard]] ResourceCategory selectedCategory() const { return selectedCategory_; }

    /// @brief The currently selected entry's id, if any.
    /// @return That entry's id, or `std::nullopt` if no row is selected.
    [[nodiscard]] std::optional<std::uint64_t> selectedEntryId() const { return selectedEntryId_; }

    /**
     * @brief Replaces the Toolkit draft list's own displayed rows -
     *        `docs/sound-mind-roadmap.md`'s `v0.Y.57.1` Installment D, the
     *        roadmap backlog's own "SoundMind Toolkit collections" entry.
     *
     * Each row is already a display-ready label (category included, e.g.
     * "My Brush (Procedural)") - the controller owns the draft's own
     * contents and ordering; this panel only ever mirrors it.
     *
     * @param names The draft's own entries, in order.
     */
    void setToolkitEntries(const QStringList& names);

    /// @brief Enables/disables "Add to Toolkit" - e.g. no entry selected,
    ///        or the current category has no portable file format at all
    ///        (Mind Grain, Layer).
    /// @param enabled `true` to enable the button.
    void setAddToToolkitEnabled(bool enabled);

    /// @brief The Toolkit draft list's own currently selected row index,
    ///        if any - used by `removeFromToolkitRequested()`.
    /// @return That row's index, or `std::nullopt` if none is selected.
    [[nodiscard]] std::optional<int> selectedToolkitEntryIndex() const;

signals:
    /// @brief The category combo's own selection changed.
    /// @param category The newly selected category.
    void categoryChanged(ResourceCategory category);

    /// @brief An entry row was selected (or the selection was cleared).
    /// @param id The newly selected entry's id, or `std::nullopt` if the
    ///        selection was cleared.
    void entrySelected(std::optional<std::uint64_t> id);

    /// @brief The inspector's "Export..." button was clicked, for the
    ///        currently selected entry.
    void exportRequested();

    /// @brief The "Import from File..." button was clicked, for the
    ///        currently selected category.
    void importFromFileRequested();

    /// @brief The inspector's "Import" button was clicked - only shown
    ///        while browsing another project; copies the currently
    ///        selected entry into the Studio's own current project.
    void importEntryRequested();

    /// @brief "Browse Other Project..." was clicked.
    void browseOtherProjectRequested();

    /// @brief "Return to This Project" was clicked.
    void returnToCurrentProjectRequested();

    /// @brief The inspector's Play button was clicked.
    void playRequested();

    /// @brief The inspector's Stop button was clicked.
    void stopRequested();

    /// @brief "Add to Toolkit" was clicked, for the currently selected
    ///        category/entry.
    void addToToolkitRequested();

    /// @brief "Remove from Toolkit" was clicked, for the Toolkit draft
    ///        list's own currently selected row.
    /// @param index The row to remove.
    void removeFromToolkitRequested(int index);

    /// @brief "Export Toolkit..." was clicked.
    void exportToolkitRequested();

    /// @brief "Import Toolkit..." was clicked.
    void importToolkitRequested();

private:
    /// @brief Rebuilds `categoryCombo_`'s own entries - called once, from
    ///        the constructor.
    void populateCategoryCombo();

    QComboBox* categoryCombo_ = nullptr;
    QLabel* browsingLabel_ = nullptr;
    QPushButton* browseOtherProjectButton_ = nullptr;
    QPushButton* returnToCurrentProjectButton_ = nullptr;
    QListWidget* entriesList_ = nullptr;
    QPushButton* importFromFileButton_ = nullptr;

    QLabel* inspectorNameLabel_ = nullptr;
    QPlainTextEdit* inspectorParametersEdit_ = nullptr;
    QLabel* inspectorRasterLabel_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* importEntryButton_ = nullptr;

    QListWidget* toolkitEntriesList_ = nullptr;
    QPushButton* addToToolkitButton_ = nullptr;
    QPushButton* removeFromToolkitButton_ = nullptr;
    QPushButton* exportToolkitButton_ = nullptr;
    QPushButton* importToolkitButton_ = nullptr;

    ResourceCategory selectedCategory_ = ResourceCategory::MindWave;
    std::optional<std::uint64_t> selectedEntryId_;

    /// @brief Whether the currently inspected entry has a playable preview
    /// at all - setInspector()'s own `canPlay`, remembered so setPlaying()
    /// can correctly restore the Play button's own visibility after a
    /// Stop (simply re-reading the button's *current* visibility wouldn't
    /// work, since it's `false` while Stop is showing).
    bool inspectorCanPlay_ = false;
};

}  // namespace sound_mind::studio
