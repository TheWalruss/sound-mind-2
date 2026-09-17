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
12. [Axis Labels](#axis-labels)
13. [Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid)
14. [Playback](#playback)
15. [Recording](#recording)
16. [Loop Mode](#loop-mode)
17. [Pooling a Layer](#pooling-a-layer)
18. [Exporting](#exporting)
19. [Saving and Project Files](#saving-and-project-files)
20. [What's Not Here Yet](#whats-not-here-yet)

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
- **View menu** - Zoom controls; see [Canvas Navigation](#canvas-navigation)
  below.
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
**Ctrl** is for **proportional** control - both axes together.

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

**File → Import Audio...** prompts for a WAV file. If it's no longer than
your project's Duration, it's imported directly as one new layer. If it's
longer, it's split into consecutive Duration-length snippets and you're
shown a list to check/uncheck - only the checked snippets become layers,
named `<filename>_0000`, `<filename>_0001`, and so on.

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
scaling/sequencing picker **File → Import Image...** does; a dropped
`.wav` with more than one computed snippet shows the same snippet picker
**File → Import Audio...** does, one picker per such file; `.smproj` opens
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
[Filter Layers](#filter-layers) below. Each row has:

- A **drag handle** (⠿) to reorder it, or a **lock icon** (🔒) if it can't
  be reordered or deleted - the **Background** layer (always present,
  bottom of the stack) and the **Equalizer** layer (always present, top
  of the stack - see [Filter Layers](#filter-layers) below) are both
  locked.
- A **visibility toggle** (●/○).
- The layer's **name** - click once to select it (see [Painting](#painting)
  below - the selected layer is the one a brush stroke paints into), or
  double-click to rename it.
- A **type tag**, for any layer type other than the ordinary kind you get
  from importing.
- An **opacity slider**, an **opacity-MindWave combo** (see
  [MindWaves](#mindwaves) below), and **translation**/**rescale** spin
  boxes (see [Layer Timing](#layer-timing) below) - except on the
  **Background** layer's row, which has none of these: it's always fully
  opaque and always first in time, so none of them apply to it.
- A **delete** button (×), for any layer except the locked one(s).

### Compositing

Every visible layer with content contributes to what the canvas shows
and Playback plays - mixed together, not one covering another. Two
layers at full opacity both come through in full, exactly like two
instruments or voices sounding at once; a layer's own **Opacity** slider
acts as its own volume in that mix, from silent (`0%`) to full strength
(`100%`), rather than making it "more see-through" the way opacity works
in an image editor. Turning a layer's opacity down never affects any
other layer - each one mixes in independently.

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
- **Preview** shows the currently selected MindWave's own field directly
  on the canvas, as a live grayscale overlay (black where it's 0, white
  where it's 1) - updating instantly as you change its generator type,
  parameters, or superposition stack, and as you select a different
  MindWave while Preview stays on. A judgment aid only - it never affects
  the actual composite, and switches off automatically when you open or
  create a different project.
- Selecting a MindWave shows its own editor: a **Generator Type** (Periodic,
  Envelope, Stepped/Noise, Spatial, or Fractal) and that type's own plain
  numeric parameters - a period, a phase, a seed, and so on. This is a
  bare-bones, functional editor, not a polished one yet - there are no
  dials or drawing tools here, just fields to type numbers into.
- **Superposition** lets one MindWave combine several others together
  (multiply, add, min, max, or average) - the **+ Add Member**/**- Remove
  Member** buttons and the small list beside them manage that MindWave's
  own combined members, each editable the same way as a top-level one.
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

The Filter Configuration panel starts with a **Filter Type** dropdown -
every new Filter layer starts as **Frequency-Axis Gradient**. Choosing a
different type shows only that type's own controls below the dropdown;
switching back and forth doesn't lose whatever you entered into a
group's own controls while it was hidden.

- **Frequency-Axis Gradient** shows two groups, **Start (t=0, lowest
  frequency)** and **End (t=1, highest frequency)**, each with **Left
  Intensity**/**Left Opacity**/**Right Intensity**/**Right Opacity** -
  the exact same gradient controls Path Gradient and Fill already use
  (see [Gradients](#gradients) below), applied across the frequency axis
  instead of along a path: at each frequency, the composite's own
  loudness blends toward that stop's own Intensity, by that stop's own
  Opacity (`0` leaves it untouched, `1` forces it all the way to
  Intensity). A fresh Filter layer starts fully transparent (both stops
  at `0` opacity) - it does nothing until you raise an Opacity. This is
  a basic, spin-box-only editor for now - just the gradient's own two
  endpoint stops, no draggable visual editor and no interior stops yet.
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

Every one of the six designed filter types now has a real, working
algorithm.

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
so there's nothing to switch), just a **Cut** group with the same
**Start**/**End** stops as Frequency-Axis Gradient, but only **Left
Cut**/**Right Cut** per stop - `0` leaves that frequency untouched, `1`
silences it completely. Unlike the ordinary Frequency-Axis Gradient
editor, there's no Intensity to set here - the Equalizer only ever cuts
toward silence, never toward some other target loudness.

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
  Shot**, or **Mind Grain**; picks which set of controls below applies (Tip
  Shape for Procedural, Harmonics/Inharmonicity for Instrument, a picker
  for Mind Shot/Mind Grain). Switching keeps Falloff/Size/Stamp Mode/
  Interval/Color/Opacity as they were - only the type-specific controls
  reset to the newly-picked type's own defaults.
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
- **Mind Shot** (Mind Shot only) - a drop-down of every Mind Shot you've
  captured so far (see [Selection and Fill](#selection-and-fill)'s own
  "Capture as Mind Shot"), empty until you capture your first one.
  Painting with it stamps the captured content back exactly as it was
  captured - a hard overwrite at its own original size, not blended or
  scaled by Falloff/Size the way Procedural/Instrument are.
- **Mind Grain** (Mind Grain only) - a drop-down of every Mind Grain you've
  captured so far (see [Selection and Fill](#selection-and-fill)'s own
  "Capture as Mind Grain"), empty until you capture your first one. The
  opposite of Mind Shot: no pixels are ever captured, only a reference to
  the region and the layer it was selected on - painting with it always
  reads that layer's *current* content, and repainting the source layer
  updates every Mind Grain stroke that reads from it immediately, wherever
  it's painted, not just the next time that stroke's own layer happens to
  be redrawn for some other reason.

  **A Mind Grain can only paint onto a layer above its own source** - the
  panel won't even let you attempt it on the wrong layer: the Mind Grain
  group turns red (with a tooltip explaining why) whenever the active
  layer isn't above the configured grain's source, the Layers panel marks
  every disallowed layer with a red **✕**, and the Paint toolbar button
  itself is disabled (its own tooltip explains why) - switch to a layer
  higher in the stack, or reorder the layers, to paint with it. Reordering
  or deleting a layer that would break an *already-painted* Mind Grain
  stroke's own ordering is refused outright too, with a dialog explaining
  which stroke(s) would break and why.
- **Falloff** - how soft the stroke's edge is, from a hard edge (`0`) to
  fully soft (`1`).
- **Brush Size** - the tip's own radius: in seconds on the time axis, and
  the equivalent frequency span on the frequency axis (using this
  project's own Hz-per-second scale, so the same number always describes
  the same *shape*, not the same pixel size, regardless of canvas
  resolution). The panel's own default (`0.2`) is a comfortably visible
  stroke on a typical project without covering too much of it at once.
- **Stamp Mode** - how densely the brush's tip is stamped along a stroke's
  path, from a drop-down:
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
  canvas zoom yet, so seconds/Hz are the only units that make sense).
- **Color** - a swatch button; click it to open a color picker. This is
  how loud the stroke paints, *and* its stereo balance, at once: the
  picked color's red channel sets the left channel's loudness, green
  sets the right channel's - full red with no green paints loud on the
  left and silent on the right, and so on. The swatch shows the color's
  hex code as well as its fill. The panel's own default (bright yellow -
  full red and green) paints loud on both channels equally.
- **Opacity** - how strongly the stroke's color is actually applied,
  regardless of what it is. The panel's own default (100%) paints at
  full strength right away, no setup required.

Two checkboxes at the top of the panel, both off by default:

- **Show bounding boxes** - outlines (cyan) every stroke's overall extent
  on the currently-shown layer.
- **Show path geometry** - traces (magenta) every stroke's own drawn
  path. A stroke actively being drawn is always traced, regardless of
  this checkbox.

**Undo** (Ctrl+Z) and **Redo** (Ctrl+Y), in the **Edit** menu, apply to
paint strokes and to a layer's own opacity, opacity-MindWave binding,
visibility, translation, and rescale (see
[Working with Layers](#working-with-layers)) - both kinds share the same
single history, undoing/redoing whichever one actually happened most
recently, in either order.

Only **Procedural**, **Instrument** (harmonic series + inharmonicity only so
far - no noise, body resonance, or envelope yet), **Mind Shot**
(capture-and-restamp), and **Mind Grain** (a live reference that updates
immediately, everywhere it's used, the moment its source is repainted)
exist today - see [What's Not Here Yet](#whats-not-here-yet) for the rest
of what's planned around painting.

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
While it's on, drag a rectangle on the canvas to select that region -
shown as a green outline. A selection stays active (and visible) even
after switching to a different tool - it isn't tied to Select mode
itself, only to having drawn one.

- **Fill** it - **Edit → Fill Selection...** opens a color picker; the
  picked color fills the selection at full strength, confined exactly to
  its own boundary. The color's red channel controls how loud the left
  channel is filled, green controls the right - the same convention
  Painting's own Color control uses.
- **Copy** it - **Edit → Copy** (Ctrl+C) captures the selection's own
  pixels onto the clipboard, leaving them in place.
- **Cut** it - **Edit → Cut** (Ctrl+X) does the same as Copy, then
  silences the selection's own region in place.
- **Paste** the clipboard - **Edit → Paste** (Ctrl+V) writes it back at
  the same position it was captured from, onto whichever layer is
  currently active in the Layers panel - which doesn't have to be the
  layer it was copied or cut from. Select a different layer's row first
  to paste onto it instead. Switches straight to [Pick](#pick) and
  selects the newly pasted result there - move, modify, delete, or
  restack it right away, with no separate click needed to find it again.
- **Capture as Mind Shot** it - **Edit → Capture as Mind Shot** stores the
  selection's own pixels permanently, named "Mind Shot 1", "Mind Shot 2",
  and so on - unlike Copy, this doesn't touch the clipboard, and the
  source pixels are left exactly as they were (no silencing, unlike Cut).
  Once captured, pick **Mind Shot** as the Tool Type in
  [Painting](#painting)'s Tool Configuration panel and select it from the
  drop-down there to paint with it - it stamps back exactly as captured,
  wherever you paint, on any layer.
- **Capture as Mind Grain** it - **Edit → Capture as Mind Grain** stores a
  *reference* to the selection's own layer and region, named "Mind Grain 1",
  "Mind Grain 2", and so on - unlike Capture as Mind Shot, no pixels are
  captured at all, and neither the clipboard nor the source layer's content
  is touched. Once captured, pick **Mind Grain** as the Tool Type and
  select it from the drop-down there to paint with it - see
  [Painting](#painting)'s own Mind Grain entry for the "only paintable
  above its own source layer" rule and the guardrails that enforce it.
- **Deselect** it - **Edit → Deselect** (Ctrl+D), or drag-clicking without
  actually dragging (a plain click) on the canvas while in Select mode.

Filling and pasting are both undoable (Ctrl+Z), the same as painting a
stroke; so is Cut's own silencing of the source region.

Only rectangular selections exist today - see
[What's Not Here Yet](#whats-not-here-yet) for what's still planned.

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

The **Playback** panel has **Play**, **Pause**, and **Stop**, a draggable
position bar with an elapsed/total time label, an output device picker,
and a volume slider. It plays the project's own real composite - every
visible layer mixed together (see [Compositing](#compositing) above),
spanning the project's own full canvas-width duration, silence included
past wherever real content ends. Pressing Play decodes that composite
once; it doesn't yet keep re-decoding live as you make further edits
during playback.

While playing, a white line sweeps across the canvas in real time,
tracking the current position - the same playhead line a video export
would show, live. Drag the position bar (whether playing or paused) to
seek to a different point; both the bar and the canvas playhead jump to
match immediately.

## Recording

The **Record** panel has an input device picker and a **Start
Recording**/**Stop Recording** toggle. Recording captures from the chosen
input device into a brand-new layer, encoded the same way an imported file
would be, the moment you stop.

## Loop Mode

The **Loop** panel turns the Studio into a fixed-length loop pedal:
**Start Loop** begins continuously capturing input in Duration-length
passes, playing the previous pass back while it records the next. By
default each new pass records over the last one, same as a standard loop
pedal; check **Freeze Loop** to freeze whatever's currently playing so it
just repeats instead of being overwritten. Input and output devices are
chosen the same way as Playback/Recording; a device change takes effect
the next time you start the loop, not immediately.

## Pooling a Layer

The **Pool Layer** toolbar button runs the topmost visible layer with
content through a full, lossless round-trip encode - a slower but
higher-fidelity representation than the fast, approximate one every import
and recording normally uses. There's no visible difference for most
material; it matters most before an audio export you want to sound as
close as possible to the original.

## Exporting

**File → Export Audio...** saves the topmost visible layer with content as
FLAC, Ogg Vorbis, or MP3 (pick the format via the save dialog's file type).

**File → Export Video...** saves it as an MP4: the spectrogram, animated
with the audio played alongside it.

Both act on the same "one layer" rule described above - see
[the note above](#important-whats-actually-shown-right-now).

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
[Filter Layers](#filter-layers) above) are further along than most - all
six designed filter types work now - but the milestone as a whole isn't
finished (no Equalizer layer yet, in particular).
`docs/sound-mind-roadmap.md` tracks what's actually being built next, in
order; this guide will grow alongside it.

Painting (see [Painting](#painting) above) exists, but only a fraction of
what's designed for it:

- Only **Procedural**, **Instrument** (harmonic series + inharmonicity
  only - no noise component, body resonance, or ADSR envelope yet), **Mind
  Shot** (capture-and-restamp), and **Mind Grain** (a live reference,
  updating immediately everywhere it's used the moment its own source is
  repainted) exist - the other painting tools (Smudge, Order/Chaos, Heal,
  Soften, Clone) aren't built yet. Instrument strokes also don't yet bind
  to a MindWave, and Loop Mode doesn't yet retrigger per note. A Mind
  Shot/Mind Grain stamp always overwrites verbatim - blend-mode selection
  (so it could blend rather than overwrite) is planned alongside
  layer/Paste blend modes. There's also no UI yet to reposition an
  already-captured Mind Grain's own referenced region, and a Mind Grain
  only ever samples its own source *layer*'s own raw content - not the
  full composite (every layer up through it, blended together) as it
  actually appears at that point in the stack; see
  `docs/sound-mind-design.md`'s own Deferred Decisions for that one.
- No **Tool Configuration Wizard** or **Tool Preset** drop-down - the
  Panel described above is the only way to set brush parameters today,
  and there's no way yet to save/reuse/export a particular brush setup.
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

- Only **Rectangle** selection - Lasso (freehand) and Wand (flood-fill by
  amplitude similarity) aren't built yet, and there's no way yet to
  combine multiple selections (add/subtract/intersect).
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

[Filter Layers](#filter-layers) (see above) have all six designed filter
types working now, but still only a slice of what's designed for the
milestone as a whole:

- **No draggable visual gradient editor for Frequency-Axis Gradient
  (including the Equalizer's own Cut editor), and no interior stops** -
  just the two endpoint stops (`t=0`, `t=1`), via plain spin boxes. (Tone
  Curve, unlike Frequency-Axis Gradient, does have a real draggable point
  editor now.)
- **MindWave-bound filter parameters exist now** (see
  [MindWaves](#mindwaves) above) for the five scalar controls listed
  above - Sigma, Size, Length, Angle, and Amount. Frequency-Axis
  Gradient/Equalizer and Tone Curve have no bindable scalar of their own
  yet, so they're untouched by this.

[MindWaves](#mindwaves) (see above) exist now, but only a narrow slice of
what's designed for them:

- **Layer opacity and the five filter-parameter scalars** can bind to a
  MindWave now; **brush parameters** can't yet (see the bullet above).
- **No canvas-space vs. operation-relative choice** - every binding today
  is implicitly canvas-space (the field is a fixed mask laid over the
  whole piece); the "retriggers fresh per note" alternative is planned for
  a later Sound Mind Instruments installment (harmonic series +
  inharmonicity only exist so far - see [Painting](#painting) above).
- **No Field Operators** - Warp (one field distorting where another
  samples from) and Reduce (collapsing a field to a plain control signal)
  aren't built. Superposition (combining several MindWaves together) is
  the one field operator that does exist.
- **No drawn-shape or step-grid generator types**, and no Continuous
  Controls interface - the MindWaves panel's own editor is plain numeric
  fields only, per generator type.

The [Path Tool](#path-tool) (see above) only places new paths today; once
a path is placed, reshaping it is Pick's job (see [Pick](#pick) above,
"Edit its own path") rather than the Path tool's own:

- Inserting or removing a node from an already-placed path (other than
  deleting the single selected node) still isn't here - see the Pick
  section above.
- No **Chord Overlay** - a selected chord/arpeggio in a (not-yet-built)
  Chord Generator drawn live on the frequency axis, independent of the
  general Frequency Grid.
- No pitch quantising while painting - Snap to Grid (see
  [Overlay Grids and Snap to Grid](#overlay-grids-and-snap-to-grid) above)
  snaps a *placed* node/move/selection to the nearest grid line, but a
  freehand stroke's own pitch doesn't yet snap to the active Frequency
  Grid while it's being painted.
- No curated preset sets for Custom Frequencies, and no way yet to
  retune the note grid's own tuning reference (fixed at 440 Hz/A4) or
  change the project's own default tempo (fixed at 120 BPM) - Custom
  Frequencies is free-entry only for now (type your own Hz values).
- No dedicated Path Gradient UI - a finished path uses the current
  Painting brush's own Color/Opacity settings (the same uniform-color
  shortcut Fill Selection's own picker uses), not a real multi-stop
  gradient along the path's own length.
