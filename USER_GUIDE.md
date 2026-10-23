# Sound Mind Studio - User Guide

This guide explains how to use Sound Mind Studio, from your first project to
the details of every panel. Read the first two parts in order; after that,
each chapter stands on its own, so jump to whatever you need.

The guide describes what the current build does. `CHANGELOG.md` lists what
changed in each version, and [What's Not Here Yet](#whats-not-here-yet) at
the end lists what's planned but not usable yet. If the guide and the
Studio ever disagree, the Studio is right and the guide is due an update.

## Contents

**Part 1 - Getting started**

1. [What Sound Mind Studio Is](#what-sound-mind-studio-is)
2. [Starting Out](#starting-out)
3. [A First Session](#a-first-session)
4. [The Main Window](#the-main-window)
5. [Working with the Canvas](#working-with-the-canvas)

**Part 2 - Building a project**

6. [Importing](#importing)
7. [Layers](#layers)
8. [Filter Layers](#filter-layers)
9. [MindWaves](#mindwaves)

**Part 3 - Drawing and editing**

10. [Painting](#painting)
11. [Pick](#pick)
12. [Selections](#selections)
13. [Paths and Resonance Profiles](#paths-and-resonance-profiles)
14. [Text](#text)
15. [Grids, Axis Labels and Snapping](#grids-axis-labels-and-snapping)
16. [Undo and History](#undo-and-history)

**Part 4 - Listening and recording**

17. [Playback](#playback)
18. [Recording](#recording)
19. [Loop Mode](#loop-mode)
20. [Configure Devices](#configure-devices)
21. [Macros](#macros)

**Part 5 - Measuring the sound**

22. [Analysis](#analysis)

**Part 6 - Files**

23. [Saving and Project Files](#saving-and-project-files)
24. [Exporting](#exporting)
25. [Remastering](#remastering)
26. [Resource Browser](#resource-browser)

**Part 7 - Reference**

27. [Performance](#performance)
28. [Keyboard Shortcuts](#keyboard-shortcuts)
29. [What's Not Here Yet](#whats-not-here-yet)
30. [Licence and Credits](#licence-and-credits)

---

# Part 1 - Getting started

## What Sound Mind Studio Is

Sound Mind Studio treats sound and pictures as the same thing. Every sound
is shown as a **spectrogram** - a picture of the sound - and every picture
can be played as sound:

- **Horizontal axis** - time, left to right.
- **Vertical axis** - frequency (pitch), low at the bottom, high at the
  top.
- **Brightness** - how loud that frequency is at that moment.
- **Color** - red is the left channel's loudness, green the right
  channel's, and blue carries phase (roughly, the fine detail that shapes
  the character of a sound rather than its pitch or volume).

Import an audio file and you see its spectrogram; import a photo and you can
hear it. You can paint, filter, select and rearrange the picture with
image-editing tools, and every change is something you hear.

### Key ideas

A few words come up throughout this guide:

- **Project** - one piece of work: a fixed length of time and frequency
  range (the **canvas**), and everything in it.
- **Layer** - like a layer in an image editor. Each holds its own sound;
  the canvas shows, and Playback plays, all visible layers combined.
- **Filter layer** - a layer that reshapes the layers beneath it instead of
  holding sound of its own.
- **Stroke** - a brush mark. Strokes stay editable: you can select one later
  and move it or change how it was painted.
- **MindWave** - a reusable shape that varies over time or frequency (a
  sine wave, an envelope, noise...). MindWaves make settings change across
  the canvas instead of staying fixed.
- **Mind Shot / Mind Grain** - a piece of the canvas captured as a brush.
- **Resonance profile** - a spectrum captured from a drawn curve, used as a
  brush.
- **Macro** - a recorded performance: the project playing and everything you
  did while it played.

## Starting Out

Launching the Studio shows the **start screen**:

- **New Project...** opens the Create Project wizard (below).
- **Open Project...** opens an existing `.smproj` file.
- **Recent Projects** lists the projects you opened last; click one to
  reopen it.
- **Documentation** opens the Quick Start, Readme, User Guide, Changelog
  and About - the same as the **Help** menu.
- **Device Configuration** lets you choose and test your audio input and
  output before opening anything. These are the same controls as
  [Configure Devices](#configure-devices), and the two stay in sync.

### Creating a project

The **Create Project** wizard asks for:

- **Name** - the project's name, also used for its file name.
- **Save Folder** - where the project is saved (**Browse...** to choose).
- **Duration** - how much time the canvas holds, in seconds. Audio longer
  than this can still be imported: it's split into Duration-length pieces
  you choose from (see [Importing Audio](#importing-audio)).

**Advanced** shows the canvas's resolution. The defaults suit most work:

- **Sample Rate** - the audio rate the project works at. It starts at the
  rate your output device runs at, so playback runs at the right speed; if
  that can't be found out, it starts at 48000 Hz, the most common rate. A
  note under the field says which.
- **Min Frequency** / **Max Frequency** - the frequency range the canvas
  covers. Sound outside it isn't kept.
- **Bin Count** - how many rows the canvas has from bottom to top. More rows
  mean finer pitch detail, and more data per layer.
- **Timestep** - milliseconds per column. Smaller means finer timing, and
  more columns for the same Duration.

These settings are fixed once the project exists. To change them later,
rebuild the project with [Remastering](#remastering).

## A First Session

This walk-through touches the main features in ten minutes. Every step links
to the chapter that covers it in full.

1. **Create a project** - **New Project...** on the start screen; give it a
   name and folder and click **OK**. The canvas opens, dark and empty.
2. **Import a sound** - **File → Import Audio...**, or drag an audio file
   onto the window. Its spectrogram appears as a new layer in the
   **Layers** panel on the right. ([Importing](#importing))
3. **Listen** - choose **Playback** from the **Input/Output** button at the
   top right, and press **Play**. A line sweeps across the canvas as it
   plays. ([Playback](#playback))
4. **Paint** - choose **Paint** from the **Tool** button in the toolbar,
   click your layer's name in the Layers panel, and drag across the canvas.
   Press Play again to hear your stroke. Open **Configure → Tool
   Configuration** to change the brush. ([Painting](#painting))
5. **Change it afterwards** - choose **Pick**, click your stroke, and drag it
   somewhere else, or change its settings in Tool Configuration.
   ([Pick](#pick))
6. **Add an effect** - click **+ Add Filter Layer** in the Layers panel,
   choose a **Filter Type** such as **Spectral Reverb**, and play again.
   ([Filter Layers](#filter-layers))
7. **Undo** - **Ctrl+Z** steps back through anything you did.
   ([Undo and History](#undo-and-history))
8. **Save and share** - **Ctrl+S** saves; **File → Export Audio...**,
   **Export Video...** or **Export Image...** make something to share.
   ([Exporting](#exporting))

## The Main Window

With a project open, the window has:

- **The canvas** (centre) - all visible layers combined.
- **The toolbar**, with:
  - **Tool** - the canvas's current tool: **Pan** (the default: just look
    around), **Paint**, **Pick**, **Select**, **Path** or **Text**. The
    button names the active tool. Turning a tool off returns to Pan.
  - **Layers** - shows or hides the Layers panel (shown by default).
  - **Configure** - opens the setup panels: **Tool Configuration**, **MIDI
    Configuration**, **Selection Configuration**, **Grid**, **Filter
    Configuration** and **MindWaves**. Any number can be open at once.
  - **History** - shows or hides the History panel.
  - **Zoom out**, **Zoom in**, **Fit to Window** and **Actual Size**.
- **Input/Output** (top right) - opens **Playback**, **Record** or
  **Loop**. Only one of the three is open at a time.
- **Macro** (top right) - **Record Macro**, **Play Macro**, **Export Macro
  Video...** and **Manage Macros...** ([Macros](#macros)).
- **The menus:**
  - **File** - New, Open, Save and Save As; Remaster; Import Audio, Image
    and MIDI; Export Audio, Video and Image.
  - **Edit** - Undo and Redo, Delete, and submenus for stack order, path
    editing, selections, the clipboard, capturing brushes, picked-path
    tools and layers; also **Image Mode** and **Decimal Separator**.
  - **View** - Zoom, **Hardware Acceleration**, **Sound Flower**, **Show
    Clipping**, **Composer Mode** and **Analysis**.
  - **Resource Browser** - opens the [Resource Browser](#resource-browser).
  - **Help** - the documentation and **About Sound Mind Studio**.
- **The status bar** (bottom) - shows the position under the mouse, in
  pixels and in time and frequency (`30, 10 px | 0.300 s, 523 Hz`), and
  short messages. Long tasks (importing, exporting, preparing playback) show
  their progress here, with a **✕** button to cancel.

Entries in the menus and right-click menus that don't apply right now - for
example **Paste** with nothing copied - are greyed out.

### Arranging panels

Panels open on the right, as tabs; opening one brings its tab to the front.
Drag a panel's title bar to move it to another edge of the window or float
it on its own, and drag the edge between panels to resize them. A panel
whose content doesn't fit shows a scrollbar.

## Working with the Canvas

### Zooming and scrolling

By default the canvas fills the space available (**Fit to Window**). Zooming
in gives it a fixed size, with scrollbars once it no longer fits.

The zoom commands are in **View → Zoom**, and the most common are on the
toolbar:

| Command | Shortcut | Effect |
|---|---|---|
| Zoom In / Zoom Out | `]` / `[` | Both axes |
| Zoom In / Out (Frequency Only) | `Alt+]` / `Alt+[` | Vertical axis only |
| Zoom In / Out (Time Only) | `Shift+]` / `Shift+[` | Horizontal axis only |
| Zoom In / Out (Coarse) | `Ctrl+]` / `Ctrl+[` | Both axes, a bigger step |
| Fit to Window | `Ctrl+0` | Fill the space available |
| Actual Size | `Ctrl+1` | One screen pixel per canvas cell |

The **mouse wheel** zooms the same way with the same modifier keys. Without a
modifier it scrolls the canvas, once the canvas is larger than the window.

The modifier keys mean the same thing here as elsewhere: **Alt** acts on the
frequency axis only, **Shift** on the time axis only. When you start a new
selection, Shift and Alt mean add and subtract instead (see
[Selections](#selections)).

### Number fields

Every number setting is a **slider with a text box**:

- **Drag the slider** to choose a value; it's applied when you let go.
  Clicking the slider, or using the arrow keys once it has focus, applies
  straight away.
- **Type in the text box** and press **Enter** (or click elsewhere). Typing
  is forgiving: spaces, a unit after the number ("12 dB") and either `.` or
  `,` as the decimal mark are fine. A value outside the allowed range is
  clamped to it; text that isn't a number is ignored.
- Some sliders cover only the useful part of what you can type - for
  example a brush's **Brush Size**, or a MindWave's **Period**. Type a
  larger value and the slider simply sits at its end.

**Edit → Decimal Separator** chooses whether numbers show a period (1.5) or a
comma (1,5); it's saved with the project.

Wherever you set a loudness - a stroke's intensity, a filter's noise floor,
text - a small **dB / value** drop-down beside it switches between decibels
(-96 to 0) and a value from 0 (silence) to 1 (full scale). It only changes
how the number is shown.

### Brightness and loudness

Brightness follows loudness on one absolute scale: black is silence
(-96 dBFS) and full brightness is full scale (0 dBFS). Imported, recorded
and painted sound all use this scale, so a painted stroke as bright as an
imported sound is about as loud.

**View → Show Clipping** marks every spot louder than full scale with a
magenta and black checker. Those spots will distort when played or exported.

### Sound Flower (polar view)

**View → Sound Flower** (`Ctrl+Alt+F`) wraps the canvas into a disk: time
runs clockwise around the ring from twelve o'clock, and frequency rises from
the centre outwards. It's another way to look at the same project - it
changes nothing, and projects always open in the flat view.

All the drawing tools work on the flower and land in the same place they
would in the flat view. The playback position becomes a turning ray, and a
MindWave preview wraps onto the disk too.

While the flower is shown, Overlay Grids, Axis Labels, bounding boxes and
path geometry, the analysis overlays and a rectangular Pick or Selection
outline are hidden, and Snap to Grid is paused. They return in the flat
view.

Images that are already flower-shaped can be unwrapped on import - see
[Polar images](#polar-images).

---

# Part 2 - Building a project

## Importing

### Importing audio

**File → Import Audio...** reads WAV, MP3, FLAC, Ogg, AIFF, M4A and Opus
files.

- A file no longer than the project's Duration becomes one new layer.
- A longer file is split into Duration-length **snippets**. A list lets you
  tick the ones to import; each becomes a layer named `<file>_0000`,
  `<file>_0001`, and so on.
- **Offset** skips that much of the start of the file before splitting, so
  you can line up a particular moment with the start of a snippet. Changing
  it recomputes the list.

Audio at a different sample rate than the project (a 44.1 kHz file in a
48 kHz project, say) is converted to the project's rate, keeping its pitch
and length.

Importing runs in the background - keep working while it encodes, or click
**✕** in the status bar to cancel, which adds nothing. New Project, Open
Project and closing the Studio wait until it's done.

### Importing images

**File → Import Image...** reads PNG, JPEG, BMP, TGA and WebP files, one or
several at a time, and asks how to fit each one to the canvas:

- **Rescale to fit project** (the default) - stretched to fill the canvas
  exactly.
- **Scale vertically to fit project, keep horizontal resolution** - the
  height fits the canvas; the width stays the image's own.
- **Scale horizontally to fit project, keep vertical resolution** - the
  other way round.
- **Scale vertically to fit project, rescale horizontal in proportion** -
  the height fits, keeping the image's proportions.
- **Keep native resolution** - no scaling.

With several files selected, **Import as sequence** places them end to end
in time on one layer, in alphabetical order (so `frame001.png`,
`frame002.png`... land in order), instead of one layer each.

#### Polar images

A picture that's already a flower - a polar spectrogram from elsewhere, or
any image radiating from a centre - can be unwrapped into an ordinary layer:

1. **File → Import Image...** and choose a single image.
2. Choose **Polar - un-warp a polar/flower-shaped image to rectangular**.
3. Click **Set origin...** and place the handles on the preview:

| Handle | Looks like | Sets |
|---|---|---|
| Crosshair | Yellow lines | The flower's centre |
| Ring | Yellow circle, dragged from its top | How far out to read |
| Arc start | Blue square on the ring | Where the layer's left edge is read from (0° = twelve o'clock, clockwise) |
| Arc end | Pink square on the ring | Where its right edge is read from; the same as Arc start reads the full circle |

Each handle has a matching number field. **Output width** sets the new
layer's width; it follows the radius until you type a value. The label
beside it shows how long the layer will play.

### Importing MIDI

**File → Import MIDI...** reads Standard MIDI Files (`.mid`, `.midi`). Every
note becomes an editable stroke, so you can Pick, move and change notes
afterwards.

A file with one channel that fits the project imports at once, as a layer
named `<file> - Ch<N> (<instrument>)`. Otherwise a dialog lets you:

- tick which channels to import (each shows its instrument and note count);
- **Separate layer per channel** (on by default), or merge the ticked
  channels into one layer;
- **Import whole file** (on by default), or untick it to choose
  Duration-length snippets, as for audio.

#### MIDI Configuration

**Configure → MIDI Configuration** decides how each instrument's notes are
painted. Each row maps a General MIDI program (0-127) to:

- **Tool Preset** - a saved brush its notes paint with; **None** uses the
  plain default brush.
- **Duration Scale** - lengthens or shortens its notes (`2.0` doubles them).
- **Pitch Offset (st)** - transposes its notes by semitones (`12` is an
  octave up).

**Add Mapping** adds a row for the program chosen beside it; **Delete**
removes a row. Changes apply at once and are saved with the project, so
every later MIDI import uses them.

### Drag and drop

Drop files onto the window to import them: images, audio and MIDI files open
the same dialogs as the File menu, and a `.smproj` file opens that project
(after asking about unsaved changes). Other files are ignored. Cancelling any
dialog cancels the whole drop. If one file fails, the status bar says so and
the others still import.

## Layers

The **Layers** panel lists every layer, top of the stack first. What you see
and hear is all the visible layers combined.

### The layer list

Above the list:

- **+ Add Layer** adds an empty layer to paint on.
- **+ Add Filter Layer** adds a [Filter layer](#filter-layers).
- **+ Generate Layer** adds a layer filled by a generator (see
  [Generated layers](#generated-layers)).

Every project has two fixed layers, marked with a lock: the **Background**
at the bottom, and the **Equalizer** at the top (see
[The Equalizer](#the-equalizer)). They can't be moved or deleted.

Each row shows the layer's name over a small preview of its content, and a
**loudness reading** (`avg -18 dB / peak -6 dB` while stopped; the level at
the playback position while playing). The **eye button** cycles the layer
through:

- **Visible** (open eye) - seen and heard;
- **Muted** (speaker crossed out) - seen, not heard;
- **Invisible** (closed eye) - neither.

**Click a layer's name to select it.** The selected layer is the one you
paint, fill and paste onto, and its row opens to show its controls:

- **Drag handle** (⠿) - drag to reorder.
- **Gain** - the layer's level, 0-100%.
- **Balance** - pans the layer left or right. Centred leaves it unchanged;
  panning keeps the side you pan towards at full level and fades the other.
- **MindWave** - lets a [MindWave](#mindwaves) vary the layer's gain across
  the canvas (it multiplies the Gain setting).
- **Translation** and **Rescale** - see [Layer timing](#layer-timing).
- **Blend Mode** - see [Blending](#blending).
- **Duplicate** (⧉) - copies the layer with everything about it.
- **Clean up phase** (✦) - removes the random phase noise imported audio
  often has in its silent parts, which shows as speckle but can't be heard.
  Nothing audible changes.
- **Delete** (×, also shown when you hover over a row) - deletes the layer
  and everything painted on it. If Mind Grain strokes elsewhere read from
  it, you're asked what to do with them. To keep a layer but get it out of
  the way, hide it instead.
- **Edit filter** (⚙, on Filter layers and the Equalizer) - selects the
  layer and opens Filter Configuration.

The Background layer has no Gain, Balance, MindWave, timing or Blend Mode
controls: it's always fully present and always first.

Double-click a name to rename it; names are kept unique by adding " (2)",
" (3)" and so on. A layer whose gain follows a MindWave shows that
MindWave's preview in a small row beneath it.

**Right-click a row** for **Hide other layers**, which hides every other
layer and remembers how each one was; **Unhide other layers** restores them.

**Keyboard** (also in **Edit → Layers**): **Page Up** / **Page Down** select
the layer above or below; **Shift+Page Up** / **Shift+Page Down** move the
selected layer; **Ctrl+Page Up** / **Ctrl+Page Down** raise or lower its gain
by 5% (the menu calls these Increase and Decrease Layer Opacity).

### Blending

**Blend Mode** decides how a layer combines with the layers beneath it:

- **Normal** (the default) - layers add together like instruments playing at
  once: two layers at full gain both come through in full. Gain is the
  layer's volume in that mix.
- **Overwrite** - replaces what's beneath, in proportion to Gain (100%
  replaces it entirely; 0% leaves it untouched).
- **Multiply**, **Screen**, **Overlay**, **Difference** and **Add** - the
  familiar image-editor blend modes, combining a layer with what's beneath
  it the way an image editor would.

A layer's gain and blend mode affect only that layer.

### Layer timing

- **Translation** - moves a layer's content earlier (negative) or later
  (positive), in canvas columns.
- **Rescale** - stretches (above `1.0x`) or squeezes (below) a layer's
  timeline, to match the pace of two clips.

Both change what you hear as well as what you see, in Playback, Loop Mode
and every export.

### Generated layers

**+ Generate Layer** fills a new layer from a generator; afterwards it's an
ordinary layer you can paint on and filter. Choose the **Generator**, set its
options and click **OK**. Every generator takes a **Seed** - the same seed
always gives the same result; **Randomize** picks a new one.

- **Lattice** - an organic, self-similar texture. **Chaos ↔ Order** runs from
  broadband noise, through rich texture, to smooth, rigid structure.
- **Fractal** - a branching tree of tones: one long tone with branches at
  musical intervals, and branches from those, each a little quieter. Towards
  **Order** the branches split evenly at octaves, fifths and thirds and hold
  their pitch; towards **Chaos** they sprout at random and slide.
- **Streaming** - eight voices that keep changing over the whole layer,
  gliding and swelling, pulling on each other. Order gives a steady drone,
  the middle keeps evolving, Chaos wanders.
- **Noise** - coloured noise over the whole canvas:
  - **Color** - **White** (equal energy at every frequency), **Pink** (equal
    per octave, the most natural-sounding), **Red / Brownian** (bass-heavy),
    **Purple / Violet** (treble-heavy), **Gray** (equally loud to the ear at
    every frequency) or **Custom spectrum**.
  - **Level** - how loud it is at 1 kHz (everywhere, for Gray).
  - **Horizontal / Vertical resolution** - how many columns and rows one
    noise cell covers: 1 is fine hiss, larger is blockier and slower.
  - **Custom spectrum** - draw the noise's shape with the gradient editor
    (low frequencies on the left).
  - **MindWave** bindings next to each resolution and under the custom
    spectrum vary them across the canvas. Binding the custom spectrum to a
    time-axis MindWave makes the noise swell and fade.

## Filter Layers

A **Filter layer** holds no sound of its own. It takes every visible layer
beneath it - down to the next Filter layer, if there is one - and reshapes
the result. Moving a Filter layer up or down changes what it affects. A
Filter layer with nothing visible beneath it does nothing.

### Adding and configuring a filter

Click **+ Add Filter Layer** in the Layers panel. Selecting a Filter layer
opens **Filter Configuration** with its settings. The panel can also be
opened from **Configure** before any Filter layer exists: dial in a filter
first, then click **+ Add Filter Layer**, and the new layer starts with those
settings.

At the top of the panel:

- **Filter Preset** - **Save...** stores the whole configuration under a
  name (if the name is taken you can replace the old one, choose another
  name, or cancel); choosing a preset from the drop-down loads it. Saved
  presets also appear in the [Resource Browser](#resource-browser), where
  they can be shared as `.smfilter` files. Under the row, "Preset: <name>"
  shows which preset is loaded, and turns orange with "* (modified)" once
  you change something.
- **Loop Preview** - tune a filter by ear. A few seconds loop with the
  filter applied - your selection's time range if you have one, otherwise
  about 4 seconds from the playback position - and every change is heard on
  the next time round. What you hear:
  - with a Filter or Equalizer layer selected: the whole project, with that
    layer using the settings you're adjusting;
  - otherwise, with a canvas selection: the filter inside the selection,
    as **Apply Filter to Selection** would do it;
  - otherwise: the filter over the whole mix.

  Click again to stop. Starting playback or opening another project stops
  it too.
- **Filter Type** - which filter. A new Filter layer starts as
  **Frequency-Axis Gradient**. Each type shows only its own settings, and
  switching types keeps what you'd entered for the others.

### Filter types

| Filter | What it does | Settings |
|---|---|---|
| **Frequency-Axis Gradient** | Pushes each frequency's loudness towards a target, following a gradient from low (left) to high (right) frequencies. A new one does nothing until you raise an Opacity. | A gradient editor (see below): **Intensity** is the target loudness, **Opacity** how strongly to push (`0` none, `1` all the way). **Link Channels** edits left and right together. |
| **Uniform Blur** | Softens evenly in every direction. | **Sigma** (strength) |
| **Edge-Preserving Blur** | Softens without smearing across sharp edges. | **Size** |
| **Directional Blur** | Softens along one direction. | **Length**, **Angle** (0° along time, 90° along frequency) |
| **Sharpen** | Tightens detail. | **Amount** |
| **Tone Curve** | Remaps loudness through a curve you draw: click to add a point, drag to move it, double-click to remove it. Starts as a straight line (no change). | The curve |
| **Speckle Add** | Boosts a scattering of cells towards full loudness. The pattern stays fixed unless you change the settings or the content beneath. | **Density**, **Intensity** |
| **Speckle Remove** | Removes isolated loud or quiet flecks without softening real detail. | **Threshold** |
| **Denoise** | Turns down everything below a loudness floor. | **Noise Floor**, **Reduction** |
| **Bit-Depth Crush** | A crunchy, lo-fi effect that steps loudness into fewer levels. | **Amount** (`0` none) |
| **Granular Noise** | Adds a coarse, blocky noise texture. Fixed like Speckle Add. | **Grain Size**, **Grain Amount** |
| **Dynamic Speckle** | Like Speckle Add, but the pattern changes on every redraw - a flickering texture. Shares Speckle Add's settings. | **Density**, **Intensity** |
| **Feedback Distortion** | A resonant, ringing smear along time. | **Amount** |
| **Spectral Wavefold** | A wavefolder: folds loudness back on itself for a harsh, dense sound. | **Fold Gain** (`1` none) |
| **Channel Balance** | Moves loudness between left and right. `0.5` leaves a balanced sound unchanged. | **Balance** (`0` all left, `1` all right) |
| **Invert** | Quiet becomes loud and loud becomes quiet. | None |
| **Convolve** | Applies a hand-edited convolution kernel (see below). | **Preset**, **Kernel Size**, the grid, **Normalize**, **Amount** |
| **Displace** | Shifts content in a direction, blending smoothly between cells. | **Distance**, **Angle** |
| **Channel Cycle** | Rotates left loudness, right loudness and phase into each other - a wild, glitchy effect. | **Angle** (`0`/`360`° none; each `120`° is one full step) |
| **Spectral Reverb** | Extends sound through time like a room. | **Pre-Delay** and **Decay** (in frames), **Room Size** (shortens the decay), **Diffusion** (smoother tail), **Absorption** (damps highs faster), **Mix** (`0` dry, `1` fully wet) |
| **Downsample** | Pixelates over square blocks. | **Mode** (**Block Hold** for hard edges, **Block Average** for a softer look), **Block Size** (`1` none) |

**Convolve** in detail:

- **Preset** drops a classic kernel into the grid (Identity, Sharpen, Edge
  Detect, Emboss, Box Blur, Gaussian Blur, Sobel X, Sobel Y), ready to edit.
- **Kernel Size** sets the grid's size (always odd); changing it starts a
  fresh grid.
- Each grid cell is the weight of one neighbouring cell.
- **Normalize** scales the kernel so a blur doesn't brighten or darken.
- **Save As New Kernel** stores the grid under a name for reuse in any
  Filter layer or project; **Load Saved Kernel** copies a saved one into the
  grid.

**MindWave bindings.** Most filter settings have a **MindWave** drop-down
beside them. Binding one makes that setting vary across the canvas,
following the MindWave: the number you set is the setting's value where the
MindWave is at its peak. Settings that work on a whole neighbourhood at once
- Kernel Size, Grain Size, and Spectral Reverb's Pre-Delay, Decay, Room Size,
Diffusion and Absorption - can't be bound.

**The gradient editor** (used by Frequency-Axis Gradient, the Equalizer,
brushes and Fill Selection): click a stop on the bar to select it and edit
its **Left/Right Intensity** and **Left/Right Opacity** below; double-click
the bar to add a stop; drag a stop to move it; **Delete Stop** removes the
selected one. The two end stops can't be moved or deleted.

### The Equalizer

Every project has one special Filter layer, the **Equalizer**, locked at the
top of the stack. It starts with no effect. Selecting it opens Filter
Configuration in Equalizer mode:

- a gradient bar whose stops have **Left Cut** and **Right Cut** (`0` leaves
  that frequency alone, `1` silences it) - the Equalizer only ever cuts;
- a **vertical EQ curve** beside it, lined up with the canvas (high at the
  top). Drag a point up or down to move it, click to add one, double-click
  to remove one. The curve and the gradient bar are the same thing, shown
  two ways;
- **Preview on Canvas**, which darkens the canvas where the Equalizer cuts.
  It stays on for the session.

## MindWaves

A **MindWave** is a reusable shape that varies across the canvas - over
time, over frequency, or both - with values from 0 to 1. Binding a MindWave
to a setting makes that setting follow the shape instead of staying fixed.
You can bind MindWaves to:

- a layer's **Gain** ([Layers](#layers));
- most **filter settings** ([Filter Layers](#filter-layers));
- a brush's **Opacity**, **Size** and **Color**, and an Instrument brush's
  **Vibrato** and **Tremolo** ([Painting](#painting));
- a generated noise layer's resolution and spectrum
  ([Generated layers](#generated-layers)).

Every MindWave drop-down also offers **Create New MindWave...**, which
creates one, binds it, and opens the MindWaves panel so you can shape it.

### The MindWaves panel

**Configure → MindWaves** shows your MindWaves, each with a small preview of
its shape.

- **+ Add MindWave** creates one (an audible sine wave) and selects it.
- Double-click a name to rename it; **×** deletes it.
- **Preview** shows the selected MindWave on the canvas as a grayscale
  overlay (black is 0, white is 1), updating as you edit it. It never
  changes the sound, and switches off when you open another project or close
  the panel.

Selecting a MindWave opens its editor:

1. **Generator Type** - the kind of shape (below).
2. That type's own settings.
3. **Axis & Timing** - which **Axis** the shape runs along (Time or
   Frequency), and its **Period** (the length of one cycle, in seconds or
   bins).
4. **Randomness** - the **Seed** (which random values you get) and **Noise
   Scale**, for the random shapes.
5. **Advanced** (click to open) - **Phase** (where the cycle starts; one
   cycle is 0 to 6.283 radians), **Noise Octaves** and **Noise Persistence**
   (detail in fractal noise), **Reversed** (run backwards: a decay becomes a
   build-up) and **Inverted** (flip it: 0 becomes 1).

Only the settings the chosen type actually uses are shown.

### Generator types

| Type | Shape | Its own settings |
|---|---|---|
| **Periodic** | A repeating wave. | **Waveform**: Sine, Triangle, Square, Sawtooth, Pulse (with **Duty Cycle**) or Rectified Sine |
| **Envelope** | A one-shot shape around a **Center**. | **Shape**: Exponential Decay or Decaying Oscillation (with **Decay Rate**), S-Curve, Peak (Bell) or Ramp (with **Steepness**) |
| **Stepped/Noise** | Steps or smooth random values. | **Shape**: Stepped (with **Step Count**), Gaussian Noise or Fractal Noise |
| **Spatial** | A two-dimensional pattern across both axes. | **Pattern**: Ripples (with **Center X/Y**), Checkerboard, Cellular, or Domain-Warped Noise (with **Domain Warp Strength**) |
| **Fractal** | A jagged, self-similar line. | **Roughness**, **Iterations** |
| **Drawn** | A curve you drew (below). | None - a status line says how many points were captured |
| **Step Grid** | A repeating list of values you type. | **Steps**, and one value (0-1) per step |
| **Continuous** | One shape you sweep between clean and wild. | **Shape** (sine to fractal noise), **Skew** (earlier or later in the cycle; `0.5` is none), **Character** (fine turbulence). Each has a dial and a number field, and a live graph shows the result. |
| **Resonance** | A captured [Resonance profile](#paths-and-resonance-profiles)'s spectrum, repeated once per Period. | **Resonance Profile** |
| **Noise** | Cells of random values. | **Color** (how values change from cell to cell along the axis: White jumps, Pink wanders, Red drifts, Purple flickers, Custom slope from -2 to +2), **Cell Width** (seconds), **Cell Height** (rows) |

**Drawing a MindWave.** Draw a stroke or a path on the canvas, select it with
[Pick](#pick), then choose **Edit → Picked Path Tools → Use Picked Path as
MindWave Shape**. The selected MindWave becomes a Drawn MindWave with that
curve. It repeats once per Period, and the curve's lowest and highest points
become 0 and 1.

### Warp

The **Warp** group under the editor lets a second MindWave bend the first:
the first is read at positions pushed back and forth by the second, giving a
less mechanical result. Tick **Enable Warp** to show **Strength** and a
second editor for the warping MindWave ("Warped by"). Untick it to remove
the warp.

---

# Part 3 - Drawing and editing

## Painting

Choose **Paint** from the **Tool** button and drag on the canvas. A white
outline follows your stroke, and it's painted when you let go.

**The selected layer is painted** - the one whose name you last clicked in
the Layers panel. With none selected, you paint on the Background layer.

Configure the brush in **Configure → Tool Configuration**.

### Tool presets

At the top of Tool Configuration, **Tool Preset** lists the brushes saved in
this project. Choosing one loads all of its settings. **Save...** saves the
current settings under a name (a taken name can be replaced, renamed or
cancelled); **Delete** removes the chosen preset, without affecting anything
already painted. Instrument and Resonance presets show a small picture of
their spectrum. **Preview** plays the current settings as a 3-second stroke
so you can hear a brush before using it; it reads **Stop** while playing.
"Preset: <name>" under the row shows which preset is loaded, turning orange
with "* (modified)" once you change something.

### Tool types

**Tool Type** chooses what the brush does. Each type shows only the settings
it uses.

| Type | What it paints | Its own settings |
|---|---|---|
| **Procedural** (default) | A plain brush in the gradient's colour. | **Tip Shape**: Circle, Square, Diamond, Triangle, Single Stroke, Cross, Star, Corner, Arc, Dot Spatter or Dapple; **Thickness** for the line and dot shapes |
| **Instrument** | A pitched tone with overtones. | **Harmonics** (how many, and each one's strength - type them, or drag the bars of the chart), **Inharmonicity** (`0` pure; small values such as `0.01`-`0.05` give a bell-like stretch), **Vibrato** and **Tremolo** (see below) |
| **Resonance** | A captured [Resonance profile](#paths-and-resonance-profiles) as partials above the painted pitch. | **Resonance Profile**, **Decay Rate** (`0` sustains; higher fades out sooner along the stroke), **Frequency Scale** (how many octaves the spectrum spreads above the pitch), **Time Span** (the stamp's length, instead of Brush Size) |
| **Mind Shot** | Stamps a captured piece of canvas (see [Capturing brushes](#capturing-brushes)). | **Mind Shot**, **Blend Mode** |
| **Mind Grain** | Stamps a live piece of another layer (see below). | **Mind Grain**, **Blend Mode** |
| **Heal** | Blurs along time only - erases a short stray mark without disturbing what's around it. | None |
| **Soften** | Blurs in every direction. | None |
| **Smudge** | Drags content along your stroke, like a finger in wet paint. A click without dragging does nothing. | None |
| **Order/Chaos** | Pushes content towards order or chaos. | **Amount**, from `-1` (Chaos: shuffles the cells under the brush) to `+1` (Order: pulls them towards the loudest moment and pitch, which can turn noise into a tone or steady a wavering pitch). `0` does nothing; smaller amounts affect fewer cells. |

**Vibrato and Tremolo** (Instrument) - each a depth and a MindWave. Binding a
MindWave makes it bend the pitch (Vibrato, in semitones) or the strength
(Tremolo) over the course of each stroke, so the whole shape plays out over
any stroke, long or short. **None** turns it off.

**Mind Shot** stamps exactly what was captured, at its own size. If the Mind
Shot has a Fundamental Frequency, each stamp is pitch-shifted to wherever you
paint it.

**Mind Grain** stores no picture - it reads its source layer's region each
time, so repainting the source updates every stroke made with it. A Mind
Grain can only paint on layers **above** its source: on other layers the
Mind Grain settings turn red, the Layers panel marks those layers with a red
**✕**, and **Paint** is disabled. Moving or deleting a layer in a way that
would break an existing Mind Grain stroke is refused, with an explanation.

For **Heal**, **Soften** and **Smudge**, **Brush Size** is also how far the
blur reaches, and **Opacity** how strongly it applies. Heal, Soften, Smudge
and Order/Chaos always stamp densely along the stroke, so they hide Stamp
Mode and Interval.

### Shared settings

- **Falloff** - how soft the edge is, from hard (`0`) to fully soft (`1`).
- **Brush Size** - the tip's radius: in seconds along time, and the matching
  span along frequency. The default (`0.2`) is a clearly visible stroke. The
  slider goes to `5`; type up to `100` for a larger brush.
- **Stamp Mode** - how often the tip is stamped along the stroke:
  - **Stroke** (default) - wherever the mouse was sampled, with no gaps;
    slow drawing stamps more densely than fast drawing.
  - **Along Curve** - at even distances along the stroke.
  - **Time Axis** - each time the stroke crosses an evenly spaced moment.
  - **Frequency Axis** - each time it crosses an evenly spaced frequency.
- **Interval** - the spacing for the last three modes, in seconds (or Hz
  for Frequency Axis; in Image Mode, pixels and rows). Its label shows the
  unit.
- **Stamp Pattern** (Along Curve only) - cycle through uneven spacings
  instead of one Interval: steps separated by spaces, each a number followed
  by `ms` or `b` (beats at the project's tempo). `100ms 200ms` alternates a
  short and a long gap. A mistake shows in red underneath, and the plain
  Interval is used until you fix it.
- **Gradient** - the stroke's loudness and stereo balance, in the
  [gradient editor](#filter-types): **Left/Right Intensity** is each
  channel's loudness (-96 to 0 dB), **Left/Right Opacity** how strongly the
  stroke applies it. A gradient with more stops changes along the stroke as
  you draw. The default (both stops at -20 dB, full opacity) is clearly
  audible. Heal, Soften, Smudge and Order/Chaos show only the Opacity.
- **Opacity MindWave**, **Size MindWave**, **Color MindWave** - bind a
  [MindWave](#mindwaves) to vary each stamp's strength, size, or which point
  of the Gradient it takes its colour from.
- **Binding Frame** - how those three read their MindWave: **Canvas Space**
  (the default) at each stamp's position on the canvas, so identical strokes
  in different places differ; **Operation-Relative** along the stroke
  itself, so identical strokes look the same anywhere.
- **Blend Mode** (Mind Shot and Mind Grain) - how a stamp combines with what
  was there: **Overwrite** (default) or the [blend modes](#blending).

At the top of the panel, **Show bounding boxes** outlines every stroke on
the shown layer in cyan, and **Show path geometry** draws every stroke's path
in magenta.

### Image Mode

**Edit → Image Mode** changes how a brush's shape is measured along the
frequency axis (for Procedural, Heal, Soften, Smudge and Order/Chaos). Off
(the default), a brush covers a fixed range of frequencies, so on the
canvas's logarithmic frequency axis a circle looks wide near the top and tall
near the bottom. On, a brush keeps a fixed size in canvas pixels - a circle
looks like a circle everywhere, and stamp spacing is even across the canvas -
but painted sounds no longer keep their musical intervals when moved. The
choice is saved with the project and only affects new strokes.

## Pick

Choose **Pick** from the **Tool** button and click anything you've painted,
filled, pasted or written - it's outlined in white. Then:

- **Move it** - drag it.
- **Change it** - open Tool Configuration (or the Text Tool panel for text):
  it shows the settings the stroke was made with. Change any of them and the
  stroke is redrawn. (Filled and pasted pieces can be moved and deleted, but
  have no settings to change.)
- **Delete it** - the **Delete** key.
- **Restack it** within its layer - **Edit → Stack Order**: **Bring to
  Front** (`Ctrl+Shift+Up`), **Send to Back** (`Ctrl+Shift+Down`), **Bring
  Forward** (`Ctrl+Up`), **Send Backward** (`Ctrl+Down`).
- **Edit its path** - **Edit → Path Editing → Edit Path** (strokes and
  paths). Its nodes appear as dots:
  - click a node to select it, and drag it to move it;
  - drag one of the selected node's cyan handles to reshape the curve - the
    opposite handle mirrors it;
  - **Toggle Node Type** switches the selected node between smooth and
    corner;
  - **Delete** removes the selected node (but not the last one);
  - **Apply Path Edit** keeps the changes as one undoable step; **Cancel
    Path Edit** throws them away.

Moving or changing an object never changes what's on top of or under it: a
stroke you filled over stays filled over.

**Overlapping objects:** click the selected object again to select the one
underneath; each further click goes one deeper.

**Several at once:** drag from an empty part of the canvas to draw a dashed
rectangle; everything entirely inside it on the selected layer is selected.
Then drag any of them to move them all, press **Delete**, or **Ctrl+C** /
**Ctrl+V** to paste copies in the same place (already selected, ready to
drag).

**Right-click** an object (with any tool) for **Edit Path**, **Bring to
Front**, **Send to Back** and **Delete**; with several selected, **Copy** and
**Delete**. A path also offers **Stroke Path with Current Tool** and **Create
Resonance Profile...** (see [Paths and Resonance
Profiles](#paths-and-resonance-profiles)). Right-clicking empty canvas offers
**Paste**, **Fill Selection...**, **Apply Filter to Selection** and
**Deselect**.

## Selections

Choose **Select** from the **Tool** button and drag on the canvas to select
a region, shown with a green outline. The selection stays when you switch
tools; it's only drawn while Select or Pick is active. **Ctrl+D** (or a click
without dragging) deselects.

**Configure → Selection Configuration** sets:

- **Selection Type**:
  - **Rectangle** (default) - drag a box.
  - **Lasso** - trace any shape.
  - **Wand** - click once to select a connected area of similar loudness.
    **Tolerance** sets how different a neighbour may be and still join (0%
    exactly the same, 100% everything reachable). **Harmonics-aware** also
    selects the clicked pitch's overtones, to catch a whole note.
- **Paste Blend Mode** - how **Paste** combines with what's there:
  **Overwrite** (default) or the [blend modes](#blending).

**Combining selections:** hold **Shift** while starting a new selection to
add to the current one, **Alt** to subtract, or **Shift+Alt** to keep only
the overlap. Any selection types can be combined.

**Rotating:** a plain Rectangle selection has a handle above its top edge -
drag it to rotate the selection.

Everything below follows the selection's exact shape, not just its bounding
box.

### Using a selection

- **Edit → Selection → Fill Selection...** fills it using the
  [gradient editor](#filter-types); the gradient runs left to right across
  the selection.
- **Edit → Selection → Apply Filter to Selection** applies the filter
  currently set up in Filter Configuration to the selection only, once.
- **Edit → Clipboard → Copy** (`Ctrl+C`) copies it; **Cut** (`Ctrl+X`)
  copies it and silences it.
- **Edit → Clipboard → Paste** (`Ctrl+V`) puts the copy back at the same
  position, on the selected layer - select another layer first to paste
  there. It combines using the Paste Blend Mode, then switches to Pick with
  the pasted piece selected, ready to move.

### Capturing brushes

- **Edit → Capture → Capture as Mind Shot** saves the selection as a
  [Mind Shot](#tool-types) brush. A dialog asks for:
  - a **Name**;
  - a **Fundamental Frequency** - set it to have the stamp pitch-shifted to
    wherever you paint it; leave it at `0` to stamp at the original pitch;
  - a **Start-Time Offset** - how much lead-in the capture has before its
    attack, so the attack lands where you paint.

  The canvas and the clipboard are left untouched.
- **Edit → Capture → Capture as Mind Grain** asks the same and saves a
  [Mind Grain](#tool-types): a live reference to this region of this layer,
  not a picture.

Both capture the selection's whole bounding box. Choose **Mind Shot** or
**Mind Grain** as the Tool Type to paint with them.

## Paths and Resonance Profiles

### The Path tool

Choose **Path** from the **Tool** button and draw as you would paint. The line
is kept as a smooth, **silent** path - shown in cyan, heard nowhere. Paths are
guides:

- [Pick](#pick) a path to move it, delete it, or edit its nodes.
- Right-click it, or use **Edit → Picked Path Tools → Stroke Picked Path with
  Current Tool**, to paint along it with the current brush (as one undoable
  step; the path stays).
- Use it for **Create Resonance from Picked Path...**, **Use Picked Path as
  MindWave Shape**, or as a branch of a branching path.

A click without dragging draws nothing.

### Branching paths

A **branching path** is a tree of curves, shown in pink. Make one in either
of two ways:

- **Merge** - select several paths or strokes with Pick, right-click, and
  choose **Merge into Branching Path** (also **Edit → Picked Path Tools →
  Merge Picked Paths into Branching Path**). They're joined by the shortest
  connections. Paths are replaced by the merged one; strokes lend their
  shape and stay where they are.
- **Grow one by hand** - Pick a stroke or path as the trunk and choose **Edit
  → Picked Path Tools → Start Branching Curve from Picked Path**. Then, for
  each branch: click the point on the tree where it should attach, draw a
  new stroke, Pick it, and choose **Add Picked Path as Branch**. **Cancel
  Branching Curve** abandons the tree.

### Resonance profiles

A **Resonance profile** turns a drawn shape into a spectrum you can paint
with:

- Pick a stroke or path (a branching path counts with all its branches) and
  choose **Edit → Picked Path Tools → Create Resonance from Picked Path...**
  (or **Create Resonance Profile...** in its right-click menu).
- For a tree grown by hand, choose **Create Resonance from Picked Graph...**.

Name it, and it's saved in the project. Choose **Resonance** as the Tool
Type to paint with it, or use it in a [Resonance MindWave](#generator-types).

## Text

Choose **Text** from the **Tool** button to write words as sound - you can
hear them and see them in the spectrogram. The **Text Tool** panel opens:

1. **Place a box** - drag on the canvas (the text stretches to fill it), or
   click to place a box the text's own size. A dashed outline shows it, with
   a live preview.
2. **Style it**:
   - **Text**, **Font**, **Size** (letter height in canvas rows), **Bold**,
     **Italic**;
   - **Left intensity** / **Right intensity** - how loud each channel is in
     the letters;
   - **Opacity** - scales the whole text;
   - **Blend mode** - how it combines with what's there;
   - **Rotation** - degrees, counterclockwise.
3. **Apply** writes it onto the selected layer. Only the letters are
   written; the space around them is left alone.

**Reset**, or switching tools, discards the box.

**Changing written text:** Pick it, and its settings appear in the Text Tool
panel. Any change redraws it in place; while you type, it redraws after a
short pause, so a typed word is one undo step. Clearing the text is ignored -
delete it with Pick. Choosing the Text tool again starts new text.

## Grids, Axis Labels and Snapping

**Configure → Grid** holds the canvas's reference aids. None of them change
the sound.

### Axis labels

- **Vertical axis (frequency)** - **Off** (default), **Hz**, **Notes**
  (`A4`, `C#5`...) or **Bin index** (the row number).
- **Horizontal axis (time)** - **Off** (default), **Seconds**,
  **Milliseconds** or **Frame index** (the column number).

Labels appear along the left and bottom edges, spaced so they never crowd.

### Frequency grid

Horizontal lines at chosen frequencies. Any combination can be on:

- **Note grid** - a line per note:
  - **Temperament** - 12-TET (default); 15, 17, 19, 22, 24, 31, 34, 41, 53 or
    72-TET; Quarter-Comma Meantone; Pythagorean.
  - **Key** / **Scale** - limit the lines to a scale (Major, the modes,
    Minor, Harmonic and Melodic Minor, Major and Minor Pentatonic, Blues,
    Whole Tone, Octatonic) in a key. Available for 12-TET, 24-TET, Meantone
    and Pythagorean.
  - **Notes...** - tick individual notes. **Octaves...** - tick which
    octaves show. Changing Temperament, Key or Scale resets the Notes choice.
- **Harmonic series** - a line at every multiple of a **Fundamental** (Hz).
- **Custom frequencies** - type your own list (`220, 440, 880`).

### Timing grid

Vertical lines, from **Mode**: **Off** (default), **Fixed Interval** (every so
many seconds) or **Tempo** (every whole, half, quarter, eighth, sixteenth or
thirty-second note at the project's tempo).

Each grid has its own **Line color**, **Line width** and **Line style**.

### Snap to Grid

With **Snap to Grid** ticked, dragging a path node, moving with Pick, or
drawing a selection lands on the nearest grid line - frequency on the nearest
frequency-grid line, time on the nearest timing-grid line. An axis with no
grid lines isn't snapped.

### Grid presets

**Grid Preset** saves the frequency grid, timing grid and snapping together
under a name; choose one to load it, **Delete** to remove it. Axis labels
aren't included. Presets also appear in the [Resource
Browser](#resource-browser).

## Undo and History

**Undo** (`Ctrl+Z`) and **Redo** (`Ctrl+Y`) cover everything you do to a
project:

- importing, recording, Loop takes and generated layers;
- adding, deleting, duplicating, renaming and reordering layers, and every
  layer setting;
- painting, filling, pasting, text, phase cleanup, and everything done with
  Pick;
- creating, editing and removing MindWaves, captured brushes, Resonance
  profiles, kernels, presets and MIDI mappings.

Zooming, scrolling, choosing tools, selections and device settings aren't
undone.

The **History** panel (the **History** toolbar button) lists every step, with
your current position highlighted. Double-click a step to jump to it.

The history is saved with the project, so Undo still works after reopening
it. Undoing back past a recording or Loop take may take a moment after
reopening, while the Studio rebuilds the earlier audio from the project's
sources.

---

# Part 4 - Listening and recording

## Playback

Open **Input/Output → Playback**. It has **Play**, **Pause** and **Stop**, a
position bar with the elapsed and total time, and a **Configure Devices**
button. Playback plays every visible, unmuted layer combined, over the
project's whole length.

While playing, a white line sweeps across the canvas. Drag the position bar
(playing or paused) to jump elsewhere.

Pressing Play prepares the sound in the background; on a large project the
status bar shows a **✕** to cancel. New Project, Open Project and closing the
Studio wait while it's being prepared.

The output device and volume are set in [Configure
Devices](#configure-devices). If nothing will be heard - the device is missing
or busy, the output gain is 0%, or the device runs at a different sample rate
than the project (which makes playback too fast or slow) - the status bar
says so when you press Play.

### Repeat and Scope

**Repeat** loops playback. **Scope** decides what playback does when you
edit:

- **Track** (default) - while Repeat is on and playback is running, your edit
  is prepared in the background and heard from the next time round, without
  interruption.
- **Delta** - each edit jumps playback to the edited region and plays just
  that region, even if nothing was playing - so you hear a new stroke
  without pressing Play. It starts a moment after the edit (longer for a big
  project); with Repeat on it loops the region, otherwise it plays it once.
- **Review** - like Delta, but plays on from the edit to the end of the track;
  with Repeat on it then loops back to the start of the track.

Unticking Repeat, or dragging the position bar, returns to playing the whole
track. Looping back is seamless.

## Recording

Open **Input/Output → Record**:

- **Start Recording** / **Stop Recording**.
- **Record into** - **New recording layer** (default), named from the
  **Name** box ("Recording"); or one of your earlier recorded layers, to
  record over it (Undo brings the old take back).
- **Live-encoding view** - shows the recording on the canvas as it comes in.
  It's drawn on top of everything; once you stop, the layer takes its place
  in the stack.
- **Configure Devices** - choose the input there.

While recording, the position line shows how far you've got. When you stop,
the take is converted to the project's sample rate and encoded in the
background ("Saving the recording..."), and the layer appears when it's done.
New Project, Open Project and closing the Studio wait for it, so a take is
never lost. The input device can't be changed while recording.

## Loop Mode

Open **Input/Output → Loop** to use the Studio as a loop station that plays
along with your project:

- **Start Loop** plays the whole project from the start and records your
  input into the **Loop Input** layer at the same time ("Preparing loop..."
  shows for a moment first).
- At the end of each pass, the take you just played replaces the previous
  one, and playback carries on from the start - you hear the new take at
  once, while the next one records.
- **Freeze Loop** stops recording and keeps the current take playing, pass
  after pass. Untick it to record again from the next pass.
- The Loop Input layer's **Gain**, **Balance**, hiding and muting apply to the
  take as you play. Edits to the rest of the project are heard from the next
  pass.
- **Live-encoding view** shows the take being recorded, drawn on top.

Loop Mode uses the input and output from [Configure
Devices](#configure-devices); a change applies the next time you start the
loop, and both are locked while it runs. Loop Mode expects the audio device
to run at the project's sample rate. New projects match it automatically;
for an older project at another rate, set the device to the project's rate
in your system's sound settings.

While a loop runs, filters and blend modes on layers above Loop Input don't
apply to the live take; ordinary playback includes them as usual.

## Configure Devices

**Configure Devices** (the button in the Playback, Record and Loop panels,
or on the start screen) is the one place audio devices are chosen.

- **Refresh Devices** looks for newly connected devices.
- **Input** - the device for Recording and Loop Mode. **Gain** (0-200%,
  100% unchanged) boosts or cuts it. **Test** shows a live level meter.
- **Output** - the device for Playback and Loop Mode, with its own **Gain**.
  **Test** plays a short tone.
- **Master limiter** (on by default) keeps the output at or below its ceiling
  (default -1 dBFS, settable from -24 to 0), turning loud moments down
  smoothly instead of distorting. It applies to Playback, to what you hear
  in Loop Mode (takes are recorded at full level), and to audio and video
  exports.
- **Normalize loudness** (off by default) plays every mix at the same
  loudness: when playback starts, the mix is measured and turned up or down
  towards the target (default -14 LUFS, the level many streaming services
  use), by at most 24 dB down or 12 dB up. It affects playback only, never
  exports.

If a device can't be opened, **Test** unticks itself and a message says why.
Settings are remembered between sessions. The input is locked while
recording or looping; the output only while looping.

## Macros

A **Macro** records a performance - the project playing and everything you do
to it meanwhile - and keeps it with the project.

- **Macro → Record Macro** starts recording a Macro ("Macro 1"...). Then
  start Playback or Loop and work: paint, change layers, filters and
  MindWaves, import, record, undo and redo. Choose **Record Macro** again to
  stop; the status bar shows its length and how many changes it holds.
- **The Macro's clock runs only while Playback or Loop does.** Pause, prepare
  your next change and resume: changes made while paused are recorded, in
  order, at the moment you paused. Seeking, Repeat's jumps and each Loop pass
  are recorded as jumps of the playhead.
- A dragged control is recorded at the value you let go at.
- **Macro → Play Macro** plays a Macro back on the canvas (you choose which).
  It's prepared in the background and starts after a moment; changes that
  took a while live happen at once. Your project isn't changed, and the
  canvas ignores clicks meanwhile. The action reads **Stop Macro**, and
  Playback's Pause, Play and Stop control it.
- **Macro → Export Macro Video...** renders a Macro to a video, with the same
  dialog as [Export Video](#exporting): the canvas changing as you changed
  it, the playhead where yours was, and the sound it played. It renders in the
  background from a copy.
- **Macro → Manage Macros...** lists your Macros, to **Rename...** or
  **Delete** them.

---

# Part 5 - Measuring the sound

## Analysis

**View → Analysis** opens meters that measure the project without changing
it.

At the top:

- **Analyze** measures loudness, stereo and pitch at once, in the background.
- **Auto-update** analyzes again whenever you change anything you can hear,
  about a third of a second after your last change. It also analyzes each
  project you open. It's off by default and remembered. On a long project
  with many layers, analysis can slow editing down - leave it off if so.
- The status line says whether the results are up to date ("Out of date"
  after a change, until you analyze again).

### Loudness

The whole mix, as you hear it, and every audible layer on its own:

- **Integrated** - the mix's loudness in LUFS, measured the way streaming
  services and broadcasters do (EBU R128). Many streaming services play
  music at about -14 LUFS.
- **Loudest 3 s** / **Loudest 400 ms** - the loudest short-term and
  momentary readings.
- **True peak** - the highest peak, including peaks between samples; red
  above 0 dBTP, where it will distort.
- **Sample peak** - the highest sample.
- **Clipped samples** - how many go beyond full scale; any at all are red.
- **At playhead** - momentary and short-term loudness at the playback
  position.
- **The graph** - momentary (thin, blue) and short-term (thick, gold) loudness
  over the project, from 0 to -60 LUFS.
- **The band view** - each frequency band's level over time (sub, bass,
  low-mid, mid, high-mid, air; brighter is louder).
- **The layer table** - each layer's integrated loudness, loudest 3 s, true
  peak and clipped samples.
- **Show loudness on canvas** - draws short-term (solid yellow) and
  momentary (thin) loudness over the canvas, from -60 LUFS at the bottom to
  0 at the top. The line breaks where the mix is silent.

### Stereo

- **Correlation** - how alike the channels are: **+1** mono, **0**
  unrelated, **below 0** (red) means parts cancel out when played in mono, on
  a phone speaker say.
- **Width** - **0** mono, about **0.5** wide, **1** fully out of phase.
- **At playhead** - both at the playback position.
- **The graph** - correlation (blue, -1 to +1) and width (gold, 0 to 1) over
  time.
- **The goniometer** - the samples around the playback position (or the whole
  mix when stopped): mono is a vertical line, wide stereo a cloud,
  out-of-phase sound leans sideways.
- **The width bars** - each frequency band's width. Bass is usually kept
  narrow.
- **Show correlation and width on canvas** - draws both over the canvas, with
  a legend in the top-left corner.

### Pitch

Follows the pitch of the mix from 50 Hz to 2 kHz - by default the pitch of
whatever is loudest.

- **At playhead** - the note, how far off in cents, and the frequency, e.g.
  "A4 +3 cents (440.8 Hz)"; "—" where there's no clear pitch.
- **At mouse** - the same, at the point under the mouse on the canvas or the
  pitch graph.
- **Voices** - how many pitches to follow at once, 1 to 6 (2 for a melody
  over a bassline, say). Voice 1 is always the highest, so the lines never
  cross; each voice keeps its own register, so one that pauses picks up where
  it was. With more than one voice, a checkbox per voice hides or shows it.
  A voice an octave from a louder one is usually lost, and two very close
  notes (within about 25 Hz) can make the quieter one drop out.
- **Ignore spikes shorter than** - removes brief jumps such as momentary
  octave errors: a jump shorter than this many milliseconds is smoothed over
  if the pitch either side agrees, and erased otherwise. Glides and vibrato
  are kept. `0` (default) shows everything; try 30-60 ms.
- **The graph** - pitch over time on a musical scale, a line per voice.
- **Show pitch on canvas** - draws the pitch lines over the canvas, so you can
  see which painted line is heard as the melody.

Voices and the spike setting are remembered between sessions.

---

# Part 6 - Files

## Saving and Project Files

**Ctrl+S** (**File → Save Project**) saves; **File → Save Project As...**
saves a copy elsewhere. Closing, opening another project or starting a new one
with unsaved changes asks you to save first.

A project is a `.smproj` file plus a folder of the same name beside it. Keep
them together - moving one without the other breaks the project. The folder
holds:

- `media/` - every layer's content;
- `sources/` - a copy of everything you imported, every recording and Loop
  take (as FLAC), and every layer brought in from another project, so the
  project never depends on files elsewhere. It can grow large, since every
  Loop take is kept.

### Opening projects

Besides **File → Open Project...**, the start screen and drag and drop, you
can open a project by starting `sound-mind-studio.exe` with the `.smproj`
file - for example by dropping the file onto the program or a shortcut to
it. The Microsoft Store version, once published, also opens a project when
you double-click it. If a project can't be opened, a message says why.

## Exporting

All exports run in the background - keep working, or click **✕** in the
status bar to cancel (nothing is left behind). One export runs at a time.

### Export Audio

**File → Export Audio...** saves the mix - every visible, unmuted layer, with
filters, exactly as Playback plays it - as FLAC, Ogg Vorbis or MP3 (choose the
file type in the save dialog).

### Export Video

**File → Export Video...** saves the spectrogram as a video with the mix as
its sound. The dialog remembers your choices:

- **Output file** - with **Browse...**; the extension follows the format.
- **Format** - **H.264 (MP4)** (recommended: small, plays everywhere), **HEVC
  (MP4)** (smaller; only where Windows provides it), **ProRes (MOV)** (very
  large, for further editing) or **MPEG-4 (MP4)** (plays everywhere, larger).
  Only formats that work on your computer are listed.
- **Resolution** - 1080p, 4K, 1440p, 720p, vertical 1080×1920 or 720×1280
  (TikTok, Reels, Shorts), square 1080×1080, the canvas's own size, or a
  custom size. The canvas keeps its proportions, with black bars where the
  shapes differ.
- **Frame rate** - 24, 30 (default) or 60.
- **Quality** - Low, Medium (default) or High.
- **Animation** - **Whole canvas** (the whole canvas, with the playhead
  sweeping across it) or **Scrolling** (a window of a chosen number of
  seconds pans across the canvas). **Show playhead** turns the line on or
  off.
- In [Sound Flower](#sound-flower-polar-view) view the video is the flower:
  with Whole canvas a ray turns from twelve o'clock; with Scrolling the
  flower turns under a fixed ray. A scrolling flower takes longer to make.
- **Open the video when the export finishes.**

The status bar shows the progress as a percentage.

### Export Image

**File → Export Image...** saves a copy of the canvas as a picture:

- **File** - with **Browse...**; the extension and the **Format** follow each
  other: **PNG**, **JPEG**, **BMP**, **TGA** or **WebP**.
- **Full resolution** (default) - one pixel per canvas column and row, shown
  as width × height. Or **Custom size** in pixels; with **Keep proportions**
  ticked, changing one side changes the other.
- **As Sound Flower** - saves the polar view as a square image (ticked when
  the canvas shows it). Its full resolution is a square twice the canvas's
  height.

The image shows every visible layer, filters applied, as the canvas does -
muted layers included, since they're still seen.

### Limiting and loudness

Audio and video exports go through the [master limiter](#configure-devices)
if it's on, so peaks over full scale are turned down instead of clipping.
Loudness normalization is never applied to exports.

## Remastering

**File → Remaster...** rebuilds the open project as a **new** project, by
replaying everything you did from its sources. The open project isn't
changed. In the dialog:

- **Sample Rate, Timestep, Bin Count, Min/Max Frequency** start at the
  project's own; change them to rebuild at a different (or higher)
  resolution. The duration stays the same, and strokes, fills, pastes and
  text land at the same times and frequencies.
- **Sources** lists every file, recording and Loop take the project was made
  from. **Replace...** swaps one for another file; everything made from it is
  rebuilt from the new one.
- **Save as** - where to save the new project.

**Remaster** without changes rebuilds the project as it is - the way to
recover layer content whose files went missing (the Studio tells you when
that's needed). When it's done, the Studio offers to open the new project.

A Mind Shot is rescaled when the frequency range or row count changes, which
only approximates its original pitch.

## Resource Browser

**Resource Browser** (in the menu bar) collects the reusable things a project
builds up: MindWaves, Tool Presets, Mind Shots, Mind Grains, Resonance
Profiles, Filter Presets, Convolution Kernels, Grid Presets and Layers. Choose
a category at the top to list them; selecting one shows its settings, and a
preview where that makes sense:

- **MindWave** - its shape, and **Play** to hear an instrument with it as
  vibrato.
- **Tool Preset** - a sample stroke painted with it, and **Play** to hear it.
- **Mind Shot** - its picture, and **Play**.
- **Resonance Profile** - its spectrum and the curve it came from, and
  **Play**.

**Sharing:**

- **Export...** saves the selected item as a file you can give to someone or
  use in another project: `.smwave` (MindWave), `.sminst` (Tool Preset),
  `.smshot` (Mind Shot), `.smresonance` (Resonance Profile), `.smfilter`
  (Filter Preset) or `.smkernel` (Convolution Kernel).
- **Import from File...** adds such a file to this project. If the name is
  taken, choose **Overwrite**, **Keep Existing** or **Keep Both** (the import
  is renamed "Name (2)").
- **Browse Other Project...** shows another project's items, read-only,
  without closing yours. Select anything - including a whole Layer - and click
  **Import** to copy it in. **Return to This Project** switches back.

**Toolkits** bundle several items into one `.smtoolkit` file. Select an item
and click **Add to Toolkit**; anything it needs (a MindWave a preset is bound
to, say) is added too, and a message says so. **Remove** takes an item out;
**Export Toolkit...** names and saves the bundle. **Import Toolkit...** adds
everything in a toolkit at once - all of it, or nothing if the file is
damaged - with the same Overwrite / Keep Existing / Keep Both choice for
taken names.

Grid Presets are saved and loaded in the [Grid panel](#grid-presets), and
Mind Grains and Layers have no file of their own; the browser lists them for
reference and for **Browse Other Project...**.

---

# Part 7 - Reference

## Performance

Most editing stays responsive on an ordinary computer. This chapter explains
what costs more, for when something feels slow.

- **The canvas never waits for an edit.** The new picture is made in the
  background; until it's ready (usually a split second, longer under heavy
  filters or with many layers) you see the previous one plus your new stroke.
- **Editing one layer is fast however many layers there are** - only that
  layer and those above it are recombined. Adding, removing, reordering or
  muting layers recombines everything, so those cost more with very many
  layers.
- **Painting** a new stroke costs the same however many strokes a layer
  already has. Undo and Redo repaint the layer's whole history, so they cost
  more on heavily painted layers.
- **Changing a MindWave** redraws only what uses it.
- **Hardware Acceleration** (**View** menu, on by default) runs the heavy
  work on the graphics card: blending layers, Uniform and Edge-Preserving
  Blur, Fractal and Noise MindWaves. It helps most with big work (a large
  canvas, a strong blur, many layers). Turn it off to compare with the
  processor, or if you suspect it of a display problem; it's remembered.
- **Every processor core** is used when turning the canvas into sound
  (playback, Repeat and Loop updates, filter previews, exports), turning
  sound into a canvas (importing, recording), analyzing, and making noise
  layers. The results are the same however many cores you have.
- **Blurs:** a larger Uniform Blur **Sigma** costs more, but the graphics
  card copes well. A larger Edge-Preserving Blur **Size** costs more on both,
  so if it feels slow, a smaller Size helps more than acceleration.
- **Fractal MindWaves** with Warp, Reversed or Inverted are worked out on the
  processor instead of the graphics card.
- **Heal** and **Soften** stay fast at any brush size.
- **Mind Grain** strokes are refreshed when their source layer changes.

## Keyboard Shortcuts

| Action | Shortcut |
|---|---|
| New Project / Open / Save | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` |
| Cut / Copy / Paste | `Ctrl+X` / `Ctrl+C` / `Ctrl+V` |
| Delete (picked object, path node) | `Delete` |
| Deselect | `Ctrl+D` |
| Bring to Front / Send to Back | `Ctrl+Shift+Up` / `Ctrl+Shift+Down` |
| Bring Forward / Send Backward | `Ctrl+Up` / `Ctrl+Down` |
| Select layer above / below | `Page Up` / `Page Down` |
| Move layer up / down | `Shift+Page Up` / `Shift+Page Down` |
| Raise / lower layer gain by 5% | `Ctrl+Page Up` / `Ctrl+Page Down` |
| Zoom in / out | `]` / `[` |
| Zoom frequency only | `Alt+]` / `Alt+[` |
| Zoom time only | `Shift+]` / `Shift+[` |
| Zoom coarse | `Ctrl+]` / `Ctrl+[` |
| Fit to Window / Actual Size | `Ctrl+0` / `Ctrl+1` |
| Sound Flower | `Ctrl+Alt+F` |

The mouse wheel zooms with the same modifiers as `]` and `[`.

## What's Not Here Yet

The [design document](docs/sound-mind-design.md) describes everything the
Studio is meant to become, and `docs/sound-mind-roadmap.md` the order it's
built in. This chapter lists what isn't there today.

### Planned

After the Beta readiness check:

- **A light mode** - the Studio is dark only for now.
- **A colour mapping choice** - which of amplitude and phase show as red,
  green and blue is fixed today.
- **Composer Mode editing** - **View → Composer Mode** shows each visible
  layer as a track with every stroke and fill as a box, but it's read-only:
  you can't yet drag a box to retime it, move it to another track, or change
  gain or EQ there.
- **The rest of Sound Mind Instruments** - an Instrument has harmonics,
  inharmonicity and vibrato/tremolo, but no noise component, body resonance
  or ADSR envelope yet. Also planned: a **Harmonic** stamp mode, MindWave
  binding for every instrument setting, and Loop Mode retriggering per note.
- **More analysis** - vibrato, formants, key and scale detection, analyzing
  only a selection, and the design document's spectral-health and
  criticality meters.
- **Editing refinements:**
  - adding or deleting a path node by double-clicking, and breaking a
    node's handles apart with Alt+drag;
  - the Alt/Shift axis restriction applies only to zooming so far, not to
    dragging with Pick, a selection's handles, or layer timing;
  - a freehand stroke's pitch snapping to the Frequency Grid as you draw
    (Snap to Grid covers nodes, moves and selections);
  - pasting where you click (Paste lands where the copy came from; Pick then
    moves it).
- **Performance hints** - speed badges on filters, blend modes, brushes and
  MindWave types, from your own computer's benchmarks.
- **Live MIDI input** - playing a connected MIDI controller. Today notes come
  from MIDI file import.

### Not planned

- **Clone** and the **Tool Configuration Wizard** are deferred; the Tool
  Configuration panel and its presets cover configuring a brush.
- **Combining several MindWaves into one** (Superposition) was removed in
  version 0.1.32.1, and may come back in another form.
- **Pooling a layer** was removed in version 0.1.46.1, with the Pool codec:
  it couldn't make a layer sound better. Older projects still open; their
  `pool/` folder is ignored.
- **Warp Selection** was removed: it didn't survive undo, redo or reopening a
  project, and was confusing.
- **The Chord Generator and Chord tool** were removed in version 0.1.50.1.
  Chords already stamped stay in your projects, and MIDI import brings in
  notes.
- **WebM/VP9 video** would need another encoder library; HEVC is offered only
  where Windows provides it.
- **The Sound Mind VST plugins**, and hosting other plugins, come after
  version 1.0.

### Limits of what's here

- **Macros** have no timeline to inspect or edit what was recorded; strokes
  appear whole rather than being drawn in; and the video doesn't follow your
  zoom or scrolling.
- **Sound Flower** hides grids, axis labels, bounding boxes and path
  geometry, the analysis overlays, and rectangular outlines.
- **Loop Mode** doesn't apply filters and blend modes above Loop Input to the
  live take, and expects the device to run at the project's sample rate.
- **Brushes:** Smudge smears each stamp on its own rather than carrying paint
  along the stroke; Order/Chaos rearranges content rather than measuring
  entropy; Resonance has one decay rate and no vibrato, tremolo or
  inharmonicity.
- **Mind Shots and Mind Grains** capture a Lasso, Wand or combined
  selection's whole bounding box. A Mind Grain's region can't be moved after
  capture, and it reads its source layer alone, not the mix at that point.
- **Selections:** a Wand or combined selection is outlined as a dashed box,
  though Fill, Copy, Cut and Paste follow its exact shape.
- **Filters:** saved Convolve kernels can't be renamed or deleted in the
  Studio.
- **MindWaves:** a layer's Blend Mode can't follow a MindWave; Vibrato and
  Tremolo always follow the stroke (no Canvas Space choice); Step Grid values
  are a plain list; Continuous's formula is a first attempt.
- **Custom Frequencies** in the Grid panel are typed only, with no ready-made
  sets.
- **Projects made before version 0.1.43.1** can't be remastered, and Undo
  stops at steps it can't rebuild in them.

## Licence and Credits

Sound Mind Studio is copyright © 2026 Andreas Lundberg. You may use it free of
charge for anything, including commercial work; everything you make with it
is yours. You may not redistribute or modify it. The full terms are in
`LICENSE.md`, next to the program.

It includes open-source software by others - Qt, FFmpeg, LAME, JUCE (with
FLAC and Ogg Vorbis), PocketFFT, Eigen and more - each under its own licence.
`THIRD_PARTY_NOTICES.md` and the `licenses` folder next to the program list
them with their licence texts. Qt and FFmpeg are separate DLLs you may replace
with your own builds; their source code is published on the project's GitHub
releases page.

**Help → About Sound Mind Studio** shows the version and copyright, and its
**Licence...** and **Open-source notices...** buttons open both documents.

Sound Mind Studio collects nothing and contacts no server - it works entirely
offline.
