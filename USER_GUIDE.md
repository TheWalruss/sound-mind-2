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
12. [Chord Generator](#chord-generator)
13. [Axis Labels](#axis-labels)
14. [Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid)
15. [Playback](#playback)
16. [Recording](#recording)
17. [Loop Mode](#loop-mode)
18. [Configure Devices](#configure-devices)
19. [Pooling a Layer](#pooling-a-layer)
20. [Exporting](#exporting)
21. [Saving and Project Files](#saving-and-project-files)
22. [What's Not Here Yet](#whats-not-here-yet)

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

Launching the Studio shows the **start screen** with two buttons and a
list:

- **New Project...** - opens the [Create Project wizard](#creating-a-project).
- **Open Project...** - browse to an existing `.smproj` file.
- **Recent Projects** - your most recently opened projects, click one to
  reopen it directly.
- **Documentation** - Quick Start, Readme, User Guide, Changelog, and
  About - the same five links the Help menu offers once a project is
  open, available here too.

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
- **Layers panel** (right, by default) - lists every layer in the
  project, top of the stack first. Shown by default; toggled from the
  **Layers** toolbar button.
- **Playback / Record / Loop panels** - dockable panels, each toggled
  from its own toolbar button; each is covered in its own section below.
  Unlike Layers, these three start hidden - click the matching toolbar
  button to show one. Whichever panels you have open (or closed) stays
  that way across New/Open Project, within the same run of the Studio.
- **File menu** - New/Open/Save/Save As, Import Audio/Image, Export
  Audio/Video.
- **View menu** - Zoom controls (see [Canvas Navigation](#canvas-navigation)
  below) and a **Hardware Acceleration** checkbox: turn GPU compute off to
  compare speed/results against the CPU path, or if you suspect it's
  causing a problem. Stays as you left it across restarts.
- **Help menu** - Quick Start, Readme, User Guide, and Changelog (each
  opens a styled HTML page in your default browser), plus About Sound
  Mind Studio.
- **Pool Layer** toolbar button - see [Pooling a Layer](#pooling-a-layer).
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

## Importing Media

### Importing Audio

**File → Import Audio...** prompts for an audio file - WAV, MP3, FLAC, Ogg,
AIFF, M4A, or Opus. If it's no longer than your project's Duration, it's
imported directly as one new layer. If it's longer, it's split into
consecutive Duration-length snippets and you're shown a list to
check/uncheck - only the checked snippets become layers, named
`<filename>_0000`, `<filename>_0001`, and so on.

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

### Drag and Drop

Dragging files onto the main window imports/opens them, by extension, with
the same choices the File menu offers: dropped images show the same
scaling/sequencing picker **File → Import Image...** does; a dropped audio
file (WAV, MP3, FLAC, Ogg, AIFF, M4A, or Opus) with more than one computed
snippet shows the same snippet picker **File → Import Audio...** does, one
picker per such file; `.smproj` opens
that project (after confirming if your current project has unsaved
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
Filter/Background/Equalizer). An unselected row shows only that preview
and a **visibility toggle** (●/○); **click a row's name to select it**
(see [Painting](#painting) below - the selected layer is the one a brush
stroke paints into) to reveal its full set of controls:

- A **drag handle** (⠿) to reorder it, or a **lock icon** (🔒) if it can't
  be reordered or deleted - the **Background** layer (always present,
  bottom of the stack) and the **Equalizer** layer (always present, top
  of the stack - see [Filter Layers](#filter-layers) below) are both
  locked, and show their lock icon whether selected or not.
- A **type tag**, for any layer type other than the ordinary kind you get
  from importing.
- An **opacity slider**, an **opacity-MindWave combo** (see
  [MindWaves](#mindwaves) below), **translation**/**rescale** spin
  boxes (see [Layer Timing](#layer-timing) below), and a **Blend Mode**
  combo (see [Compositing](#compositing) below) - except on the
  **Background** layer's row, which has none of these: it's always fully
  opaque and always first in time, so none of them apply to it.
- A **delete** button (×), for any layer except the locked one(s).

Double-clicking a row's name renames it, whether the row is currently
selected or not. A layer whose opacity is bound to a MindWave shows a
smaller, indented row directly beneath it, with that MindWave's own
grayscale preview.

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

### Important: what's actually shown and played right now

The canvas and Playback now show/play a real composite of every visible
layer, mixed together (see [Compositing](#compositing) above) - opacity
included, so two overlapping layers genuinely blend rather than one
hiding the other. **A few other operations still work on a single
layer, not the composite**: **Recording's result, Loop Mode, Pooling,
and Audio/Video Export** all still use the **topmost layer that's both
visible and has content**, skipping hidden ones - each of these is
about capturing, processing, or exporting one specific layer's own data,
not "everything currently audible," so extending them to the composite
is separate, still-open work. This means:

- The canvas and Playback reflect every visible layer's own opacity;
  Recording/Loop Mode/Pooling/Export don't read it yet for the single
  layer they act on.
- To compare two layers on the canvas, toggle visibility - both stay
  visible together if you leave them both on, now genuinely blended, not
  one replacing the other.
- For Pooling/Export specifically, importing a new layer or reordering
  one to the top still changes which single layer gets acted on.

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
you'll hear lined up as well. Recording's result, Loop Mode, Pooling,
and Export are the exception - see the note above.

### MindWaves

The **MindWaves** panel (toggle it from the toolbar) manages a small
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
  rename it; the **×** button deletes it.
- Every row shows a small grayscale mini-preview of its own field,
  always on - regardless of selection, and independent of the Preview
  toggle below (which does something different - see its own entry). The
  same small thumbnail the Layers panel already shows for a layer with a
  MindWave-bound opacity.
- **Preview** shows the currently selected MindWave's own field directly
  on the canvas, as a live grayscale overlay (black where it's 0, white
  where it's 1) - updating instantly as you change its generator type,
  parameters, or superposition stack, and as you select a different
  MindWave while Preview stays on. A judgment aid only - it never affects
  the actual composite, and switches off automatically when you open or
  create a different project, or when you close this panel.
- Selecting a MindWave shows its own editor: a **Generator Type** (Periodic,
  Envelope, Stepped/Noise, Spatial, Fractal, Drawn, Step Grid, or
  Continuous) and that type's own plain numeric parameters - a period, a
  phase, a seed, and so on. This is a bare-bones, functional editor, not a
  polished one yet - there are no dials or drawing tools here, just fields
  to type numbers into.
- **Drawn** shapes come from a curve you draw yourself, rather than a
  formula: draw an ordinary stroke anywhere on the canvas, switch to Pick
  and select it, then choose **Edit → Use Picked Path as MindWave Shape**
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
  formula or a drawn curve: a **Steps** spin box sets how many, and each
  one gets its own plain value box (`0`-`1`) below it - a fresh Step Grid
  starts with four steps rising from `0.25` to `1.0`. Loops using the same
  **Period** control every other generator type has, one equal-length
  segment per step. There's no visual grid to click on yet - just the
  numbered list of value boxes.
- **Continuous** offers three plain dials instead of picking a generator
  type by hand: **Shape** sweeps from a clean sine cycle (`0`) toward
  fractal noise (`1`); **Skew** biases the cycle earlier or later (`0.5`
  is no bias); **Character** layers fine turbulence on top, independent of
  Shape - it does something even at Shape `0`. This is a best-effort,
  provisional take on a friendlier way into MindWaves, not a finished
  design - it may change in a future update. All three dials are saved
  with your project, so reopening one shows them exactly where you left
  them.
- **Superposition** lets one MindWave combine several others together
  (multiply, add, min, max, or average) - **+ Add Member** starts the
  first one. The rest of Superposition's own controls (**- Remove
  Member**, the Blend Mode drop-down, and the small member list/editor)
  stay hidden until at least one member exists, then appear for managing
  them, each editable the same way as a top-level MindWave.
- **Warp** lets one MindWave distort the position another is sampled at,
  giving a wavier, less mechanically regular result than either shape
  alone - check **Enable Warp** to reveal a **Strength** spin box and a
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

The Filter Configuration panel is available even before you've added a
Filter layer at all - dial in the settings you want first, then click
**+ Add Filter Layer**, and the new layer lands already configured that
way rather than starting from scratch. Whatever you last set stays in
place for the *next* Filter layer too, so adding several in a row can
start each one from the same settings.

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
  **Amount**) has its own MindWave combo right beside its spin box - the
  same **None**-or-pick-a-MindWave choice as a layer's own opacity
  binding (see [MindWaves](#mindwaves) above), but with a genuinely
  different effect here: binding one of these varies *that filter's own
  parameter* across the canvas, so the filter itself computes a different
  blur/sharpen at every cell instead of applying one fixed strength
  everywhere. The spin box still sets that parameter's ceiling value
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
  - **Save As New Kernel** stores the current grid permanently, named
    "Kernel 1", "Kernel 2", and so on, for reuse later - on this Filter
    layer, a different one, or a different project entirely. **Load
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
loudness.

## Painting

Click the **Paint** toolbar button to switch the canvas into paint mode;
click it again (or switch tools) to leave it. While it's on, dragging on
the canvas draws a stroke - you'll see it live as a black-outlined white
line while drawing (visible over any painted color underneath, including
a matching one), and it's applied to the spectrogram once you release
the mouse.

**Which layer gets painted**: whichever layer's name you last clicked in
the Layers panel (see [Working with Layers](#working-with-layers)). If
you haven't clicked one yet, painting targets the **Background** layer -
it's a real, paintable canvas like any other, not just a fixed floor to
import onto (never the locked Equalizer layer at the top, which has
nothing to paint onto in the first place).

The **Tool Configuration** toolbar button opens a dockable panel (off by
default, alongside Layers/Playback/Record/Loop) with the brush's own
settings:

- **Tool Type** - **Procedural** (the default), **Instrument**, **Mind
  Shot**, **Mind Grain**, **Heal**, **Soften**, **Smudge**, or
  **Order/Chaos**; picks which set of controls below applies (Tip Shape for
  Procedural, Harmonics/Inharmonicity for Instrument, a picker for Mind
  Shot/Mind Grain, an Amount slider for Order/Chaos - Heal/Soften/Smudge add
  no controls of their own at all, see their own entries below). The panel
  also hides whichever of the shared controls below (Falloff, Brush Size,
  Stamp Mode, Stamp Interval, Color, Opacity, Blend Mode) the selected type
  doesn't actually use - see each control's own entry for which types hide
  it.
  Switching still keeps every shared value as it was underneath, even a
  currently-hidden one - only the type-specific controls reset to the
  newly-picked type's own defaults.
- **Tip Shape** (Procedural only) - the stroke's cross-section (Circle,
  Square, Diamond, and others - only Circle/Square/Diamond have a distinct
  shape so far; the rest currently paint the same as Circle).
- **Harmonics** (Instrument only) - how many overtones above the
  fundamental to synthesize, each with its own **strength** spin box
  (fundamental first) - a fresh Instrument starts with a plausible
  falling four-harmonic series (`1.0, 0.5, 0.25, 0.125`). Each harmonic
  paints as a single exact-frequency spike rather than a soft geometric
  blob - a stroke's own Falloff/Size still soften and bound it, but only
  along the *time* axis (how the stroke fades in/out as you paint it),
  not across frequency.
- **Inharmonicity** (Instrument only) - stretches the harmonic series
  sharp of a pure integer series, the way a real vibrating body's own
  overtones do (a piano string, for instance). `0` (the default) is
  perfectly harmonic; small positive values (try `0.01`-`0.05`) give
  higher harmonics an audibly metallic, bell-like stretch.
- **Vibrato**/**Tremolo** (Instrument only) - each a depth spin box paired
  with a MindWave bind combo. Binding a MindWave makes it modulate pitch
  (Vibrato, in semitones) or strength (Tremolo, as a fraction toward
  silence) across the course of a stroke, following that MindWave's own
  shape - a longer stroke shows more of the shape, a short one only a
  sliver of it. Set back to **None** for no modulation at all, regardless
  of the depth value. This tracks the stroke's own position along itself,
  not real elapsed time, so the same MindWave shape always plays out fully
  over any one stroke, however long or short it is.
- **Mind Shot** (Mind Shot only) - a drop-down of every Mind Shot you've
  captured so far (see [Selection and Fill](#selection-and-fill)'s own
  "Capture as Mind Shot"), empty until you capture your first one.
  Painting with it stamps the captured content back at its own original
  size, not scaled by Falloff/Size the way Procedural/Instrument are -
  combined with what's already there per its own **Blend Mode** control
  (Overwrite by default, reproducing the original "stamps back exactly
  as it was captured" behavior). The panel hides Falloff, Brush Size,
  Color, and Opacity for Mind Shot, since none of them have any effect
  on it - only Stamp Mode/Interval (which control the stamp's own
  placement, not its content) still apply.
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
  every disallowed layer with a red **✕**, and the Paint toolbar button
  itself is disabled (its own tooltip explains why) - switch to a layer
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
  resolution). The panel's own default (`0.2`) is a comfortably visible
  stroke on a typical project without covering too much of it at once.
  Hidden for Mind Shot/Mind Grain, which don't use it.
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

  Choosing any mode but Stroke enables the **Interval** spin box next
  to it, which sets the spacing (seconds for Along Curve/Time Axis, Hz for
  Frequency Axis) - it's disabled and ignored under Stroke, where an
  interval wouldn't mean anything. This is the same idea as the legacy
  Studio's "Curve" tool, minus its pixel-based unit (this build has no
  canvas zoom yet, so seconds/Hz are the only units that make sense). For
  Heal/Soften/Smudge/Order-Chaos, Interval is likewise hidden - it's always
  66% of Brush Size, so it stays proportional as Brush Size changes.
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
  gradient varies across a selection. The panel's own default (both
  endpoint stops at `0` dB, full opacity) paints loud on both channels
  equally at full strength right away, no setup required. Hidden for
  Mind Shot and Mind Grain, which don't paint toward a fixed target at
  all; for Heal, Soften, Smudge, and Order/Chaos, only the Intensity
  fields hide - Opacity stays, as the blend's own strength (see each
  tool's own entry below).
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
paint strokes and to a layer's own opacity, opacity-MindWave binding,
visibility, translation, rescale, and blend mode (see
[Working with Layers](#working-with-layers)) - both kinds share the same
single history, undoing/redoing whichever one actually happened most
recently, in either order.

Only **Procedural**, **Instrument** (harmonic series + inharmonicity only so
far - no noise, body resonance, or envelope yet), **Mind Shot**
(capture-and-restamp), **Mind Grain** (a live reference that updates
immediately, everywhere it's used, the moment its source is repainted),
**Heal** (temporal blur), **Soften** (radial blur), **Smudge** (directional
smear), and **Order/Chaos** (spectral order/chaos push) exist today - see
[What's Not Here Yet](#whats-not-here-yet) for the rest of what's planned
around painting.

## Pick

Click the **Pick** toolbar button (next to Paint) to switch the canvas
into pick mode; click it again (or Paint) to leave it. While it's on,
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
- **Restack** it within its own layer - **Edit → Bring to Front**
  (Ctrl+Shift+Up), **Send to Back** (Ctrl+Shift+Down), **Bring Forward**
  (Ctrl+Up), or **Send Backward** (Ctrl+Down). This only changes which
  object renders on top where they overlap - it doesn't move, modify, or
  select anything else.
- **Edit its own path** - **Edit → Edit Path** (a brush stroke only;
  a filled selection or a pasted region has no path to edit). Brings its
  nodes and handles back onto the canvas as small dots:
  - **Click a node** to select it (shown larger, in white).
  - **Drag a node** to move it - its own handles (if any) move with it.
  - **Click, then drag, one of the selected node's own handles** (shown
    in cyan, connected to the node by a thin line - only the *selected*
    node's own handles are ever shown) to reshape the curve on that
    side. The opposite handle always mirrors it, to keep the curve
    smooth through the node.
  - **Edit → Toggle Node Type** switches the selected node between a
    smooth curve point and a sharp corner. Converting to smooth extends
    its own two handles out a comfortable distance, rounding out the
    corner right away - drag either handle further from there to reshape
    it more.
  - **Delete key** removes the selected node (refused if it's the only
    node left).
  - **Edit → Apply Path Edit** commits everything changed so far as a
    new, undoable edit to the stroke. **Edit → Cancel Path Edit**
    discards it instead, leaving the stroke exactly as it was.

Clicking empty canvas space deselects. If an object is entirely covered
by another one on top of it, click the already-selected (covering)
object again to select the one underneath - each further click on the
same spot cycles to the next one down, wrapping back to the topmost once
you reach the bottom.

Move/Modify/Delete/a completed path edit are all undoable (Ctrl+Z), the
same as painting a new stroke. Restacking currently isn't - if you bring
something to front and change your mind, restack it back rather than
pressing Ctrl+Z.

Copying a picked object isn't here yet, and path editing doesn't yet
support inserting/deleting a node by double-clicking, or detaching a
handle from its own mirrored pair - see
[What's Not Here Yet](#whats-not-here-yet).

## Selection and Fill

Click the **Select** toolbar button (next to Paint/Pick) to switch the
canvas into select mode; click it again (or Paint/Pick) to leave it.
While it's on, drag (or, for Wand, click) on the canvas to select a
region - shown as a green outline, dashed for a Wand or combined
selection (see below). A selection stays active (still scoping Fill/
Copy/Paste, and usable again the moment you switch back to Select) even
after switching to a different tool - it isn't tied to Select mode
itself, only to having drawn one - but its green outline only shows
while Select or Pick is the active tool, not while Paint or another tool
is.

The **Selection Configuration** toolbar button opens a dockable panel
(off by default, alongside the others):

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
- **Paste Blend Mode** - how **Edit → Paste** (below) combines its clip
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

- **Fill** it - **Edit → Fill Selection...** opens the same draggable
  gradient editor Painting's brush and Filter Configuration's
  Frequency-Axis Gradient use (see [Painting](#painting)/[Filter
  Layers](#filter-layers) above), seeded with a fully-opaque default;
  accepting fills the selection, confined exactly to its own boundary. A
  gradient with more than two stops varies continuously left-to-right
  across the selection, not just a single flat color.
- **Copy** it - **Edit → Copy** (Ctrl+C) captures the selection's own
  pixels onto the clipboard, leaving them in place.
- **Cut** it - **Edit → Cut** (Ctrl+X) does the same as Copy, then
  silences the selection's own region in place.
- **Paste** the clipboard - **Edit → Paste** (Ctrl+V) writes it back at
  the same position it was captured from, onto whichever layer is
  currently active in the Layers panel - which doesn't have to be the
  layer it was copied or cut from. Select a different layer's row first
  to paste onto it instead. Combines with the destination per the
  Selection Configuration panel's own **Paste Blend Mode** (above).
  Switches straight to [Pick](#pick) and selects the newly pasted result
  there - move, modify, delete, or restack it right away, with no
  separate click needed to find it again.
- **Capture as Mind Shot** it - **Edit → Capture as Mind Shot** stores the
  selection's own pixels permanently, named "Mind Shot 1", "Mind Shot 2",
  and so on - unlike Copy, this doesn't touch the clipboard, and the
  source pixels are left exactly as they were (no silencing, unlike Cut).
  Once captured, pick **Mind Shot** as the Tool Type in
  [Painting](#painting)'s Tool Configuration panel and select it from the
  drop-down there to paint with it - it stamps back exactly as captured,
  wherever you paint, on any layer. Always captures the selection's own
  full bounding box, even for a Lasso/Wand/combined selection - not yet
  confined to its actual shape.
- **Capture as Mind Grain** it - **Edit → Capture as Mind Grain** stores a
  *reference* to the selection's own layer and region, named "Mind Grain 1",
  "Mind Grain 2", and so on - unlike Capture as Mind Shot, no pixels are
  captured at all, and neither the clipboard nor the source layer's content
  is touched. Once captured, pick **Mind Grain** as the Tool Type and
  select it from the drop-down there to paint with it - see
  [Painting](#painting)'s own Mind Grain entry for the "only paintable
  above its own source layer" rule and the guardrails that enforce it.
  Also always references the selection's own full bounding box, same as
  Mind Shot above.
- **Deselect** it - **Edit → Deselect** (Ctrl+D), or drag-clicking without
  actually dragging (a plain click) on the canvas while in Select mode.

Filling and pasting are both undoable (Ctrl+Z), the same as painting a
stroke; so is Cut's own silencing of the source region.

Rectangle, Lasso, and Wand selections all exist today, can be combined
with each other, and a Rectangle selection can be rotated.

## Path Tool

Click the **Path** toolbar button (next to Select) to switch the canvas
into Path mode; click it again (or Paint/Pick/Select) to leave it. While
it's on, each click on the canvas places one more node, building a path
one node at a time - a live line always shows the path built so far, plus
a segment out to wherever the cursor currently is.

- **Finish** it - **Edit → Finish Path** commits the path as a new,
  undoable paint object (using the current Painting brush's own color/
  opacity settings), exactly like a freehand stroke.
- **Cancel** it - **Edit → Cancel Path** discards everything placed so
  far instead.
- **Smooth Nodes** - the toolbar checkbox next to Path controls which
  node type the *next* click places: unchecked (the default) places a
  sharp-cornered node, checked places a smooth one. Flip it mid-path to
  mix both kinds in the same path.

A path placed this way becomes a real paint object once finished, exactly
like a freehand stroke - it's selectable/movable/modifiable/deletable
with [Pick](#pick) afterward, the same as anything painted by hand.

Reshaping an already-placed path's own nodes and handles is [Pick](#pick)'s
own job, not the Path tool's - see its own "Edit its own path" bullet.
Inserting a node (other than at either end, by placing a new path that
starts/ends where the old one did) isn't here yet; see
[What's Not Here Yet](#whats-not-here-yet).

## Chord Generator

The **Chord Generator** toolbar button (next to Tool Configuration) opens
a dockable panel (off by default) that builds a chord, arpeggio, or a
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
**Reference** (tuning, in Hz) spin boxes controlling how beats-suffixed
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

To actually place it, click the **Chord** toolbar button (next to Path) to
arm it, then click anywhere on the canvas - the click only sets *when*
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

The **Grid** toolbar button opens a dockable panel (off by default) with
two drop-downs, one per canvas axis:

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

- **Note grid** - one line per semitone (12-TET), against the project's
  own tuning reference.
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
[Configure Devices](#configure-devices), not from this panel.

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
  checked *and* something is already playing: it keeps playing from
  wherever it already was, re-rendered "in place", without jumping.
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

## Recording

The **Record** panel has a single **Start Recording**/**Stop Recording**
toggle. Recording captures from the input device chosen in
[Configure Devices](#configure-devices) into a brand-new layer, encoded
the same way an imported file would be, the moment you stop. That input
device (and its Gain) can't be changed while recording is in progress.

## Loop Mode

The **Loop** panel turns the Studio into a fixed-length loop pedal:
**Start Loop** begins continuously capturing input in Duration-length
passes, playing the previous pass back while it records the next. By
default each new pass records over the last one, same as a standard loop
pedal; check **Freeze Loop** to freeze whatever's currently playing so it
just repeats instead of being overwritten. Input and output devices are
chosen from [Configure Devices](#configure-devices); a device change
takes effect the next time you start the loop, not immediately. Both
devices are locked (can't be changed) while a loop is running.

While a pass is being captured, the canvas grows continuously as audio
comes in, rather than waiting silently until the whole pass finishes -
watch the spectrogram fill in live, in real time.

## Configure Devices

The **Configure Devices** toolbar button opens a dockable panel (off by
default) that's the *only* place input and output devices are chosen -
Playback, Recording, and Loop Mode all use whatever's set here rather
than having pickers of their own.

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

The Input combo (and its Gain) locks while Recording or Loop Mode is
running; the Output combo (and its Gain) locks only while Loop Mode is
running, since Recording never touches output.

## Pooling a Layer

The **Pool Layer** toolbar button runs the topmost visible layer with
content through a full, lossless round-trip encode - a slower but
higher-fidelity representation than the fast, approximate one every import
and recording normally uses. There's no visible difference for most
material; it matters most before an audio export you want to sound as
close as possible to the original.

Pooling runs in the background - you can keep working while it processes,
and a small **✕** button appears in the status bar to cancel it if you
change your mind; cancelling leaves the layer exactly as it was, as if
pooling had never been started.

## Exporting

**File → Export Audio...** saves the topmost visible layer with content as
FLAC, Ogg Vorbis, or MP3 (pick the format via the save dialog's file type).

**File → Export Video...** saves it as an MP4: the spectrogram, animated
with the audio played alongside it.

Both kinds of export run in the background - you can keep working while
one encodes, and a small **✕** button appears in the status bar to cancel
it if you change your mind; cancelling removes whatever it had written so
far rather than leaving a broken file behind. Only one export (of either
kind) runs at a time - starting another while one is already in progress
just shows a reminder to wait or cancel first.

Both act on the same "one layer" rule described above - see
[the note above](#important-whats-actually-shown-and-played-right-now).

## Saving and Project Files

**Ctrl+S** (or **File → Save Project**) saves in place; **File → Save
Project As...** saves a copy elsewhere. A project is a `.smproj` file plus
a same-named folder next to it (a `media/` subfolder holding every layer's
actual encoded data, and a `pool/` subfolder once you've pooled at least
one layer) - keep the file and its folder together; moving or copying one
without the other breaks the project.

Closing the window, opening a different project, or starting a new one
while you have unsaved changes prompts you to save first.

## What's Not Here Yet

The [design document](docs/sound-mind-design.md) describes the Studio's
full intended scope - generators, analysis tools, a Composer Mode track
view, Sound Flower's polar view, MIDI import, chord/sequence generation,
and a Sound Mind VST plugin, among others none of which exist in the
Studio yet. MindWave-driven modulation (see [MindWaves](#mindwaves)
above) has made a real start - layer opacity binding works - but it's
far from its own full scope either; see the note further down. Filter
layers (see
[Filter Layers](#filter-layers) above) have finished their own milestone -
all twenty designed filter types work now, the Equalizer layer included.
`docs/sound-mind-roadmap.md` tracks what's actually being built next, in
order; this guide will grow alongside it.

Painting (see [Painting](#painting) above) exists, but only a fraction of
what's designed for it:

- Only **Procedural**, **Instrument** (harmonic series, inharmonicity, and
  MindWave-driven vibrato/tremolo - no noise component, body resonance, or
  ADSR envelope yet), **Mind Shot** (capture-and-restamp), **Mind Grain**
  (a live reference, updating immediately everywhere it's used the moment
  its own source is repainted), **Heal** (temporal blur), **Soften**
  (radial blur), **Smudge**
  (a simple per-stamp directional smear, not a real stateful "brush load"
  carried across the whole stroke the way a classic paint program's own
  Smudge tool works), and **Order/Chaos** (concrete permutation/reordering
  mechanics rather than a formal entropy metric) exist - **Clone** and the
  Tool Configuration Wizard/Tool Preset library (see the next point) are
  **permanently deferred, until further notice**, not merely not yet
  scheduled. Instrument strokes
  also don't yet bind to a MindWave, and Loop Mode doesn't yet retrigger
  per note. There's also no UI yet to reposition an
  already-captured Mind Grain's own referenced region, and a Mind Grain
  only ever samples its own source *layer*'s own raw content - not the
  full composite (every layer up through it, blended together) as it
  actually appears at that point in the stack; see
  `docs/sound-mind-design.md`'s own Deferred Decisions for that one.
- No **Tool Configuration Wizard** or **Tool Preset** drop-down - the
  Panel described above is the only way to set brush parameters today,
  and there's no way yet to save/reuse/export a particular brush setup.
  Permanently deferred, until further notice, along with Clone above.
- Pick can't **copy** a picked object yet - only move, modify a stroke's
  own brush settings, delete it, restack it within its own layer, or (for
  a brush stroke) edit its own path.
- Path editing (Edit → Edit Path) can move a node, drag a handle, or
  toggle a node's type, but can't yet insert or delete a node by
  double-clicking a segment or an existing node, or detach a handle from
  its own mirrored pair with Alt+drag - only whole-node delete (Delete
  key) is here today.
- Restacking (Bring to Front/Send to Back/Bring Forward/Send Backward)
  isn't undoable (Ctrl+Z) yet - see [Pick](#pick) above.
- The Alt/Shift/Ctrl axis-restriction mnemonic (see
  [Canvas Navigation](#canvas-navigation) above) only applies to Zoom so
  far - holding a modifier while dragging to move or resize a Pick
  selection, or while adjusting a layer's own translation/rescale, does
  nothing extra yet.

Selection (see [Selection and Fill](#selection-and-fill) above) exists,
but only a fraction of what's designed for it:

- **Rectangle** (rotatable), **Lasso** (freehand), and **Wand**
  (flood-fill by amplitude similarity, optionally harmonics-aware)
  selection all exist, and can be combined (add/subtract/intersect). A
  Wand or combined selection shows as a dashed bounding box rather than
  its own exact outline (a Lasso selection still gets a real curve) - the
  underlying Fill/Copy/Cut/Paste still respect its precise shape
  regardless. Mind Shot/Mind Grain capture from a Lasso/Wand/combined
  selection still captures/references its full bounding box, not confined
  to its actual shape.
- **Warp Selection was removed** - it never actually survived a layer
  rebuild (undo/redo, reopening a project) the way every other edit does,
  and the workflow needed to use it at all (draw a curve, Pick it, keep a
  separate rectangular selection, invoke the action while still in Pick
  mode) proved too confusing in practice. No replacement is planned.
- **Paste** always lands back at the exact position it was copied/cut
  from - there's no click-to-place gesture yet to paste somewhere else on
  the same layer (pasting onto a *different* layer is supported today;
  see [Selection and Fill](#selection-and-fill) above).
- Fill only ever produces a uniform color, not a real two-color gradient
  across the selection - there's no UI yet to pick a second color.

[Compositing](#compositing) (see above) mixes every visible layer
together on the canvas and in Playback, but only a fraction of what's
designed for it:

- **Normal (mixing) is the only blend mode** - a fuller catalogue
  (Multiply, Screen, and the rest of the usual image-editor set) isn't
  designed or built yet.
- **MindWave-bound opacity exists now** (see [MindWaves](#mindwaves)
  above), but nothing else does yet - blend mode itself still can't vary
  spatially, only opacity.
- **Recording's result, Loop Mode, Pooling, and Audio/Video Export**
  still act on a single (topmost) layer, not the composite - see
  [the note above](#important-whats-actually-shown-and-played-right-now).
- **Playback decodes the composite once, at Play** - it doesn't yet keep
  re-decoding live as you make further edits during playback.

[Filter Layers](#filter-layers) (see above) have finished their own
milestone - all twenty-one designed filter types work now - but a couple
of loose ends remain:

- **MindWave-bound filter parameters exist now** (see
  [MindWaves](#mindwaves) above) for nearly every scalar control across
  every filter type - see [Filter Layers](#filter-layers)'s own list
  above. Three remain permanently unbindable: Convolve's Kernel Size,
  Granular Noise's Grain Size, and Spectral Reverb's Pre-Delay/Decay/Room
  Size/Diffusion/Absorption - each shapes a fixed-size grid or a
  computation spanning many cells at once, not a value one cell owns on
  its own, so there's no meaningful "this cell's own value" for a
  MindWave to drive.
- **Convolve's own saved kernels have no rename or delete UI yet** - once
  saved, a kernel stays in the project's own library permanently (there's
  no way to remove or rename one from within the Studio itself).

[MindWaves](#mindwaves) (see above) exist now, but only a narrow slice of
what's designed for them:

- **Layer opacity, every filter-parameter scalar, and Sound Mind
  Instruments' own Vibrato/Tremolo** can bind to a MindWave now (see
  [Painting](#painting) above for Vibrato/Tremolo); no other brush
  parameter can yet. A layer's **Blend Mode** can't bind to a MindWave
  either - it's a single, fixed choice per layer, not spatially varying.
- **Vibrato/Tremolo track a stroke's own position along itself, not a
  genuine per-note clock** - every other binding in this codebase is
  canvas-space (a fixed mask laid over the whole piece); Vibrato/Tremolo
  instead follow the stroke's own progress, so a MindWave's shape always
  plays out fully over one stroke, however long or short it is. A real
  "retriggers exactly the same way for every distinct note, regardless of
  stroke length" alternative is still planned, not yet built.
- **Warp and Reduce exist now**, alongside Superposition - Warp (one field
  distorting where another samples from) is directly editable in the
  MindWaves panel; Reduce (collapsing a field to a plain control signal)
  has no standalone UI of its own yet - Vibrato/Tremolo above are its only
  current use.
- **Every generator type the design doc names now exists**, including
  Drawn, Step Grid, and Continuous (see above) - the MindWaves panel's own
  editor is still plain numeric fields (or, for Step Grid, a plain list of
  value boxes; or, for Continuous, three dials) rather than a fully
  polished interface, and Continuous's own exact formula is an explicitly
  provisional first attempt, not a finished design.

The [Path Tool](#path-tool) (see above) only places new paths today; once
a path is placed, reshaping it is Pick's job (see [Pick](#pick) above,
"Edit its own path") rather than the Path tool's own:

- Inserting or removing a node from an already-placed path (other than
  deleting the single selected node) still isn't here - see the Pick
  section above.
- No way yet to reopen a stamped chord/arpeggio's own instrument for
  editing after the fact - see [Chord Generator](#chord-generator) above;
  re-timing/re-voicing it with Pick works today, but re-pointing it at a
  different instrument doesn't yet.
- Driving a sequence live from a connected MIDI controller isn't here -
  the Chord Generator's own Chord Builder/Custom Notation modes are the
  only ways to build a sequence today.
- No pitch quantising while painting - Snap to Grid (see
  [Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid) above)
  snaps a *placed* node/move/selection to the nearest grid line, but a
  freehand stroke's own pitch doesn't yet snap to the active Frequency
  Grid while it's being painted.
- No curated preset sets for Custom Frequencies, and no way yet to
  retune the note grid's own tuning reference (fixed at 440 Hz/A4) or
  change the project's own default tempo (fixed at 120 BPM) - Custom
  Frequencies is free-entry only for now (type your own Hz values).
- No *separate* Path Gradient UI, but none is needed any more - a
  finished path uses whatever gradient Painting's own Gradient editor
  held at the moment it was drawn (see [Painting](#painting) above), and
  a gradient with more than two stops really does vary continuously
  along the path's own length as it's stamped, the same way it varies
  across a Fill Selection or Filter Configuration's own frequency axis.
  Editing the panel's Gradient afterward doesn't retroactively change an
  already-finished path, the same snapshot-at-draw-time rule every other
  tool setting already follows.
