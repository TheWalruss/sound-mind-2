# Sound Mind Studio - User Guide

This guide covers every screen and control in the current build. Sound
Mind Studio is under active development - `CHANGELOG.md` lists exactly
what's shipped, and the [What's Not Here Yet](#whats-not-here-yet) section
at the end of this guide is honest about what's planned but not yet
usable. If something described here doesn't match what you see, the
in-app behavior is correct and this guide is due an update.

## Contents

1. [What Sound Mind Studio Is](#what-sound-mind-studio-is)
2. [Starting Out](#starting-out)
3. [Creating a Project](#creating-a-project)
4. [The Main Window](#the-main-window)
5. [Canvas Navigation](#canvas-navigation)
6. [Importing Media](#importing-media)
7. [Working with Layers](#working-with-layers)
8. [Painting](#painting)
9. [Pick](#pick)
10. [Selection and Fill](#selection-and-fill)
11. [Path Tool](#path-tool)
12. [Text Tool](#text-tool)
13. [Chord Generator](#chord-generator)
14. [Axis Labels](#axis-labels)
15. [Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid)
16. [Playback](#playback)
17. [Recording](#recording)
18. [Loop Mode](#loop-mode)
19. [Macros](#macros)
20. [Configure Devices](#configure-devices)
21. [Pooling a Layer](#pooling-a-layer)
22. [Exporting](#exporting)
23. [Saving and Project Files](#saving-and-project-files)
24. [Resource Browser](#resource-browser)
25. [Analysis](#analysis)
26. [Performance](#performance)
27. [Licence and Credits](#licence-and-credits)
28. [What's Not Here Yet](#whats-not-here-yet)

## What Sound Mind Studio Is

Sound Mind Studio encodes sound as a **spectrogram** - a picture of that
sound - and can decode any spectrogram-shaped picture back into sound. The
two are treated as one continuous thing, not a conversion with something
lost in translation:

- **Horizontal axis** - time, left to right.
- **Vertical axis** - frequency, low at the bottom, high at the top.
- **Brightness** - loudness at that frequency, at that moment.
- **Color** - red is the left channel's loudness, green is the right
  channel's, and blue carries phase (roughly, the fine detail that shapes
  timbre rather than pitch or volume).

Import an audio file and you'll see its spectrogram. Import an ordinary
photo and the Studio will treat its pixels as amplitude/phase data and let
you hear it - "sound and image are one continuous surface" is the
project's own design principle for this, not a side effect.

## Starting Out

Launching the Studio shows the **start screen**: a left-aligned header
(the Studio's own logo and name) above two side-by-side columns, each
scrolling independently if its own content doesn't fit the window:

- **Left column**:
  - **New Project...** - opens the [Create Project wizard](#creating-a-project).
  - **Open Project...** - browse to an existing `.smproj` file.
  - **Recent Projects** - your most recently opened projects, click one
    to reopen it directly.
- **Right column**:
  - **Documentation** - Quick Start, Readme, User Guide, Changelog, and
    About - the same five links the Help menu offers once a project is
    open, available here too.
  - **Device Configuration** - the same input/output device, gain, and
    "Test" controls [Configure Devices](#configure-devices) offers once
    a project is open, available here too - so you can pick and test
    your audio devices before creating or opening anything. Either copy
    of these controls stays in sync with the other automatically.

## Creating a Project

The Create Project wizard asks for:

- **Name** - the project's display name and file name.
- **Save Folder** - where the project file and its data folder are
  created (use **Browse...** to pick one).
- **Duration** - roughly how much time the canvas holds, in seconds. This
  sets the canvas's width in practice - importing audio longer than this
  splits it into duration-length pieces you choose from individually (see
  [Importing Audio](#importing-audio)) rather than truncating it.

Click **Advanced** to also set:

- **Sample Rate** - the audio sample rate new imports/recordings are
  encoded at (Hz).
- **Min Frequency** / **Max Frequency** - the frequency range the
  spectrogram covers. Content outside this range isn't represented.
- **Bin Count** - how many frequency bins tall the spectrogram is - more
  bins means finer vertical (pitch) resolution, at the cost of more data
  per layer.
- **Timestep** - milliseconds per horizontal pixel - smaller means finer
  time resolution, and more columns for the same Duration.

The defaults are reasonable for general use; there's no wrong answer to
start with, and nothing here is locked in stone until real data is
imported at that resolution.

## The Main Window

Once a project is open, the title bar shows the project's name next to
"Sound Mind Studio", and the window has:

- **The canvas** (center) - shows the current spectrogram. See
  [Working with Layers](#working-with-layers) for exactly which layer
  that is.
- **Tool dropdown** - a single toolbar button choosing the canvas's
  current tool: **Pan** (the default - navigate the canvas without
  painting, picking, or placing anything), **Paint**, **Pick**,
  **Select**, **Path**, or **Chord**. Its own label always names whichever
  one is active; turning a tool off (rather than choosing a different one)
  falls back to Pan automatically. See each tool's own section below.
- **Layers panel** (right, by default) - lists every layer in the
  project, top of the stack first. Shown by default; toggled from the
  **Layers** toolbar button.
- **Input/Output dropdown** (top-right of the menu bar) - choose
  **Playback**, **Record**, or **Loop** to show that panel; each is
  covered in its own section below. Only one of the three stays open at
  a time - choosing another closes whichever one was open. All three
  start hidden; whichever one you have open (or none) stays that way
  across New/Open Project, within the same run of the Studio.
- **Configure dropdown** - choose **Tool Configuration**, **MIDI
  Configuration**, **Chord Generator**, **Selection Configuration**,
  **Grid**, **Filter Configuration**, or **MindWaves** to show that
  panel; each is covered in its own section below. Unlike Input/Output,
  any number of these can stay open together.
- **Macro dropdown** (top-right of the menu bar, next to Input/Output) -
  **Record Macro**, **Play Macro**, **Export Macro Video...** and
  **Manage Macros...** - see [Macros](#macros) below.
- **File menu** - New/Open/Save/Save As, Remaster (see
  [Remastering](#remastering)), Import Audio/Image, Export Audio/Video.
- **View menu** - Zoom controls (see [Canvas Navigation](#canvas-navigation)
  below), a **Hardware Acceleration** checkbox: turn GPU compute off to
  compare speed/results against the CPU path, or if you suspect it's
  causing a problem (stays as you left it across restarts), **Sound
  Flower** (see [Canvas Navigation](#canvas-navigation) below), and
  **Composer Mode** (see [What's Not Here Yet](#whats-not-here-yet)
  below).
- **Help menu** - Quick Start, Readme, User Guide, and Changelog (each
  opens a styled HTML page in your default browser), plus About Sound
  Mind Studio.
- **Status bar** (bottom) - the left side shows the mouse cursor's
  position while it's over the canvas, both in pixels and in time/
  frequency (e.g. `30, 10 px   |   0.300 s, 523 Hz`), clearing once the
  cursor leaves. A temporary message (e.g. "Imported ...") briefly covers
  it when one is shown.

Every dock panel can be dragged to a different edge of the window, or
floated, like any Qt dock widget. Every panel can also be resized - drag
the edge between two stacked panels (or between a panel and the canvas)
to make one taller/shorter or wider/narrower; if a panel's own content no
longer fits at the size you've given it, a scrollbar appears inside it
rather than the panel refusing to shrink.

By default, opening a panel on the right-hand side adds it as a full-size
tab alongside whatever else is already open there and switches to it
immediately, rather than a smaller panel stacked below the others left
hidden behind whichever tab was already showing - click a tab to switch
between them yourself afterward. Dragging a panel's tab out (to a
different edge, or to float it) breaks it out of that shared tab group
into its own stack, if you'd rather see two of them at once.

## Canvas Navigation

By default the canvas always shrinks or grows to exactly fill the space
available (**Fit to Window**) - the same behavior the Studio always had.
Zooming in switches to a fixed size instead, scrollbars appearing once
the canvas no longer fits.

**Keyboard modifiers** carry one consistent meaning here and (as it's
built out further) anywhere else an action could affect time and
frequency independently: **Alt** restricts to the **frequency axis**
(vertical), **Shift** restricts to the **time axis** (horizontal), and
**Ctrl** is for **proportional** control - both axes together. (Starting a
*new* Select-mode selection is the one exception - Shift/Alt there mean
Add/Subtract instead; see [Selection and Fill](#selection-and-fill)'s own
"Combining selections".)

Zoom, in the **View → Zoom** menu (the four most common of these are also
toolbar buttons):

- **Zoom In**/**Zoom Out** - `]`/`[` - proportional, the normal step.
- **Zoom In (Frequency Only)**/**Zoom Out (Frequency Only)** -
  `Alt+]`/`Alt+[` - stretches or compresses the vertical axis only; the
  horizontal axis doesn't change.
- **Zoom In (Time Only)**/**Zoom Out (Time Only)** - `Shift+]`/`Shift+[` -
  the horizontal-axis counterpart.
- **Zoom In (Coarse)**/**Zoom Out (Coarse)** - `Ctrl+]`/`Ctrl+[` -
  proportional, at a bigger step than plain `]`/`[`. (Ctrl usually means
  "proportional," but plain `]`/`[` is already proportional by default -
  here Ctrl means a bigger step instead.)
- **Fit to Window** - `Ctrl+0` - back to the default, always-fills-the-
  space behavior.
- **Actual Size** - `Ctrl+1` - exactly 100%, one screen pixel per encoded
  pixel.
- The **mouse wheel**, over the canvas, zooms the same way under the same
  modifiers (Ctrl/Alt/Shift). An unmodified wheel scrolls the canvas
  instead, once it's larger than the visible area.

### Number fields

Every number setting is a **slider with a text box** beside it:

- Drag the slider to pick any value in the allowed range. The text box
  previews the value as you drag; it's applied when you let go.
- Click the slider or use the arrow keys (once the slider has focus) to
  nudge the value - that applies straight away.
- Type an exact value in the text box. Nothing changes until you press
  **Enter** or click elsewhere. Typing is forgiving: surrounding spaces,
  leading or trailing zeros, and a unit after the number ("12 dB") are
  all fine, and either `.` or `,` works as the decimal mark. Values
  outside the allowed range are clamped to it, and anything that isn't a
  number (e.g. "1.2.3") just reverts. The box then shows the value it
  actually used.
- A few sliders cover only the useful part of a wider range, so they're
  easier to set finely: **Brush Size** and **Interval** in Tool
  Configuration. You can still type a larger value into the text box; the
  slider then just sits at its end.

**Edit → Decimal Separator** chooses whether numbers are shown with a
period (1.5) or a comma (1,5). It's saved with the project.

Wherever you set a loudness level - a stroke's **Left/Right Intensity**,
Spectral Gate's **Noise floor** - a small **dB / value** drop-down beside
those fields switches how they're shown: in dB (-96 to 0), or as a value
from 0 (silence) to 1 (full scale), the same scale the canvas's
brightness uses. It's only a display choice; switching never changes the
level itself.

### Brightness and clipping

The canvas's brightness follows how loud each spot really is, on one
absolute scale: black is silence (-96 dBFS), full brightness is full scale
(0 dBFS). Imported, recorded, and painted content all use this same scale.

**View → Show Clipping** marks every spot louder than full scale with a
magenta/black checker. Those spots will distort when played or exported
- full brightness can't show how far over they are, so this is how to
find them.

### Sound Flower (Polar View)

**View → Sound Flower** (`Ctrl+Alt+F`) is a checkable action that projects
the canvas onto a circular disk instead of the familiar flat rectangle -
time wraps around the ring (twelve o'clock = the very start, advancing
clockwise) and frequency radiates outward from the centre (lowest at the
centre, highest at the outer edge). It's purely a different way of looking
at, and working with, the exact same data - toggling it never modifies
your project, and it isn't saved with it (a project always opens back in
flat view).

Every paint tool, Pick, Selection, Path, and Chord Stamp still work while
Sound Flower is active - clicks and drags are automatically translated
between the disk and the same underlying canvas coordinates flat view
uses, so a stroke painted on the flower lands in exactly the same place a
stroke painted on the flat view would. The playback position indicator
becomes a rotating ray from the centre instead of a moving vertical line.
A MindWave preview (see [MindWaves](#mindwaves)) warps onto the disk the
same way the canvas itself does, so it's still visible while you work.

A few things are hidden while Sound Flower is active, rather than drawn
incorrectly: Overlay Grids, the Chord Overlay, Axis Labels, Show bounding
boxes/path geometry, and a Rectangle-shaped Pick or Selection's own
highlight box (a Lasso selection's own curve still shows correctly).
Snap to Grid is also suspended while it's active. All of this returns
the moment you switch back to flat view.

### Polar Image Import

You can import a source image that's already encoded in polar form - a
spectrogram exported as a circular flower from elsewhere, or any image
whose content radiates from a centre point. The Image Import wizard
un-warps it back to a rectangular layer automatically.

1. Open **File → Import Image...** and choose a single image file (Polar
   is only offered for a single file - a multi-file import has no one
   source image to preview or pick against).
2. In the size-mode list, select **Polar - un-warp a polar/flower-shaped
   image to rectangular**.
3. Click **Set origin...**, next to that option, to open the graphical
   picker.

The picker shows your source image with four draggable handles:

| Handle | Appearance | What it controls |
|--------|-----------|-------------------|
| **Crosshair** | Yellow lines spanning the whole preview | The flower's own centre |
| **Ring** | Yellow circle, dragged from its own top point | The maximum sampling radius |
| **Arc start** | Blue square on the ring | Where the unwrapped image's own left edge samples from (0° = twelve o'clock, clockwise) |
| **Arc end** | Pink square on the ring | Where its own right edge samples from - equal to Arc start means a full circle |

Every handle has a matching number field (Origin X/Y, Radius, Arc start/end in
degrees) kept in sync both ways - drag a handle and its own number field
updates live; type a value and the handle jumps to match. An **Output
width** field sets the unwrapped layer's own width in pixels - it starts
out tracking the radius automatically (roughly enough pixels for square
arc pixels) until you type a value directly, after which it stays put
regardless of further radius changes. A duration label next to it shows
how long the resulting layer will play at your project's own timestep.

Click **OK** in the picker, then **OK** in the wizard, to import. The
unwrapped result becomes a new layer, exactly like any other image
import - sized to your project's own canvas height and the output width
you chose.

## Importing Media

### Importing Audio

**File → Import Audio...** prompts for an audio file - WAV, MP3, FLAC, Ogg,
AIFF, M4A, or Opus. If it's no longer than your project's Duration, it's
imported directly as one new layer. If it's longer, it's split into
consecutive Duration-length snippets and you're shown a list to
check/uncheck - only the checked snippets become layers, named
`<filename>_0000`, `<filename>_0001`, and so on.

An **Offset** field above that list discards that much audio off the very
start before splitting, so the whole split restarts fresh at that point
instead of the grid always being anchored to the file's own beginning -
useful for lining up a specific moment in the recording with a snippet
boundary. Changing it recomputes the list live; every row resets to
checked, since a changed offset is a genuinely new split.

Importing runs in the background - you can keep working while it encodes,
and a small **✕** button appears in the status bar to cancel it if you
change your mind; cancelling adds nothing to the project at all, as if it
had never been started. New/Open Project and closing the Studio are all
disabled while an import is running, so it always finishes (or gets
cancelled) against the same project it started with.

### Importing Images

**File → Import Image...** lets you select one or more image files
(PNG/JPG/JPEG/BMP/TGA/WebP), then asks how to fit each to the project's
canvas dimensions:

- **Rescale to fit project** - stretched to exactly fill the canvas,
  ignoring the source's own aspect ratio. The default.
- **Scale vertically to fit project, keep horizontal resolution** - height
  matches the canvas; width stays the source's own native pixel width.
- **Scale horizontally to fit project, keep vertical resolution** - the
  mirror of the option above.
- **Scale vertically to fit project, rescale horizontal in proportion** -
  height matches the canvas; width scales to preserve the source's aspect
  ratio.
- **Keep native resolution** - no rescaling at all.

If you selected more than one file, an **Import as sequence** checkbox
also appears. Checking it disables the options above (a sequence always
scales proportionally) and lays the images out end-to-end in time instead
of stacking them independently - files are sequenced in alphabetical
order regardless of the order you selected them in, which makes a set of
files like `frame001.png`, `frame002.png`, ... land in the right order
automatically.

### Importing MIDI

**File → Import MIDI...** prompts for a Standard MIDI File (`.mid`/`.midi`).
Unlike audio or image import, a MIDI-imported layer stays fully editable
afterward - every note is Pickable and modifiable, since it's stored as a
real, non-destructive paint operation rather than a fixed picture.

If the file has just one channel and fits within your project's own
Duration, it imports immediately as one new layer named
`<filename> - Ch<N> (<instrument name>)` - no picker, nothing to choose.
Otherwise, a picker lets you:

- Check which channels to import (each row shows the channel number, its
  instrument name, and how many notes it has).
- Choose **Separate layer per channel** (checked by default) to give each
  channel its own layer, or uncheck it to merge every checked channel into
  one shared layer instead.
- Choose **Import whole file** (checked by default) to import each
  checked channel's entire content as-is, or uncheck it to instead pick
  specific project-length snippet(s) of a longer file - the same
  "snippets" concept **Import Audio...** already uses, listed below the
  checkbox once it's unchecked.

### MIDI Configuration

**MIDI Configuration**, in the **Configure** dropdown, opens a dockable
panel (off by default) mapping a MIDI instrument (a General MIDI
program, 0-127) to one of your saved Tool Presets:

- **Add Mapping** - pick a program from the drop-down beside it, then
  click to add a row for it. Each program can only be mapped once; once
  added, it disappears from the drop-down.
- **Tool Preset** - which saved preset that program's notes paint
  through. Leave it "None" to use the plain default brush (an opaque
  circular Procedural tip) - the same thing every program used before
  this panel existed.
- **Duration Scale** - multiplies how long each note on that program
  lasts (`2.0` doubles it, `0.5` halves it).
- **Pitch Offset (st)** - shifts every note on that program up or down
  by this many semitones (`12` is one octave up, `-12` one octave down).
- **Delete** - removes that program's mapping, making it paint through
  the plain default brush again and putting it back in the Add Mapping
  drop-down.

Every change here applies immediately - there's no separate save step.
A mapping is stored with the project and reused automatically the next
time you import a MIDI file that uses the same program, not just the one
you had open when you set it up. There's still no way to adjust a note's
own strength/velocity - nothing in this build's paint model yet
represents how hard a note was played.

### Drag and Drop

Dragging files onto the main window imports/opens them, by extension, with
the same choices the File menu offers: dropped images show the same
scaling/sequencing picker **File → Import Image...** does; a dropped audio
file (WAV, MP3, FLAC, Ogg, AIFF, M4A, or Opus) with more than one computed
snippet shows the same snippet picker **File → Import Audio...** does, one
picker per such file; a dropped MIDI file (`.mid`/`.midi`) shows the same
channel/snippet picker **File → Import MIDI...** does, whenever the file
actually has more than one channel or snippet to choose from (skipped,
same as the menu action, for a file with nothing to choose); `.smproj`
opens that project (after confirming if your current project has unsaved
changes). Anything else is ignored. **Cancelling any one of these pickers
cancels the whole drop** - nothing in it is imported or opened, even files
unrelated to the dialog you cancelled. If a dropped file fails to import
for some other reason, you'll see a message in the status bar rather than
a popup, so one bad file in a multi-file drop doesn't interrupt the rest.

## Working with Layers

The **Layers** panel lists every layer, top of the stack first. A **+ Add
Layer** button above the list adds a new, empty (silent) layer and
selects it immediately - the only way to get a layer to paint onto from
scratch, rather than from an import. A second button, **+ Add Filter
Layer**, adds a Filter layer instead - see
[Filter Layers](#filter-layers) below. Layer names are kept unique
automatically - adding or renaming a layer to a name that's already
taken appends `" (2)"`, `" (3)"`, and so on.

Each row's name is shown overlaid on a small preview of that layer's own
content (a plain background for a layer type with nothing to preview -
Filter/Background/Equalizer). Any row with real content also shows a
small **loudness indicator** - `avg X dB / peak Y dB` over the whole
layer while playback is stopped, switching to a single live `X dB`
reading at the current position once Playback or Loop starts, and
freezing at whatever it last showed while paused. This is shown whether
or not the row is selected. An unselected row shows only that preview,
the loudness indicator (if any), and a **visibility button** (an open
eye/crossed-out speaker/closed eye icon) - click it to cycle the layer
through
**Visible** (open eye - seen and heard), **Muted** (crossed-out speaker -
still shown on the canvas, but silent during playback), and **Invisible**
(closed eye - excluded from both, same as turning visibility off always
worked); **click a row's name to select it**
(see [Painting](#painting) below - the selected layer is the one a brush
stroke paints into) to reveal its full set of controls:

- A **drag handle** (⠿) to reorder it, or a **lock icon** (🔒) if it can't
  be reordered or deleted - the **Background** layer (always present,
  bottom of the stack) and the **Equalizer** layer (always present, top
  of the stack - see [Filter Layers](#filter-layers) below) are both
  locked, and show their lock icon whether selected or not.
- A **type tag**, for any layer type other than the ordinary kind you get
  from importing.
- A **Gain** slider (the layer's opacity, 0-100%), a **Balance** slider
  (pans this layer's own contribution between left and right - centered
  leaves it completely unchanged; panning either way holds the channel
  you're panning toward at full strength and fades out only the other
  one), each with tick marks and a readout beside it ("75%", "C",
  "L 40%"). Drag freely: the readout follows the handle, and the change
  is applied when you let go (arrow keys apply straight away). Also an
  **opacity-MindWave combo** (see [MindWaves](#mindwaves) below),
  **translation**/**rescale** number fields (see
  [Layer Timing](#layer-timing) below), and a **Blend Mode** combo (see
  [Compositing](#compositing) below) - except on the **Background**
  layer's row, which has none of these: it's always fully opaque and
  always first in time, so none of them apply to it.
- A **duplicate** button (⧉), for any layer except the locked one(s) -
  copies it, and everything about it (content, opacity and its own bound
  MindWave, blend mode, translation/rescale, filter configuration), into
  a new layer, named uniquely and selected immediately.
- A **clean up phase** button (✦), for any selected layer that actually
  has content - zeroes out phase data anywhere the layer is already
  silent (both left and right channels below the display floor).
  Imported audio's own silent stretches often decode with random,
  meaningless phase that shows up as visual noise on the canvas despite
  being completely inaudible; this clears it without touching any cell
  that actually carries sound. A one-time cleanup, not an ongoing effect:
  whatever was painted on the layer becomes part of it. Undo (Ctrl+Z)
  takes it back.
- A **Pool** button (⬇), for any selected layer that actually has content
  (including **Background**, unlike duplicate/clean-up-phase above) - see
  [Pooling a Layer](#pooling-a-layer).
- A **delete** button (×), for any layer except the locked one(s) - also
  appears just by hovering over the row, even without selecting it first,
  so it's never more than a click away. Deleting a layer deletes
  everything painted on it. If Mind Grain strokes on other layers read
  from it, you're asked whether to delete those strokes too or cancel. To keep a layer's content but get
  it out of the way, hide it instead.
- An **edit filter** button (⚙), always visible (no need to select the
  row first) on any **Filter** or **Equalizer** layer - jumps straight to
  that layer's own configuration, selecting it and opening the Filter
  Configuration panel if it isn't already open. See
  [Filter Layers](#filter-layers) below.

Double-clicking a row's name renames it, whether the row is currently
selected or not. A layer whose opacity is bound to a MindWave shows a
smaller, indented row directly beneath it, with that MindWave's own
grayscale preview.

**Right-click any layer's row** for **Hide other layers** - hides every
other layer in the project, remembering each one's own exact prior state
(a layer that was already Muted comes back Muted, not fully Visible).
Once used, the same menu also offers **Unhide other layers**, restoring
them all in one step; both are undoable as a single step too.

**Keyboard shortcuts** (Edit → Layers menu) work without touching the mouse:
**Page Up**/**Page Down** select the layer above/below the currently
selected one; **Shift+Page Up**/**Shift+Page Down** move the selected
layer up/down one position in the stack; **Ctrl+Page Up**/**Ctrl+Page
Down** nudge its opacity up/down by 5%. Each is a no-op where it doesn't
apply - nothing selected, already at the top/bottom of the stack, or
blocked by the locked Background/Equalizer layer.

### Generating a Layer

**+ Generate Layer** (in the Layers panel) adds a new layer filled by a
generator - an ordinary layer afterwards, fully paintable and filterable.
Pick the **Generator**, set its options, and click OK. Every generator
takes a **Seed**: the same seed always reproduces exactly the same result
(**Randomize** picks a new one).

- **Lattice** - an organic, self-similar texture. The **Chaos ↔ Order**
  slider runs from broadband noise (Chaos) through rich, coherent texture
  (the middle) to smooth, rigid structure (Order).
- **Fractal** - a branching tree of tones: one long tone across the canvas,
  with branches sprouting from it at musical intervals, and branches from
  those, each a little quieter. The **Chaos ↔ Order** slider sets how
  orderly it is: toward Order, branches split evenly at octaves, fifths and
  thirds and hold their pitch - a rigid, repeating shape; toward Chaos,
  they sprout at random moments and intervals and slide in pitch; in
  between, the branching melodic shapes it's meant for.
- **Streaming** - eight voices that keep changing over the whole length of
  the layer, each gliding around its own register and swelling and fading,
  all pushing and pulling on each other. Toward **Order** they settle into
  a steady drone; around the middle they keep evolving without losing
  shape; toward **Chaos** they wander restlessly.
- **Noise** - colored noise across the whole canvas:
  - **Color** - **White** (equal energy at every frequency), **Pink**
    (equal energy per octave - the most natural-sounding), **Red /
    Brownian** (deeper, bass-heavy), **Purple / Violet** (bright,
    treble-heavy), **Gray** (sounds equally loud at every frequency - reads
    flat on the canvas), or **Custom spectrum**.
  - **Level** - how loud the noise reads at 1 kHz (for Gray, everywhere),
    in dB or as a 0-1 value.
  - **Horizontal / Vertical resolution** - how many time columns /
    frequency rows one noise "cell" spans: 1 is fine-grained hiss; larger
    values give blockier, slower-changing noise.
  - **Custom spectrum** - draw the noise's shape with the gradient editor
    (lowest frequency at the left, highest at the right; each stop's left
    and right intensity are the two channels' levels).
  - **MindWave bindings** - next to each resolution, and under the custom
    spectrum, pick a MindWave to vary it across the canvas: where the
    MindWave reads 0, the resolution is the finest (1) or the custom
    spectrum is silent; where it reads 1, it's the value you set. Binding
    the custom spectrum to a time-axis MindWave makes the noise swell and
    fade over time.

### Compositing

Every visible layer with content contributes to what the canvas shows
and Playback plays. **Blend Mode** (a drop-down on each layer's own row,
except Background) chooses how: **Normal** mixes layers together rather
than one covering another - two layers at full opacity both come through
in full, exactly like two instruments or voices sounding at once; a
layer's own **Opacity** slider acts as its own volume in that mix, from
silent (`0%`) to full strength (`100%`), rather than making it "more
see-through" the way opacity works in an image editor. **Overwrite**
replaces whatever's beneath a layer set to this mode, faded by that
layer's own Opacity the same way every other blend mode is - full Opacity
replaces it entirely, `0%` leaves it untouched, and values in between mix
the two proportionally. **Multiply**,
**Screen**, **Overlay**, **Difference**, and **Add** are the familiar
image-editor blend modes, each combining a layer with what's beneath it
by the same formula an image editor would, rather than summing real
acoustic energy the way Normal does. Turning a layer's opacity down, or
changing its blend mode, never affects any other layer - each one mixes
in independently. Every layer defaults to Normal, matching how
compositing has always worked.

### Layer Timing

Two more controls, useful once you have multiple layers you want to line
up in time:

- **Translation** - shifts a layer's content earlier or later, in
  spectrogram columns (roughly proportional to time - the exact column
  width depends on your project's Timestep). Positive moves it later,
  negative moves it earlier.
- **Rescale** - stretches (`>1.0x`) or compresses (`<1.0x`) a layer's own
  timeline, for matching the pacing of two clips that don't quite line
  up.

Both affect Playback too, not just what the canvas shows - a layer's
translation/rescale shifts where its own content lands before mixing
into the composite, so "lining up" two layers visually is exactly what
you'll hear lined up as well, in Playback, Loop Mode and every export.

### MindWaves

The **MindWaves** panel (toggle it from the **Configure** dropdown) manages a small
library of reusable waveforms - each one a shape that varies across the
canvas (over time, over frequency, or both) rather than a single fixed
number. A MindWave can bind to a **layer's own opacity** (instead of a
flat volume, the layer fades in and out following the MindWave's own
shape as it plays) or to one of a **Filter layer's own scalar
parameters** (see [Filter Layers](#filter-layers) below) - in both cases
the bound value varies across the canvas following the MindWave's own
shape instead of staying fixed.

- **+ Add MindWave** creates a new one with a sensible default (a plain,
  audible sine wave) and selects it. Double-click a MindWave's name to
  rename it (renaming to a name another MindWave already has offers
  Replace/Rename/Cancel, like every other saved resource in this app -
  Replace here means the *other* MindWave with that name goes away,
  replaced by this one); the **×** button deletes it.
- Every MindWave-binding drop-down (a layer's own Opacity MindWave, any
  Instrument/Filter parameter binding) also offers **Create New
  MindWave...** at the bottom of its list - picking it creates a new
  MindWave the same way **+ Add MindWave** does, binds it immediately,
  and brings this panel into view so you can shape it right away, without
  switching over to this panel first. The option's drop-down then shows
  the new MindWave as selected.
- Every row shows a small grayscale mini-preview of its own field,
  always on - regardless of selection, and independent of the Preview
  toggle below (which does something different - see its own entry). The
  same small thumbnail the Layers panel already shows for a layer with a
  MindWave-bound opacity.
- **Preview** shows the currently selected MindWave's own field directly
  on the canvas, as a live grayscale overlay (black where it's 0, white
  where it's 1) - updating instantly as you change its generator type
  or parameters, and as you select a different
  MindWave while Preview stays on. A judgment aid only - it never affects
  the actual composite, and switches off automatically when you open or
  create a different project, or when you close this panel.
- Selecting a MindWave shows its own editor: a **Generator Type** (Periodic,
  Envelope, Stepped/Noise, Spatial, Fractal, Drawn, Step Grid, Continuous,
  Resonance, or Noise) and that type's own plain numeric parameters - a period, a
  phase, a seed, and so on. This is a bare-bones, functional editor, not a
  polished one yet - there are no dials or drawing tools here, just fields
  to type numbers into. Every MindWave also has **Reversed** (run the
  wave backwards, e.g. a decay becomes a build-up) and **Inverted** (flip
  the result, so loud becomes quiet) checkboxes. Periodic offers a
  **Rectified Sine** shape, and Envelope offers **Peak (Bell)** and
  **Ramp**.
- **Drawn** shapes come from a curve you draw yourself, rather than a
  formula: draw an ordinary stroke anywhere on the canvas, switch to Pick
  and select it, then choose **Edit → Picked Path Tools → Use Picked Path as
MindWave Shape**
  - this applies the picked curve to whichever MindWave is currently
  selected in this panel, switching its own Generator Type to Drawn along
  the way. The shape then loops using the same **Period** control every
  other generator type has, independent of how long the stroke itself took
  to draw, and its own lowest/highest points always map to the field's own
  `0`/`1` range regardless of where on the canvas you drew it. The Drawn
  editor itself shows only a status line (how many points were captured,
  or instructions if nothing has been yet) - there's nothing else to type
  numbers into for this generator type.
- **Step Grid** cycles through a hand-typed list of values instead of a
  formula or a drawn curve: a **Steps** number field sets how many, and each
  one gets its own plain value box (`0`-`1`) below it - a fresh Step Grid
  starts with four steps rising from `0.25` to `1.0`. Loops using the same
  **Period** control every other generator type has, one equal-length
  segment per step. There's no visual grid to click on yet - just the
  numbered list of value boxes.
- **Continuous** offers three controls instead of picking a generator
  type by hand: **Shape** sweeps from a clean sine cycle (`0`) toward
  fractal noise (`1`); **Skew** biases the cycle earlier or later (`0.5`
  is no bias); **Character** layers fine turbulence on top, independent of
  Shape - it does something even at Shape `0`. Each one has both a real
  turnable dial (drag up/down to adjust by ear) and an exact numeric field
  beside it - edit either one and the other updates to match. A live
  preview graph above all three plots the actual resulting shape, updating
  as you turn any dial or the shared Period/Phase fields above them. All
  three values are saved with your project, so reopening one shows them
  exactly where you left them.
- **Resonance** repeats a captured Resonance profile's own spectrum
  shape once per period, the same way Step Grid repeats a hand-typed list -
  a **Resonance Profile** drop-down picks which captured profile to use (see
  [Pick](#pick)'s "Create Resonance from Picked Path..." for how
  to capture one). With nothing picked yet, or nothing captured in the
  project at all, the generator produces a flat neutral value instead.
- **Noise** fills the canvas with cells of random values - **Cell Width**
  (seconds) and **Cell Height** (frequency rows) set how coarse they are.
  The **Color** decides how the values move from one cell to the next along
  the MindWave's axis: **White** jumps freely, **Pink** wanders, **Red /
  Brownian** drifts slowly and smoothly, **Purple / Violet** flickers back
  and forth, and **Custom slope** lets you dial anywhere from -2 (red)
  through 0 (white) to +2 (purple). The **Seed** picks which random values
  you get.
- **Warp** lets one MindWave distort the position another is sampled at,
  giving a wavier, less mechanically regular result than either shape
  alone - check **Enable Warp** to reveal a **Strength** number field and a
  small nested editor for the warp source (its own generator type and
  parameters, edited the same way as a top-level MindWave). Higher
  strength values push the distortion further; unchecking Enable Warp
  removes it entirely.
- Back in the **Layers** panel, each row's opacity-MindWave combo lets you
  bind that layer's opacity to any MindWave in the library, or set it back
  to **None** for a plain, fixed opacity. A binding multiplies the layer's
  own opacity slider by the MindWave's own shape at every point - it
  doesn't replace the slider, so both still matter together.

### Filter Layers

A **Filter layer** doesn't hold its own painted content - instead, it
composites every visible layer beneath it (down to the next Filter layer
below it, if there is one) and reshapes the result. Click **+ Add Filter
Layer** to add one; it's tagged **Filter** in its own row, and selecting
it (click its name, the same as any other layer) opens the **Filter
Configuration** panel with its own controls.

The Filter Configuration panel (also reachable directly from the
**Configure** dropdown) is available even before you've added a
Filter layer at all - dial in the settings you want first, then click
**+ Add Filter Layer**, and the new layer lands already configured that
way rather than starting from scratch. Whatever you last set stays in
place for the *next* Filter layer too, so adding several in a row can
start each one from the same settings.

**Loop Preview** (just under the Filter Preset row) lets you tune a filter
by ear. Click it and a few seconds loop over and over with the filter
applied: your selection's time range if you have one, otherwise about 4
seconds from the playback position. Every change you make in the panel is
heard from the next time round the loop. What you hear depends on what's
selected:

- **A Filter (or Equalizer) layer selected** - the whole project, with that
  layer using the settings you're dialling in: exactly what playback will
  sound like.
- **Otherwise, with a canvas selection** - the filter applied inside the
  selection only, the way **Apply Filter to Selection** would, but without
  changing anything until you actually apply it.
- **Neither** - the filter applied to the whole mix, as if a Filter layer
  sat at the top.

Click **Loop Preview** again to stop. Starting normal playback, or opening
another project, stops it too.

The Tool, Filter and Grid panels all show the **active preset** just under their preset row: "Preset: <name>" after you load or save one, and "Preset: <name> * (modified)" in orange as soon as you change any setting afterwards.

A **Filter Preset** row sits above the Filter Type dropdown - **Save...**
stores the panel's own current, complete configuration (whichever type is
selected, with every one of its own settings) under a name you choose
(if that name is already taken, you're asked to Replace the existing
preset, pick a different name, or cancel), and the drop-down beside it
lists every Filter Preset you've saved;
picking one loads it immediately (switching Filter Type if needed) and
the drop-down resets to its own placeholder, ready to pick again. Saved
presets also show up in the **Resource Browser**'s own Filter Preset
category, where they can be exported to a `.smfilter` file and imported
into another project.

The Filter Configuration panel starts with a **Filter Type** dropdown -
every new Filter layer starts as **Frequency-Axis Gradient**. Choosing a
different type shows only that type's own controls below the dropdown;
switching back and forth doesn't lose whatever you entered into a
group's own controls while it was hidden.

- **Frequency-Axis Gradient** shows a draggable gradient bar spanning
  low frequency (left) to high frequency (right), the same gradient
  editor Painting's own brush and Fill Selection use: click a stop on
  the bar to select it and edit its **Left Intensity**/**Left
  Opacity**/**Right Intensity**/**Right Opacity** below, double-click
  anywhere on the bar to add a new stop there, and drag an interior stop
  left/right to move it (the two endpoint stops, at the far left/right,
  can't be moved or deleted - **Delete Stop** is disabled while one of
  them is selected). At each frequency, the composite's own loudness
  blends toward the nearest stops' own interpolated Intensity, by their
  interpolated Opacity (`0` leaves it untouched, `1` forces it all the
  way to Intensity). **Link Channels** mirrors every edit onto both
  Left/Right fields at once, for the common case of an identical stereo
  effect. A fresh Filter layer starts fully transparent (every stop at
  `0` opacity) - it does nothing until you raise an Opacity.
- **Uniform Blur** softens evenly in every direction - one **Sigma**
  control (the blur's own strength, in bins/columns).
- **Edge-Preserving Blur** softens without smearing across a sharp
  boundary the way Uniform Blur would - one **Size** control (the
  window's own size, in bins/columns).
- **Directional Blur** softens along one direction only - **Length**
  (how far the smear reaches) and **Angle** (0° smears along time,
  90° along frequency).
- **Sharpen** is Blur's own opposite, tightening detail instead of
  softening it - one **Amount** control (higher pushes further).
- Each of those five controls (**Sigma**, **Size**, **Length**, **Angle**,
  **Amount**) has its own MindWave combo right beside its number field - the
  same **None**-or-pick-a-MindWave choice as a layer's own opacity
  binding (see [MindWaves](#mindwaves) above), but with a genuinely
  different effect here: binding one of these varies *that filter's own
  parameter* across the canvas, so the filter itself computes a different
  blur/sharpen at every cell instead of applying one fixed strength
  everywhere. The number field still sets that parameter's ceiling value
  (what the MindWave's own peak maps to); **None** leaves it at a flat,
  unvarying value the way it always worked before.
- **Tone Curve** remaps loudness through a curve you draw yourself: click
  anywhere on the curve area to add a point, drag a point to move it, and
  double-click a point (other than the two fixed endpoints) to remove it.
  The curve always passes exactly through every point, smoothly and
  without overshooting past a point's own neighbors, however steep the
  curve gets between them. A fresh Filter layer of this type starts as a
  straight diagonal line (input equals output - no effect) until you
  drag a point.
- **Speckle Add** randomly boosts a scattering of cells toward full
  loudness - **Density** (how many cells get hit) and **Intensity** (how
  far they jump toward full loudness when hit). The pattern is fixed for
  a given Filter layer - it won't change on its own from one repaint to
  the next, only if you touch these controls or the content underneath.
- **Speckle Remove** cleans up isolated loud/quiet flecks without
  softening real detail the way Edge-Preserving Blur would - one
  **Threshold** control (how far a cell has to stand out from its own
  immediate surroundings before it's corrected).
- **Denoise** quietens anything below a loudness floor - **Noise Floor**
  (the threshold) and **Reduction** (how much quieter anything below it
  gets).
- **Bit-Depth Crush** is a lo-fi, "crunchy" quantization effect - one
  **Amount** control (`0` is no effect at all; higher values quantize
  loudness into fewer and fewer discrete steps).
- **Granular Noise** overlays a coarse, blocky noise texture - **Grain
  Size** (how big each block is) and **Grain Amount** (how loud the
  texture is). Like Speckle Add, its own pattern stays fixed until you
  change these controls or the content underneath.
- **Dynamic Speckle** is Speckle Add's live counterpart - the same
  **Density**/**Intensity** controls (editing either Speckle Add's or
  Dynamic Speckle's own copy updates both, since they're the same
  setting), but the pattern genuinely re-randomizes on every repaint
  instead of staying put - a flickering, static-like texture rather than
  a fixed one.
- **Feedback Distortion** pushes a passage into a resonant, decaying
  smear along time - one **Amount** control (higher rings/smears longer).
- **Spectral Wavefold** is a classic wavefolder distortion - one **Fold
  Gain** control (`1` is no effect; higher values fold loudness back on
  itself repeatedly for a harsher, more harmonically dense sound).
- **Channel Balance** redistributes loudness between the left and right
  channels - one **Balance** control (`0` sends everything to the left
  channel, `1` to the right; `0.5` leaves an already-balanced signal
  untouched, but isn't a no-op if the two channels differ - loudness is
  redistributed from their shared total, not scaled independently).
- **Invert** flips loudness inside out - quiet becomes loud, loud becomes
  quiet. No controls of its own.
- **Convolve** applies an arbitrary, hand-edited convolution kernel over
  the spectrogram:
  - **Preset** picks one of eight classic starting kernels (Identity,
    Sharpen, Edge Detect, Emboss, Box Blur, Gaussian Blur, Sobel X, Sobel
    Y) and drops it straight into the grid below, ready for further
    hand-editing - it's a one-time starting point, not a sticky choice,
    so the dropdown resets itself right after.
  - **Kernel Size** sets the grid's own side length (always odd - an even
    value rounds up); changing it replaces the current kernel with a
    fresh, blank (identity) one of the new size rather than trying to
    preserve the old values.
  - The grid itself is a size-by-size block of editable cells - each one
    a coefficient the corresponding neighboring pixel is weighted by.
  - **Normalize** divides the kernel by the sum of its own positive
    coefficients first, so a pure-positive kernel (a blur) doesn't
    brighten or darken the image overall.
  - **Amount** is a dry/wet mix - `0` is no effect regardless of the
    kernel, `1` is the fully convolved result.
  - **Save As New Kernel** asks for a name (defaulting to "Kernel 1",
    "Kernel 2", and so on, still editable; a name that's already taken
    offers Replace/Rename/Cancel the same way Filter Preset's own Save...
    does) and stores the current grid permanently under it, for reuse
    later - on this Filter layer, a different one, or a different project
    entirely. **Load
    Saved Kernel** drops a previously saved kernel's own values into the
    grid (again, a one-time copy you can keep editing, not a live link -
    editing it afterward doesn't change the saved copy, and deleting the
    saved copy later wouldn't affect this Filter layer).
- **Displace** shifts content sideways from a source position - **Distance**
  (how far) and **Angle** (`0`° shifts along time, `90`° along frequency),
  smoothly blended between cells rather than jumping in whole steps.
- **Channel Cycle** continuously rotates left loudness, right loudness, and
  phase into each other - one **Angle** control (`0`°/`360`° is no effect;
  every `120`° is one full step of the rotation, with anything in between
  blending smoothly). This is a wild, glitchy creative effect - the result
  won't sound or look like a "corrected" version of the original, and
  that's the point.
- **Spectral Reverb** extends a sound through time the way a physical
  space would:
  - **Pre-Delay** and **Decay** (both in frames) shape the reverb tail's
    own timing - Pre-Delay is a gap before the tail begins, Decay is how
    long the tail itself takes to fade out.
  - **Room Size** scales Decay down - a smaller room decays faster than
    the full configured length.
  - **Diffusion** smears the tail across nearby frequencies, for a
    smoother, less metallic-sounding reverb.
  - **Absorption** damps higher frequencies faster than lower ones, the
    way a real room's own walls and air would.
  - **Mix** blends the reverberated result back in with the original -
    `0` is no effect at all, `1` is fully wet.
- **Downsample** is a "pixelate" effect - reduces effective resolution
  over square blocks:
  - **Mode** picks how a block's own value is chosen: **Block Hold** reads
    the block's own top-left corner cell, for the classic, hard-edged
    blocky look; **Block Average** takes the mean of the block's own
    cells instead, for a smoother, more "lo-fi resample" feel.
  - **Block Size** sets how big each block is, in bins/columns - `1` is no
    effect at all.

Every one of the twenty-one designed filter types now has a real, working
algorithm, completing this milestone. Every one of them except Invert (no
parameters of its own) also has at least one MindWave-binding combo, the
same "None" or any MindWave `setAvailableMindWaves()` currently lists you
already know from Sigma/Size/Length/Angle/Amount above - Speckle Add's and
Dynamic Speckle's Density and Intensity, Speckle Remove's Threshold,
Denoise's Noise Floor and Reduction, Bit-Depth Crush's Amount, Granular
Noise's Grain Amount, Feedback Distortion's Amount, Spectral Wavefold's
Fold Gain, Channel Balance's Balance, Convolve's Amount, Displace's
Distance and Angle, Channel Cycle's Angle, Spectral Reverb's Mix, and
Downsample's Block Size. Three parameters have no binding combo,
deliberately - Convolve's Kernel Size, Granular Noise's Grain Size, and
Spectral Reverb's Pre-Delay/Decay/Room Size/Diffusion/Absorption - each
shapes a fixed-size grid or a computation spanning many cells at once,
not a single value one cell owns on its own. Downsample's own Block Size
*is* bindable, unlike Granular Noise's near-identical-sounding Grain
Size - the difference is what a MindWave-bound value would mean: Granular
Noise's own per-block *random* offset has no well-defined per-cell
version, while Downsample's block size is a plain kernel-shape parameter
(the same treatment Sigma/Size/Length already get), so it can genuinely
vary smoothly across the canvas.

A Filter layer with nothing beneath it (or with everything beneath it
hidden) has nothing to filter, so it has no effect. An ordinary Filter
layer isn't locked - it can be reordered or deleted like a Normal layer,
and reordering one changes exactly what it composites (everything
between it and the next Filter layer down).

**Every project has one special Filter layer: the Equalizer**, always
present, locked at the very top of the stack (it can't be reordered or
deleted - see [Working with Layers](#working-with-layers) above). It
starts with no effect (its Cut is `0` everywhere). Selecting it opens the
Filter Configuration panel in a dedicated mode: no Filter Type
drop-down (the Equalizer is always Frequency-Axis Gradient underneath,
so there's nothing to switch), just the same draggable gradient bar
Frequency-Axis Gradient uses, but with both Intensity fields hidden and
the Opacity fields relabeled **Left Cut**/**Right Cut** - `0` leaves that
frequency untouched, `1` silences it completely. Unlike the ordinary
Frequency-Axis Gradient editor, there's no Intensity to set here - the
Equalizer only ever cuts toward silence, never toward some other target
loudness. It's a real multi-stop gradient, the same as any other - drag
the bar to add, move, or remove cut points, not just a single start/end
value.

Alongside the gradient bar sits a **vertical EQ curve** - a frequency-
response-style plot of the same curve, oriented to match the canvas
beside it (highest frequency at the top, lowest at the bottom). Drag a
point on it up or down to retune which frequency it affects; click empty
space to add a new point there; double-click an interior point to remove
it. It's kept in sync with the gradient bar automatically - edit either
one and the other updates to match. Check **Preview on Canvas** to
overlay the curve's own effect directly on the spectrogram (darker means
more cut), so you can see what the Equalizer is about to do before
committing to it; the checkbox stays checked for the rest of the session
even if you switch to a different layer and back.

## Painting

Choose **Paint** from the **Tool** dropdown to switch the canvas into
paint mode; choose **Pan** (or any other tool) to leave it. While it's on,
dragging on the canvas draws a stroke - you'll see it live as a black-outlined white
line while drawing (visible over any painted color underneath, including
a matching one), and it's applied to the spectrogram once you release
the mouse.

**Which layer gets painted**: whichever layer's name you last clicked in
the Layers panel (see [Working with Layers](#working-with-layers)). If
you haven't clicked one yet, painting targets the **Background** layer -
it's a real, paintable canvas like any other, not just a fixed floor to
import onto (never the locked Equalizer layer at the top, which has
nothing to paint onto in the first place).

**Sound-mode vs. Image-mode**: **Edit → Image Mode** toggles how a brush
stamp's own shape is measured on the frequency axis, for the Procedural,
Heal, Soften, Smudge, and Order/Chaos tools. Unchecked (Sound-mode, the
default), a stroke's frequency-axis extent is fixed in Hz - since the
canvas's vertical axis is logarithmic, a circular stamp actually reads as
a wide oval near the top of the frequency range and a tall egg shape near
the bottom. Checked (Image-mode), a stamp instead keeps a fixed *pixel*
footprint on the canvas - a circle looks like a circle no matter where you
paint it (with **Fit to Window** zoom, the canvas keeps square cells in
Image-mode, with black bars filling any leftover space, so round stays
round on screen), and Along Curve / Frequency Axis stamp spacing is
counted in canvas pixels, so it's even across the whole canvas - at the
cost of a moved/painted sound's own pitch relationships
no longer lining up the way they would in Sound-mode. This is saved with
the project, so it stays set the way you left it. Switching modes never
changes anything you've already painted - only new strokes reflect it.

**Tool Configuration**, in the **Configure** dropdown, opens a dockable
panel (off by default) with the brush's own settings:

- **Tool Preset** - a drop-down at the top of the panel listing every
  preset saved in the current project, empty ("(none saved yet)") until
  you save your first one. Picking one loads every setting below - Tool
  Type and everything under it - at once. **Save...** prompts for a name
  (a name that's already taken offers Replace/Rename/Cancel, the same
  as every other saved resource in this app) and stores the panel's
  *current* settings as a new preset; **Delete**
  removes whichever preset is currently selected (no confirmation - like
  removing a saved MindWave, this never affects anything already painted
  with it). There's no Wizard yet (see "What's Not Here Yet" below) - this
  is a plain save/load of the panel as it stands. A saved **Instrument**
  or **Resonance** preset shows a small icon of its own
  harmonic/spectral content next to its name, so you can recognize one at
  a glance without loading it first; other preset types show no icon.
  Next to Save/Delete, **Preview** plays the panel's own *current*
  settings - whether or not you've saved them yet - as a representative
  3-second stroke, so you can audition a brush before painting with it;
  it becomes **Stop** while playing.
- **Tool Type** - **Procedural** (the default), **Instrument**,
  **Resonance**, **Mind Shot**, **Mind Grain**, **Heal**,
  **Soften**, **Smudge**, or **Order/Chaos**; picks which set of controls
  below applies (Tip Shape for Procedural, Harmonics/Inharmonicity for
  Instrument, a picker plus Decay Rate/Frequency Scale/Time Span for
  Resonance, a picker
  for Mind Shot/Mind Grain, an Amount slider for Order/Chaos -
  Heal/Soften/Smudge add no controls of their own at all, see their own
  entries below). The panel
  also hides whichever of the shared controls below (Falloff, Brush Size,
  Stamp Mode, Stamp Interval, Color, Opacity, Blend Mode) the selected type
  doesn't actually use - see each control's own entry for which types hide
  it.
  Switching still keeps every shared value as it was underneath, even a
  currently-hidden one - only the type-specific controls reset to the
  newly-picked type's own defaults.
- **Tip Shape** (Procedural only) - the stroke's cross-section: Circle,
  Square, Diamond, Triangle (pointing up), Single Stroke (one diagonal
  bar), Cross (+), Star (five points), Corner (an L), Arc (the top half of
  a ring), Dot Spatter (scattered small dots), or Dapple (a few large soft
  blotches).
- **Thickness** (Procedural only, for the shapes it applies to) - the line
  width of Single Stroke, Cross, Star, Corner and Arc, or the dot size of
  Dot Spatter and Dapple, as a fraction of Brush Size. Hidden for Circle,
  Square, Diamond and Triangle.
- **Harmonics** (Instrument only) - how many overtones above the
  fundamental to synthesize, each with its own **strength** number field
  (fundamental first) - a fresh Instrument starts with a plausible
  falling four-harmonic series (`1.0, 0.5, 0.25, 0.125`). Each harmonic
  paints as a single exact-frequency spike rather than a soft geometric
  blob - a stroke's own Falloff/Size still soften and bound it, but only
  along the *time* axis (how the stroke fades in/out as you paint it),
  not across frequency. Alongside the numeric strength fields sits a
  **draggable bar chart** of the same series - click or drag any bar to
  set that harmonic's strength by eye/ear instead of typing a number; the
  two stay in sync, so editing either one updates the other.
- **Inharmonicity** (Instrument only) - stretches the harmonic series
  sharp of a pure integer series, the way a real vibrating body's own
  overtones do (a piano string, for instance). `0` (the default) is
  perfectly harmonic; small positive values (try `0.01`-`0.05`) give
  higher harmonics an audibly metallic, bell-like stretch.
- **Vibrato**/**Tremolo** (Instrument only) - each a depth number field paired
  with a MindWave bind combo. Binding a MindWave makes it modulate pitch
  (Vibrato, in semitones) or strength (Tremolo, as a fraction toward
  silence) across the course of a stroke, following that MindWave's own
  shape - a longer stroke shows more of the shape, a short one only a
  sliver of it. Set back to **None** for no modulation at all, regardless
  of the depth value. This tracks the stroke's own position along itself,
  not real elapsed time, so the same MindWave shape always plays out fully
  over any one stroke, however long or short it is.
- **Resonance Profile** (Resonance only) - a
  drop-down of every Resonance profile you've captured so far (see
  [Pick](#pick)'s own "Create Resonance from Picked Path..."),
  empty until you capture your first one. Paints the captured spectrum as
  a series of partials above the stroke's own pitch, the same spirit as
  Instrument's own harmonic series, but with each partial's own strength
  coming from the curve's own shape instead of hand-typed numbers. Unlike
  every other paintable tool, Resonance has no Falloff dial of its own
  (fixed at a hard edge) and replaces the shared Brush Size with two
  controls of its own:
  - **Decay Rate** controls how quickly loudness decays across the
    stroke, as if the curve were struck once at the stroke's own start and
    left to ring out - `0` (the default) is full, constant sustain for the
    whole stroke; higher values make it fade out faster as you keep
    painting.
  - **Frequency Scale** controls how many octaves above the painted pitch
    the spectrum spreads - the bottom of the spectrum always stays
    exactly on the pitch you painted; a larger value stretches the rest
    of it further upward.
  - **Time Span** controls this brush's own stamp duration, the same role
    Brush Size plays for every other tool.
- **Mind Shot** (Mind Shot only) - a drop-down of every Mind Shot you've
  captured so far (see [Selection and Fill](#selection-and-fill)'s own
  "Capture as Mind Shot"), empty until you capture your first one.
  Painting with it stamps the captured content back at its own original
  size, not scaled by Falloff/Size the way Procedural/Instrument are -
  combined with what's already there per its own **Blend Mode** control
  (Overwrite by default, reproducing the original "stamps back exactly
  as it was captured" behavior). If the picked Mind Shot has a
  Fundamental Frequency set (see "Capture as Mind Shot"), each stamp is
  also pitch-shifted to match wherever it lands - painting the same Mind
  Shot at a higher or lower pitch on the canvas genuinely changes its own
  pitch, not just its position; a Mind Shot with no Fundamental Frequency
  set stamps back at the same pitch everywhere, as it always has. The
  panel hides Falloff, Brush Size, Color, and Opacity for Mind Shot,
  since none of them have any effect on it - only Stamp Mode/Interval
  (which control the stamp's own placement, not its content) still apply.
- **Mind Grain** (Mind Grain only) - a drop-down of every Mind Grain you've
  captured so far (see [Selection and Fill](#selection-and-fill)'s own
  "Capture as Mind Grain"), empty until you capture your first one. The
  opposite of Mind Shot: no pixels are ever captured, only a reference to
  the region and the layer it was selected on - painting with it always
  reads that layer's *current* content, and repainting the source layer
  updates every Mind Grain stroke that reads from it immediately, wherever
  it's painted, not just the next time that stroke's own layer happens to
  be redrawn for some other reason. Combines with what's already there per
  its own **Blend Mode** control, same as Mind Shot above.

  **A Mind Grain can only paint onto a layer above its own source** - the
  panel won't even let you attempt it on the wrong layer: the Mind Grain
  group turns red (with a tooltip explaining why) whenever the active
  layer isn't above the configured grain's source, the Layers panel marks
  every disallowed layer with a red **✕**, and the Tool dropdown's own
  **Paint** entry is disabled (its own tooltip explains why) - switch to a layer
  higher in the stack, or reorder the layers, to paint with it. Reordering
  or deleting a layer that would break an *already-painted* Mind Grain
  stroke's own ordering is refused outright too, with a dialog explaining
  which stroke(s) would break and why. The panel hides Falloff, Brush Size,
  Color, and Opacity for Mind Grain, the same as Mind Shot.
- **Heal** (Heal only, no controls of its own) - a temporal blur: instead of
  painting a target color, each pixel blends toward the average of its own
  neighboring cells *in time* (same frequency) - useful for erasing a
  short, stray mark without disturbing the surrounding texture. **Brush
  Size doubles as the blur window** (a bigger brush averages across more
  neighboring moments, as well as covering more canvas), **Opacity is the
  blend strength** (how much of the averaged result replaces the original -
  100% fully replaces it, lower values only partially smooth it), and the
  Gradient editor's Intensity fields hide entirely (there's no fixed
  target color for a blur to paint toward). Repeated or overlapping
  strokes over the same spot blur it further each time, the same way a
  real blur brush would. **Stamp Mode is always Along Curve and Stamp
  Interval is always fixed to 66% of Brush Size** - both hidden from the
  panel, for consistent results; see Stamp Mode's own entry below.
- **Soften** (Soften only, no controls of its own) - the same idea as Heal,
  but blurs uniformly in every direction (time *and* frequency) rather than
  time alone, for a general softening instead of Heal's own
  defect-erasing, single-axis blend. Brush Size/Opacity/Gradient work
  exactly the same way Heal's own do, and Stamp Mode/Interval are fixed
  the same way too.
- **Smudge** (Smudge only, no controls of its own) - drags pixels along the
  direction you're actually dragging the stroke, the way a finger smudges
  wet paint. Brush Size/Falloff/Opacity/Gradient all work the same way
  Heal's/Soften's own do, and Stamp Mode/Interval are fixed the same way
  too; the smear's own direction and reach come from how you're moving the
  stroke itself, not a separate control. A single click (no drag) has
  nothing to smear along, so it does nothing.
- **Order/Chaos** (Order/Chaos only) - pushes a region toward spectral order
  or spectral chaos, via one **Amount** slider from `-1` (full Chaos) to
  `+1` (full Order), `0` (the default) having no effect:
  - **Chaos** (negative) scrambles a random selection of pixel intensities
    within the brush - at `-1`, every pixel in the stroke's own footprint
    gets randomly reshuffled among themselves, leaving the overall loudness
    and color of that area unchanged, just rearranged.
  - **Order** (positive) finds the loudest moment and loudest frequency
    within the brush and pulls a random selection of pixels toward them -
    the loudest pixels end up closest, the quietest end up farthest,
    building up horizontal and vertical structure. Audibly, this can turn
    noise into a steady tone, sharpen a transient, or steady a wavering
    pitch.

  Smaller Amount magnitudes affect a smaller, randomly-chosen fraction of
  the brush's own pixels rather than all of them. Opacity still works the
  same way it does for Heal/Soften/Smudge - how strongly each affected
  pixel's own new value actually replaces the original - while Amount
  itself controls how much of the brush participates at all; the Color
  swatch has no effect, and Stamp Mode/Interval are fixed the same way as
  Heal/Soften/Smudge.
- **Falloff** - how soft the stroke's edge is, from a hard edge (`0`) to
  fully soft (`1`). Hidden for Mind Shot/Mind Grain, which don't use it.
- **Brush Size** - the tip's own radius: in seconds on the time axis, and
  the equivalent frequency span on the frequency axis (using this
  project's own Hz-per-second scale, so the same number always describes
  the same *shape*, not the same pixel size, regardless of canvas
  resolution) - unless **Edit → Image Mode** is checked (see below), which
  changes what that frequency-axis shape actually looks like. The panel's
  own default (`0.2`) is a comfortably visible stroke on a typical project
  without covering too much of it at once. The slider runs up to `5`; type
  a value up to `100` into the box for a bigger brush. Hidden for Mind
  Shot/Mind Grain, which don't use it.
- **Stamp Mode** - how densely the brush's tip is stamped along a stroke's
  path, from a drop-down. **Hidden for Heal, Soften, Smudge, and
  Order/Chaos**, where it's fixed to Along Curve instead - manual testing
  showed these four tools only look consistently good with dense,
  curve-following stamping, so they no longer expose the choice:
  - **Stroke** (the default) - stamped exactly as densely as the path
    itself was drawn, with no gaps; unchanged from how painting has always
    worked. Not an even spacing - drawing slowly stamps more densely over
    a given distance than drawing quickly, and the spacing has nothing to
    do with Brush Size, unlike every mode below.
  - **Along Curve** - stamped at even intervals measured along the path's
    own length, so a longer stroke gets proportionally more stamps.
  - **Time Axis** - stamped every time the path crosses an evenly-spaced
    moment in time (see [Axis Labels](#axis-labels); "Interval" below is
    in seconds).
  - **Frequency Axis** - stamped every time the path crosses an
    evenly-spaced frequency, the same idea rotated onto the other axis
    ("Interval" is in Hz).

  Choosing any mode but Stroke enables the **Interval** number field next
  to it, which sets the spacing (seconds for Along Curve/Time Axis, Hz for
  Frequency Axis - or, with **Edit → Image Mode** on, canvas pixels for
  Along Curve and frequency bins for Frequency Axis; the field's own unit
  label always says which) - it's disabled and ignored under Stroke, where
  an interval wouldn't mean anything. Its slider runs from 0 to the size of
  the open project in that unit: the project's whole duration in seconds,
  its width in pixels, or the full height of the frequency axis in Hz or
  bins. Type a value into the box to go beyond that. This is the same idea as the legacy
  Studio's "Curve" tool. For
  Heal/Soften/Smudge/Order-Chaos, Interval is likewise hidden - it's always
  66% of Brush Size, so it stays proportional as Brush Size changes.
- **Stamp Pattern** (Along Curve only) - an optional text field, shown right
  below Interval only while Stamp Mode is Along Curve, for cycling through a
  non-uniform sequence of spacings instead of one fixed Interval. Enter
  whitespace-separated steps, each a number followed by `ms` (milliseconds)
  or `b` (beats, at this project's own tempo) - for example, `100ms 200ms`
  alternates a short gap and a long one along the stroke, repeating for as
  long as the stroke lasts. Leave it blank to use the fixed Interval above,
  unchanged. An invalid entry (a step missing its `ms`/`b` suffix, or any
  other malformed text) shows a red error message underneath and simply
  falls back to the fixed Interval until fixed - it's never rejected
  outright, so an in-progress edit is never lost. Not available for Time
  Axis/Frequency Axis - a repeating pattern has no consistent meaning
  measured against a fixed time/frequency grid the way it does along a
  stroke's own length.
- **Gradient** - the same draggable gradient bar/stop editor Filter
  Configuration's Frequency-Axis Gradient uses (see [Filter
  Layers](#filter-layers) above), here editing the stroke's own
  gradient: click a stop to select it and edit its **Left
  Intensity**/**Left Opacity**/**Right Intensity**/**Right Opacity**,
  double-click the bar to add a new stop, drag an interior stop to move
  it. This is how loud the stroke paints, *and* its stereo balance, at
  once - Left/Right Intensity set each channel's own target loudness,
  Left/Right Opacity set how strongly the stroke actually pushes toward
  it (`0` leaves the canvas untouched, `1` paints at full strength). A
  gradient with more than two stops varies continuously along the
  stroke's own length as you draw it, the same way Fill Selection's own
  gradient varies across a selection. Intensity is in dBFS, from -96
  (silence) to 0 (full scale, as loud as a single tone can be without
  distorting) - the same scale imported and recorded audio is shown on,
  so a stroke as bright as some imported sound is also about as loud.
  The panel's own default (both endpoint stops at -20 dB, full opacity)
  paints clearly audibly on both channels right away, no setup
  required. Hidden for
  Mind Shot and Mind Grain, which don't paint toward a fixed target at
  all; for Heal, Soften, Smudge, and Order/Chaos, only the Intensity
  fields hide - Opacity stays, as the blend's own strength (see each
  tool's own entry below).
- **Opacity MindWave / Size MindWave / Color MindWave** - three MindWave-
  binding drop-downs (**None** or pick a MindWave from the library), one
  per parameter, available for every tool except Mind Shot/Mind Grain
  (which don't use a gradient or a footprint radius at all). Each is
  sampled at every stamp's own real position on the canvas as you paint -
  the same fixed-mask-over-the-whole-piece behavior a layer's own opacity
  binding already uses (see [MindWaves](#mindwaves) below), not tied to
  the stroke's own position along itself the way Vibrato/Tremolo are (see
  below):
  - **Opacity MindWave** multiplies the stamp's own strength at that
    point - dark areas of the field paint faintly or not at all, bright
    areas paint at full strength.
  - **Size MindWave** scales the stamp's own footprint at that point -
    dark areas shrink the stamp down (all the way to nothing, at the
    field's own darkest), bright areas paint at the full Brush Size
    configured above.
  - **Color MindWave** changes *where along the stroke's own Gradient*
    that stamp samples its color/intensity from, instead of the stroke's
    own position - so the same stroke can pick up different colors from
    its own Gradient bar depending purely on where it crosses the bound
    field, independent of drawing order or direction. For Heal, Soften,
    Smudge, and Order/Chaos (whose own Gradient only exposes Opacity, not
    Intensity - see Gradient's own entry above), this still changes which
    stop's own Opacity gets used, just never an Intensity value none of
    those four tools read anyway.
  
  None of the three bind a MindWave by default - painting behaves exactly
  as before this existed until you deliberately pick one.
- **Binding Frame** - **Canvas Space** (the default) or **Operation-
  Relative**, choosing how *all three* of the combos above are sampled.
  Canvas Space samples each field at a stamp's own real position on the
  canvas, as described above - two identical strokes drawn at different
  points look different, since each sees a different part of the same
  fixed field. Operation-Relative instead samples by progress along the
  stroke itself, the same way Vibrato/Tremolo already do (see above) - two
  identical strokes drawn anywhere in the piece look the same as each
  other, since each one sees the bound field's own shape play out fully
  over its own length, however long or short that stroke is.
- **Blend Mode** - how a Mind Shot/Mind Grain stamp combines with what's
  already there: **Overwrite** (the default) replaces it entirely, same
  as before this control existed; **Normal**, **Multiply**, **Screen**,
  **Overlay**, **Difference**, and **Add** blend it in instead - see
  [Compositing](#compositing) above for what each one does. Shown *only*
  for Mind Shot/Mind Grain - every other tool type paints via its own
  Opacity/Falloff gradient blend instead, which has no blend mode choice
  of its own.

Two checkboxes at the top of the panel, both off by default:

- **Show bounding boxes** - outlines (cyan) every stroke's overall extent
  on the currently-shown layer.
- **Show path geometry** - traces (magenta) every stroke's own drawn
  path. A stroke actively being drawn is always traced, regardless of
  this checkbox.

**Undo** (Ctrl+Z) and **Redo** (Ctrl+Y), in the **Edit** menu, apply to
everything you do to a project, in the order you did it:

- importing audio, images and MIDI, recording, Loop takes and generated
  layers;
- adding, deleting, duplicating, renaming and reordering layers, and every
  layer setting (visibility, gain, balance, MindWave binding, position,
  rescale, blend mode, filter settings);
- painting, filling, pasting, text, chords, Pick's edits and restacking;
- pooling and phase cleanup;
- creating, editing and removing MindWaves, Mind Shots, Mind Grains,
  Resonances, kernels, presets and MIDI mappings.

Zooming, scrolling, choosing tools and panels, selections and device
settings aren't part of it.

The **History** panel (toggle it from the toolbar) lists everything in the
project's history, in order, with your current position highlighted.
Double-click any entry to jump straight to it - the same as clicking Undo
or Redo repeatedly, just in one step.

**The history is saved with the project.** After reopening it, Undo still
steps back through what you did before. A step that brings back an
earlier version of a layer's audio (undoing a recording over a layer, a
Loop take or pooling) needs that version, which isn't kept once the
project is closed: the Studio remakes it by replaying the project's
history from its sources (see [Saving and Project Files](#saving-and-project-files)),
which can take a moment. In a project made with a version before 0.1.43.1
that isn't possible, and Undo stops at such a step.

Only **Procedural**, **Instrument** (harmonic series + inharmonicity only so
far - no noise, body resonance, or envelope yet), **Mind Shot**
(capture-and-restamp), **Mind Grain** (a live reference that updates
immediately, everywhere it's used, the moment its source is repainted),
**Heal** (temporal blur), **Soften** (radial blur), **Smudge** (directional
smear), and **Order/Chaos** (spectral order/chaos push) exist today - see
[What's Not Here Yet](#whats-not-here-yet) for the rest of what's planned
around painting.

## Pick

Choose **Pick** from the **Tool** dropdown to switch the canvas into pick
mode; choose **Pan** (or any other tool) to leave it. While it's on,
clicking any painted object - a brush stroke, a filled selection, or a
pasted region - selects it - shown with a white outline around it,
regardless of the "Show bounding boxes" setting - ready to:

- **Move** it - click and drag it to a new position. Moving (or
  modifying) an object never changes where it sits relative to anything
  else - a stroke you cut or filled over stays cut/filled over, even
  after you move or modify it.
- **Modify** it - open **Tool Configuration** (if it isn't already);
  it's pre-filled with exactly the settings the stroke was painted with,
  tool type included. Change anything (tool type, tip shape/harmonics,
  falloff, size, stamp mode/interval, color, opacity) and the selected
  stroke updates to match. Only a brush stroke has settings to reopen this
  way - a filled selection or a pasted region can still be moved and
  deleted, just not "modified" through this panel.
- **Delete** it - **Edit → Delete** (or the Delete key).
- **Restack** it within its own layer - **Edit → Stack Order → Bring to
  Front** (Ctrl+Shift+Up), **Send to Back** (Ctrl+Shift+Down), **Bring
  Forward** (Ctrl+Up), or **Send Backward** (Ctrl+Down). This only changes
  which object renders on top where they overlap - it doesn't move,
  modify, or select anything else.
- **Edit its own path** - **Edit → Path Editing → Edit Path** (a brush stroke or
  a [Path Tool](#path-tool) path; a filled selection, a pasted region or a
  branching path has no single path to edit). Brings its
  nodes and handles back onto the canvas as small dots:
  - **Click a node** to select it (shown larger, in white).
  - **Drag a node** to move it - its own handles (if any) move with it.
  - **Click, then drag, one of the selected node's own handles** (shown
    in cyan, connected to the node by a thin line - only the *selected*
    node's own handles are ever shown) to reshape the curve on that
    side. The opposite handle always mirrors it, to keep the curve
    smooth through the node.
  - **Edit → Path Editing → Toggle Node Type** switches the selected node between a
    smooth curve point and a sharp corner. Converting to smooth extends
    its own two handles out a comfortable distance, rounding out the
    corner right away - drag either handle further from there to reshape
    it more.
  - **Delete key** removes the selected node (refused if it's the only
    node left).
  - **Edit → Path Editing → Apply Path Edit** commits everything changed so far as a
    new, undoable edit to the stroke. **Edit → Path Editing → Cancel Path Edit**
    discards it instead, leaving the stroke exactly as it was.

**Selecting several objects at once:** drag from an empty part of the
canvas to draw a dashed rectangle. Every object lying entirely inside it
(on the active layer) is selected, each one highlighted. Then:

- **Delete** removes them all.
- **Drag any one of them** to move them all together.
- **Edit → Clipboard → Copy** (Ctrl+C) copies them, and **Paste** (Ctrl+V)
  adds duplicates at the same positions on the active layer, already
  selected, so you can drag them where you want. (With a region
  [Selection](#selection-and-fill) active, Copy copies the selected region
  instead, as always; Paste pastes whichever you copied last.)

- **Merge into Branching Path** (right-click, or **Edit → Picked Path Tools →
  Merge Picked Paths into Branching Path**) joins every selected path into
  one branching path (pink), adding the fewest, shortest connections that
  link them all - each path is joined to its nearest neighbor at their two
  closest points. Paths drawn with the [Path Tool](#path-tool) (and
  branching paths) are replaced by the merged one; painted strokes lend
  their shape but stay where they are, still sounding. Use the result for
  **Create Resonance Profile...** or **Stroke Path with Current Tool**.

Each of these is a single undo step, however many objects are involved.
Clicking anything outside the selection replaces it as usual.

Clicking empty canvas space deselects. If an object is entirely covered
by another one on top of it, click the already-selected (covering)
object again to select the one underneath - each further click on the
same spot cycles to the next one down, wrapping back to the topmost once
you reach the bottom.

**Right-clicking** an object (in any tool mode, not just Pick) selects
it and opens a small menu with **Edit Path**, **Bring to Front**,
**Send to Back**, and **Delete** - the same actions as above, without
reaching for the Edit menu. With several objects selected, the menu
offers **Copy** and **Delete** for all of them. A [Path Tool](#path-tool) path (or a
branching path) also offers:

- **Stroke Path with Current Tool** - paints along the path (every branch
  of a branching path) with the brush currently set up in Tool
  Configuration, as one undoable step. The path itself stays, so you can
  stroke it again with a different brush.
- **Create Resonance Profile...** - the same as **Create Resonance from
  Picked Path...** below; for a branching path, every branch counts.

Right-clicking empty canvas space instead opens **Paste**, **Fill
Selection...**, **Apply Filter to Selection**, and **Deselect** (see
[Selection and Fill](#selection-and-fill) below).

Entries that don't apply right now - **Paste** with nothing copied, the
selection entries with no selection, **Toggle Node Type** with no node
selected, a whole submenu with nothing in it that applies - are **grayed
out**, in the right-click menus and the **Edit** menu alike. Their keyboard
shortcuts still work whenever they do apply.

Move/Modify/Delete/a completed path edit and restacking are all undoable
(Ctrl+Z), the same as painting a new stroke.

Copying a picked object isn't here yet, and path editing doesn't yet
support inserting/deleting a node by double-clicking, or detaching a
handle from its own mirrored pair - see
[What's Not Here Yet](#whats-not-here-yet).

## Selection and Fill

Choose **Select** from the **Tool** dropdown to switch the canvas into
select mode; choose **Pan** (or any other tool) to leave it. While it's
on, drag (or, for Wand, click) on the canvas to select a
region - shown as a green outline, dashed for a Wand or combined
selection (see below). A selection stays active (still scoping Fill/
Copy/Paste, and usable again the moment you switch back to Select) even
after switching to a different tool - it isn't tied to Select mode
itself, only to having drawn one - but its green outline only shows
while Select or Pick is the active tool, not while Paint or another tool
is.

**Selection Configuration**, in the **Configure** dropdown, opens a
dockable panel (off by default):

- **Selection Type** - **Rectangle** (the default) drags out an
  axis-aligned box, corner to corner. **Lasso** traces a freehand,
  irregularly-shaped region as you drag - rendered live as a smooth curve
  while you draw it, the same way a paint stroke's own preview is. A
  Lasso drag that never gathers enough points to enclose any real area (a
  short click, or a straight two-point line) clears the selection
  instead, the same as a Rectangle drag that never really moved.
- **Wand** selects every connected region of similar loudness starting
  from a single click - no drag needed (dragging before releasing is
  ignored; only the click position matters). Two controls appear only
  while Wand is selected:
  - **Tolerance** - how different a neighboring pixel's own loudness may
    be and still join the selection, from `0%` (only pixels at *exactly*
    the clicked point's own loudness) to `100%` (everything reachable,
    regardless of loudness).
  - **Harmonics-aware** - when checked, also selects along the clicked
    point's own overtone rows (2x, 3x, ... its frequency), the same way a
    note's harmonics naturally stack above its fundamental - useful for
    selecting a whole note at once rather than just its loudest partial.
- **Paste Blend Mode** - how **Edit → Clipboard → Paste** (below) combines its clip
  with the destination: **Overwrite** (the default) replaces it entirely,
  same as before this control existed; **Normal**, **Multiply**,
  **Screen**, **Overlay**, **Difference**, and **Add** blend it in
  instead - see [Compositing](#compositing) above for what each one does.
  Read fresh at the moment you paste, so changing it doesn't affect a
  paste you already made.

Fill/Copy/Cut/Paste (below) all confine themselves exactly to a Lasso or
Wand selection's own shape, not just its bounding box - copying an
irregular region and pasting it elsewhere leaves whatever was already on
the destination, outside that shape, untouched.

**Combining selections**: hold **Shift** while starting a new selection
(Rectangle, Lasso, or Wand) to **add** it to the selection you already
have, **Alt** to **subtract** it, or **Shift+Alt** together to keep only
where the two **overlap**. Works across shapes - Wand-select a note, then
Shift-drag a Rectangle around its harmonic to add that too, or Alt-drag
to carve a piece back out. A combining drag/click that ends up selecting
nothing leaves your existing selection exactly as it was, rather than
clearing it.

**Rotating a Rectangle selection**: a plain, freshly-drawn Rectangle
selection (not Lasso, Wand, a combined selection, or a pasted one) shows a
small handle above its own top edge - drag it to rotate the whole
selection around its own center. Fill/Copy/Cut/Paste all confine
themselves to the rotated shape exactly like they do for a Lasso
selection. Rotating back to (approximately) no rotation returns the
selection to a plain, axis-aligned rectangle.

- **Fill** it - **Edit → Selection → Fill Selection...** opens the same draggable
  gradient editor Painting's brush and Filter Configuration's
  Frequency-Axis Gradient use (see [Painting](#painting)/[Filter
  Layers](#filter-layers) above), seeded with a fully-opaque default;
  accepting fills the selection, confined exactly to its own boundary. A
  gradient with more than two stops varies continuously left-to-right
  across the selection, not just a single flat color.
- **Apply a filter to** it - **Edit → Selection → Apply Filter to Selection** bakes
  whatever filter is currently configured in the [Filter
  Layers](#filter-layers) panel over the selection only, confined exactly
  to its own boundary the same way Fill is - the rest of the layer is left
  untouched. This is a one-time bake of the panel's current settings, not
  a live filter that keeps reacting to further changes; undo it like any
  other edit if the result isn't what you wanted.
- **Copy** it - **Edit → Clipboard → Copy** (Ctrl+C) captures the selection's own
  pixels onto the clipboard, leaving them in place.
- **Cut** it - **Edit → Clipboard → Cut** (Ctrl+X) does the same as Copy, then
  silences the selection's own region in place.
- **Paste** the clipboard - **Edit → Clipboard → Paste** (Ctrl+V) writes it back at
  the same position it was captured from, onto whichever layer is
  currently active in the Layers panel - which doesn't have to be the
  layer it was copied or cut from. Select a different layer's row first
  to paste onto it instead. Combines with the destination per the
  Selection Configuration panel's own **Paste Blend Mode** (above).
  Switches straight to [Pick](#pick) and selects the newly pasted result
  there - move, modify, delete, or restack it right away, with no
  separate click needed to find it again.
- **Capture as Mind Shot** it - **Edit → Capture → Capture as Mind Shot** opens a small
  dialog before storing the selection's own pixels permanently: a **Name**
  (defaulting to "Mind Shot 1", "Mind Shot 2", and so on, still editable; a
  name that's already taken offers Replace/Rename/Cancel after you
  confirm the dialog), a
  **Fundamental Frequency**, and a **Start-Time Offset**. Unlike Copy, this
  doesn't touch the clipboard, and the source pixels are left exactly as
  they were (no silencing, unlike Cut). Once captured, pick **Mind Shot** as
  the Tool Type in [Painting](#painting)'s Tool Configuration panel and
  select it from the drop-down there to paint with it - it stamps back
  exactly as captured, wherever you paint, on any layer, **pitch-shifted to
  match wherever you stamp it** if you set a Fundamental Frequency (leave it
  at `0`, the default, for a verbatim stamp at every pitch, unchanged from
  before this dialog existed). Start-Time Offset shifts the stamp's own
  placement earlier by that many seconds, so a capture with a little
  lead-in before its own real attack still lands its attack exactly where
  you paint, instead of the capture's own geometric middle landing there.
  Always captures the selection's own full bounding box, even for a
  Lasso/Wand/combined selection - not yet confined to its actual shape.
- **Capture as Mind Grain** it - **Edit → Capture → Capture as Mind Grain** opens the
  same dialog Capture as Mind Shot does (defaulting to "Mind Grain 1", "Mind
  Grain 2", and so on, with the same Replace/Rename/Cancel on a name
  collision) and stores a *reference* to the selection's own layer
  and region - unlike Capture as Mind Shot, no pixels are captured at all,
  and neither the clipboard nor the source layer's content is touched. Once
  captured, pick **Mind Grain** as the Tool Type and select it from the
  drop-down there to paint with it - see [Painting](#painting)'s own Mind
  Grain entry for the "only paintable above its own source layer" rule and
  the guardrails that enforce it, and Capture as Mind Shot's own entry just
  above for what Fundamental Frequency/Start-Time Offset do (identical
  either way). Also always references the selection's own full bounding
  box, same as Mind Shot above.
- **Create Resonance from Picked Path...** - **Edit → Picked Path Tools →
  Create Resonance from Picked Path...** prompts for a name (defaulting
  to "Resonance 1", "Resonance 2", and so on; a name that's already taken
  offers Replace/Rename/Cancel), then
  analyzes whichever path/stroke is currently Picked (every branch of a
  branching path) and stores the result
  permanently under that name - a confirmation appears in the status bar.
  Pick **Resonance** as the Tool Type in
  [Painting](#painting)'s Tool Configuration panel and select it from the
  drop-down there to paint with it (see "Resonance Profile"/"Decay
  Rate"/"Frequency Scale"/"Time Span" there).
- **Branching Curve editor** - build a real tree/graph for a Resonance
  brush, instead of just one straight stroke. Draw a trunk stroke
  as an ordinary paint stroke, switch to Pick and click it, then **Edit
  → Picked Path Tools → Start Branching Curve from Picked Path**. To grow a branch: click a
  precise point on the trunk (or on any already-added branch) - this
  arms that point as the next branch's own attachment point. Switch to
  Paint and draw a new stroke, switch back to Pick and click it, then
  **Edit → Picked Path Tools → Add Picked Path as Branch**. Repeat as many times as you
  like, grafting new branches onto the trunk or onto earlier branches.
  When the tree is complete, **Edit → Picked Path Tools → Create Resonance from
  Picked Graph...** prompts for a name and stores the whole tree's own
  spectrum, the same way Create Resonance from Picked Path
  does for a single curve. **Edit → Picked Path Tools → Cancel Branching Curve** abandons an
  in-progress tree without saving anything, if you want to start over.
  Every branch past the trunk needs its own attachment point clicked
  first - adding a branch with no point armed does nothing.
- **Deselect** it - **Edit → Selection → Deselect** (Ctrl+D), or drag-clicking without
  actually dragging (a plain click) on the canvas while in Select mode.

Filling and pasting are both undoable (Ctrl+Z), the same as painting a
stroke; so is Cut's own silencing of the source region.

Rectangle, Lasso, and Wand selections all exist today, can be combined
with each other, and a Rectangle selection can be rotated.

## Path Tool

Choose **Path** from the **Tool** dropdown to switch the canvas into Path
mode; choose **Pan** (or any other tool) to leave it. Draw with it exactly
like the Paint tool - press, drag, release - and the line you drew is kept
as a **path**, smoothed into curves as you go. Unlike a brush stroke, a path
is **silent**: it paints nothing and changes nothing you hear. It's a guide
you can use afterwards:

- It's drawn on the canvas as a **cyan** line on every visible layer. A
  **branching path** - made by merging several selected paths (see
  [Pick](#pick)), or kept from the Branching Curve editor - is drawn in
  **pink**, with a dashed join where each branch connects to another.
- [Pick](#pick) it to select, move or delete it, or to **edit its nodes**
  (**Edit → Path Editing → Edit Path**) exactly like a painted stroke's.
- Use it anywhere a Picked stroke's path is used: **Create Resonance from
  Picked Path...**, **Use Picked Path as MindWave Shape**, or as a branch in
  the Branching Curve editor.

A press without dragging draws nothing. Each finished path is one undoable
step, and switching tools mid-drag discards the line being drawn.

## Text Tool

Choose **Text** from the **Tool** dropdown to write words onto the canvas -
they're written as sound, so you can hear them and see them in the
spectrum. The **Text Tool** panel opens alongside:

1. **Place a box** - drag on the canvas to draw it (the text is stretched to
   fill it), or just click to place a box the text's own size, centred where
   you clicked. A dashed outline shows the box, with a live preview of the
   text inside it.
2. **Style it** in the panel - every change shows in the preview right away:
   - **Text**, **Font**, **Size** (letter height in canvas cells), **Bold**,
     **Italic**;
   - **Left intensity** / **Right intensity** - how loud each channel is
     written where a letter is solid (dB or a 0-1 value - use the unit
     toggle); letter edges fade out smoothly;
   - **Opacity** - scales the whole text's loudness;
   - **Blend mode** - how it combines with what's already on the layer
     (the same choices as Paste);
   - **Rotation** - in degrees, counterclockwise.
3. **Apply** writes it onto the active layer, as one undoable step. Only the
   letters themselves are written - the space around and between them is
   left exactly as it was.

**Reset** discards the box without writing anything; switching to another
tool does the same.

**Editing applied text.** Applied text stays editable, like any painted
object:
- [Pick](#pick) it, and its settings fill the Text Tool panel (which comes to
  the front), with a note that you're editing the picked text.
- Change anything there - the wording, font, size, intensities, opacity,
  blend mode or rotation - and the text is redrawn in the same place.
- Changes apply as you make them. While you're typing, the text redraws
  after a short pause, so typing a word is a single step to undo.
- Pick can also move, copy or delete it as before, and moved or copied text
  stays editable.
- Clearing the text box entirely is ignored rather than erasing the text;
  delete it with Pick to remove it.
- Choosing the Text tool again goes back to setting up new text.

## Chord Generator

**Chord Generator**, in the **Configure** dropdown, opens a dockable
panel (off by default) that builds a chord, arpeggio, or a
hand-written sequence, and stamps it onto the canvas as a series of notes.
An **Input Mode** switch at the top chooses between two independent ways
to build what gets stamped:

### Chord Builder

- **Root/Octave** - which pitch class and octave the chord is built on
  (e.g. root `A`, octave `4`, is A4).
- **Category/Chord** - one of five chord families (Triads, 6th, 7th, 9th,
  Extended/Added), and a specific chord within it (e.g. Minor, Dominant
  7th). A live text label shows the resolved note names as you adjust
  these.
- **Mode** - **Block Chord** plays every note together, for a chosen
  Duration; **Arpeggio** spreads the same notes out over time instead,
  with its own Order (Ascending, Descending, Up-Down, Down-Up,
  Alternating, Outside-In, Inside-Out, Random, or a hand-typed Custom
  sequence of indices), BPM, rhythmic Subdivision, Note Duration
  (percentage of each step actually held), and Repeats (how many times
  the whole sequence cycles).
### Custom Notation

Instead of picking a chord, type a sequence directly using Sound Mind's
own compact notation - a space-separated list of tokens:

- A **note**: `pitch:duration` - e.g. `A4:0.5` (a note name) or `440:0.5`
  (a raw Hz value). Duration is in seconds by default, or beats with a
  `b` suffix (e.g. `1b` is one beat at the BPM below).
- A **rest**: `z` followed by a duration - e.g. `z0.25` - advances the
  timeline without playing anything.
- A **chord**: two or more notes joined with `+` - e.g.
  `A4:0.5+C#5:0.5+E5:0.5` - every note in the group starts together.

Notes play one after another: each token starts right when the previous
one's own longest note (or a rest) ends - use `z` to leave a gap. For
example, `A4:0.5 z0.25 C5:0.5+E5:0.5` plays A4 for half a second, waits a
quarter second, then plays C5 and E5 together for half a second. Typos are
flagged immediately below the text box as you type, with **BPM** and
**Reference** (tuning, in Hz) number fields controlling how beats-suffixed
durations and note names are resolved.

While you're building a chord or arpeggio, its notes are drawn live as
dashed amber lines on the canvas's own frequency axis, so you can see
where they'll land before committing anything. This overlay only shows
while the Chord Generator panel is open, or the Chord tool is the
active one - it disappears once neither is true, rather than staying
visible unconditionally.

### Instrument and Placing It

Either way, a stamped sequence always plays through whatever the
[Painting](#painting) tool is currently configured as (Procedural,
Instrument, Mind Shot, or Mind Grain) - there's no separate instrument
picker in the Chord Generator itself; switch the Painting tool's own
settings to change what it sounds like.

To actually place it, choose **Chord** from the **Tool** dropdown to arm
it, then click anywhere on the canvas - the click only sets *when*
the sequence starts (its own time position); every note's own pitch
always comes from the panel above, regardless of where vertically you
click. While the panel is configured, its notes preview live on the
canvas as horizontal lines on the frequency axis - independent of, and in
addition to, the ordinary Frequency Grid (see
[Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid) below) -
so you can see what you're about to stamp before you click.

A stamped chord is a real, editable paint object afterward - select it
with [Pick](#pick) and drag it: left/right re-times it, up/down uniformly
re-voices every note in it, both without repainting anything. Reopening a
stamped chord's own instrument for editing isn't here yet; see
[What's Not Here Yet](#whats-not-here-yet).

## Axis Labels

**Grid**, in the **Configure** dropdown, opens a dockable panel (off by
default) with two drop-downs, one per canvas axis:

- **Vertical axis (frequency)** - **Off** (the default), **Hz** (round
  frequency values), **Notes** (the nearest note name, e.g. `A4`, `C#5`,
  against the project's own tuning reference), or **Bin index** (the
  raw row number the layer's own data is actually stored at).
- **Horizontal axis (time)** - **Off** (the default), **Seconds**,
  **Milliseconds**, or **Frame index** (the raw column number).

Labels appear as small tick marks and text along the canvas's own left
edge (frequency) and bottom edge (time), thinning themselves out - fewer,
more widely spaced ticks - so they never crowd together and stay legible
regardless of a project's own duration or frequency range. Purely a
display aid, like everything else in this panel eventually will be: axis
labels never affect encoding, decoding, or any stored pixel data.

## Overlay Grids and Snap to Grid

The same **Grid** panel (see [Axis Labels](#axis-labels) above) also has a
**Frequency Grid** section, a **Timing Grid** section, and a **Snap to
Grid** checkbox at the bottom.

**Frequency Grid** draws horizontal reference lines across the canvas -
any combination of these can be on at once:

- **Note grid** - one line per semitone, against the project's own tuning
  reference, narrowed by:
  - **Temperament** - the tuning system: 12-TET (the default) plus
    15/17/19/22/24/31/34/41/53/72-TET, Quarter-Comma Meantone, and
    Pythagorean. Only 12-TET, 24-TET, Meantone, and Pythagorean have a
    standard 12-notes-per-octave structure for the Key/Scale filter below
    to mean anything against - picking any other temperament grays Key
    and Scale out, and the **Notes...** dialog shows plain numbered steps
    instead of letter names for it.
  - **Key**/**Scale** - picking a Scale other than Chromatic (Major,
    Dorian, Phrygian, Lydian, Mixolydian, Minor, Locrian, Harmonic Minor,
    Melodic Minor, Major/Minor Pentatonic, Blues, Whole Tone, or
    Octatonic) restricts the grid to that scale's own notes, rooted at
    whichever Key you pick - e.g. Key C + Scale Major shows only C Major's
    own seven notes. Changing either one recomputes which notes are
    included from scratch, discarding any changes you made in the Notes
    dialog below it.
  - **Notes...** - opens a checklist of every note the current
    Temperament has (one row per semitone for 12-TET, more for a finer
    temperament), with Select All/Deselect All - check or uncheck
    individual notes directly, on top of (or instead of) a Key/Scale
    pick. Stays as you left it until Temperament, Key, or Scale next
    changes.
  - **Octaves...** - the same kind of checklist, for which octaves
    appear; unchecking one removes every note in it, regardless of which
    notes are otherwise included.
- **Harmonic series** - one line per integer multiple of a **Fundamental**
  you set (in Hz).
- **Custom frequencies** - a **Frequencies (Hz)** field you type your own
  list into (comma- or space-separated, e.g. `220, 440, 880`); anything
  that isn't a valid positive number is ignored rather than blocking the
  rest of the list.

**Timing Grid** draws vertical reference lines, from one **Mode**
drop-down (only one at a time, unlike Frequency Grid's combinable
sources):

- **Off** (the default) - no lines.
- **Fixed Interval** - one line every **Interval** (seconds).
- **Tempo** - one line per **Subdivision** (Whole/Half/Quarter/Eighth/
  Sixteenth/Thirty-second note) at the project's own default tempo.

Each of Frequency Grid and Timing Grid has its own **Line color** swatch,
**Line width** (pixels), and **Line style** (Solid/Dash/Dot) - set them
independently so the two grids stay visually distinct when both are on
at once. Purely a display aid, like Axis Labels: neither grid affects
encoding, decoding, or any stored pixel data.

**Grid Preset** saves the Frequency Grid, Timing Grid, and Snap to Grid
state above together as one named, reloadable unit - **Save...** names
and saves the panel's current settings (if the name you type is already
taken, you're asked to Replace the existing preset, pick a different
name, or cancel); picking a saved preset from the drop-down loads it
back immediately; **Delete** removes whichever preset is currently
selected. Axis Labels aren't included - just the grid/snap settings.
Grid Presets also show up in the [Resource Browser](#resource-browser)
for browsing alongside your other saved resources, though saving,
loading, and deleting one is this panel's own job, not the Resource
Browser's.

**Snap to Grid**, once checked, makes a Path node drag, a whole-object
Pick move, or a Selection drag land on the nearest active grid line
instead of exactly where you release the mouse - frequency snaps to the
nearest active Frequency Grid line, and time snaps to the nearest active
Timing Grid line, independently of each other. With a grid's own sources
all off, that axis simply isn't snapped (nothing to snap to).

## Playback

The **Playback** panel has **Play**, **Pause**, and **Stop**, and a
draggable position bar with an elapsed/total time label. It plays the
project's own real composite - every visible layer mixed together (see
[Compositing](#compositing) above), spanning the project's own full
canvas-width duration, silence included past wherever real content ends.
Pressing Play decodes that composite once, unless **Repeat** (below) is
checked. The output device and volume it plays through are set from
[Configure Devices](#configure-devices), not from this panel - open it
directly with this panel's own **Configure Devices** button.

While playing, a white line sweeps across the canvas in real time,
tracking the current position - the same playhead line a video export
would show, live. Drag the position bar (whether playing or paused) to
seek to a different point; both the bar and the canvas playhead jump to
match immediately.

The first time you press Play in a session, preparing that composite runs
in the background rather than freezing the app - for a large project, a
small **✕** button appears in the status bar to cancel it if you change
your mind; cancelling just means playback doesn't start. New/Open Project
and closing the Studio are all disabled while playback is being prepared,
the same as while an import or a pool is running.

### Repeat Playback

The **Scope** drop-down decides how an edit affects playback, and
**Repeat** decides what happens once that reaches its own end:

- **Track** (the default) - only reacts to an edit while **Repeat** is
  checked *and* something is already playing: it keeps playing without
  interruption while the edit is prepared in the background, and the
  fresh sound takes over at the exact moment the loop restarts. (If the
  edit changes the track's length it reloads in place at the same
  position instead.)
- **Delta** - painting (or any other edit) jumps playback to whatever you
  just edited and plays through just that region - automatically, even if
  nothing was playing yet and even with **Repeat** unchecked, so you hear
  a fresh brush stroke immediately without pressing Play yourself. With
  **Repeat** checked it then loops that region repeatedly; unchecked, it
  plays through once and stops right there.
- **Review** - jumps to the edit like Delta (same automatic, no-Play-
  needed behavior), but plays through to the end of the track instead of
  stopping at the edit. With **Repeat** checked it then loops back to the
  very start of the track (not back to the edit itself); unchecked, it
  just stops once it reaches the end.

Unchecking Repeat, or dragging the position bar yourself, always goes
back to playing (or looping) the whole track.

A loop-back (Repeat checked, reaching the active region's own end) is
seamless - no audible gap or skip at the seam, regardless of region
length.

## Recording

The **Record** panel has a **Start Recording**/**Stop Recording** toggle,
its own **Configure Devices** button, and two settings for where the
recording goes:

- **Record into** - **New recording layer** (the default) adds a brand-new
  layer, named from the **Name** box (default "Recording"); or pick one of
  your earlier recorded layers to record over it - its content is
  replaced by the new recording (Edit → Undo brings the old one back).
- **Live-encoding view** - shows the recording on the canvas as it comes
  in. It's drawn on top of everything else rather than at its own place
  in the layer stack (that would be too slow while recording); once you
  stop, the recorded layer shows in the stack as usual.

Recording captures from the input device chosen in [Configure
Devices](#configure-devices), encoded the same way an imported file would
be, the moment you stop. While recording, the playback position bar moves
across the canvas to show how far you've got. That input device (and its
Gain) can't be changed while recording is in progress.

## Loop Mode

The **Loop** panel turns the Studio into a loop station that plays along
with your whole project:

- **Start Loop** plays the whole project from the start and, at the same
  time, records your input into the **Loop Input** layer. (While the
  project's sound is being prepared, the status bar says "Preparing
  loop..." for a moment.)
- When playback reaches the end, the take you just recorded replaces the
  previous one, and playback carries on from the start - you hear the new
  take straight away, together with the rest of the project, while the
  next pass records.
- Check **Freeze Loop** to stop recording and keep the current take: the
  project and that take keep playing, pass after pass, uninterrupted.
  Uncheck it to start recording again at the beginning of the next pass.
- The Loop Input layer's own **Gain** and **Balance** (and hiding or
  muting it) apply to the take as you play. Edits to the rest of the
  project are heard from the next pass on.
- The playback position bar sweeps across the canvas as the loop plays,
  and the Loop Input layer shows each take once its pass is complete.
- Check **Live-encoding view** to watch the take being recorded appear on
  the canvas as it comes in, drawn on top of everything else (not at the
  Loop Input layer's own place in the stack, which would be too slow
  while looping).

While a loop is running, filters and blend modes on layers *above* Loop
Input don't apply to the live take yet - once you stop, ordinary
playback includes them as usual.

Input and output devices are chosen from [Configure
Devices](#configure-devices) - open it directly with this panel's own
**Configure Devices** button; a device change takes effect the next time
you start the loop, not immediately. Both devices are locked (can't be
changed) while a loop is running.

## Macros

A **Macro** records a performance: the project playing, and everything you
do to it while it plays. It's kept with the project.

- **Macro → Record Macro** starts recording a new Macro ("Macro 1",
  "Macro 2", ...). Then start Playback or Loop and work: paint, change
  layer settings, filters and MindWaves, import, record. Undo and Redo
  count too.
- **Its clock only runs while Playback or Loop runs.** Pause, set up your
  next move, and resume: the changes you made while paused are recorded,
  in order, at the moment you paused, so the pause doesn't appear in the
  performance. Seeking, Repeat jumping back, and each new Loop pass are
  recorded as jumps of the playhead.
- **Record Macro** again stops recording; the status bar reports its
  length and how many events it holds.
- **Macro → Manage Macros...** lists the recorded Macros with their
  length and event count, to **Rename...** or **Delete** them.

- **Macro → Export Macro Video...** renders a Macro (you pick which, if
  there are several) to a video, set up with the same dialog as
  [Export Video](#exporting): the canvas, changing at each moment
  you changed something - complete at once, however long it took live -
  with the playhead line where yours was and the audio it played. It's
  rendered from a copy, so you can keep working; the status bar shows it
  running, with a Cancel button.

- **Macro → Play Macro** plays a Macro back in the Studio (you pick which,
  if there are several). It's prepared in the background and starts after
  a moment, then plays without a break - a change that took a while live
  is instant - while preparation keeps ahead of it. The canvas shows the
  performance as it unfolds; your project isn't changed, and the canvas
  shows it again when the Macro ends. Meanwhile the action reads **Stop
  Macro**, the transport's **Pause**, **Play** and **Stop** work on it, and
  the canvas doesn't respond to clicks.

A dragged control (gain, a filter or MindWave setting) is recorded as the
value you let go at, not the movement.

## Configure Devices

Open **Configure Devices** from the **Configure Devices** button inside
the Playback, Record, or Loop panel (it's the same shared panel either
way, off by default) - it's the *only* place input and output devices are
chosen; Playback, Recording, and Loop Mode all use whatever's set here
rather than having pickers of their own. The same controls are also
available on the [start screen](#starting-out)'s own **Device
Configuration** section, before any project exists - the two stay in
sync with each other automatically, so it doesn't matter which one you
actually use.

- **Refresh Devices** re-scans for newly connected devices.
- **Input**: pick the active input device - applies to both Recording and
  Loop Mode at once. A **Gain** slider (0-200%, 100% is unchanged) boosts
  or cuts the signal before it's captured. Click **Test** to see a live
  level meter respond as you speak or play into the device, without
  starting a real recording.
- **Output**: pick the active output device - applies to both Playback
  and Loop Mode at once. A **Gain** slider works the same way. Click
  **Test** to play a brief tone through the device, confirming you hear
  it from the right place.
- **Output processing**:
  - **Master limiter** (on by default) keeps the output at or under its
    ceiling (default -1 dBFS). Where the mix would go over, it is turned down
    smoothly for a moment instead of distorting. It applies to Playback, to
    what you hear in Loop Mode (your takes are still recorded at their full
    level) and to every audio or video export. The number next to it is the
    ceiling (-24 to 0 dBFS).
  - **Normalize loudness** (off by default) plays every mix at the same
    loudness: when playback starts, the Studio measures the mix's integrated
    loudness and turns the output up or down toward the target (default
    -14 LUFS, the level many streaming services use). The correction is at
    most 24 dB down or 12 dB up, and it holds steady across pause and resume.
    It affects playback only, so exports keep the mix's own level; use the
    [Analysis](#analysis) panel to see that level.

  Both settings are remembered between sessions.

If a device can't be opened, **Test** un-checks itself and a warning says
why - a Test that quietly does nothing never happens. Pressing **Play** also
re-tries your chosen output device automatically, and the status bar tells
you if nothing will be heard (device missing or in use by another program,
output gain at 0%) or if the device runs at a different sample rate than the
project (which makes playback sound faster/slower and shifted in pitch - set
the device to the project's rate in your system sound settings).

The Input combo (and its Gain) locks while Recording or Loop Mode is
running; the Output combo (and its Gain) locks only while Loop Mode is
running, since Recording never touches output.

## Pooling a Layer

Select a layer in the Layers panel, then click its own **Pool** (⬇) button
(next to Duplicate/Clean Up Phase, once selected) to run it through a
full, lossless round-trip encode - a slower but higher-fidelity
representation than the fast, approximate one every import and recording
normally uses. There's no visible difference for most material; it
matters most before an audio export you want to sound as close as
possible to the original. Only a layer with real content shows this
button - a brand-new empty layer, or a Filter/Equalizer layer, has
nothing to pool.

Pooling runs in the background - you can keep working while it processes,
and a small **✕** button appears in the status bar to cancel it if you
change your mind; cancelling leaves the layer exactly as it was, as if
pooling had never been started.

## Exporting

**File → Export Audio...** saves the whole mix - every visible, unmuted
layer, with filters applied, exactly what Playback plays - as FLAC, Ogg
Vorbis, or MP3 (pick the format via the save dialog's file type).

**File → Export Video...** saves it as a video: the composite spectrogram,
animated with the mixed audio played alongside it. A dialog sets it up,
and remembers your choices for next time:

- **Output file** - with **Browse...**; the extension follows the format.
- **Format** - **H.264 (MP4)**, recommended: small files that play
  everywhere. **HEVC (MP4)**: smaller still, listed only where Windows
  provides an HEVC encoder. **ProRes (MOV)**: very large files, for further
  editing. **MPEG-4 (MP4)**: works everywhere, larger files. Only formats
  that work on your computer are listed.
- **Resolution** - 1080p, 4K, 1440p, 720p, vertical 1080×1920 or 720×1280
  (TikTok, Reels, Shorts), square 1080×1080, the canvas's own size, or a
  custom width and height. The canvas keeps its proportions, with black
  bars where the shapes differ.
- **Frame rate** - 24, 30 (the default) or 60 frames a second.
- **Quality** - Low, Medium (the default) or High: bigger files at higher
  quality.
- **Animation** - **Whole canvas**: the whole canvas is shown throughout,
  with the playhead sweeping across it. **Scrolling**: a window a chosen
  number of seconds wide pans from the start of the canvas to its end,
  with the playhead crossing it. **Show playhead** turns the line (bright
  yellow, with a dark outline) on or off.
- In [Sound Flower (polar) view](#sound-flower-polar-view) the video is
  the flower, and the dialog says so. With Whole canvas a ray turns
  clockwise from twelve o'clock; with Scrolling the flower itself turns,
  bringing the moment being played under a fixed ray at twelve o'clock.
  A scrolling polar video takes longer to make than the others.
- **Open the video when the export finishes** - opens it in your video
  player.

The status bar shows how far the export has got, as a percentage.

Both run the audio through the master limiter first if it's on in
[Configure Devices](#configure-devices), so a mix that goes over full scale
is turned down at its peaks instead of clipping. Loudness normalization is
never applied to exports.

Both kinds of export run in the background - you can keep working while
one encodes, and a small **✕** button appears in the status bar to cancel
it if you change your mind; cancelling removes whatever it had written so
far rather than leaving a broken file behind. Only one export (of either
kind) runs at a time - starting another while one is already in progress
just shows a reminder to wait or cancel first.

## Saving and Project Files

**Ctrl+S** (or **File → Save Project**) saves in place; **File → Save
Project As...** saves a copy elsewhere. A project is a `.smproj` file plus
a same-named folder next to it - keep the file and its folder together;
moving or copying one without the other breaks the project. The folder
holds:

- `media/` - every layer's encoded content;
- `pool/` - once you've pooled at least one layer;
- `sources/` - a copy of every file you imported (audio, images, MIDI),
  every recording and Loop take (as lossless FLAC files), and every layer
  brought in from another project. The project never depends on the
  originals elsewhere on your computer. This folder can grow large: every
  Loop take is kept. **Save As** copies it along.

Closing the window, opening a different project, or starting a new one
while you have unsaved changes prompts you to save first.

### Remastering

**File → Remaster...** rebuilds the open project as a **new** project by
replaying everything you did to it, from its own sources. The open project
isn't changed. In the dialog:

- **Sample Rate, Timestep, Bin Count, Min/Max Frequency** start at the
  project's own settings. Change them to rebuild at a higher (or just
  different) resolution. The duration stays the same. Strokes, fills,
  pastes and text land at the same times and frequencies; text is redrawn
  at the new resolution, and pasted pieces are resampled.
- **Sources** lists every file, recording and Loop take the project was
  made from. **Replace...** swaps one for another file: everything that
  used it is rebuilt from the new one, and the edits placed in time stay
  where they were.
- **Save as** - where to save the new project.

Pressing **Remaster** without changing anything rebuilds the project as it
is, which is how to recover layer content whose files went missing (the
Studio says so when you open such a project). When it's done, the Studio
offers to open the new project.

Remastering needs the project's complete history, so it isn't available
for projects made with a version before 0.1.43.1. A Mind Shot is resampled
in proportion when the frequency range or bin count changes, which only
approximates its original pitch.

A project can also be opened from outside the Studio: start
`sound-mind-studio.exe` with the `.smproj` file's path (for example by
dropping the file onto the program, or onto a shortcut to it). The
Microsoft Store version, once it's published, also makes double-clicking a
`.smproj` file open it. If the project can't be opened, a message says why
and the start screen stays up.

## Resource Browser

**Resource Browser** (its own entry in the menu bar, between **View** and
**Help**) opens a panel for browsing, exporting,
and importing the "library" resources a project accumulates - MindWaves,
Tool Presets (Procedural, Instrument, Resonance, and every other brush
type with saved settings), Mind Shots, Resonance Profiles, Filter Presets
(a whole saved Filter layer configuration), Convolution Kernels (a saved
kernel for the Convolve filter type specifically), Grid Presets (see
[Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid) - saved/
loaded/deleted from the Grid panel itself, browsable here like everything
else), Mind Grains, and Layers. Pick a category from the drop-down at the
top to see every one of that kind your current project has; selecting an
entry shows its parameters in the inspector, plus a rendered preview
where one makes sense:

- **MindWave** - a small grayscale preview of the field it actually
  generates, the same preview the MindWaves panel's own rows show, plus a
  **Play** button: hear a default instrument modulated by this MindWave
  (its vibrato), painted as the same representative stroke Tool Preset
  uses below.
- **Tool Preset** - a preview stroke: a straight 3-second line from 3 kHz
  to 5 kHz, painted with the preset's own configuration, so you can see
  what it actually looks like before picking it, plus a **Play** button
  to hear it (an Instrument bound to a MindWave plays its vibrato/tremolo
  too). The preview image is cropped to frequencies at or above 1 kHz, so
  the stroke itself (always well above that) fills the small preview
  rather than being squeezed down alongside the rest of the frequency
  range.
- **Mind Shot** - its own rendered spectrogram, plus a **Play** button to
  hear it.
- **Resonance Profile** - a plot of its spectrum, and, alongside it, the
  source path or branching curve it was computed from (if captured since
  this rendering was added - an older entry just shows the spectrum
  alone), plus a **Play** button: hear a default Resonance brush using
  this profile's own spectrum, the same representative stroke as above
  (no Play button for an entry with an empty spectrum - nothing to
  preview).

- **Export...** saves the selected entry to its own standalone file (a
  MindWave as `.smwave`, a Tool Preset as `.sminst`, a Mind Shot as
  `.smshot`, a Resonance Profile as `.smresonance`, a Filter Preset as
  `.smfilter`, a Convolution Kernel as `.smkernel`), which you can hand to
  someone else or import into a different project later. Grid Presets,
  Mind Grains, and Layers have no standalone file of their own yet - a
  Mind Grain only makes sense tied to its own layer, and a Layer is
  portable by browsing its own project directly (see below).
- **Import from File...** reads one of those same file types back in,
  adding it to your current project's own library under its saved name -
  unless that name is already taken, in which case you're asked whether
  to use the imported version (**Overwrite**), keep what you already
  have (**Keep Existing**), or keep both (**Keep Both**, which
  auto-numbers the import as "Name (2)", "Name (3)", and so on).
- **Browse Other Project...** opens a second `.smproj` file read-only,
  without closing or changing the one you're working on, and shows its own
  libraries in the same panel. Select anything in it - including a whole
  Layer, with its painted content - and click **Import** to copy it into
  your current project (the same Overwrite/Keep Existing/Keep Both prompt
  as **Import from File...** above, if the name collides). **Return to
  This Project** switches the panel back to your own project's own
  libraries.
- **Toolkit draft**: build up a bundle of several resources to share as one
  file. Select an entry (any category with its own file format) and click
  **Add to Toolkit** - it appears in the draft list below, and you can keep
  adding more from other categories, or even from a browsed other project,
  before exporting. If the entry you add depends on something else - a
  Tool Preset or Filter Preset with a parameter bound to a MindWave, say -
  that MindWave is added automatically too, and a message tells you which
  extra resources were pulled in, so the bundle still works correctly once
  imported somewhere else. **Remove** takes a selected draft entry back
  out. **Export Toolkit...** asks for a name, then a destination
  `.smtoolkit` file, and clears the draft once written. **Import
  Toolkit...** reads one back in and adds every bundled resource to your
  current project in one step - either everything in the file is added,
  or (if something in it is malformed) nothing is, never a partial
  import; the same Overwrite/Keep Existing/Keep Both prompt appears for
  any bundled entry whose name collides with one you already have.

## Analysis

**View → Analysis** opens the Analysis panel - meters that read the project
without ever changing it.

At the top, above the three tabs (so they're there whichever tab you're
on):

- **Analyze** measures the project - loudness, stereo and pitch all at
  once. It runs in the background, so you can keep working.
- **Auto-update** analyzes again by itself whenever you change anything you
  can hear - painting, layers, filters, MindWaves, recordings, undo and
  redo.
  If an analysis is already running when you make a change, it's stopped
  and a fresh one starts about a third of a second after your last change.
  A quick series of edits therefore costs one analysis. Turning it on
  analyzes straight away if the results are missing or out of date, and it
  also analyzes each project you open. It's off by default and remembered
  between sessions. On a long project with many layers, analysis can keep
  your computer busy while you edit, so leave it off if editing feels slow.
- The status line says whether the results are up to date. Without
  Auto-update, they say **Out of date** once you change the project, until
  you analyze again.

### Loudness

**Analyze** measures the whole mix, exactly as you hear it, and every
audible layer on its own (each with its own volume, balance and timing).

- **Integrated** - the loudness of the whole mix in LUFS, measured the way
  streaming services and broadcasters do (EBU R128). Many streaming
  services play music at around -14 LUFS.
- **Loudest 3 s / Loudest 400 ms** - the loudest short-term and momentary
  readings anywhere in the mix.
- **True peak** - the highest peak, including the peaks that fall between
  samples. Above 0 dBTP it turns **red**: those peaks will distort.
- **Sample peak** - the highest sample.
- **Clipped samples** - how many samples go beyond full scale, which
  distort (hard-clip) when played or exported. Any at all shows in **red**.
- **At playhead** - the momentary (400 ms) and short-term (3 s) loudness at
  the playback position, following playback.
- **The graph** shows momentary (thin, blue) and short-term (thick, gold)
  loudness over the whole project, from 0 down to -60 LUFS, with the
  playback position marked.
- **The band view** below it shows each frequency band's level over time -
  sub, bass, low-mid, mid, high-mid and air, lowest at the bottom, brighter
  meaning louder.
- **The layer table** lists each layer heard on its own: integrated
  loudness, loudest 3 s, true peak and clipped samples (red when
  clipping).
- **Show loudness on canvas** draws the mix's loudness right over the
  canvas, lined up with the time axis:
  - short-term loudness is a solid yellow line, and momentary loudness a
    thinner, fainter one;
  - the scale runs from -60 LUFS at the bottom of the canvas to 0 LUFS at
    the top, which is not the canvas's frequency axis;
  - the line breaks where the mix is silent.

### Stereo

The **Stereo** tab (filled in by the same **Analyze**) shows the mix's
stereo image:

- **Correlation** - how alike the two channels are: **+1** is mono, **0**
  means unrelated channels, and **below 0** (shown in **red**) means parts
  of the left and right cancel each other out when the mix is played in
  mono - on a phone speaker, say.
- **Width** - **0** is mono, around **0.5** is a wide stereo image, **1**
  is fully out of phase.
- **At playhead** - both, at the playback position.
- **The graph** shows correlation (blue, -1 at the bottom to +1 at the top,
  with a dashed line at 0) and width (gold, 0 to 1) over time.
- **The goniometer** plots the samples around the playback position (or
  across the whole mix when stopped): mono shows as a vertical line, wide
  stereo as a cloud, and out-of-phase sound leans toward horizontal.
- **The width bars** show each frequency band's width - bass is usually
  kept narrow, highs wider.
- **Show correlation and width on canvas** draws both over the canvas, lined
  up with the time axis, each on its own scale over the canvas's full
  height:
  - correlation is blue, from -1 at the bottom to +1 at the top, with a
    dashed line at 0;
  - width is orange, from 0 at the bottom to 1 at the top.

  A small legend in the top-left corner of the canvas names each line on
  the canvas and its scale.

### Pitch

The **Pitch** tab (also filled in by **Analyze**) follows the pitch of the
mix from 50 Hz to 2 kHz. By default it follows the main pitch, the
fundamental of whatever is loudest. It can also follow several voices at
once, such as a melody and a bassline:

- **At playhead** - the note at the playback position, how far off it is
  in cents (+20 cents is a little sharp), and its frequency, e.g.
  "A4  +3 cents  (440.8 Hz)". It shows "—" where there's no clear pitch
  (silence, noise). With several voices, each sounding voice gets its own
  line, numbered: "1: A4 ...", "2: A2 ...".
- **At mouse** - the same, for the moment under the mouse: hover over the
  canvas, or over the pitch graph below, and it shows the pitch (or
  pitches) at that horizontal position.
- **Voices** - how many simultaneous pitches to follow, from 1 to 6 (for
  example, 2 for a melody over a bassline). Each voice gets its own track
  and color. The voices are layers from top to bottom: voice 1 is always
  the highest pitch heard at that moment, the next voice the next one
  down, and so on, so the lines never cross. Each voice also has a home
  register that follows its own pitch, so a voice that pauses picks up
  again where it was. Until a voice has been heard, its register is spread
  across the range: with 2 voices, about 450 Hz and 140 Hz. A guitar or
  bass playing alone therefore sits on the low voice, and a singer coming
  in over it takes the high one. Changing the setting analyzes again by
  itself if there are results showing, and it's remembered between
  sessions.
  - With more than one voice, a row of checkboxes appears, one per voice
    in that voice's color, such as "Voice 1 (highest)" or "Voice 2
    (lowest)". Untick a voice to hide it from the graph, the readouts and
    the canvas. The choice holds until you close the Studio.
  - Limits: a voice an exact octave above or below a louder one is
    usually swallowed by it. Two notes whose frequencies are very close
    (within about 25 Hz, including one voice's overtones) can make the
    quieter one drop out now and then.
- **Ignore spikes shorter than** removes brief pitch jumps, such as
  momentary octave errors or the click of a note's attack. A jump lasting
  less than this many milliseconds is removed:
  - if the pitch on both sides of the jump agrees, the gap is smoothed over
    with a glide between them;
  - otherwise (a blip in silence, or right at a change of note) it's
    erased, shown as no pitch.

  Gentle glides and vibrato are never touched. `0` (the default) shows
  every detected pitch. A good starting point is 30-60 ms. The setting
  applies straight away to the readout, the graph and the canvas line,
  without analyzing again, and it's remembered between sessions.
- **The graph** draws the pitch over time on a musical scale, with a line at
  every C - one line per voice, each in its own color.
- **Show pitch on canvas** draws the same pitch lines right over the canvas
  (the first voice in bright green, the others in their own colors), so you
  can see which painted line is being heard as the melody.

## Performance

Most editing stays responsive on an ordinary machine, but a few things
genuinely cost more than others. None of this is a setting you need to
tune up front - it's here for when something feels slower than
expected, so you know what's actually driving it.

- **Hardware Acceleration** (the checkbox in the **View** menu, also
  mentioned under [The Main Window](#the-main-window)) lets several of
  the heavier operations below run on the GPU instead of the CPU when
  one's available. It's on by default and safe to leave there. GPU
  acceleration tends to help more the bigger the operation is - a large
  canvas, a strong blur, many layers - and can occasionally be a wash
  (or even slightly slower) for something small and cheap, since
  dispatching work to the GPU at all has its own fixed cost. Turn it
  off if you want to compare speed or results against the CPU path
  directly, or if you suspect it's causing a visual problem; it stays
  as you left it across restarts. (Before version 0.1.41.6, installed
  copies couldn't find their GPU programs and quietly used the CPU for
  everything; that's fixed.)
- **Using every CPU core**: turning the canvas into sound (preparing
  playback, Repeat and Loop refreshes, filter previews, exports) and
  turning sound into a canvas (importing, recording) share the work
  across all of your computer's cores. So does **Analyze**, which measures
  the mix and each layer at the same time. On a typical multi-core laptop
  these run several times faster than with a single core. The result is
  exactly the same whatever the number of cores.
- **Noise**: a noise MindWave is computed on the GPU when Hardware
  Acceleration is on (about 6x faster than on the CPU). A noise Generate
  Layer uses every CPU core. Either way the noise is the same - the GPU's
  values differ from the CPU's only in the sixth decimal place.
- **Filters**: **Uniform Blur** and **Edge-Preserving Blur** (see
  [Filter Layers](#filter-layers) above) both use the GPU when
  Hardware Acceleration is on. For Uniform Blur, a larger **Sigma**
  costs more either way, but GPU acceleration scales with it well - the
  bigger the blur, the more it helps. **Edge-Preserving Blur** doesn't
  scale quite as cleanly: a larger **Size** costs more on the CPU *and*
  the GPU, and the GPU's own advantage shrinks the bigger Size gets - so
  if Edge-Preserving Blur feels slow, reaching for a smaller Size helps
  more than toggling Hardware Acceleration does.
- **Layers and blend modes**: every layer that's visible and has
  content contributes to the composite the canvas and Playback show
  (see [Compositing](#compositing) above), and every blend mode -
  Normal included - uses the GPU when it's available. Editing one
  layer in a project with many others is fast regardless of how many
  layers there are - only that layer (and anything stacked above it)
  actually needs recompositing; everything below it is reused from the
  previous composite. Adding, removing, or reordering a layer, muting/
  unmuting, or editing a MindWave used anywhere in the project still
  recomposites the whole stack, so those will cost more on a project
  with a very large number of layers, regardless of acceleration.
- **Painting**: adding a new stroke to a layer with a long painting
  history is fast regardless of how many earlier strokes that layer
  already has - only the new stroke itself is actually (re)painted, on
  top of what was already there. Undo and redo are the exception -
  both re-paint a layer's whole history to guarantee correctness, so
  they'll cost more the more that layer has been painted on. A Mind
  Grain stroke re-samples its source layer whenever that source's own
  content actually changes, not on every unrelated edit to the layer
  it's painted on - still correct for the common case (the source
  changing), just not refreshed by something unrelated happening to
  the grain-painted layer itself.
- **MindWaves**: a plain **Fractal**-type MindWave (see
  [MindWaves](#mindwaves) above) evaluates on the GPU when Hardware
  Acceleration is on, and scales well to large canvases as a result. If
  you combine a Fractal MindWave with **Warp**, or turn on **Reversed**
  or **Inverted**, it falls back to evaluating on the CPU instead - still
  correct, just not GPU-accelerated in that combination yet.
- **Painting**: the **Heal** and **Soften** brushes blend each stroke
  toward a local average of the surrounding content - both stay fast
  regardless of how large you set the brush's own **Size**.

## Licence and Credits

Sound Mind Studio is copyright © 2026 Andreas Lundberg. You may use it free
of charge for anything, including commercial work; everything you create
with it is yours. You may not redistribute or modify it. The full terms are
in `LICENSE.md`, next to the program.

It includes open-source software by others - Qt, FFmpeg, LAME, JUCE (with
FLAC and Ogg Vorbis), LibTIFF, PocketFFT, Eigen and more - each under its
own licence. `THIRD_PARTY_NOTICES.md` and the `licenses` folder next to the
program list them all with their licence texts. Qt and FFmpeg are separate
DLLs you're free to replace with your own builds; their source code is
published on the project's GitHub releases page.

**Help → About Sound Mind Studio** shows the version and copyright, and
its **Licence...** and **Open-source notices...** buttons open both
documents.

Sound Mind Studio collects nothing and contacts no server - it works
entirely offline.

## What's Not Here Yet

The [design document](docs/sound-mind-design.md) describes the Studio's
full intended scope; `docs/sound-mind-roadmap.md` tracks what's being
built, in order. This section lists what isn't in the Studio today.

### Planned

These are scheduled after the Beta readiness check:

- **Image export** - saving the canvas as a PNG, JPEG or other image.
- **A light mode** - the Studio is dark only for now.
- **A color mapping choice** - which of the two channels' amplitude and
  phase show as red, green and blue is fixed today.
- **Composer Mode editing** - Composer Mode (**View** menu) shows each
  visible layer as a track with every stroke and fill as a box, but it's
  read-only: you can't yet drag a box to retime it, move it to another
  track, or change an object's or track's own gain or EQ from there.
- **The rest of Sound Mind Instruments** - an Instrument has a harmonic
  series, inharmonicity and MindWave-driven vibrato/tremolo, but no noise
  component, body resonance or ADSR envelope yet. Also planned: a
  **Harmonic** stamp mode (stamps spaced by the instrument's own
  harmonics), MindWave binding for every instrument setting, and Loop
  Mode retriggering per note.
- **More analysis** - the [Analysis](#analysis) panel has loudness, stereo
  and pitch. Vibrato characterization, formant content, key/scale
  detection, and analysing just a selection are planned; so are the
  design document's spectral-health and criticality/pattern meters.
- **Editing refinements:**
  - inserting or deleting a path node by double-clicking, and detaching a
    handle from its mirrored pair with Alt+drag (today you can move nodes
    and handles, toggle a node's type, and delete the selected node);
  - the Alt/Shift/Ctrl axis restriction (see
    [Canvas Navigation](#canvas-navigation)) applies only to Zoom so far,
    not to dragging a Pick, a selection's handles, or a layer's
    translation and rescale;
  - pitch quantising while painting - Snap to Grid snaps placed nodes,
    moves and selections, but a freehand stroke's pitch doesn't snap to
    the Frequency Grid as you draw;
  - pasting where you click - **Paste** lands where the clip was copied
    from (or on another layer at the same place); Pick then moves it.
- **Performance Hints** - speed badges on filters, blend modes, brushes
  and MindWave generators, from your own machine's benchmark results.
- **Live MIDI input** - playing a sequence from a connected MIDI
  controller. Today sequences come from the Chord Generator's Chord
  Builder and Custom Notation, or from MIDI file import.

### Not planned

- **Clone** and the **Tool Configuration Wizard** are deferred until
  further notice; the Tool Configuration panel and its presets cover
  configuring a tool.
- **Superposition** (combining several MindWaves into one with a blend
  mode) was removed in v0.1.32.1. It may come back in a different form.
- **Warp Selection** was removed: it didn't survive undo, redo or
  reopening a project, and was confusing to use.
- **Video export** doesn't offer WebM/VP9 (it would need an extra encoder
  library), and lists HEVC only where Windows provides an HEVC encoder.
- **The Sound Mind VST plugins** and hosting other plugins come after
  version 1.0.

### Limits of what's here

- **Macros** (see [Macros](#macros)): there's no timeline view to inspect
  or edit what a Macro recorded, strokes appear whole rather than being
  drawn in, and the video doesn't follow your zoom or scrolling (it shows
  the whole canvas, or the export dialog's Scrolling window).
- **Sound Flower**: Overlay Grids, the Chord Overlay, Axis Labels,
  bounding boxes and path geometry, the analysis overlays, and a
  Rectangle-shaped Pick or Selection highlight are hidden while the polar
  view is on, rather than drawn incorrectly.
- **Loop Mode**: while a loop runs, filters and blend modes on layers
  above Loop Input don't apply to the live take.
- **Brushes:** **Smudge** smears each stamp on its own rather than
  carrying paint along the whole stroke, and **Order/Chaos** reorders
  content rather than measuring entropy. **Resonance** has a single decay
  rate and no vibrato/tremolo or inharmonicity of its own.
- **Mind Shots and Mind Grains:** capturing from a Lasso, Wand or
  combined selection takes its whole bounding box. There's no way to move
  a Mind Grain's referenced region after capturing it, and a Mind Grain
  samples its source layer on its own, not the composite as it looks at
  that point in the stack.
- **Selections:** a Wand or combined selection shows as a dashed bounding
  box rather than its exact outline; Fill, Copy, Cut and Paste still
  follow its exact shape.
- **Filter layers:** Convolve's Kernel Size, Granular Noise's Grain Size,
  and Spectral Reverb's Pre-Delay, Decay, Room Size, Diffusion and
  Absorption can't bind to a MindWave - each shapes a whole neighbourhood
  of cells, not one cell's own value. Saved Convolve kernels can't be
  renamed or deleted from within the Studio.
- **MindWaves:** a layer's Blend Mode can't bind to a MindWave.
  Vibrato/Tremolo always follow the stroke's own progress (they have no
  Canvas Space choice). Reduce has no panel of its own - Vibrato/Tremolo
  are its only use. Step Grid values are a plain list of boxes, and
  Continuous's formula is a provisional first attempt.
- **Chords:** a stamped chord or arpeggio can be re-timed and re-voiced
  with Pick, but not pointed at a different instrument.
- **Custom Frequencies** is free entry only, with no curated preset sets.
