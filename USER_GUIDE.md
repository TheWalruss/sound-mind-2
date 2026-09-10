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
5. [Importing Media](#importing-media)
6. [Working with Layers](#working-with-layers)
7. [Painting](#painting)
8. [Pick](#pick)
9. [Playback](#playback)
10. [Recording](#recording)
11. [Loop Mode](#loop-mode)
12. [Pooling a Layer](#pooling-a-layer)
13. [Exporting](#exporting)
14. [Saving and Project Files](#saving-and-project-files)
15. [What's Not Here Yet](#whats-not-here-yet)

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
- **Pool Layer** toolbar button - see [Pooling a Layer](#pooling-a-layer).
- **Status bar** (bottom) - the left side shows the mouse cursor's
  position while it's over the canvas, both in pixels and in time/
  frequency (e.g. `30, 10 px   |   0.300 s, 523 Hz`), clearing once the
  cursor leaves. A temporary message (e.g. "Imported ...") briefly covers
  it when one is shown.

Every dock panel can be dragged to a different edge of the window, or
floated, like any Qt dock widget.

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
scratch, rather than from an import. Each row has:

- A **drag handle** (⠿) to reorder it, or a **lock icon** (🔒) if it can't
  be reordered or deleted - only the **Background** layer (always present,
  bottom of the stack) is locked today.
- A **visibility toggle** (●/○).
- The layer's **name** - click once to select it (see [Painting](#painting)
  below - the selected layer is the one a brush stroke paints into), or
  double-click to rename it.
- A **type tag**, for any layer type other than the ordinary kind you get
  from importing.
- An **opacity slider**, and **translation**/**rescale** spin boxes (see
  [Layer Timing](#layer-timing) below) - except on the **Background**
  layer's row, which has none of these: it's always fully opaque and
  always first in time, so neither applies to it.
- A **delete** button (×), for any layer except the locked one(s).

### Important: what's actually shown right now

Sound Mind Studio's eventual design composites every visible layer
together. **That compositing doesn't exist yet.** Right now, the canvas -
and Playback, Recording's result, Loop Mode, Pooling, and video export -
all show or use exactly one layer: the **topmost layer that's both
visible and has content**, skipping hidden ones. This means:

- The opacity slider doesn't currently have any visible effect - it's
  there, and it's saved with the project, but nothing reads it yet.
- Importing a new layer, or reordering one to the top, changes what's on
  screen and what plays - even without touching visibility.
- To compare two layers, toggle one's visibility off and the other's on,
  rather than expecting to see both at once.

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

Both are visual/timing adjustments to the spectrogram only - they don't
currently affect a layer's own audio when it's the one being played,
recorded from, or exported (see the note above: only one layer is ever
actually played at a time today, so "lining up" two layers is something
you'll be able to see, by toggling visibility, before you'll be able to
hear it composited).

## Painting

Click the **Paint** toolbar button to switch the canvas into paint mode;
click it again (or switch tools) to leave it. While it's on, dragging on
the canvas draws a stroke - you'll see it live as a yellow outline while
drawing, and it's applied to the spectrogram once you release the mouse.

**Which layer gets painted**: whichever layer's name you last clicked in
the Layers panel (see [Working with Layers](#working-with-layers)). If
you haven't clicked one yet, painting targets the topmost layer - which,
in a brand new project, is the **Background** layer itself: it's a real,
paintable canvas like any other, not just a fixed floor to import onto.

The **Tool Configuration** toolbar button opens a dockable panel (off by
default, alongside Layers/Playback/Record/Loop) with the brush's own
settings:

- **Tip Shape** - the stroke's cross-section (Circle, Square, Diamond,
  and others - only Circle/Square/Diamond have a distinct shape so far;
  the rest currently paint the same as Circle).
- **Falloff** - how soft the stroke's edge is, from a hard edge (`0`) to
  fully soft (`1`).
- **Brush Size**.
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

**Undo** (Ctrl+Z) and **Redo** (Ctrl+Y), in the new **Edit** menu, apply
to paint strokes.

Only the **Procedural** brush exists today - see
[What's Not Here Yet](#whats-not-here-yet) for the rest of what's
planned around painting.

## Pick

Click the **Pick** toolbar button (next to Paint) to switch the canvas
into pick mode; click it again (or Paint) to leave it. While it's on,
clicking a painted stroke selects it - shown with a white outline around
it, regardless of the "Show bounding boxes" setting - ready to:

- **Move** it - click and drag it to a new position.
- **Modify** it - open **Tool Configuration** (if it isn't already);
  it's pre-filled with exactly the settings the stroke was painted with.
  Change anything (tip shape, falloff, size, color, opacity) and the
  selected stroke updates to match.
- **Delete** it - **Edit → Delete** (or the Delete key).

Clicking empty canvas space deselects. Every one of these is undoable
(Ctrl+Z), the same as painting a new stroke.

Copying a picked stroke, and directly reshaping its underlying path (drag
its own nodes and handles), aren't here yet - see
[What's Not Here Yet](#whats-not-here-yet).

## Playback

The **Playback** panel has **Play**, **Pause**, and **Stop**, a draggable
position bar with an elapsed/total time label, an output device picker,
and a volume slider. It plays the topmost visible layer with content -
see [the note above](#important-whats-actually-shown-right-now). Playback
reflects the project as it currently stands; there's no separate "render"
step.

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
full intended scope - filter layers, MindWave-driven modulation,
generators, analysis tools, a Composer Mode track view, Sound Flower's
polar view, MIDI import, chord/sequence generation, and a Sound Mind VST
plugin, among others none of which exist in the Studio yet.
`docs/sound-mind-roadmap.md` tracks what's actually being built next, in
order; this guide will grow alongside it.

Painting (see [Painting](#painting) above) exists, but only a fraction of
what's designed for it:

- Only the **Procedural** brush - the other paintbrush types (Sound Mind
  Instrument, Mind Shot, Mind Grain) and other painting tools (Smudge,
  Order/Chaos, Heal, Soften, Clone) aren't built yet.
- No **Tool Configuration Wizard** or **Tool Preset** drop-down - the
  Panel described above is the only way to set brush parameters today,
  and there's no way yet to save/reuse/export a particular brush setup.
- Pick can't **copy** a picked stroke, and can't reshape its underlying
  path directly (dragging its own individual nodes/handles) - only move,
  modify its brush settings, or delete it.
