# Sound Mind Studio — User Guide

Sound Mind Studio is a visual audio editor. Sound is encoded as a spectrogram — a picture of frequency against time — which you can paint, layer, filter, and transform. The Non-Stationary Gabor Transform (NSGT) ensures that decoding the edited image back to audio gives you exactly what you painted, with no codec artefacts introduced.

---

## Table of Contents

1. [The Sound Mind Philosophy](#1-the-sound-mind-philosophy)
2. [Concepts](#2-concepts)
3. [Interface Overview](#3-interface-overview)
4. [Getting Started](#4-getting-started)
5. [Working with Single TIFF Files](#5-working-with-single-tiff-files)
6. [Working with Projects](#6-working-with-projects)
7. [Paint Tools](#7-paint-tools)
8. [Brush Parameters & Blend Modes](#8-brush-parameters--blend-modes)
9. [The Gradient Editor](#9-the-gradient-editor)
10. [Filters](#10-filters)
11. [Layers](#11-layers)
12. [Layer Transforms](#12-layer-transforms)
13. [MindWaves](#13-mindwaves)
14. [View & Navigation](#14-view--navigation)
- [Sound Flowers](#sound-flowers)
15. [Page Modes & RGB Channel Mapping](#15-page-modes--rgb-channel-mapping)
16. [Overlays & Grids](#16-overlays--grids)
17. [Chord Generator](#17-chord-generator)
18. [The Mind-Shot Editor](#18-the-mind-shot-editor)
19. [MindGrains](#19-mindgrains)
20. [Encoding & Decoding Audio](#20-encoding--decoding-audio)
21. [Importing Media as Layers](#21-importing-media-as-layers)
22. [Converting Images to Audio](#22-converting-images-to-audio)
23. [The Op Log](#23-the-op-log)
24. [Re-edit Mode](#24-re-edit-mode)
25. [History Panel & Undo/Redo](#25-history-panel--undoredo)
26. [Remaster](#26-remaster)
27. [Project Settings](#27-project-settings)
28. [File Formats](#28-file-formats)
29. [Analysis Tools](#29-analysis-tools)
30. [Keyboard Shortcuts](#30-keyboard-shortcuts)
31. [Common Workflows](#31-common-workflows)
32. [Live Mode](#32-live-mode)
33. [Canvas Right-Click Menu](#33-canvas-right-click-menu)
34. [Studio Profiles](#34-studio-profiles)
35. [Video Export](#35-video-export)

---

## 1. The Sound Mind Philosophy

The Sound Mind symbol is composed of five interwoven forms. Each one reflects a principle that guided the design of the studio and its codec:

**The Möbius Strip** — One surface, no boundary, no break. It stands for nonduality and indivisibility: audio and image are not two things but one continuous form. Encode → edit → decode is a perfect loop — the NSGT codec is mathematically lossless, so every pixel you paint is a direct instruction to the synthesis engine with nothing lost in translation.

**The Infinity Symbol (∞)** — Recurrence, eternity, perpetuity. Every spectrogram is an open field with no final state. Layers, blend modes, and transforms compose without limit. There is no ceiling on what can be created or how deep a composition can go — only an endless unfolding.

**The Diamond** — The symbol of choice kept permanently open. The diamond is also hardness, brilliance, and ground: the clarity of structure and the resilience of a form that holds. Unlimited undo, non-destructive layers, and revisable transforms embody the explicate order — every decision made explicit, every path remaining open. The studio is built around the freedom to choose again.

**The Heart** — Love, compassion, life. The heart carries rhythm — the pulse underlying all music — and within it: reflection, symmetry, complexity. The tools are designed for intuitive, expressive painting and sculpting of sound, answering to feeling as much as measurement. A reminder of strength in expression and of why we create at all.

**Phi (φ)** — More than the golden ratio: consciousness itself. Phi stands for the integrated information of IIT and the universal consciousness field of Strømme's theory. The frequency axis encodes the implicate order of mind into the geometry of the canvas — log-spaced NSGT bins, octaves equally spaced, the piano keyboard aligned cleanly to the vertical axis. Switch to mel display to perceive equal pitch distance at equal pixel distance. The proportions of human hearing are the proportions of awareness made visible.

---

## 2. Concepts

### What is a Spectrogram?

A spectrogram is a 2D representation of audio:

- **Horizontal axis** — time (left to right)
- **Vertical axis** — frequency (low at the bottom, high at the top)
- **Pixel brightness** — loudness at that frequency and moment in time

Sound Mind uses the **Non-Stationary Gabor Transform (NSGT)** to produce spectrograms with perfect reconstruction. Encoding audio to a TIFF and then decoding it back, without any edits, reconstructs the original samples exactly.

### The Sound Mind TIFF Format

Each Sound Mind TIFF stores **four 16-bit grayscale pages**:

| Page | Contents | Value range |
|------|----------|-------------|
| 0 | Left channel amplitude | 0–65535 → −96 dB to 0 dB |
| 1 | Right channel amplitude | 0–65535 → −96 dB to 0 dB |
| 2 | Left channel phase | 0–65535 → −π to +π radians |
| 3 | Right channel phase | 0–65535 → −π to +π radians |

The canvas height is determined by the codec settings. At the default 75 bins-per-octave, the frequency axis is **724 pixels tall**. Width is determined by the audio duration and timestep. At the default 10 ms/pixel, one minute of audio produces an image approximately 6000 pixels wide.

### Frequency Scale

Frequencies are displayed on a **logarithmic scale** by default. The log scale matches the NSGT's native log-spaced frequency bins, so no interpolation is applied — every pixel maps to exactly one NSGT bin. Mel and linear scale modes are also available and apply a display-only Y-axis remap with no effect on the stored data or audio.

### What Editing Does

- **Painting amplitude pixels brighter** makes that frequency louder at that moment in time.
- **Painting amplitude pixels darker** (toward zero) makes it quieter or silent.
- **Editing phase pixels** changes the waveform shape at that time-frequency bin — altering the perceived timbre or creating pitch-shift effects without changing loudness.
- **Applying filters** reshapes the spectral content across regions or the whole layer.
- **Compositing layers** blends multiple sounds together, with blend modes controlling how they interact.

---

## 3. Interface Overview

```
┌───────────────────────────────────────────────────────────────────────┐
│  Studio  Project  Edit  View  Page  Tools  Filters                   │
├──────────────┬──────────────────────────────────────┬─────────────────┤
│              │  Toolbar: Fit · Scale · Page · R/G   │                 │
│  Tools       ├──────────────────────────────────────┤  Layers         │
│  Panel       │                                      │  Panel          │
│  (left)      │         Spectrogram Canvas           │  (right)        │
│              │                                      │                 │
│  Tool        │     < paint, view, navigate >        │  Layer list     │
│  selector    │                                      │  + controls     │
│              │                                      │                 │
│  Brush       │                                      │  History        │
│  params      │                                      │  Panel          │
│              │                                      │  (right, below) │
│  Tool        ├──────────────────────────────────────┤                 │
│  options     │  Status: time · freq · activity      │                 │
└──────────────┴──────────────────────────────────────┴─────────────────┘
```

### The Tools Panel (left dock)

Contains the tool selector buttons at the top, then below them:

- **Brush parameters** — Size, Opacity, Falloff, R value, G value, Blend mode (for brush-based tools)
- **Tool-specific controls** — varies by the active tool (curve editor, gradient editor, Mind-Shot picker, etc.)

### The Spectrogram Canvas (centre)

The main editing area. Displays the active layer's spectrogram, with the composite of all other layers shown semi-transparently behind it. The brush cursor is drawn as a circle or rectangle overlay; the system cursor is not used for painting.

### The Layers Panel (right dock)

Lists all layers in the current project. Each row shows controls for one layer. Toggle with **Ctrl+L**.

### The History Panel (right dock, below Layers)

Displays the undo/redo stack and the op log entries for the active project. Toggle with **Ctrl+H**.

### The Overlay Panel (floating dock)

Configures visual grid overlays on the canvas. Toggle with **Ctrl+G**.

### The Status Bar (bottom)

Displays the cursor's time position (seconds from start), frequency (Hz and nearest note name), the current pixel value (amplitude in dB or phase in radians), and any background activity (encoding progress, saving, etc.).

### The Toolbar

| Control | Description |
|---------|-------------|
| **Fit** | Fit the full spectrogram into the window |
| **Scale** combo | Frequency scale: Mel · Linear · Log |
| **Page** buttons | Switch display page: 0 (Amp L) · 1 (Amp R) · 2 (Phase L) · 3 (Phase R) · RGB |
| **☀ Light** | Light mode toggle — inverts amplitude pages (0/1) and RGB view so silence appears white and louder content appears dark |
| **R/G toggle** | Quick toggle between R-only and G-only view while in single-channel mode |
| **▶ / ⏸** | Play / pause the decoded project audio |
| **■** | Stop playback |
| **[ ]** | Set loop IN / OUT markers (see Loop Preview below) |
| **⟳** | Toggle loop preview (loops between IN and OUT markers) |
| Position label | Elapsed playback time (M:SS.s) |
| **Follow** | Follow mode toggle — locks the playback cursor at a fixed screen position and scrolls the image underneath as playback advances |
| Volume slider | Adjust playback volume |

### Loop Preview

Loop preview lets you hear a section of the project on continuous repeat while you edit.

1. Click **▶** to play the project (renders and decodes if needed).
2. While playing, click **[** at the moment you want the loop to start, and **]** at the moment you want it to end.
   - Alternatively, draw a rectangular selection first; the selection's left and right edges are used automatically if no explicit markers are set.
3. Click **⟳** to toggle loop mode. Playback jumps back to the IN marker whenever it passes the OUT marker.
4. Click **⟳** again (or **■** Stop) to exit loop mode.

Loop markers are shown as vertical dashed lines — green for IN, red for OUT.

---

## 4. Getting Started

### Opening a File

- **Studio → Open TIFF** (`Ctrl+O`) — open a Sound Mind TIFF for standalone editing
- **Project → Open Project** (`Ctrl+Shift+O`) — open a `.smsproj` multi-layer project
- **Studio → Recent Projects** — lists the 10 most recently opened projects; click any entry to reopen it
- **Studio → Recent Files** — lists the 10 most recently opened standalone TIFFs
- **Studio → Favorite Directories** (Windows) — folder shortcuts from `%USERPROFILE%\Links`; click any entry to open it in Explorer

### Creating a New Project

**Project → New Project** (`Ctrl+Shift+N`)

You will be prompted to name the project and choose a save location. The project saves as a `.smsproj` file alongside a folder (same name) that holds `media/`, `sources/`, `instruments/`, `kernels/`, and `ops/` subfolders.

The **New Project** dialog lets you set the canvas duration before the silent background layer is created. Codec parameters are hidden under a **▶ Codec settings** disclosure section — click it to expand if you need to change them from their defaults.

| Setting | Default | Description |
|---------|---------|-------------|
| **Duration** | 0 min 30 sec | Length of the silent background canvas. Good for sketching; increase for full songs. |
| *(▶ Codec settings)* | | |
| Sample rate | 44100 Hz | Audio sample rate for all encoding and decoding in this project |
| Bins per octave | 70 | Frequency resolution of the NSGT (higher = finer pitch detail, larger files) |
| Timestep | 11.0 ms/pixel | Milliseconds per horizontal pixel (time resolution) |
| Frequency scale | Variable-Q | Music-optimised zone-based bpo allocation (recommended); or Log for uniform bpo |
| TIFF variant | stripped | `stripped` for clips up to ~30 s; `tiled` for longer material |

All codec settings can be changed later via **Project → Project Settings**, though changing them after layers have been encoded will cause a mismatch on decode.

### Importing Audio Directly

- **Studio → Encode Audio → TIFF** (`Ctrl+E`) — converts an audio file to a standalone TIFF
- In a project, **Project → Add Layer** (`Ctrl+Shift+A`) — accepts audio, image, or existing TIFF files

---

## 5. Working with Single TIFF Files

Single-file mode lets you open one TIFF, edit it directly, and save it back. There are no layers; all editing targets the one open file.

### Typical Workflow

1. **Studio → Open TIFF** (`Ctrl+O`)
2. Select a paint tool and adjust brush parameters
3. Paint on the canvas; apply filters from the Filters menu
4. **Studio → Save TIFF** (`Ctrl+S`) to overwrite, or **Save TIFF As** (`Ctrl+Shift+S`) for a copy
5. **Studio → Decode TIFF → Audio** (`Ctrl+D`) to hear the result

### Saving

| Action | Shortcut | Effect |
|--------|----------|--------|
| Save TIFF | Ctrl+S | Overwrites the open file |
| Save TIFF As | Ctrl+Shift+S | Saves a copy under a new name |
| Save Active Layer TIFF | — | Writes only the active layer back to its TIFF (project mode) |

---

## 6. Working with Projects

A project holds multiple **layers**, each backed by its own TIFF file in the project's `media/` folder. Layers composite from bottom to top to form the final output.

### Creating and Populating a Project

1. **Project → New Project** (`Ctrl+Shift+N`)
2. **Project → Add Layer** (`Ctrl+Shift+A`) — import audio, image, or TIFF; or create an empty layer
3. Arrange layers in the Layers panel; paint and filter the active layer
4. **Project → Render Project** to composite all layers
5. **Project → Decode Rendered → WAV** (`Ctrl+Shift+D`) to export audio — or use **Project → Bake Project** (`Ctrl+Shift+B`) to do steps 4–5 plus re-import the result as a new layer in one command

### Project Folder Structure

```
MyProject.smsproj          ← JSON project file
MyProject/
  media/                   ← layer TIFFs and the rendered TIFF
  sources/                 ← copies of original audio/image files
  instruments/             ← Mind-Shot PNG/TIFF/JSON files
  kernels/                 ← saved custom convolution kernels
  ops/                     ← op log swatch data (NPY files for paste operations)
```

The entire folder is portable — move or copy `MyProject.smsproj` and `MyProject/` together.

### Rendering and Exporting

| Action | Shortcut | Description |
|--------|----------|-------------|
| Render Project | — | Composites all visible layers (blend modes, opacity, transforms) into a single TIFF stored as the Rendered layer |
| Decode Rendered → WAV | Ctrl+Shift+D | Decodes the Rendered layer to a WAV file in the project folder |
| **Bake Project** | **Ctrl+Shift+B** | One-click pipeline: render (if needed) → decode → re-encode → add layer (see below) |
| Export WAV | — | Exports the most recent render to a user-chosen WAV path |
| **Export Video…** | **Ctrl+Shift+O** | Renders the canvas as an animated video with synchronised audio — see §22 |
| Export Composite TIFF | — | Saves the current visual composite as a TIFF without modifying any layer |
| Save Active Layer TIFF | — | Writes the active layer's current pixel data back to its TIFF file |

### Volume Control

Two buttons in the playback toolbar let you inspect and adjust the output level before or after decoding:

**Check Volume** — reads the peak amplitude from the rendered composite (or the live composite if no render exists) and displays the result in dBFS next to the button. Values near 0 dBFS indicate the loudest frames are close to the ceiling; large negative values indicate headroom.

**Adjust…** — opens a small dialog with a single spin box (−60 to +20 dB, 0.5 dB steps). The value you enter is stored in the project file as `volume_gain_db` and applied every time the project is decoded to WAV (via Decode Rendered → WAV, Play, or loop preview).

| Value | Behaviour |
|-------|-----------|
| 0.0 dB (default) | Safety-normalization: the decoded audio is only scaled if the peak exceeds 1.0, and a warning is printed |
| Any non-zero value | The signal is scaled by the specified gain and hard-clipped to [−1, 1]; no safety normalization warning |

The volume gain is also written to the TIFF `ImageDescription` metadata tag (`SoundMind:VolumeGainDb`) so the setting travels with the file.

### Project Settings

**Project → Project Settings** lets you configure how new audio layers are encoded and how the rendered output is decoded. The "Discard future ops" behaviour is shown directly; all codec parameters are under a **▶ Codec settings** disclosure section.

| Setting | Default | Description |
|---------|---------|-------------|
| Discard future ops | Ask | Whether to confirm before discarding redo history when a new edit is made after undo |
| *(▶ Codec settings)* | | |
| Sample rate | 44100 Hz | Audio sample rate for encoding and decoding |
| Bins per octave | 75 | Frequency resolution of the NSGT. Higher = more frequency detail, larger files. Must match across encode/decode. |
| Timestep | 10.0 ms | Milliseconds per time-axis pixel. Determines the hop length. |
| Frequency scale | Variable-Q | Music-optimised zone-based bpo (recommended); or Log for uniform bpo across the range |
| TIFF variant | stripped | `stripped` for clips; `tiled` for long files (enables random-access seeking) |
| Tile size | 256 px | Tile dimension for the tiled variant |

### Resize Project

**Project → Resize Project** — destructively resizes the time axis of all layers by scaling or cropping. Useful for trimming the duration of the whole project or changing the pixel-per-ms density.

---

## 7. Paint Tools

Select a tool from the **Tools panel** at the left. Painting always targets the active layer. The brush cursor appears as a circle overlay on the canvas (sized to the brush diameter); the actual painted shape follows the selected **Tip** (see [Section 8](#8-brush-parameters--blend-modes)).

### Tool Panel Layout

Tools are grouped into four **categories**, each shown as one button in the horizontal tool bar. Clicking the button activates the last-used tool in that category. Clicking the small drop-down arrow opens a menu listing all tools within the category.

| Category | Tools |
|----------|-------|
| **Navigate** | Pan · Measure · Pick |
| **Paint** | Brush · Smudge · Stamp · Heal · Order · Chaos |
| **Select** | Select · Lasso · Wand |
| **Transform** | Warp · Curve · Fill |

### Tool Selector Overview

| Tool | Icon | Category | Shortcut | Description |
|------|------|----------|----------|-------------|
| **Pan** | ✥ | Navigate | — | Navigate the canvas; hold Space with any tool for temporary pan |
| **Measure** | ⇔ | Navigate | M | Click+drag to measure time, Hz, semitones, and pixel distance between two points |
| **Brush** | ✦ | Paint | — | Unified painting brush with Procedural / Mind-Shot / MIDI / MindGrain tip types |
| **Smudge** | 〰 | Paint | — | Pushes pixels along the stroke direction |
| **Stamp** | ⊕ | Paint | — | Clone brush; copies pixels from a source point to the cursor |
| **Heal** | ✚ | Paint | — | Healing brush — replace painted pixels by temporal interpolation from neighbours |
| **Order** | ◈ | Paint | — | Pushes the painted region toward spectral order (reduces H_n) |
| **Chaos** | ◉ | Paint | — | Pushes the painted region toward spectral chaos (raises H_n) |
| **Select** | ⬚ | Select | — | Rubber-band selection with transform handles, cut/copy/paste |
| **Lasso** | ✏ | Select | — | Freehand polygon selection; cut/copy/delete irregular regions |
| **Wand** | ⋆ | Select | — | Click to flood-fill-select pixels by amplitude similarity |
| **Warp** | ⇝ | Transform | — | Warp a selection along a Bézier curve in time or frequency |
| **Pick** | ⊙ | Navigate | — | Click to select a painted op; double-click to re-edit it |
| **Curve** | ∿ | Transform | — | Draw and edit a Bézier curve, then stamp along it |
| **Fill** | ▧ | Transform | — | Fill a rubber-band selection with a directional gradient |

---

### Pan Tool

Switches to scroll-hand navigation. Left-click and drag to pan. **Hold Space** with any other tool to pan temporarily without switching tools.

---

### Brush Tool

The unified painting tool that combines four tip types in one. When **Show Configuration** is checked in the Tools panel with the Brush tool active, a compact radio-button row (Procedural / Mind-Shot / MIDI / MindGrain) appears directly in the panel for switching tip types. Alternatively, open the tool configuration wizard (the **Configure** button) to select the tip type from the full-sized wizard step.

#### Tip: Procedural

The classic soft-edged airbrush. Paints a soft stamp at each mouse event; holding still or moving slowly accumulates opacity. The stamp blends the target R and G amplitude values into existing pixels, weighted by the brush kernel and opacity.

**Parameters:** Size, Opacity, Falloff, Tip shape, R, G, Blend mode. See [Section 8](#8-brush-parameters--blend-modes).

#### Tip: Mind-Shot

Stamps a Mind-Shot swatch as the brush. Select the Mind-Shot from the **Shot:** dropdown (populated from the project's instruments folder). Two sub-modes:

- **Kernel mode** (greyscale PNG only, no TIFF): the Mind-Shot provides a float32 alpha mask; the tool's R/G colour values are blended through it.
- **Swatch mode** (has a paired `.tiff` file): the Mind-Shot stores up to four spectrogram pages (amplitude L/R + phase L/R) that can be stamped directly.

Resize the stamp with the **Width** and **Height** spinboxes; enable **Constrain proportions** to lock the aspect ratio.

**Amplitude pages (0 & 1):**
- **Keep Mind-Shot amplitude** — blits the Mind-Shot's own amplitude data at the current opacity.
- **Use colour setting** — derives a shape mask from the Mind-Shot's amplitude envelope and fills it with the tool's R/G values (same as the Procedural tip, but shaped by the Mind-Shot).

**Phase pages (2 & 3):**
- **Ignore (preserve existing)** *(default)* — phase pages are not written; any existing phase data under the stamp is left untouched.
- **Keep Mind-Shot phase** — blits the Mind-Shot's phase data at the current opacity.
- **Use colour setting** — fills phase with the tool's phase colour values, shaped by the amplitude envelope.

#### Tip: MIDI

Paints a synthesised MIDI note at the clicked position, using the GM program profiles to determine harmonics and envelope.

| Control | Description |
|---------|-------------|
| **Program** | GM program number (0–127) or 128 (Drums). Names follow the General MIDI standard. |
| **Velocity** | Note loudness, 1–127. Mapped logarithmically to dBFS amplitude. |
| **Duration** | Note length in spectrogram columns. |
| **Reload MIDI Programs** | Re-reads the MIDI profiles JSON and re-renders all MIDI operations in the project. Use this after editing the profiles to hear the updated sound. |

When you click Apply, all MIDI notes painted in the current session are committed as a **MidiBrushOp** — a semantic op that stores the note events so they can be re-rendered when profiles change.

> **Tip:** use **Project → MIDI Instrument Editor…** to adjust any instrument's harmonics, ADSR envelope, noise bands, and sound source override interactively, preview the result as a spectrogram with audio playback, and save changes to the project. See [Section 35 — MIDI Instrument Editor](#35-midi-instrument-editor).

#### Tip: MindGrain

Uses a **MindGrain layer** as a live granular synthesis source. Select the source layer from the **Source:** dropdown (lists all MindGrain layers in the project). Each stamp:

1. Looks up the selected MindGrain layer's buffer (the composited result of all layers beneath it, cropped to the configured column slice).
2. Extracts a grain of `grain_duration_cols` width from the scan position.
3. Applies direction, pitch shift, time stretch, and envelope from the MindGrain defaults or Modulation panel overrides.
4. Stamps the grain at the current cursor position.

Add a MindGrain layer from the Layers panel (click the **+** button and choose **Add MindGrain…**). The dialog lets you configure the slice bounds and all default granular parameters. The buffer rebuilds lazily whenever any layer beneath it changes.

#### Modulation section (all tip types)

The collapsible **Modulation** section in the Brush panel applies to all tip types:

| Control | Description |
|---------|-------------|
| **Scatter X / Y** | Random position jitter (in px) applied per stamp |
| **Scatter axis** | Constrain jitter to "x", "y", or "both" |
| **Size rand** | Scale stamp size randomly by ±N% (0 = off, 1 = ±100%) |
| **Density** | Stamps per pixel of stroke travel (1 = contiguous; > 1 = denser) |
| **Density rand** | Randomise density by ±N fraction per stamp |
| **Envelope** | Amplitude window across stamp width: `flat`, `hann`, `triangle`, `fade_in`, `fade_out`, `ramp_up`, `ramp_down` |
| **Direction** | `forward`, `reverse`, or `ping_pong` (alternates each stamp within a stroke) |
| **Pitch shift** | Vertical row offset in semitones (positive = up in row index) |
| **Pitch rand** | Extra random pitch offset ± this many semitones |
| **Time stretch** | Horizontal scale of the stamp (0.1× to 8×) |

##### Pitch expression controls

| Control | Description |
|---------|-------------|
| **Detune (±¢)** | Per-stamp random ¢-offset drawn from a ±N¢ range. Models gamelan ombak detuning (10–30 ¢), ensemble heterophony, or didgeridoo wobble. Applied after quantise, so the core pitch stays on the grid while the ornament floats around it. |
| **Vibrato depth** | Amplitude of sinusoidal pitch oscillation in ¢. Zero = off. |
| **Vibrato rate** | Oscillation speed in Hz (0.1–20). Tied to absolute column position so replaying the same position gives the same oscillation phase. |
| **Bend from prev** | When checked, each stamp glides from the previous stamp's row position toward the current target row. Creates legato portamento between notes. Reset at the start of each new stroke. |
| **Bend time** | Duration of the glide in ms. A shorter value means a quick snap; longer values create slow, languid slides. |
| **Pitch quantise** | Snaps each stamp's row to the nearest active frequency grid line before painting. Requires the Overlay frequency grid to be enabled. Useful with musical tradition presets — any loaded frequency preset automatically provides the quantise grid. Disable for free-contour traditions (Persian avaz, Aboriginal vocal gesture, Overtone Singing). |

**Processing order within each stamp:** scatter → pitch shift → bend from prev → pitch quantise → (core position recorded for next bend) → detune → vibrato. Detune and vibrato add subtle ornament on top of an already-quantised or glided core pitch.

---

### Smudge Tool

Pushes pixel values along the stroke direction. At each drag event, pixels are sampled from one step behind the cursor and mixed into the current position. This stretches, spreads, and blends sound in time and frequency.

- Smudge activates on drag only (not press).
- The smear direction follows the instantaneous movement of the stroke.
- **Parameters:** Size, Opacity, Falloff, Tip.
- Blend mode is not applied to the Smudge tool; it always blends by averaging.

**Use cases:** Stretching a resonance over time; smearing transients to soften them; blending the boundary between two regions.

---

### Stamp Tool (Clone)

Copies pixels from a source point to the current brush position.

**Workflow:**
1. **Alt+click** to set the source point. A blue crosshair marks it.
2. **Left-click or drag** to paint. Pixels are sampled from a corresponding offset relative to the source.
3. As you drag, the source point tracks with the stroke — if you move right by 20 px, the sample point also moves right by 20 px.

**Cross-layer stamping:** In the Tools panel, use the **Source layer** dropdown to sample from a different layer than the one you are painting onto. Changing the source layer clears the current source point.

**Parameters:** Size, Opacity, Falloff, Tip, Blend mode.

---

### Curve Tool

Draws a segmented cubic Bézier curve and stamps brush marks along its length. Ideal for melodic lines, precise spectral contours, and rhythmically-timed patterns.

The cursor outline and the stamp-dot preview along the curve both reflect the **selected brush tip shape** (square, diamond, line, etc.), so the preview matches what will be painted.

#### Phase 1 — Placing Nodes

- **Left-click** to place a node. A preview shows the curve and stamp dots as you move.
- **Right-click** or **double-click** to stop placing and enter edit mode.
- The **corner/smooth toggle button** (see Node Controls below) sets the type of node placed on each click *before* you click — no need to convert nodes retroactively.

#### Phase 2 — Editing

| Action | Effect |
|--------|--------|
| Drag a node | Move it; handles follow |
| Drag a handle | Adjust curvature. Opposite handle mirrors by default (smooth) |
| Alt+drag a handle | Move one handle independently, creating a corner |
| Double-click a segment | Insert a new node at that position |
| Double-click a node | Delete that node |
| Click a node | Select it and show node controls in the Tools panel |
| Delete key | Delete the selected node |

#### Node Controls

The **Corner ↔ Smooth** button in the Curve section of the Tools panel serves two roles:

| State | Button label | Click action |
|-------|-------------|--------------|
| No node selected | **Place: Corner** or **Place: Smooth** | Flips the placement default — subsequently placed nodes use the new type |
| Node selected | **→ Smooth** or **→ Corner** | Converts that node to the opposite type and updates the placement default to match |

**Smooth** nodes have Bézier handles: dragging them curves the path and the opposite handle mirrors to maintain a smooth tangent. **Corner** nodes have no handles; the path meets at a sharp kink.

- **Delete Node** — removes the selected node.

#### Curve Gradient

A full gradient editor (see [Section 9](#9-the-gradient-editor)) controls color and opacity along the curve. t=0 is the start of the curve; t=1 is the end.

#### Curve Options

| Option | Description |
|--------|-------------|
| **Paint mode** | **Airbrush** — stamps the selected procedural brush tip. **Mind-Shot** — stamps the active Mind-Shot swatch. **MIDI** — stamps synthesised notes using the active MIDI program. |
| **Interval value** | Numeric spacing between stamps |
| **Interval unit** | **px** — spacing in image pixels along the curve arc. **ms** — spacing in milliseconds of audio time (converts to pixels using the layer's timestep). **hz** — spacing in Hz (converts to pixel rows using the frequency scale). |
| **Interval axis** | **Curve** — stamps at arc-length intervals along the Bézier path. **Time** — stamps at regular time-axis (horizontal pixel) intervals. **Frequency** — stamps at regular frequency-axis (vertical pixel) intervals. |

#### Applying

- **Apply** — stamps along the entire curve and records the result as a single undoable op in the op log.
- **Reset** — discards the current path and starts fresh.

---

### Fill Gradient Tool

Fills a rectangular selection with a directional gradient. Use it for fades, silence regions, or color washes across a time-frequency area.

#### Workflow

1. **Drag** on the canvas to draw the selection rectangle.
2. Configure the **gradient** in the tool panel.
3. Set the **direction angle** (0–360°). A compass arrow previews the direction.
4. **Apply** to fill. The selection stays active so you can tweak and re-apply.
5. **Clear** to dismiss the selection.

#### Direction Angles

| Angle | Flow direction |
|-------|---------------|
| 0° | → Right (increasing time): t=0 at left, t=1 at right |
| 90° | ↓ Down (decreasing frequency): t=0 at top, t=1 at bottom |
| 180° | ← Left (decreasing time): t=0 at right, t=1 at left |
| 270° | ↑ Up (increasing frequency): t=0 at bottom, t=1 at top |

---

### Mind-Shot Tool

Stamps a Mind-Shot swatch as the brush. The Mind-Shot's pixel values form the alpha mask that controls how strongly the brush colour is applied at each position.

- **Instr Width / Instr Height** — stamp footprint in image pixels. The canvas cursor appears as a matching rectangle.
- The active Mind-Shot is selected from the Mind-Shot Editor (see [Section 17](#17-the-mind-shot-editor)).
- Mind-Shots have two modes:
  - **Kernel mode** — a float32 grayscale alpha mask. The R/G values from the brush parameters are applied through this mask.
  - **Swatch mode** — a uint16 (H, W, 2) spectrogram patch. The raw pixel data of the swatch is blitted directly, bypassing R/G values. Useful for custom spectral shapes that can't be described with a single amplitude level.
- **Parameters:** Instr Width, Instr Height, Opacity, Blend mode.

---

### Select Tool

Draws a rectangular selection with eight resize handles and a rotate knob. Supports cut, copy, paste, and delete operations. Pasted regions float with perspective-transform handles.

The Select tool generates a **pixel-accurate mask** from its current quad corners. Whether the marquee is axis-aligned, rotated, skewed, or perspective-warped, every pixel precisely inside the quad is selected. This mask is combined with other selection operations (Lasso, Wand) using the [boolean modifiers](#boolean-selection-modifiers) described below.

#### Drawing a Selection

Left-click and drag to draw the rectangle. Eight handles appear at the corners and edge midpoints, plus a rotate knob above the top edge.

#### Transform Handles

| Handle | Modifier | Action |
|--------|----------|--------|
| Corner (TL/TR/BR/BL) | — | Resize from that corner; opposite corner stays fixed |
| Corner | Shift | Proportional resize, anchored to opposite corner |
| Corner | Alt | Move only the dragged corner (perspective/skew) |
| Edge (T/R/B/L) | — | Resize along one axis |
| Edge | Shift | Skew along the edge direction |
| Inside selection | — | Move the entire selection |
| Rotate knob | — | Rotate around the centre |

> The selection transform is non-destructive until you cut, delete, or commit a paste. Dragging handles repositions the marquee only; pixels are unchanged.

#### Cut, Copy, Paste, Delete

| Action | Shortcut | Effect |
|--------|----------|--------|
| Copy | Ctrl+C | Copies the selected region to the internal clipboard (all 4 pages, full 16-bit depth) |
| Cut | Ctrl+X | Copies to clipboard and fills the selection area with silence |
| Delete | Delete | Fills the selection with silence without copying |
| Paste | Ctrl+V | Enters floating-paste mode |

The clipboard is internal to the session (not the OS clipboard).

#### Floating Paste

After pressing `Ctrl+V`, the copied region appears as a floating overlay with the same transform handles.

- **Drag** to reposition.
- **Drag handles** to resize, skew, rotate, or apply perspective.
- **Paste opacity** and **paste blend mode** are set in the Tools panel and control how the pasted pixels merge with the layer underneath.
- **Enter** or **double-click** to commit. The result is recorded as a `PasteOp` in the op log and can be re-edited with the Pick tool.
- **Escape** cancels without modifying the layer.

You can paste again (`Ctrl+V`) after committing to place another copy.

#### Paste Transforms

While a paste preview is active the **Paste Transforms** toolbar appears above the canvas. Four additional transforms are available via the toolbar buttons *or* keyboard shortcuts:

| Action | Key | Button |
|--------|-----|--------|
| Flip horizontal (left↔right) | `H` | ↔ Flip H |
| Flip vertical (top↔bottom) | `V` | ↕ Flip V |
| Rotate 90° clockwise | `]` | ↻ 90° CW |
| Rotate 90° counter-clockwise | `[` | ↺ 90° CCW |

Transforms are applied to the floating image immediately and can be chained freely. The toolbar disappears when paste mode ends (commit or Escape). The same actions also appear in the **Edit** menu.

#### Show Op Geometry

The **Show op geometry** checkbox appears at the bottom of the Tools panel and is available regardless of which tool is active.

When checked, the studio draws dashed outlines over the active layer showing the footprint of every recorded operation:

| Op type | Overlay colour | Shape |
|---------|---------------|-------|
| Fill Gradient | Orange | Bounding rectangle |
| Cut / Delete | Red | Bounding rectangle |
| Filter | Sky-blue | Bounding rectangle (or a viewport border for whole-canvas filters) |
| Warp | Purple | Bounding rectangle + Bézier path |
| Paste | Green | Quadrilateral (four corners of the pasted region) |
| Curve | Yellow | Bézier path |

The checkbox is automatically checked when you switch to the **Select**, **Lasso**, or **Wand** tool, but you can uncheck it at any time. The overlay updates whenever an operation is applied or a different layer becomes active.

#### Working with op geometry overlays

**Right-click** on any visible geometry shape to open a context menu with two actions:

| Action | Effect |
|--------|--------|
| **Duplicate** | Re-applies the operation on top of the current layer content and appends a new copy to the op log. Fully undoable. |
| **Delete** | Removes the operation from the op log and rebuilds the layer by replaying the remaining operations in order. Shows a confirmation prompt. **This cannot be undone.** |

> **Note on Delete:** because audio-sourced layers store their encoded audio as the initial canvas, deleting an op rebuilds from that base. The result is correct for layers where the op log captures all edits made after encoding.

---

### Boolean Selection Modifiers

Every selection tool (Select, Lasso, Wand) supports modifier keys that combine the new selection with the currently active one:

| Modifier held at drag/click | Operation | Effect |
|-----------------------------|-----------|--------|
| *(none)* | Replace | Replaces the existing selection |
| **Shift** | Union | Adds the new area to the existing selection |
| **Alt** | Subtract | Removes the new area from the existing selection |
| **Shift + Alt** | Intersect | Keeps only pixels that are in both the old and new selections |

The combined result is a pixel-accurate boolean mask. You can freely mix tool types — for example, draw a rectangle with the Select tool, then Shift+lasso a curved region to add it, then Alt+click with the Wand to punch out a frequency band.

---

### Lasso Tool

Draws a freehand polygon selection. Unlike the rectangular Select tool, the Lasso lets you trace an irregular boundary around any region — for example, the body of a single note isolated from its neighbours, or a diagonal frequency smear.

#### Drawing a Lasso Selection

1. Click and **hold** the left mouse button, then drag to trace the outline.
2. Release the button to close the polygon. The traced path is rasterised into a pixel mask and highlighted with a dashed blue outline.
3. **Escape** clears the selection.

Hold **Shift**, **Alt**, or **Shift+Alt** during the drag to combine with the existing selection (see [Boolean Selection Modifiers](#boolean-selection-modifiers)).

#### Cut, Copy, Delete

| Action | Shortcut | Effect |
|--------|----------|--------|
| Copy | Ctrl+C | Copies all selected pixels (non-selected pixels in the bounding box are zeroed) |
| Cut | Ctrl+X | Copies then silences the selected pixels |
| Delete | Delete | Silences the selected pixels without copying |

> Paste (Ctrl+V) is not available after a Lasso cut/copy — use the **Select** tool if you need to paste with transform handles.

---

### Magic Wand Tool

Selects pixels by spectral amplitude similarity. Click anywhere on the canvas to grow a selection outward from that seed pixel, including all connected pixels whose amplitude is within the configured tolerance. Useful for isolating a contiguous frequency blob or a region of uniform loudness.

Hold **Shift**, **Alt**, or **Shift+Alt** while clicking to combine the wand selection with the existing one (see [Boolean Selection Modifiers](#boolean-selection-modifiers)).

#### Parameters (Tools panel)

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Tolerance** | 0.05 | Maximum amplitude distance from the seed pixel (0–1 scale) for a pixel to be included |
| **Mode** | Contiguous | How the fill spreads (see table below) |
| **Min amp** | 0.0 | Exclude pixels whose amplitude is below this threshold |

#### Fill Modes

| Mode | Behaviour |
|------|-----------|
| **Contiguous** | 4-connected flood fill — grows from the seed and stops at amplitude discontinuities |
| **Global** | Selects all pixels anywhere in the image that match the tolerance, regardless of connectivity |
| **Freq-locked** | Fills only along the same frequency row (horizontal) as the seed |
| **Time-locked** | Fills only along the same time column (vertical) as the seed |

The selection is shown as a dashed gold bounding box. **Escape** clears it. Cut, Copy, and Delete work the same as for the Lasso tool.

#### Content-aware Selection (API)

Three additional factory methods are available on `SelectionMask` for programmatic or scripted selection of musically meaningful regions:

| Method | Description |
|--------|-------------|
| `from_envelope_peaks(image_data, seed_x, seed_y, …)` | Selects all time columns whose column-wise peak amplitude meets the threshold around the seed, plus all rows in those columns above the threshold. Best for isolating a single transient or note onset. |
| `from_harmonic_region(image_data, seed_x, seed_y, …)` | Horizontal flood-fill on the fundamental row (seed_y) and each harmonic overtone row (2×, 3×, … 32×) derived from the NSGT log-frequency spacing. Best for selecting a pitched note together with all its harmonics. |
| `from_connected_blob(image_data, seed_x, seed_y, …)` | 4-connected BFS on raw amplitude pages selecting all pixels above a minimum amplitude that are connected to the seed. Equivalent to Contiguous wand mode on the underlying pixel data. |

These are not yet exposed in the Tools panel; they are available via the Python API for custom selection scripts.

---

### Freeze Tool

The Spectral Freeze brush copies a single vertical slice (time column) of the spectrogram and paints it repeatedly wherever you drag, effectively holding that moment in time. This is useful for:

- Extending a sustained tone or pad beyond its natural duration.
- Replacing a noisy or transient frame with the cleaner texture from a neighbouring frame.
- Creating a static drone from a single moment of a rich recording.

#### Workflow

1. **Alt+click** on the time column you want to freeze. A vertical cyan line marks the source column.
2. **Left-drag** across the canvas to paint the frozen slice. The brush opacity controls the blend strength; dragging slowly over the same area accumulates the effect.

**Parameters:** Size, Opacity, Falloff, Tip (standard brush controls).

> The Freeze brush modifies pixels directly and is recorded in the undo history like any other brush stroke.

---

### Heal Tool

The Healing Brush replaces each pixel under the brush with a weighted average of the pixels in the same frequency row but from the surrounding time columns. This makes damaged, noisy, or over-painted spectral regions blend seamlessly with their temporal context — analogous to the healing brush in image editors like Photoshop.

**Use cases:**
- Remove a click or dropout from an otherwise clean recording.
- Erase an accidental paint stroke and restore the original spectral texture.
- Smooth a boundary between a painted region and the surrounding audio.

#### How it works

For each pixel (row, column) under the brush:
1. Sample a few columns to the left and right (the sample radius scales with brush size: `size ÷ 4`).
2. Compute a distance-weighted average of those neighbour columns at the same frequency row.
3. Blend the average into the pixel using the brush opacity and the kernel weight at that position.

**Parameters:** Size, Opacity, Falloff, Tip. The larger the brush, the more neighbour columns are sampled.

> The Heal brush works best at moderate sizes (16–40 px) and full opacity. Very large sizes may look at distant temporal context and produce unexpected results if the surrounding audio is very different.

---

### Order Tool

The Order brush locally sculpts a region toward **spectral order** — concentrating energy into fewer, stronger frequency bins and making the phase more coherent. It is the order-direction companion to [Chaos](#chaos-tool).

**Use cases:**
- Push an overly noisy or disordered region (amber zone in the Criticality Meter overlay) toward the critical green band.
- Strengthen the harmonic structure of a weak pitched passage.
- Clean up spectral splatter left by a Chaos stroke without using Heal.

#### How it works

For each brush stamp, the brush kernel (selected tip shape + falloff) defines a 2D weight field over the footprint. Then, for every **column** within the footprint:

1. **Amplitude (pages 0 and 1):** Compute the mean amplitude µ across all frequency rows in that column slice. Expand contrast around µ:
   ```
   result[row] = amp[row] + (amp[row] − µ) × kernel_weight[row] × opacity
   ```
   Rows above µ move further above; rows below µ move further below. Clipped to [0, 65535]. This concentrates the column's energy distribution, reducing normalised Shannon entropy H_n.

2. **Phase (pages 2 and 3):** Compute the circular mean of the column's phase values, then pull each phase sample toward it by the kernel weight × opacity. Uses circular arithmetic to handle the phase-wrap boundary correctly.

The blend is smooth at the brush edge (controlled by Falloff) — only the very centre of a hard brush achieves full effect.

#### Controls

| Control | Effect |
|---------|--------|
| **Size** | Brush footprint diameter |
| **Opacity** | Effect strength (0 = no change, 1 = full contrast expansion) |
| **Falloff** | Edge softness (0 = hard disc, 1 = full gradient to 0) |
| **Tip** | Any of the 48 tip shapes |
| **Channel** | `r > 0` → process L amplitude + phase; `g > 0` → process R amplitude + phase |

**Blend mode** is bypassed — Order directly applies a mathematical transform rather than painting a colour.

**Tip:** Run the Criticality Meter first (`Ctrl+Alt+C`) to see the zone overlay. Switching to Order auto-enables the overlay so amber (too ordered) and green (critical) zones are visible while painting.

---

### Chaos Tool

The Chaos brush locally sculpts a region toward **spectral chaos** — dispersing energy more broadly across frequency bins and randomising phase relationships. It is the chaos-direction companion to [Order](#order-tool).

**Use cases:**
- Break up an overly rigid, static region (violet zone in the Criticality Meter overlay) and pull it toward the critical green band.
- Add organic texture, grain, or variation to a synthesised or over-processed passage.
- Introduce controlled noise in specific frequency bands without affecting the surrounding audio.

#### How it works

A seeded random number generator is initialised at the start of each stroke (`on_press`) with a 32-bit seed stored in the op log, so every **Remaster replay produces byte-identical results**.

For every column within the brush footprint:

1. **Amplitude (pages 0 and 1):** Add seeded uniform noise scaled by the kernel weight and opacity:
   ```
   result[row] = amp[row] + noise[row] × kernel_weight[row] × MAX_PX
   ```
   where `noise[row] ∈ (−1, +1)` and `MAX_PX = 65535`. Clipped to [0, 65535].

2. **Phase (pages 2 and 3):** Add bounded seeded angular jitter:
   ```
   φ_new = wrap(φ + jitter × kernel_weight)
   ```
   where `jitter ∈ (−π, +π)` and `wrap` keeps the result in [−π, +π].

#### Controls

| Control | Effect |
|---------|--------|
| **Size** | Brush footprint diameter |
| **Opacity** | Noise amplitude (0 = no change, 1 = full ±65535 noise range) |
| **Falloff** | Edge softness |
| **Tip** | Any of the 48 tip shapes |
| **Channel** | `r > 0` → L pages; `g > 0` → R pages |

**Blend mode** is bypassed.

> **Low-opacity strokes (10–30%) work well** — full opacity adds very strong noise; for subtle texture injection, use small brush sizes and low opacity with multiple slow passes.

---

### Avalanche Tool

The Avalanche brush applies a normal paint stamp then runs a **BTW sandpile cascade** — a Self-Organised Criticality model — across the brush footprint. The cascade propagates energy (or corrects entropy) through neighbouring columns until the system falls below its threshold or the maximum iteration count is reached.

**Use cases:**
- Create natural-looking amplitude diffusion from a dense painted region outward.
- Drive a region toward the critical H_n band using entropy-mode self-correction.
- Simulate avalanche-like spectral events — sudden redistributions of energy triggered by painting a single column.

#### Mode: Amplitude

Load for each column = mean amplitude / max amplitude. When load exceeds the **Threshold**:

```
excess   = load − threshold
shed     = excess × topple_fraction × MAX_PX
```

Shed amplitude is removed from the toppling column and distributed into its left and/or right neighbour, weighted proportionally to each row's existing amplitude in that neighbour (preserving spectral shape). Amplitude that would flow beyond the radius boundary is dissipated (lost).

#### Mode: Entropy

Each column in the cascade radius is tested against the H_n critical band (H_n Low / H_n High thresholds):

- **H_n < low** (too ordered) → inject noise scaled by the Topple fraction → raises H_n toward the band.
- **H_n > high** (too chaotic) → contrast expansion toward the column mean → lowers H_n toward the band.

No amplitude is transferred between columns; each corrects itself independently. Direction restricts which columns in the radius are processed.

#### Controls

| Control | Description |
|---------|-------------|
| **Mode** | Amplitude (load-driven shedding) or Entropy (H_n self-correction) |
| **Direction** | Symmetric — shed/correct in both directions; Rightward — only columns at c₀ and to the right; Leftward — only columns at c₀ and to the left |
| **Threshold** | Load threshold above which a column topples (Amplitude mode) |
| **Topple** | Fraction of excess load shed per iteration |
| **Max radius** | Maximum column distance from stroke centre included in the cascade |
| **Max iters** | Maximum cascade iterations per stamp |
| **H_n Low / High** | Critical band bounds (Entropy mode only) |
| **Slow cascade** | Animate cascade frames as red column overlays (80 ms per frame) after each stroke |
| **Size / Opacity / Falloff / Tip / Channel** | Standard brush controls |

**Blend mode** is bypassed — the cascade directly modifies amplitude values.

> **Tip:** Amplitude mode with a high Threshold (0.80+) and wide brush creates sweeping energy redistribution. Entropy mode with a narrow H_n band produces subtle, self-organising texture adjustments. Combine both by painting with Amplitude mode, then refining with Entropy mode at lower opacity.

---

### Warp Tool

Warps pixels by displacing them along a Bézier curve. Used to bend a sound in time or frequency — for example, to add a pitch vibrato, a rhythmic groove, or a stretching effect.

#### Controls (Tools panel)

| Control | Description |
|---------|-------------|
| **Region** | Dropdown listing `⟨Whole layer⟩` (default) and any saved rect ops from the op log. `⟨Whole layer⟩` applies the warp across the entire image; a specific rect op confines the effect to that region. A selection drawn with the Select tool is also offered here automatically when you switch to the Warp tool. |
| **Curve** | The Bézier path to warp along (click in the canvas overlay to select a logged curve or the active live curve). |
| **Axis** | **Frequency** — each time column is shifted vertically by the curve's y-deflection (pitch bends). **Time** — each frequency row is shifted horizontally by the curve's x-deflection (timing changes). |
| **Mode** | **Displace** — every line in the region shifts by the full curve deflection (current position follows the curve exactly). **Stretch** — the first line of the region is unaffected; the last line is displaced by the full curve deflection; all lines in between are scaled by a linear ramp. The result stretches the image progressively, with the amount of stretching determined by the curve shape. |
| **Apply** | Warps the region by bilinear interpolation and records a `WarpOp` in the op log. |
| **Clear All** | Resets the region to `⟨Whole layer⟩` and deselects the curve. |

#### Workflow

1. *(Optional)* Draw a selection with the **Select** tool to confine the warp to a specific region. If you skip this step the warp applies to the whole layer.
2. Switch to the **Warp** tool. The existing selection (if any) is automatically offered in the Region dropdown.
3. Draw a Bézier path in the canvas (same node-placement controls as the Curve tool), or click a previously logged curve in the overlay.
4. Set the **Axis** and **Mode** (see table above).
5. Click **Apply** — the region is warped by bilinear interpolation and a `WarpOp` is recorded in the op log.

#### Displace vs Stretch

| | Displace | Stretch |
|-|----------|---------|
| First line of region | Shifted by curve deflection at that position | Unaffected (0 displacement) |
| Last line of region | Shifted by curve deflection at that position | Shifted by full curve deflection |
| Lines in between | Each shifted by its own curve value | Linearly interpolated between 0 and full deflection |
| Typical use | Uniform pitch bends, vibrato, timing offsets | Gradual time-stretch, pitch-glide, smear effects |

---

### Text Tool

Places a text annotation on the active layer as a spectrogram element. The text is rendered into the amplitude pages of the layer using PIL — it reads back as visible frequency energy in the decoded audio.

#### Controls (Tools panel)

| Control | Description |
|---------|-------------|
| **Text** | The string to render. Multi-line text is supported. |
| **Font** | Font family name. Common system fonts are listed; type to enter a custom name. |
| **Size** | Font size in points (6–512). |
| **Bold / Italic** | Style toggles. |
| **Color R** | Page-0 amplitude intensity (0.0–1.0). Controls energy in the lower-frequency channel. |
| **Color G** | Page-1 amplitude intensity (0.0–1.0). Controls energy in the higher-frequency channel. |
| **Opacity** | Blend opacity (0.0–1.0). |
| **Blend** | Blend mode used when compositing the text onto the layer. Defaults to **Screen** so the text adds rather than replaces existing spectral content. |
| **Apply** | Commit the text to the layer pixel data and record a `TextOp` in the op log. |
| **Reset** | Discard the current placement without committing. |

#### Workflow

1. Select the **Text** tool from the Transform category in the toolbar.
2. Type your text in the **Text** field and adjust font/style/colour settings.
3. **Click** on the canvas to place a text box automatically sized to the rendered text, centred at the click point. Or **click-drag** to draw a custom bounding box.
4. A live preview appears as a yellow-outlined overlay with the text rendered inside it.
5. Drag the **eight edge/corner handles** to resize or skew the text box. Drag the **yellow circular knob** above the top edge to rotate it. Drag inside the box to move it.
6. Adjust text/style settings at any time — the preview updates immediately.
7. Click **Apply** to blit the text into the layer and record the operation. Click **Reset** (or switch to another tool) to discard without committing.

> **Tip:** Use **Screen** blend mode (default) to add text energy on top of existing sound. Use **Normal** to replace. Adjust **Color R / G** to place the text energy primarily in the bass (R) or treble (G) range.

---

### Pick Tool

Click anywhere on the canvas to select the topmost painted operation whose footprint covers that point. Useful for identifying what was painted where and for launching re-edit mode.

- **Click** — selects the topmost op under the cursor. If multiple ops overlap, repeated clicks cycle through them from newest to oldest.
- **Double-click** — opens **re-edit mode** for the selected op (see [Section 22](#22-re-edit-mode)).
- **Escape** — deselects.
- The selected op is highlighted on the canvas with a label showing its type and timestamp.

---

### Measure Tool

Measures the distance between any two points on the spectrogram canvas. Designed for checking timing, interval, and frequency relationships without interrupting your editing flow.

**How to use:**

1. Select **Measure** from the Navigate category in the Tools panel (or press **M**).
2. **Click and drag** from point A to point B. A line is drawn between the two endpoints as you drag.
3. Release the mouse button to finalise. The readout box near the midpoint of the line shows:

| Dimension | Values shown |
|-----------|-------------|
| Horizontal (time) | Elapsed time as `M:SS.mmm` · Beats and bars (if a BPM grid is configured) · Pixel distance |
| Vertical (frequency) | Hz at point A and B · Interval in semitones and tones · Pixel distance |

The measurement persists on the canvas until you start a new drag. To clear it, switch away from the Measure tool and back.

**Tip:** If a BPM timing grid is configured (in the Overlay Settings dock), the beat and bar readout will tell you the exact rhythmic distance between two events — useful for checking phrase lengths or loop boundaries.

---

## 8. Brush Parameters & Blend Modes

The following controls appear in the Tools panel whenever a brush-based tool is active (Airbrush, Smudge, Stamp, Mind-Shot, Curve, Fill):

| Parameter | Range | Description |
|-----------|-------|-------------|
| **Size** | 1–500 px | Brush diameter in image pixels |
| **Opacity** | 0.0–1.0 | How strongly the brush blends with existing pixels per event. 0 = no effect; 1 = full replacement (subject to blend mode) |
| **Falloff** | 0.0–1.0 | Edge softness. 0 = hard geometric edge; 1 = full distance-field gradient from the shape interior to its boundary. Has no effect at Size 1. |
| **R (Page 0)** | 0.0–1.0 | Target intensity for the left-channel amplitude page. 0 = silence; 1 = 0 dB (full scale) |
| **G (Page 1)** | 0.0–1.0 | Target intensity for the right-channel amplitude page |
| **Tip** | (dropdown) | Brush tip shape — see [Brush Tip Shapes](#brush-tip-shapes) below |
| **Blend mode** | (see below) | How the brush blends with existing pixels |

> **Tips:** To paint silence, set R=0, G=0, Opacity=1, Blend mode=Normal. To paint only the left channel, set G=0 and G opacity=0 (in the gradient editor when using Curve/Fill).

### Brush Tip Shapes

The **Tip** dropdown selects the geometric shape of the brush kernel. The kernel is computed at the current **Size** and then stamped at every paint event. All 48 shapes respect **Falloff** (distance-field feathering from the shape boundary inward). The **Circle** tip uses a special radial-gradient falloff that fades linearly from the center outward, matching the behaviour of traditional round brushes.

**Basic shapes** — simple closed fills:

| Tip | Description |
|-----|-------------|
| **Circle** | Standard filled circle (default) |
| **Square** | Axis-aligned filled square |
| **Diamond** | Square rotated 45° (L¹-norm disc) |
| **Triangle ▲** | Equilateral triangle, tip pointing up, inscribed in the bounding circle |
| **Ring ○** | Unfilled circle outline (~20% of radius thick) |

**Line shapes** — lines and line compounds through the brush centre:

| Tip | Description |
|-----|-------------|
| **Line — vertical** | Single vertical stroke |
| **Line — horizontal** | Single horizontal stroke |
| **Line — 45°** | Diagonal stroke, upper-left to lower-right |
| **Line — 135°** | Diagonal stroke, upper-right to lower-left |
| **Plus (+)** | Vertical + horizontal lines combined |
| **Cross (×)** | Both diagonal lines combined |
| **Asterisk — 6 pt ✳** | Six lines at 0°, 60°, 120° and their opposite directions |
| **Asterisk — 5 pt** | Five rays (half-lines) at 72° spacing, starting from straight up |
| **Asterisk — 8 pt** | Four full lines at 0°, 45°, 90°, 135° (plus + cross combined, 8 endpoints) |

**Corners and arcs** — open L-shapes, thin arcs, and filled quarter-sectors:

| Tip | Description |
|-----|-------------|
| **Corner — NE (└)** | Two half-lines meeting at centre: one going up, one going right |
| **Corner — NW (┘)** | Up + left |
| **Corner — SE (┌)** | Down + right |
| **Corner — SW (┐)** | Down + left |
| **Arc — NE** | Thin quarter-circle arc in the upper-right quadrant |
| **Arc — NW** | Thin quarter-circle arc in the upper-left quadrant |
| **Arc — SE** | Thin quarter-circle arc in the lower-right quadrant |
| **Arc — SW** | Thin quarter-circle arc in the lower-left quadrant |
| **Filled corner — NE** | Curved right-angle shape: two straight edges meeting at the bounding-box corner (up + right), connected by a concave arc — the bounding-box quadrant outside the inscribed circle |
| **Filled corner — NW** | Same, upper-left corner (up + left) |
| **Filled corner — SE** | Same, lower-right corner (down + right) |
| **Filled corner — SW** | Same, lower-left corner (down + left) |

**Right triangles** — half of the bounding square cut along a diagonal:

| Tip | Description |
|-----|-------------|
| **Right triangle — NE** | Right triangle occupying the upper-right half of the bounding square (above the TL→BR diagonal) |
| **Right triangle — NW** | Upper-left half (above the TR→BL diagonal) |
| **Right triangle — SE** | Lower-right half (below the TR→BL diagonal) |
| **Right triangle — SW** | Lower-left half (below the TL→BR diagonal) |

**V shapes** — two line arms diverging from a tip point on the circle boundary. Acute variants have a ~50° total opening angle; obtuse variants ~110°:

| Tip | Description |
|-----|-------------|
| **V acute — ↑** | Narrow V, tip at top, arms spread downward |
| **V acute — ↓** | Narrow V, tip at bottom, arms spread upward |
| **V acute — ←** | Narrow V, tip at left, arms spread rightward |
| **V acute — →** | Narrow V, tip at right, arms spread leftward |
| **V obtuse — ↑/↓/←/→** | Wide V variants, same four directions |

**Spatter and dapple** — procedurally generated dot patterns, always the same visual for a given brush size:

| Tip | Description |
|-----|-------------|
| **Spatter — dense** | ~50 small dots, uniformly distributed in the brush circle |
| **Spatter — medium** | ~22 medium dots, uniform |
| **Spatter — sparse** | ~8 larger dots, uniform |
| **Spatter — fine mist** | ~110 tiny dots — almost like airbrush overspray |
| **Spatter — coarse** | ~5 large blobs |
| **Spatter — cloud** | ~40 dots, Gaussian density — dense at centre, fading outward |
| **Spatter — rim** | ~22 dots concentrated near the circle edge |
| **Spatter — edge spray** | ~30 dots concentrated in the right half of the brush circle |
| **Dapple — grid** | Regular grid of dots with slight random jitter |
| **Dapple — rings** | Concentric rings of evenly-spaced dots |

> **Tip shapes and the Curve tool:** when the Curve tool is set to **Airbrush** paint mode, the selected tip shape is stamped at each interval point along the curve — so diagonal lines, V shapes, and spatter patterns can all be painted along a Bézier path. The cursor outline and the stamp-dot preview along the curve both reflect the chosen tip shape before you commit.

### Brush Blend Modes

Blend modes for paint tools apply at the brush level, pixel by pixel, each time the brush stamps. The formula is:

```
result = existing + (blend(existing, target) − existing) × alpha
```

where `alpha` is the kernel weight × opacity, and `blend(existing, target)` depends on the mode. The **Overwrite** mode is an exception — it bypasses alpha and writes the target value directly into every pixel touched by the brush, regardless of opacity.

**Replace**

| Mode | Formula | Effect |
|------|---------|--------|
| **Normal** | `target` | Standard replace towards the target value |
| **Overwrite** | `target` (ignores opacity) | Fully replaces every pixel touched by the brush — opacity slider has no effect |

**Darken** — output is never brighter than the existing pixels:

| Mode | Formula | Effect |
|------|---------|--------|
| **Multiply** | `existing × target` | Suppresses energy proportionally; target=1 leaves existing unchanged |
| **Color Burn** | `1 − (1−existing)/target` | Aggressive darkening; harder curve than Multiply |
| **Darken** | `min(existing, target)` | Takes whichever is quieter — painting can only darken |

**Lighten** — output is never darker than the existing pixels:

| Mode | Formula | Effect |
|------|---------|--------|
| **Screen** | `1 − (1−existing)(1−target)` | Lightens without hard clipping — inverse of Multiply |
| **Additive** | `existing + target` | Adds energy directly; can reach maximum |
| **Color Dodge** | `existing / (1 − target)` | Aggressive brightening; harder curve than Screen |
| **Lighten** | `max(existing, target)` | Takes whichever is louder — painting can only brighten |
| **Reflect** | `existing² / (1 − target)` | Amplifies bright areas; black target = no change |
| **Glow** | `target² / (1 − existing)` | Reverse of Reflect — bright existing areas amplify the target |

**Contrast**

| Mode | Formula | Effect |
|------|---------|--------|
| **Overlay** | Multiply where existing < 0.5; Screen otherwise | Increases contrast — dark areas get darker, bright areas get brighter |

**Comparative** — result depends on the relationship between existing and target:

| Mode | Formula | Effect |
|------|---------|--------|
| **Difference** | `|existing − target|` | Highlights where the brush and existing differ; painting over identical content → silence |
| **Negation** | `1 − |1 − existing − target|` | Brightening complement of Difference; identical content → full brightness |
| **Xor** | `existing + target − 2 × existing × target` | Similar to Additive minus Multiply; bright where inputs differ, dark where they match |

The same blend mode set applies to the Paste tool (via the Tools panel while in floating-paste mode) and to layer compositing.

---

## 9. The Gradient Editor

The gradient editor appears in the Tools panel when the **Curve** or **Fill** tool is active, and in the **Gradient Fill** filter dialog. It controls how paint color and opacity vary along the path, across the filled region, or across the frequency axis.

### The Gradient Bar

A horizontal bar previews the gradient from t=0 (left) to t=1 (right). The bar has a **checkerboard background** — visible where a stop has zero opacity — so transparency is always easy to spot. Small markers below the bar represent **stops**. There are always at least two stops — one at t=0 and one at t=1. Stop colors use the current RGB channel mapping to visually distinguish the left and right channels (e.g. red/amber for the left amplitude page, green/cyan for the right amplitude page).

| Interaction | Effect |
|-------------|--------|
| Click a stop | Select it; its values load into the spinboxes |
| Drag a stop | Reposition it along the bar. The t=0 and t=1 stops cannot move. |
| Double-click the bar | Insert a new stop at that position, values interpolated from neighbours |
| Delete Stop button | Remove the selected stop (disabled for terminal stops) |

### Stop Controls

When a stop is selected:

| Control | Range | Effect |
|---------|-------|--------|
| **Left intensity** | 0.0–1.0 | Left-channel (page 0) amplitude target at this stop |
| **Left opacity** | 0.0–1.0 | Blend strength for the left channel. 0 = no effect; 1 = full write |
| **Right intensity** | 0.0–1.0 | Right-channel (page 1) amplitude target at this stop |
| **Right opacity** | 0.0–1.0 | Blend strength for the right channel |
| **Link channels** | checkbox | When checked, editing Left intensity/opacity also updates Right, and vice versa |

Between stops all four values are linearly interpolated. New gradients start with zero intensity and zero opacity on both stops (transparent / no-effect), so the first visible change only appears when you explicitly set non-zero values.

### Examples

**Uniform color:** Leave both stops at the same Left, Right, opacity. The entire curve or fill paints at one consistent color and strength.

**Fade to silence (left → right):** t=0: Left=1, Right=1, Left op=1, Right op=1. t=1: Left=0, Right=0, Left op=1, Right op=1. Direction = 0°.

**Left channel only:** Uncheck Link channels. All stops: Left=1, Left op=1, Right=0, Right op=0. The right channel is untouched at every point.

**Opacity envelope:** Keep Left and Right the same on all stops; vary only Left opacity and Right opacity. The target color stays fixed but the brush strength varies — useful for shaping the dynamics of a note.

---

## 10. Filters

Filters are in the **Filters menu**. Each filter opens a modeless dialog. Applying a filter records a `filter` op in the op log, which can be re-edited from the Op Log dock.

### Common Dialog Controls

Every filter dialog has:

- **Channel scope** — Both channels · Left only (page 0) · Right only (page 1)
- **Selection only** — When checked, applies the filter only within the current selection. If the active selection has a non-rectangular boundary (lasso, magic wand, rotated/skewed rectangle, or a boolean composite of multiple selections), the filter result is blended back only for pixels inside the selection mask — pixels outside are left unchanged. If no selection is active, the filter is applied to the entire layer.
- **Preview** — applies the filter to the canvas immediately so you can hear and see the result, but does not commit; you can adjust the parameters and Preview again as many times as you like
- **Apply** — commits the operation and closes the dialog; the filter is pushed to the undo stack and recorded in the op log
- **Cancel** — restores the layer exactly to the state it had when the dialog was opened, discards any previewed changes, and closes the dialog

---

### Curves (`Ctrl+M`)

A tone-curve editor. Drag control points on the curve to map input pixel intensities to output intensities. The X axis is input brightness (0–65535); the Y axis is output brightness.

- **Left-click** an empty area to add a control point
- **Drag** a control point to reshape the curve
- **Right-click** a control point to remove it (minimum 2 points remain)
- **Reset** restores the identity (diagonal) curve

**Examples:**
- Pull the curve down to reduce amplitude (quieten)
- Raise the curve to boost amplitude
- Create an S-shape for contrast enhancement
- Set the output of the top end to zero to hard-limit peaks

---

### Sharpen

Increases edge contrast using an unsharp-mask algorithm.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Amount | 0.1–5.0 | 1.0 | eligible |

Higher values produce stronger sharpening. Very high amounts can introduce ringing artefacts.

---

### Denoise

A smoothing filter that reduces grain and speckle noise while preserving broad spectral structure.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Strength | 0.0–1.0 | 0.5 | eligible |

Higher strength = more smoothing, but finer detail is lost.

---

### Speckle

Adds or removes random grain texture.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Mode | add / remove | add | — |
| Amount (σ) | 0.001–0.5 | 0.05 | eligible |
| Radius (remove) | 1–10 | 1 | ⚠ blend-mask |

**Add** injects Gaussian noise scaled by Amount. **Remove** applies a median-based speckle filter with the given radius.

---

### Gaussian Blur

Soft, isotropic blur using a Gaussian kernel.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Sigma | 0.1–50.0 px | 2.0 | ⚠ blend-mask |

Larger sigma = broader blur. Blurs spread sound across time and frequency, reducing sharpness and transient clarity.

---

### Box Blur

Uniform blur using a rectangular averaging kernel. Faster than Gaussian; produces slightly blockier edges.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Radius | 1–50 px | 3 | ⚠ blend-mask |

---

### Motion Blur

Directional blur along a configurable angle.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Length | 1–200 px | 10 | ⚠ blend-mask |
| Direction | 0–360° | 0° | — |

0° = rightward (time stretch forward). 90° = upward (frequency smear toward high). A compass indicator shows the direction graphically.

---

### Smooth Motion Blur

Like Motion Blur but with a triangular (linear) falloff across the blur length, giving softer edges.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Length | 1–200 px | 10 | ⚠ blend-mask |
| Direction | 0–360° | 0° | — |

---

### Median Blur

Non-linear smoothing that replaces each pixel with the median of its neighbourhood. Preserves hard spectral edges while eliminating impulse noise and speckle.

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Size (odd) | 3–31 px | 3 | ⚠ blend-mask |

---

### Sobel Edge

Edge detection based on the Sobel gradient magnitude (`√(Gx² + Gy²)`).

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Amount | 0.1–10.0 | 1.0 | eligible |

The Amount scales the raw gradient magnitude. Use this to isolate transient edges, spectral boundaries, or harmonic overtone edges. High values heavily boost edge contrast.

---

### Channel Balance

Redistributes energy between the left and right channels while preserving the per-pixel sum (L + R).

| Parameter | Range | Default | MindWave |
|-----------|-------|---------|----------|
| Balance | 0.0–1.0 | 0.5 | eligible |

- 0.0 — all energy to left channel; right channel zeroed
- 0.5 — equal distribution (no change if already balanced)
- 1.0 — all energy to right channel; left channel zeroed

Useful for creating hard panning effects or correcting stereo imbalance.

---

### Invert

Inverts all amplitude values: `output = 65535 − input`. No parameters — use the Scope controls to limit the effect to one channel or a selection.

Viewing an inverted layer in **Light Mode** produces a display identical to the original layer in normal mode. This makes Invert useful for:

- Creating complementary layers that cancel out when composited with Normal blend mode
- Checking symmetry: if an inverted layer looks identical in Light Mode, the amplitude distribution is balanced around the midpoint
- Artistic amplitude reversal (silence becomes loud, loud becomes silence)

---

### Rotate Colors

Cyclically redistributes pixel intensities between the three display channels (R, G, B) that correspond to the TIFF pages under the chosen mapping. At 0° nothing changes; at 120° one full cyclic shift has occurred (the content that appeared red now appears green, etc.). Fractional angles linearly interpolate between steps, so the transform is continuous.

| Parameter | Range | Default | MindWave | Description |
|-----------|-------|---------|----------|-------------|
| Angle | 0–360° | 0° | per-pixel | Rotation through the three-channel cycle; 120° = one full step |
| Mapping | Yellow / Purple / Cyan (+ Phase variants) | Yellow | — | Which TIFF pages are treated as R / G / B |

Page 3 (phase R) is not part of the rotation cycle and is left unchanged.

When **Angle** is MW-bound, the rotation is computed per pixel: `eff_angle(x,y) = mw(x,y) × angle`. Where the wave is bright each pixel's channels are rotated by the full configured angle; where the wave is dark they are not rotated at all. This creates a spatially varying colour field rather than a global hue shift. Binding Angle to a noise wave produces a different hue at every region of the spectrogram; binding to a sine wave creates smooth iridescent bands.

**Mappings:**

| Name | R page | G page | B page |
|------|--------|--------|--------|
| Yellow | Amp L (0) | Amp R (1) | Phase L (2) |
| Purple | Amp L (0) | Phase L (2) | Amp R (1) |
| Cyan | Phase L (2) | Amp R (1) | Amp L (0) |

The `+ Phase` variants use the same page assignment; the suffix is for display-mapping reference only.

---

### Gradient Fill (`Ctrl+Shift+G`)

Applies a multi-stop gradient to the **Left** (page 0) and **Right** (page 1) amplitude channels along a configurable direction. Each gradient stop defines a target amplitude and blend strength (opacity) for each channel at a given position along the gradient direction.

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Gradient | — | Transparent | Interactive multi-stop gradient editor (see [Section 9](#9-the-gradient-editor)) |
| Direction | 0–360° | 90° | Gradient angle: 0° = horizontal (left→right in time), 90° = vertical (top→bottom, high→low frequency) |

**Blend formula:** at each pixel, `new_value = original * (1 − opacity) + intensity × 65535 × opacity`. An opacity of 0 leaves the pixel unchanged; 1 fully replaces it with the stop's intensity value.

**Using Gradient Fill as a spectral equalizer:**

At 90° (vertical), the gradient applies uniformly across all time frames but differently at each frequency row. This makes it a powerful non-destructive equalizer:

| Goal | Gradient setup |
|------|---------------|
| Cut heavy bass rumble | Add a stop at the bottom (t ≈ 0.9–1.0) with Intensity=0, Opacity=0.7–1.0 |
| Boost the midrange | Add stops with high intensity and opacity in the middle frequency band |
| Roll off harsh highs | Create a fade toward zero intensity at the top (t = 0.0) |
| High-pass filter | t=0 stop: Intensity=0, Opacity=1.0; t=0.3 stop: Intensity=1, Opacity=0. Everything below t=0.3 is silenced. |
| Notch a hum | Insert two stops just above and below the hum frequency row; set both to Intensity=0, Opacity=1.0 |

> **Tip:** Use **Gradient Fill as an adjustment layer** for non-destructive EQ — add it above your audio layer, shape the gradient to taste, and toggle it on/off with the ●/○ button without ever touching the original audio data.

> **Tip:** The checkerboard in the gradient bar marks transparent (zero-opacity) regions — the audio underneath those frequency rows is left completely unchanged.

---

### Custom Convolve (`Ctrl+Shift+K`)

Apply any convolution kernel you define manually.

| Control | Description |
|---------|-------------|
| Kernel size | Odd integer from 3 to 21 |
| Preset selector | Built-in presets: identity, edge, sharpen, etc. |
| Kernel canvas | Left-drag to paint positive values; right-drag for negative. Brush value sets the weight at the cursor. |
| Brush value | −1.0 to 1.0 |
| Normalize | If checked, divides the kernel by the sum of its positive weights before applying |
| Save / Load | Save the kernel as a `.npy` file; load a previously saved kernel |
| Clear | Sets all kernel cells to zero |

---

### Reverb

Simulates room reverberation by convolving each frequency bin's time series with an exponentially-decaying causal impulse response, then mixing the result with the dry signal. Works entirely in the spectrogram domain — no decoding required.

**Algorithm summary:** every time frame produces an echo trail that decays toward silence over the RT60 time. Room size scales the effective tail length. Diffusion spreads the tail across neighbouring frequency bins (vertical blur). Absorption attenuates the high-frequency content of the tail faster than the low-frequency content, modelling air damping. The final result is a wet/dry crossfade.

| Parameter | Range | Default | MindWave | Description |
|-----------|-------|---------|----------|-------------|
| Pre-delay | 0–200 ms | 20 ms | ⚠ blend-mask | Silence before the first reflection arrives |
| Decay (RT60) | 0.1–10 s | 1.5 s | ⚠ blend-mask | Time for the reverb tail to decay by 60 dB |
| Room size | 0–1 | 0.6 | ⚠ blend-mask | Scales the effective tail length within the RT60 budget (0 = tight/small, 1 = full) |
| Diffusion | 0–1 | 0.5 | ⚠ blend-mask | Gaussian vertical (frequency) spread of the reverb tail — higher = more diffuse, cloud-like reverb |
| Absorption | 0–1 | 0.4 | ⚠ blend-mask | High-frequency damping of the reverb tail — higher = darker, more air-absorbed sound |
| Mix (wet) | 0–1 | 0.4 | ⚠ blend-mask | Wet/dry crossfade: 0 = dry only, 1 = wet only |

**Scope:** Channels (L+R / L only / R only) and optional selection clipping work the same as all other filters.

**Tips:**
- Apply to a selection to add reverb only to a specific region or instrument layer.
- Combine with the **Edit Artifact Detector** afterward to catch any amplitude discontinuities at the selection boundary and smooth them with a Hann taper.
- Large rooms with high diffusion (0.8+) and low absorption create a dense, hall-like wash; small rooms (size 0.2, diffusion 0.2) with short decay create tighter, studio ambience.
- The reverb is recorded to the op log and fully re-editable via the Pick tool.

---

### Phase Watermark

Converts the visual content drawn in the amplitude pages into a **phase-encoded watermark** on a constant low-amplitude carrier. After the filter is applied, the drawn image is no longer visible as brightness in the spectrogram — it is encoded into the *colour* (phase) of the spectrogram and is nearly inaudible.

#### How it works

Each NSGT time-frequency bin stores a complex number `A · exp(iφ)` where `A` is the amplitude (pages 0/1, displayed as R and G in RGB view) and `φ` is the phase (pages 2/3, displayed as B). The filter:

1. **Reads** the amplitude pages as a greyscale source image (the drawing you painted).
2. **Maps** pixel brightness → phase angle: dark pixels → −π, mid-tones → 0, bright pixels → +π (or a narrower sub-range if **Phase range** is set to Half or Quarter).
3. **Writes** the encoded phase to pages 2/3.
4. **Replaces** the amplitude pages with a constant carrier at the specified dBFS level (default −60 dBFS, below the typical audible threshold in normal listening conditions).

Because the carrier is non-zero, every bin has a physically meaningful phase, and the phase information survives a **decode → encode** round-trip when the NSGT codec is a tight frame.

#### Parameters

| Parameter | Range | Default | MindWave | Description |
|-----------|-------|---------|----------|-------------|
| Carrier level | −90 to −20 dBFS | −60 dBFS | — | Amplitude of the constant carrier. Lower = quieter; −70 to −80 dBFS is typically below the hearing threshold in quiet rooms. Above −40 dBFS becomes faintly audible as coloured noise. |
| Source pages | Both / Left only / Right only | Both | — | Which amplitude channels to read as the source image. **Both** averages L and R. **Left / Right** uses only that channel. |
| Phase range | Full / Half / Quarter | Full | — | Fraction of [−π, +π] mapped to the full luminance range. Full maximises colour contrast in the B channel; narrower ranges produce subtler gradients and smoother audio. |
| Encoding | Mono / Stereo | Mono | — | **Mono** writes the same phase map to both Phase L and Phase R. **Stereo** encodes the Left amplitude channel into Phase L and the Right amplitude channel into Phase R, producing independent hue patterns per ear. |
| Phase blur σ | 0 – 20 px | 0 | ⚠ blend-mask | Gaussian smoothing applied in cos/sin phase space before writing. Smooths sharp edges in the phase image (reducing spectral artefacts at boundaries) at the cost of spatial resolution. |

#### Typical workflow

1. Paint or write anything on a layer using brushes, the text tool, or imported imagery — the content should be in the amplitude pages (R/G visible in RGB view).
2. Optionally draw a rectangle selection around the region you want to watermark.
3. Open **Filters → Phase Watermark…**
4. Set Carrier level to −60 dBFS (or lower for completely inaudible output).
5. Click **Preview** to see the colour shift in the spectrogram; click **Apply**.
6. The amplitude pages now show the carrier (uniform dim amber), and the B channel shows the encoded face/text as a hue pattern.
7. Decode and play back — the audio is the carrier (broadband coloured noise at the set level).

#### Hiding something in an existing mix

To hide a visual element within an audio layer that already has content:

1. Use the **Rectangle Selection** tool to frame the region you want to hide into.
2. In the selection, reduce the existing amplitude to nearly zero using **Curves** (pull the right end of the curve all the way down).
3. Paint the face/text at full brightness within the selection.
4. Apply **Phase Watermark** with **Apply to selection only** checked.

The surrounding audio content provides psychoacoustic masking that can make the carrier level tolerable even at −50 dBFS.

---

### Offset

Displaces the image by fetching each output pixel from a shifted source position. The source coordinate for output pixel `(x, y)` is `(x − distance × cos(angle), y − distance × sin(angle))`. Source coordinates that fall outside the image produce 0 (silence / black). Bilinear interpolation is used for sub-pixel accuracy.

In image space `x` is the column (horizontal) and `y` is the row (vertical, increasing downward), so:

| Angle | Direction content appears to shift |
|-------|-------------------------------------|
| 0° | Rightward (forward in time) |
| 90° | Downward (toward lower frequencies) |
| 180° | Leftward (backward in time) |
| 270° | Upward (toward higher frequencies) |

#### Parameters

| Parameter | Range | Default | MindWave | Description |
|-----------|-------|---------|----------|-------------|
| Distance | 0 – 500 px | 10 | per-pixel | How many pixels each output pixel is displaced from its source |
| Angle | 0 – 360° | 0° | per-pixel | Direction of the displacement vector |

#### MindWave binding

The Offset filter uses **per-pixel coordinate modulation** — unlike other filters where a MindWave acts as an opacity blend mask, here the wave directly scales the parameter at each pixel *before* the source coordinate is computed:

- **Distance MW-bound:** `eff_distance(x,y) = mw(x,y) × distance`. Where the wave is bright, the pixel is pulled from far away; where it is dark, the displacement approaches zero and the original value is kept. The result is a spatially non-uniform warp along the configured direction.
- **Angle MW-bound:** `eff_angle(x,y) = mw(x,y) × angle`. The direction of the displacement rotates across the canvas according to the wave. With a distance of 50 px and a horizontal sine bound to angle, the displacement fans from 0° on wave troughs to the full configured angle on wave crests.
- **Both MW-bound:** Distance and angle vary independently per pixel, producing a full 2-D per-pixel displacement field.

Because the MindWave scales from 0 to the configured value, setting `angle = 360°` and binding a Sine wave to Angle gives a full rotation of displacement direction from 0° to 360° across the wave period.

Binding Distance to a Perlin Noise wave and Angle to a directional Sine wave is especially effective for producing organic smear trails across the spectrogram.

---

## 11. Layers

Layers composite bottom-to-top to form the final image. Each layer holds its own TIFF data.

### Layer Panel Row Controls

Each row in the Layers panel contains, left to right:

| Control | Description |
|---------|-------------|
| **⠿ (drag handle)** | Click and drag to reorder layers |
| **●/○ (visibility)** | Click to toggle visibility in the composite (● = visible, ○ = hidden) |
| **Name label** | Layer name; blue italic = adjustment layer; red = load error (tooltip shows the error or TIFF path) |
| **Filter tag** | *(Adjustment layers only)* `[filter_name]` showing the active filter |
| **⚙ button** | Opens the floating Layer Configuration panel for this layer |

Click a row to make that layer active for painting and filtering.

### Layer Configuration Panel

Clicking **⚙** on a layer row opens a small floating tool window with all per-layer settings:

| Control | Description |
|---------|-------------|
| **Blend mode** | Select the compositing blend mode; disabled for adjustment layers |
| **Opacity** | Slider 0–100% — how strongly the layer contributes to the composite |
| **MindWave** | Optional per-pixel opacity modulator; select a MindWave from the project to link it, or **(none)** to remove (see [Section 13](#13-mindwaves)) |
| **Enable transform** | Checkbox to apply the geometric transform; *normal layers only* |
| **Edit…** | Toggle interactive transform handles on the canvas (see [Section 12](#12-layer-transforms)); *normal layers only* |
| **Rename…** | Rename the layer |
| **Duplicate** | Insert a copy directly above |
| **Delete** | Remove the layer (confirmation dialog) |
| **Edit filter…** | *(Adjustment layers only)* Reopen the filter parameter editor |

Click ⚙ again to dismiss the panel.

### Adding Layers

**Project → Add Layer** (`Ctrl+Shift+A`) opens a dialog where you can:

- **Choose a TIFF file** — the file is copied into `media/` if it is not already there
- **Choose an audio file** (WAV, MP3, FLAC, OGG) — encoded automatically using the project's codec settings
- **Choose an image file** (PNG, JPG, BMP, etc.) — converted to a Sound Mind TIFF using the selected channel mapping and import size mode (aspect-ratio scale, height-only squash, stretch to fill, stretch width, native dimensions, or sequential for multi-file imports)
- **Create an empty layer** — a blank (all-zeros) TIFF at the project dimensions
- **Add an adjustment layer** — opens the filter picker directly (equivalent to Project → Add Adjustment Layer…)

### Adjustment Layers

An adjustment layer is a non-destructive filter applied to the running composite of all layers below it. Add one via **Project → Add Layer** (`Ctrl+Shift+A`) and choosing "Adjustment layer", or via **Project → Add Adjustment Layer…**. Unlike normal layers, an adjustment layer:

- Carries **no TIFF file of its own** — it is a filter specification stored in the project file.
- **Applies its filter to the running composite** during compositing; every layer above it composites on top of the filtered result.
- Is shown in the Layers panel with a **blue italic label** and a `[filter_name]` tag.
- Blend mode and transform controls are not applicable and are disabled in the configuration panel.

#### Supported Adjustment Filters

Every filter in the Filters menu is available as an adjustment layer. The parameter registry is shared, so baking (Filters menu) and adjustment-layer paths always use the same definitions.

| Filter | Parameters | Effect |
|--------|-----------|--------|
| Gaussian Blur | Sigma, Apply to phase | Low-pass blur — smooths time and frequency edges |
| Box Blur | Radius, Apply to phase | Fast rectangular blur |
| Median Blur | Size, Apply to phase | Edge-preserving noise reduction |
| Motion Blur | Length, Angle, Apply to phase | Directional blur along a configurable angle |
| Smooth Motion Blur | Length, Angle, Apply to phase | Motion blur with a triangular (softer) falloff |
| Sharpen | Amount, Apply to phase | Enhances contrast between neighbouring frames |
| Denoise | Strength, Apply to phase | Reduces speckle noise |
| Sobel Edge | Amount | Extracts edge gradients (√(Gx²+Gy²)) |
| Channel Balance | Balance | Redistributes energy between L and R channels |
| Rotate Colors | Angle (per-pixel MW), Mapping | Cycles intensity between amplitude and phase pages |
| Invert | — | Flips all amplitude values: 65535 − input |
| Reverb | Pre-delay, Decay, Room size, Diffusion, Absorption, Mix, Phase diffusion | Spectral room reverberation along the time axis |
| Speckle (Add Noise) | Amount | Injects Gaussian noise |
| Speckle (Remove) | Radius | Median-based speckle suppression |
| Gradient Fill | Gradient (stops), Direction (angle) | Per-frequency spectral equalizer / fade |
| Offset | Distance (per-pixel MW), Angle (per-pixel MW), Apply to phase | Pixel displacement — each output pixel is fetched from a shifted source position; MindWaves modulate sampling coordinates directly, not opacity |
| Curves | Curve (control points), Amount (MW) | Tone-curve mapping applied at the given blend strength; Amount MW is mathematically exact (linear blend) |
| Custom Convolve | Kernel (grid), Normalize, Amount (⚠ MW) | Arbitrary convolution kernel; Amount MW uses blend-mask mode (⚠ approximation) |

The **Opacity** slider controls how strongly the filter is applied: 0.0 = no effect, 1.0 = full strength. This lets you dial in subtle amounts without committing to a pixel-baked edit.

#### MindWave-Bound Parameters

Each numeric parameter in an adjustment layer can be linked to a MindWave directly in the parameter editor. When MindWaves are defined in the project, a **MW** checkbox appears next to every eligible parameter. Checking it reveals a wave-name dropdown and keeps the spinbox as the **scale** value — the maximum parameter value applied where the MindWave evaluates to 1.0.

At composite time the blend formula is:

`output(x, y) = unfiltered(x, y) × (1 − mw(x, y) × opacity) + filtered(x, y) × mw(x, y) × opacity`

This makes the filter spatially selective: areas where the MindWave is bright receive full filter effect; dark areas are left unchanged.

**Blend-mask mode (⚠):** For kernel-based parameters — blur radii, reverb settings, and similar — a small **⚠** indicator appears next to the MW dropdown. These parameters control the shape of a convolution kernel rather than a per-pixel multiplier, so the exact per-pixel formula `param(x,y) = mw(x,y) × scale` cannot be applied directly. Instead the filter runs once at `param = scale` and the result is blended using the MindWave mask. This is an efficient approximation and works well in practice; the ⚠ is a reminder that the result is not identical to applying a spatially-varying kernel.

Parameters without the ⚠ symbol (Sharpen Amount, Denoise Strength, Sobel Amount, Speckle Amount, Channel Balance, Curves Amount) are linear-scale: the blend-mask formula is mathematically equivalent to the exact per-pixel version.

**Offset Distance, Offset Angle, and Rotate Colors Angle** behave differently from all other MW-eligible parameters. Rather than using blend-mask mode, the MindWave directly scales the parameter per pixel before any computation: `eff_param(x,y) = mw(x,y) × configured_value`. For Offset this varies the source sampling coordinate spatially; for Rotate Colors it varies the per-pixel hue rotation angle. See the individual filter sections for full details.

> If multiple parameters on the same filter are MW-bound simultaneously, their MindWave arrays are multiplied together to form the combined blend mask.

#### Editing an Adjustment Layer

Click **⚙** on an adjustment layer row, then click **Edit filter…** to reopen its parameter editor. The dialog supports the same **Preview / Apply / Cancel** workflow as regular filter dialogs: Preview applies updated parameters to the composite live, Apply commits and logs an `adjustment_layer_edit` op, and Cancel restores the previous parameters without logging anything.

> Adjustment layers are included in **Project → Render Project** and affect the exported audio.

#### The Equalizer Layer

Every new project starts with a pre-configured **Equalizer** adjustment layer at the top of the layer stack. It is:

- **Bypassed by default** — its visibility is off (○), so it has zero effect until you turn it on. Click the ○ button on its row to enable it.
- **Locked** — a 🔒 icon replaces the normal drag handle. The layer cannot be renamed, reordered below other layers, or deleted.

**Opening the Equalizer Panel:**

Click **⚙** on the Equalizer row, then **Edit filter…**. Because the layer is locked, this opens the **Equalizer Panel** instead of the generic gradient editor.

**Equalizer Panel controls:**

| Element | Description |
|---------|-------------|
| Gradient bar | Vertical bar: top = high frequencies, bottom = low frequencies (matches the spectrogram display). |
| Stops | Draggable handles on the bar. Left-click to select, drag to reposition. Double-click inside the bar to add a stop. Right-click a non-endpoint stop to delete it. |
| Cut | Attenuation strength for the selected stop. **0** = no attenuation (audio passes through unchanged). **1** = full cut (silence at that frequency). Values between 0 and 1 are proportional. |
| Band labels | Static dotted lines mark the boundaries of the standard Spectral Balance bands: **Sub-bass** (20–60 Hz), **Bass** (60–250 Hz), **Low-mid** (250–500 Hz), **Mid** (500–2000 Hz), **High-mid** (2 kHz–4 kHz), **Presence** (4–6 kHz), **Brilliance** (6–20 kHz). The positions are derived from the project's row count so they always align with the spectrogram grid. |
| Delete Stop | Removes the currently selected stop (disabled for the two fixed endpoints). |
| Preview | Applies the current curve to the composite without committing. |
| Apply | Commits the curve and logs an `adjustment_layer_edit` op. |
| Cancel | Reverts to the parameters the layer had before this edit session. |

**Typical use:**

1. Enable the Equalizer layer (click ○ → ●).
2. Click **⚙ → Edit filter…** to open the Equalizer Panel.
3. Add stops at the frequencies you want to attenuate and raise their **Cut** value.
4. Use **Preview** to hear the effect, then **Apply** to commit.

To bypass the Equalizer temporarily, click ● on its row to hide it (○) without changing its curve.

#### The Background Layer

Every new project also starts with a **Background** layer at the **bottom** of the layer stack. It holds the silent canvas created when the project was first set up. It is:

- **Always visible** — the visibility button is disabled; the Background layer cannot be hidden.
- **Locked** — a 🔒 icon replaces the normal drag handle. The layer cannot be renamed, reordered above other layers, or deleted.

New layers (normal, TIFF imports, adjustment layers) are always inserted **above** the Background layer, so it remains the permanent floor of the compositing stack.

When importing audio or images, you can also configure:

- **Snippet duration** (audio) — if set, splits the audio into multiple layers at that interval, one layer per snippet
- **Import size mode** (image) — aspect-ratio scale, height-only squash, stretch to fill, stretch width, native dimensions, or sequential (multi-file) (see [Section 19](#19-importing-media-as-layers))
- **Channel mapping** (image) — controls how RGB image channels map to TIFF pages (see [Section 19](#19-importing-media-as-layers))
- **Project columns** (image) — splits wide images into multiple layers

### Renaming Layers

Click **✎** on a layer row. A dialog prompts for the new name. The name must be unique within the project. When the project is saved, the layer's TIFF file is renamed on disk to match.

### Duplicating Layers

Click **⧉** on a layer row. A copy of the layer is inserted directly above the original. The TIFF file is copied on disk. The duplicate's name gets ` copy` appended (or ` copy 2`, ` copy 3`, etc. if that name is already taken).

### Merging Layers (Merge Down)

Click the **⤓** button on a layer row to merge that layer onto the layer immediately below it (skipping any adjustment layers in between).

**What happens:**

1. The lower layer's canvas is expanded to the union of both layers' bounding boxes (zero-padded with silence on the right and bottom as needed).
2. The selected layer is composited onto the expanded lower layer using the selected layer's blend mode and opacity. If the selected layer has a non-destructive transform enabled, it is baked into the composite at this point.
3. The merged pixel data replaces the lower layer's TIFF on disk.
4. Any adjustment layers that were between the two layers remain in their current position — they are not merged and continue to apply to the merged layer.
5. The selected layer is removed; the merged layer (the former lower layer) becomes the active layer. The merged layer keeps the lower layer's name.

The ⤓ button is **greyed out** when there is no eligible target layer below (the layer is at the bottom of the stack, or only adjustment layers exist below it). It is **hidden** for locked layers and adjustment layers.

> **Note:** Merge down is a destructive operation — the merged state is immediately written to the lower layer's TIFF. The project op history records the merge, but a later Remaster will apply both layers' stroke / paint ops sequentially on a single canvas rather than rebuilding the exact composite result.

### Deleting Layers

Click **✕** on a layer row and confirm. The layer is removed from the project list; its TIFF file on disk is not deleted.

### Reordering Layers

Drag a row by its drag handle to reposition it. The topmost row in the panel is the topmost compositing layer.

### Layer Blend Modes

Blend modes control how a layer combines with the composite of all layers below it. The first visible layer always composites as Normal regardless of its setting, so that Multiply and Darken don't produce an all-black result on an empty base.

**Replace**

| Mode | Formula | Typical use |
|------|---------|-------------|
| **Normal** | `layer` (opacity-crossfaded onto base) | Default; general-purpose layering |
| **Overwrite** | `layer` (opacity slider ignored — always full strength) | Force a layer to always appear at 100% regardless of opacity |

**Darken** — result is never brighter than the base:

| Mode | Formula | Typical use |
|------|---------|-------------|
| **Multiply** | `base × layer` | Gating — only frequencies present in both layers survive |
| **Color Burn** | `1 − (1−base)/layer` | Aggressive darkening; harder than Multiply |
| **Darken** | `min(base, layer)` | Keeps the quieter value at each pixel; effective masking |

**Lighten** — result is never darker than the base:

| Mode | Formula | Typical use |
|------|---------|-------------|
| **Screen** | `1 − (1−base)(1−layer)` | Lightens without hard clipping; softer than Additive |
| **Additive** | `base + layer` | Mixing complementary sounds; adding harmonics |
| **Color Dodge** | `base / (1 − layer)` | Aggressive brightening; harder than Screen |
| **Lighten** | `max(base, layer)` | Keeps whichever layer is louder at each frequency-time position |
| **Reflect** | `base² / (1 − layer)` | Amplifies bright base regions; a dark overlay layer has no effect |
| **Glow** | `layer² / (1 − base)` | Reverse of Reflect — the layer amplifies bright areas of the base |

**Contrast**

| Mode | Formula | Typical use |
|------|---------|-------------|
| **Overlay** | Multiply where base < 0.5; Screen where ≥ 0.5 | Contrast enhancement; emphasises shared spectral peaks |

**Comparative**

| Mode | Formula | Typical use |
|------|---------|-------------|
| **Difference** | `|base − layer|` | Highlights dissimilar regions; null where layers are identical |
| **Negation** | `1 − |1 − base − layer|` | Brightening complement of Difference; identical regions → maximum |
| **Xor** | `base + layer − 2 × base × layer` | Creates complex patterns where the layers partially overlap |

### The Rendered Layer

After **Project → Render Project**, the fully composited output is stored as the **Rendered** layer (shown at the top of the Layers panel in italic). This is the layer decoded to audio by **Decode Rendered → WAV**. Re-rendering overwrites it.

---

## 12. Layer Transforms

Each layer can have an independent geometric transform that is applied before compositing. Transforms are non-destructive: the underlying TIFF pixel data is never modified.

### Enabling a Transform

Check the **Enable transform** checkbox in the layer's configuration panel (⚙ button). When unchecked, the raw pixels are used as-is (no resampling, no quality loss). The checkbox is also enabled automatically the first time an interactive drag commits a non-identity transform.

### Interactive Transform Handles

Click the **Edit…** button in a layer's configuration panel (⚙) to toggle **interactive transform handles** on the canvas. An amber bounding box with handles appears over the layer:

| Handle | Location | Action |
|--------|----------|--------|
| **Center drag** | Inside the bounding box | Translate — moves the layer in time and frequency |
| **Corner drag** | TL / TR / BR / BL squares | Proportional scale from the opposite corner |
| **Edge drag** | T / B squares | Scale vertically (frequency axis), anchored on the opposite edge |
| **Edge drag** | L / R squares | Scale horizontally (time axis), anchored on the opposite edge |
| **Rotate knob** | Circle above the top edge | Rotation — drag in an arc around the layer centre |

**Modifier keys while dragging:**
- **Shift** (while moving) — constrain movement to the horizontal or vertical axis, whichever is larger

**To exit interactive mode:** click **Edit…** again, or press **Escape**. The transform is committed on each mouse-up — every drag that moves or scales is recorded as a transform op and can be undone with `Ctrl+Z`.

> The handles are drawn in the amber colour to distinguish them from the blue selection-tool handles. The bounding box reflects the full extent of the layer in canvas space, including any rotation or skew already applied.

### Transform Parameters

The full set of transform parameters can be viewed and edited numerically by opening the layer's configuration panel (⚙):

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Translate X** | 0 px | Horizontal shift. Positive = shift right in time. |
| **Translate Y** | 0 px | Vertical shift. Positive = shift down (toward lower frequency). |
| **Scale X** | 1.0 | Horizontal scale multiplier. > 1 = stretch in time; < 1 = compress. |
| **Scale Y** | 1.0 | Vertical scale multiplier. > 1 = stretch in frequency; < 1 = compress. |
| **Rotation** | 0° | Rotation in degrees, clockwise on screen. |
| **Skew X** | 0° | Horizontal shear in degrees. |
| **Skew Y** | 0° | Vertical shear in degrees. |
| **Flip H** | off | Mirror the layer left ↔ right (time reversal). |
| **Flip V** | off | Mirror the layer top ↔ bottom (frequency inversion). |

Transforms are applied in this order around the canvas centre: **flip → scale → skew → rotate → translate**.

### Compositing Behaviour

When a layer's transform is enabled, the compositing engine applies the full affine transform to the layer's pixels before blending. Pixels that fall outside the canvas boundaries are clipped; areas of the canvas not covered by the transformed layer are transparent (silence) for that layer. This means:

- A layer that is **scaled down** will leave silence in the uncovered area.
- A layer that is **translated past the canvas edge** will be partially or fully invisible.
- A layer with a **native height different from the project canvas** (e.g. imported with native dimensions) can be scaled to fit using the Scale Y handle.

---

## 13. MindWaves

A **MindWave** is a named parametric waveform that produces a per-pixel scalar field in [0, 1] across the canvas. Linking a MindWave to a layer multiplies the layer's effective opacity by that field at every pixel during compositing — areas where the waveform evaluates to 1.0 contribute at full opacity; areas where it evaluates to 0.0 are fully transparent. Any value in between produces partial blending.

Typical uses: frequency-dependent fades (fade out only the high-frequency content of a layer), rhythmic breathing envelopes (let a layer pulse in and out), radial masks (soft falloff from the canvas centre), and organic noise-driven texture mixing.

### Opening the MindWave Editor

**View → MindWaves** (shortcut **Ctrl+W**) opens the **MindWaves dock panel**. The panel can be floated, docked alongside other panels, or closed at any time; the View menu entry and the shortcut toggle it.

### Creating and Editing MindWaves

The panel shows a scrollable list of all MindWaves in the current project.

| Control | Action |
|---------|--------|
| **+** | Create a new MindWave with default settings (Sine, Axis X) |
| **−** | Delete the selected MindWave |
| **Name** field | Rename the selected MindWave — press Enter or click away to confirm |
| **Function** combo | Choose the waveform function (see tables below) |
| **Axis** combo | **X** — waveform varies along the time axis; **Y** — along the frequency axis; **XY** — full 2D (locked for 2D functions) |

Changing any parameter immediately re-composites the canvas. The panel shows only the parameter rows relevant to the selected function; irrelevant rows are hidden automatically.

### Waveform Functions

#### 1D Functions (Axis = X or Y)

| Function | Description |
|----------|-------------|
| **Sine** | `sin(2π × coord / period + phase)` — smooth oscillation |
| **Cosine** | `cos(...)` — identical to Sine with a 90° phase shift |
| **Abs Sine** | `|sin(...)|` — full-wave rectified; creates bumps with no silence between cycles |
| **Triangle** | Symmetric linear rise-and-fall |
| **Sawtooth Right** | Ramps 0 → 1 then drops (ascending sawtooth) |
| **Sawtooth Left** | Drops 1 → 0 then jumps (descending sawtooth) |
| **Square** | ±1 square wave; Duty Cycle = 0.5 gives a symmetric wave |
| **Pulse** | ±1 with configurable Duty Cycle |
| **Staircase** | Ascending sawtooth quantised to N discrete levels (Steps parameter) |
| **Exp Decay** | Exponential falloff from 1 to 0 — natural amplitude-envelope shape |
| **Bounce** | Decaying oscillation: `cos(2πx/period) × exp(−x/decay)` |
| **Sigmoid** | S-curve transition centred at Center X/Y; Steepness controls sharpness |
| **Smooth Noise** | Gaussian-filtered random noise (Seed, Octaves) |

#### 2D Functions (Axis = XY)

| Function | Description |
|----------|-------------|
| **Perlin Noise** | Multi-octave smooth noise (Period X/Y, Octaves, Seed) |
| **Checkerboard** | Alternating tiles at Period X × Period Y |
| **Cross Sine** | `sin(2πx/period_x) × sin(2πy/period_y)` — grid of peaks |
| **Ripple** | Concentric sine rings radiating from (Center X, Center Y); radial period = Period X |
| **Diagonal Sine** | Sine wave along the Angle direction; Period X = pixels per cycle along that axis |
| **Voronoi** | Distance to the nearest random seed — organic cell-like blobs |
| **Warped Noise** | Smooth noise with coordinate domain-warping (Warp Strength) |
| **L-System** | Fractal branching structure evaluated as a per-pixel opacity field — see [L-System MindWave](#l-system-mindwave) |

### Parameters

| Parameter | Applies to | Description |
|-----------|------------|-------------|
| **Amplitude** | All | Scales the waveform swing. 1.0 = full [0, 1] range; 0.5 = half-depth modulation |
| **Offset** | All | DC offset — centre of the output range. Amplitude 1.0 + Offset 0.5 spans exactly [0, 1] |
| **Period X** | 1D (X), 2D | Cycle length along the X (time) axis; labelled just **Period** for 1D-X functions. The displayed value uses the unit chosen in **Period unit** (see below) |
| **Period Y** | 1D (Y), 2D | Cycle length along the Y (frequency) axis, in the same unit as Period X |
| **Period unit** | 1D, 2D | Unit in which Period X/Y are displayed and entered: **Pixels** (default) — raw canvas columns or rows; **Milliseconds** — converted via the project timestep; **Beats** — converted via the project timestep and BPM. Internally periods are always stored in pixels; the unit is a per-wave display preference and is saved with the project |
| **Phase** | 1D | Phase offset in degrees |
| **Duty Cycle** | Square, Pulse | Fraction of each cycle held at maximum (0.5 = symmetric) |
| **Steps** | Staircase | Number of discrete output levels |
| **Decay** | Exp Decay, Bounce | 1/e decay distance in pixels |
| **Center X / Y** | Sigmoid, Ripple | Inflection point or ripple origin in canvas pixels |
| **Steepness** | Sigmoid | Slope sharpness — higher values give a sharper S-curve |
| **Angle** | Diagonal Sine | Wave direction in degrees |
| **Seed** | Noise functions | RNG seed — change for a different random pattern |
| **Octaves** | Noise functions | Number of noise octaves layered together |
| **Warp** | Warped Noise | Coordinate domain-warp scale (0 = no warp, higher = more turbulence) |
| **Preset** | L-System | Built-in grammar: Plant, Fern, Koch Curve, Dragon Curve, Sierpinski Triangle, Hilbert Curve. Selecting a preset auto-fills the Angle spinbox with the canonical turn angle for that grammar |
| **Projection** | L-System | **Pitch-Time** — turtle origin left-centre, trunk grows upward, X→time, Y→pitch; **Heading-Freq** — path distance→column, heading direction→row; **Polar Bloom** — canvas-centred origin, symmetric grammars radiate outward |
| **Falloff** | L-System | **Gaussian** (default) — bell-curve cross-section from branch centreline; **Linear** — tent-profile falloff |
| **Mode** | L-System | **Static** — full structure rendered at once; **Growth** — branches revealed left-to-right so the x-axis acts as a temporal growth axis (see [L-System MindWave](#l-system-mindwave)) |
| **Depth** | L-System | Recursion depth 1–6; auto-reduced if estimated segment count would exceed 500 000 |
| **Angle** | L-System | Turn angle in degrees for `+` / `−` commands |
| **Step length** | L-System | Forward step size in canvas pixels per `F` command |
| **Randomization** | L-System | 0 = rigid geometry; 1 = chaotic (adds angle jitter, step jitter, and branch-skip probability above 60%) |
| **Branch width** | L-System | Falloff radius in pixels — how far the influence of each branch extends from its centreline. Default 8 px |
| **Branch decay** | L-System | Per-branch-depth amplitude multiplier (0.5–1.0). Trunk = full opacity; each nested child branch is attenuated by this factor. Default 0.75 |

**Output formula:** `value = clip(Offset + Amplitude/2 × wave, 0, 1)` where `wave ∈ [−1, 1]` is the raw function output before mapping. With Amplitude = 1.0 and Offset = 0.5, the waveform spans the full [0, 1] range.

### Superposition

A MindWave can be defined as the **superposition** of other waves already in the project. Select **Superposition** from the top of the Function dropdown; the parameter controls are replaced by a **sub-wave stack** where you build the result by combining any number of earlier-defined waves.

Each entry in the stack specifies a **source wave** and a **blend mode** that determines how it is folded into the running result:

| Blend Mode | Formula | Effect |
|------------|---------|--------|
| **Multiply** | `result = result × wave` | Default. Both waves must be high for the output to be high. Preserves the shape of whichever is quieter. |
| **Add** | `result = clip(result + wave, 0, 1)` | Sums the two; saturates at 1. Regions where either wave is active are included. |
| **Screen** | `result = 1 − (1−result)(1−wave)` | Like Add but never exceeds 1; softer highlights. |
| **Min** | `result = min(result, wave)` | Keeps whichever is darker — logical AND of the two fields. |
| **Max** | `result = max(result, wave)` | Keeps whichever is brighter — logical OR of the two fields. |
| **Average** | `result = (result + wave) / 2` | Simple mean; reduces dynamic range. |

The stack starts at 1.0 (fully opaque) and each entry modifies that running value in list order. Drag the **⠿** handle on any row to reorder; click **×** to remove; click **+ Add…** to append a new entry.

**Interference patterns.** Combining a horizontal sine with a vertical sine using Multiply produces a grid of peaks. Adding a slow noise wave on top (using Multiply or Min) breaks the regularity into an organic pattern. Chaining several waves with different blend modes and periods gives interference textures that would otherwise require scripting.

**Circular references are not permitted.** A superposition wave can only reference waves that appear *earlier* in the MindWave list — the **+ Add…** menu shows only eligible names. This ordering rule guarantees that the evaluation chain always terminates.

**Renaming or deleting a wave that is referenced** by a superposition wave is blocked: a dialog lists the dependent waves that need to be updated first.

### L-System MindWave

The **L-System** 2D wave function generates a fractal branching field using the same grammar engine as the Generators dock (Features 9/10). Rather than stamping a layer, it evaluates the structure as a per-pixel opacity field in [0, 1] that you can link to any layer or adjustment-layer parameter.

#### How the field is computed

1. The chosen preset's grammar is expanded to the specified depth and rendered into a binary centreline image using Bresenham line drawing.
2. A Euclidean distance transform maps each pixel to the distance of its nearest centreline pixel and the amplitude weight (branch decay^depth) of that nearest pixel.
3. A falloff function (Gaussian or Linear) and branch width determine how quickly the field decays from zero on the centreline to zero in open space.
4. The result is a float32 `(H, W)` field in `[0, 1]` that is bright along branch centrelines and fades organically outward.

Branch depth amplitude weighting means trunk segments (depth 0) produce the brightest regions; nested child branches are progressively dimmer according to **Branch decay**. With Branch decay = 1.0 all branches are equally bright.

#### Static vs Growth mode

| Mode | Behaviour |
|------|-----------|
| **Static** | The full structure is rendered once. The wave is time-invariant — the same field at every canvas position. Evaluation is cached automatically per `(H, W, params)`. |
| **Growth** | At canvas column *c*, only branches whose leftmost projected x-coordinate ≤ *c* contribute to the field. The canvas x-axis becomes a temporal growth axis: the trunk and early-path segments appear first (left), and later branches are revealed progressively to the right. |

**Growth mode combined with Superposition** can produce time-evolving opacity effects: multiply an L-System Growth wave with a slow horizontal sine to create a pulsing reveal of the branching structure from left to right.

For **Heading-Frequency** projection, Static and Growth modes are identical — the horizontal band structure already encodes path distance on the x-axis.

#### Superposition compatibility

The L-System wave produces the same `(H, W)` float64 `[0, 1]` field as every other 2D wave. All Superposition blend modes (Multiply, Add, Screen, Min, Max, Average) work without special-casing. Useful combinations:

- **L-System × Sine** — the sine acts as a time-varying amplitude envelope; branch intensity pulses horizontally across the canvas even in Static mode.
- **L-System × Perlin Noise** — organically modulates branch cluster intensities on a scale larger than the branch structure itself.
- **Polar Bloom L-System × Radial Ripple** — constrains pulsing to the radial axis, producing a breathing snowflake.

#### MW-bound adjustment-layer parameters

Standard blend-mask mode applies for kernel-based parameters: the filter runs once at the configured scale and the result is blended using the L-System field as a per-pixel opacity mask. For linear-scale parameters the blend is mathematically identical to per-pixel parameter scaling — areas bright on the wave receive full filter effect; dark areas are unfiltered.

### Linking a MindWave to a Layer

Open the **Layer Configuration panel** (⚙ button on any layer row) and choose a MindWave from the **MindWave** dropdown at the top of the form. Select **(none)** to remove the link.

When linked, the compositor applies: `eff_opacity(x, y) = layer.opacity × mindwave(x, y)` at each pixel. The standard Opacity slider still applies — it sets the ceiling of the per-pixel modulation.

A small **∿** symbol appears to the right of the layer name in the Layers panel when a MindWave is linked, with a tooltip showing the linked name.

MindWaves can also be bound to **individual filter parameters** on adjustment layers, rather than (or in addition to) the layer's overall opacity. See [MindWave-Bound Parameters](#mindwave-bound-parameters) in the Adjustment Layers section for details.

### Canvas Preview

Click **Preview** in the MindWave Editor panel to project the current MindWave onto the canvas as a greyscale overlay at 50% transparency. Bright areas indicate high values (high effective opacity); dark areas indicate low values (transparent). The overlay is for visual reference only and does not affect compositing or rendering. A label in the bottom-left corner of the canvas confirms the preview is active. Click **Preview** again to dismiss it.

The preview updates live as you adjust parameters.

### Import and Export

- **Export…** — saves the selected MindWave to a `.mindwave` file (plain JSON). Use this to share waveform presets between projects or with other users.
- **Import…** — loads a `.mindwave` file into the current project. If a MindWave with the same name already exists, a numeric suffix is appended to avoid a collision.

MindWaves are also serialised inline inside the project file (`.smsproj`) — no manual export is needed to persist them across saves.

---

## 14. View & Navigation

### Zooming

All zoom commands are also available under **View → Zoom** (submenu).

| Action | Effect |
|--------|--------|
| Ctrl+Scroll wheel | Zoom in/out along the active axis (see Zoom Toolbar) |
| Ctrl+Shift+Scroll | Time axis zoom only (horizontal), regardless of axis setting |
| Ctrl+Alt+Scroll | Frequency axis zoom only (vertical), regardless of axis setting |
| Ctrl++ | Zoom in along the active axis |
| Ctrl+- | Zoom out along the active axis |
| Ctrl+0 | Fit entire spectrogram in window (aspect ratio preserved) |
| Ctrl+Shift+0 | Fit height — scale vertical axis to fill the window |
| Ctrl+Alt+0 | Reset zoom to 1:1 (one screen pixel per spectrogram pixel) |
| Ctrl+] | Zoom time axis wider |
| Ctrl+[ | Zoom time axis narrower |
| Ctrl+Shift+] | Zoom frequency axis in |
| Ctrl+Shift+[ | Zoom frequency axis out |

#### Zoom Toolbar

A toolbar at the bottom of the window provides persistent zoom controls:

| Control | Description |
|---------|-------------|
| **−** / **+** buttons | Step zoom out / in by 25% along the active axis |
| Log-scale slider | Drag to set any zoom level from ~1% to ~10 000% along the active axis |
| **⊞ Both** / **↔ W** / **↕ H** | Axis selector — click to cycle between **Both** (proportional), **W** (time/horizontal only), and **H** (frequency/vertical only). Affects the slider, ± buttons, and Ctrl+Scroll wheel. |
| Percentage label | Displays the current zoom level for the active axis |
| **1:1** button | Reset to pixel-perfect 1:1 zoom |
| **Fit H** button | Scale vertical axis so the spectrogram fills the window height |
| **Fit W** button | Scale horizontal axis so the spectrogram fills the window width |
| **Fill screen** button | Scale both axes independently to fill the viewport exactly (no letterboxing) |

The **Ctrl+Shift+Scroll** and **Ctrl+Alt+Scroll** shortcuts always zoom their specific axis regardless of the axis selector setting.

### Canvas Orientation

The canvas can be reoriented for display without modifying the underlying audio data. This lets you view and work on a spectrogram in any rotation or mirror, then restore it. The orientation is saved with the project.

#### Orientation Toolbar (bottom)

| Button | Shortcut | Effect |
|--------|----------|--------|
| **↻ 90°** | — | Rotate canvas 90° clockwise |
| **↺ 90°** | — | Rotate canvas 90° counter-clockwise |
| **↔ Flip H** | — | Mirror the canvas horizontally (left ↔ right) |
| **↕ Flip V** | — | Mirror the canvas vertically (top ↔ bottom) |
| **⊡ Default** | — | Restore the original (unrotated) orientation |

The label next to the buttons shows the current orientation (e.g. **90° ↻**, **↔ H**).

These options are also available under **View → Canvas Orientation**.

#### How orientation affects playback and tools

- **Paint tools** — strokes always go to the correct underlying pixel regardless of orientation. Painting in a rotated view writes to the same audio data as painting in the default view.
- **Playback cursor** — sweeps in the correct direction for the active orientation (vertical for 0°/180°/flipped, horizontal for 90° rotations).
- **Loop markers** — follow the same direction as the playback cursor.
- **Spectrogram grid** — time and frequency grid lines are rotated/flipped to match the display.
- **Coordinate readout** — the status bar always shows the underlying raw time (seconds) and frequency (Hz), not the screen position.

> **Note**: Complex analysis overlays (pitch tracker, formant display, etc.) are drawn in raw image coordinates and may not align correctly with heavily rotated views. Restore the default orientation before using those overlays.

### Panning

| Action | Effect |
|--------|--------|
| Middle-mouse drag | Pan |
| Space (tap) | Play / Pause |
| Space + left-drag | Pan temporarily with any tool active |
| Pan tool + left-drag | Pan |

### Waveform View Panel

**View → Waveform View** (or **Ctrl+Alt+W**) opens a resizable panel below the spectrogram that displays amplitude-envelope waveforms. The panel is hidden by default; toggle it on at any time without losing your current view layout.

The waveform is derived column-by-column from the spectrogram image: for each time column the peak amplitude across all frequency rows is taken, normalized to [0, 1], and square-root-compressed for visual balance. The result is rendered as a symmetric filled shape (mirrored above and below a centre line), synchronized to the spectrogram's horizontal scroll position and zoom level.

#### Display modes

| Mode | Description |
|------|-------------|
| **Composite** | A single gray waveform computed from the rendered composite image. Shows the overall amplitude envelope of the full mix. |
| **All Layers** | Each normal layer is drawn in a distinct color (8-color palette; colors are stable across refreshes and are consistent with any layer-color uses elsewhere). Layers are drawn in stack order (bottom layer first, so upper layers appear on top). |
| **Active Layer** | All layers are drawn faint; the currently active layer is highlighted in its palette color at full opacity. Useful for auditing a single layer's contribution to the mix. |

Select the mode using the **Composite / All Layers / Active Layer** buttons in the panel toolbar.

#### Synchronization

The waveform panel updates automatically whenever:
- You scroll the spectrogram horizontally.
- You zoom in or out (time axis).
- The composite is recomputed (any edit that triggers a composite rebuild).
- You switch the active layer.

The visible time range always matches the portion of the spectrogram currently visible in the viewport.

#### Resizing

The panel is part of a vertical splitter with the spectrogram. Drag the divider between the spectrogram and the panel to adjust the relative heights. The default split gives the spectrogram 80% of the space.

### Frequency Scale

The toolbar scale combo or **View → Frequency Scale** switches between:

| Scale | Description |
|-------|-------------|
| **Logarithmic** (default) | Native NSGT scale — each pixel is one NSGT bin, no interpolation. Octaves equally spaced. |
| **Mel** | Perceptual scale — remaps bins so equal vertical distance = equal perceived pitch. Slightly more compressed at high frequencies than log. |
| **Linear** | Equal vertical distance = equal Hz. Bass frequencies get very little space. |

The scale is visual only; it does not affect the stored pixel data or the audio.

---

## Sound Flowers

A **Sound Flower** is a polar-coordinate rendering of the spectrogram. Instead of the familiar flat time-vs-frequency rectangle, the image is projected onto a circular canvas: the time axis wraps around the ring and the frequency axis radiates from the centre outward. The result looks like a mandala or flower whose rings encode pitch and whose rotation encodes time.

Sound Flowers are a display mode — they share exactly the same underlying pixel data as the flat view. Toggling the mode never modifies your audio; it only changes how the canvas is drawn and how mouse input is mapped.

### The coordinate convention

| Dimension | Direction | Meaning |
|-----------|-----------|---------|
| **Angle (θ)** | 0 at 12 o'clock, increases clockwise | Time — left edge of the flat spectrogram maps to 12 o'clock |
| **Radius (r)** | 0 at the centre, max at the outer ring | Frequency — centre = lowest bin, outer ring = highest bin |

The flower radius equals the canvas height (number of frequency bins). At the default "fill window" zoom level the flower fills the entire viewport.

### Enabling Sound Flower view

**View → Sound Flower** (`Ctrl+Alt+F`) is a checkable menu action. Clicking it once switches to polar view; clicking again returns to the flat spectrogram. The toggle state is not saved with the project — the studio always opens in flat view.

A status bar message confirms the mode: *Sound Flower — polar view active*.

### Painting in polar mode

All paint tools — Brush (all tip types), Smudge, Stamp, Heal, Order, Chaos, Avalanche, Fill, Curve, and Select — work normally on the polar canvas. Every mouse click or drag is automatically inverse-transformed from the circular scene back to the correct pixel in the underlying rectangular image, so painting a stroke on the flower is identical to painting the equivalent stroke on the flat view.

- Points inside the disk map to a unique rectangular coordinate.
- Points outside the disk are ignored (no paint is applied).
- The brush-circle cursor overlay is replaced by a crosshair in polar mode, since the circular cursor would be misleading on a curved canvas.
- Zoom and pan work exactly as in flat mode — the view is a larger scene rect (`2H × 2H` pixels) and all the usual scroll and zoom shortcuts apply.
- The **Snap to Grid** feature is suspended while polar mode is active.

### Switching back

Click **View → Sound Flower** again (`Ctrl+Alt+F`) or uncheck the menu item to return to the flat rectangular view. Your edits are preserved — the flower and the flat view are the same data.

---

### Polar Image Import

You can import a source image that is already encoded in polar form — for example, a spectrogram that was exported from another tool as a circular flower, or any image whose content radiates from a centre point. The wizard un-warps it back to a rectangular spectrogram automatically.

#### Opening the polar wizard

1. Open **Studio → Import…** (`Ctrl+Shift+I`) and select **Image** as the import type.
2. Browse to the source image file.
3. In the **Size mode** group select **Polar — un-warp a polar/flower-shaped image to rectangular**.
4. The **Set origin…** button appears below the size mode list. Click it.

#### The origin picker dialog

The dialog shows a scaled preview of the source image with an interactive overlay:

| Handle | Appearance | What it controls |
|--------|-----------|-----------------|
| **Crosshair** | Full-width and full-height yellow lines | Origin (flower centre) — the point that maps to the centre of the disk |
| **Ring** | Yellow circle | Sampling radius — the outer boundary of the disk; pixels beyond this ring are not sampled |
| **Arc start** | Orange square on the ring | Start angle of the arc (0° = 12 o'clock, clockwise) |
| **Arc end** | Orange square on the ring | End angle of the arc — if equal to the start, a full 2π circle is used |

All handles are draggable. The corresponding spinboxes update in real time and can also be edited directly for precise values.

| Control | Default | Description |
|---------|---------|-------------|
| **Origin X / Y** | Image centre | Flower centre in source pixel coordinates |
| **Radius** | 45% of min(W, H) | Maximum sampling radius in source pixels |
| **Arc start** | 0° | Start angle in degrees (0 = 12 o'clock, clockwise) |
| **Arc end** | 360° | End angle; 360 means full circle |
| **Output width** | ⌊2π × radius⌋ | Width of the resulting rectangular spectrogram in pixels. The default gives square arc pixels; increase it to oversample the angular axis. |

Click **OK** to confirm. Click **Cancel** to go back to the wizard without importing.

#### What the import does

After you click **Import** in the wizard, the studio:

1. Samples the source image using bilinear interpolation along radial lines from the origin.
2. Maps the arc (θ_start → θ_end) to the horizontal (time) axis and the radius (0 → max_r) to the vertical (frequency) axis, with r = 0 at the bottom and r = max_r at the top, matching the standard NSGT convention.
3. Resizes the result to the project canvas height (bin count) using LANCZOS resampling.
4. Applies the selected RGB→TIFF channel mapping (same as a regular image import).
5. Adds the result as a new layer, exactly like any other image import.

Partial-arc imports produce a layer whose horizontal axis spans only the selected arc angle — one full revolution (360°) maps to a layer as wide as the output width setting; a half-revolution produces a layer half that width.

#### Open as Sound Flower

Every import type (audio, image, MIDI, and project) has an **Open as Sound Flower after import** checkbox at the bottom of the wizard. When ticked, the studio automatically enables Sound Flower polar view (`Ctrl+Alt+F`) once the import finishes. This is especially useful for polar image imports: import the flower → the result is immediately displayed back in polar form, so you can inspect and paint it without manually toggling the view.

---

## 15. Page Modes & RGB Channel Mapping

### Single-Page Modes

Each page is displayed as a grayscale image:

| Shortcut | View |
|----------|------|
| Ctrl+1 | Page 0 — Left channel amplitude |
| Ctrl+2 | Page 1 — Right channel amplitude |
| Ctrl+3 | Page 2 — Left channel phase |
| Ctrl+4 | Page 3 — Right channel phase |

### RGB Mode

`Ctrl+5` switches to RGB composite view, combining three TIFF pages into the R, G, and B display channels. The channel mapping determines which pages go where.

Projects open in RGB mode by default so you immediately see amplitude and phase together.

**View → RGB Channel Mapping** (submenu) offers three colour families, each with three variants:

**Yellow family** — L amplitude drives red, R amplitude drives green (mono audio appears yellow):

| Preset | R display | G display | B display |
|--------|-----------|-----------|-----------|
| **Yellow** | Page 0 (Amp L) | Page 1 (Amp R) | — (zero) |
| **Yellow phase** *(default)* | Page 0 (Amp L) | Page 1 (Amp R) | Phase of dominant channel |
| **Yellow balanced** | Page 0 (Amp L) | Page 1 (Amp R) | (Amp L + Amp R) / 2 |

**Purple family** — L amplitude drives red, R amplitude drives blue (mono appears magenta):

| Preset | R display | G display | B display |
|--------|-----------|-----------|-----------|
| **Purple** | Page 0 (Amp L) | — (zero) | Page 1 (Amp R) |
| **Purple phase** | Page 0 (Amp L) | Phase of dominant channel | Page 1 (Amp R) |
| **Purple balanced** | Page 0 (Amp L) | (Amp L + Amp R) / 2 | Page 1 (Amp R) |

**Cyan family** — R amplitude drives green, L amplitude drives blue (mono appears cyan):

| Preset | R display | G display | B display |
|--------|-----------|-----------|-----------|
| **Cyan** | — (zero) | Page 1 (Amp R) | Page 0 (Amp L) |
| **Cyan phase** | Phase of dominant channel | Page 1 (Amp R) | Page 0 (Amp L) |
| **Cyan balanced** | (Amp L + Amp R) / 2 | Page 1 (Amp R) | Page 0 (Amp L) |

*Phase of dominant channel* — uses left phase (page 2) where left amplitude ≥ right, otherwise right phase (page 3).

> Painting always targets pages 0 and 1 (amplitude). RGB mode is display-only.

### Light Mode

The **☀ Light** toggle button in the toolbar inverts the display for amplitude content, giving a white-background view where silence is bright and louder sounds appear as dark marks — like ink on paper.

| Mode | Affected pages | Effect |
|------|----------------|--------|
| Single page 0 or 1 | Amplitude L / R | Pixel values inverted: `255 − value` |
| RGB view | All three display channels | Each R/G/B channel inverted |
| Single page 2 or 3 | Phase L / R | **No change** — phase values have no "silence = dark" convention |

Light mode is purely cosmetic and has no effect on the underlying data, encoding, decoding, or any filter or analysis tool. It is especially useful when:

- Working in a bright environment where the default dark background causes eye strain
- Printing or exporting screenshots where white paper is the background
- Reviewing sparse sounds (many silent regions) where dark-on-white makes the content easier to spot

Light mode is not persisted between sessions — it resets to off on next launch.

### Follow Mode

The **Follow** toggle button in the playback panel locks the playback cursor at a fixed screen position (approximately 25% from the left edge of the viewport) and scrolls the spectrogram image underneath it as audio plays — rather than having the cursor sweep across a static image.

| Mode | Cursor behaviour | Viewport behaviour |
|------|-----------------|-------------------|
| Follow off (default) | Cursor moves right across the image | Image stays still; user scrolls manually |
| Follow on | Cursor stays at ~25% from left | Image scrolls left continuously during playback |

Follow mode is most useful when:

- Listening through a long piece and watching the spectrogram pass by like a score
- Monitoring live-mode output in real time
- Comparing spectrogram shape with what you hear without tracking the cursor with your eyes

Follow mode only activates while playback is running. When playback stops, the viewport stays at its current scroll position. The mode is not persisted between sessions.

---

## 16. Overlays & Grids

**Ctrl+G** opens the Overlay panel. All overlays are display-only and do not affect encoding or decoding.

### Axis Labels

| Setting | Options |
|---------|---------|
| **Vertical axis labels** | Off · Hz values · Note names (A4, C5, …) · Pixel coordinates · **Frequency Bands** (tick marks and names for perceptual bands: Sub-bass, Bass, Low-mid, Mid, High-mid, Presence, Brilliance) |
| **Horizontal axis labels** | Off · Seconds · Milliseconds · Pixel coordinates |

### Frequency Grid

Vertical lines marking specific frequencies.

| Setting | Description |
|---------|-------------|
| **Enabled** | Toggle the frequency grid |
| **Note grid** | Draws lines at each semitone (or selected subset) using A4 = reference Hz as anchor |
| **Reference Hz** | Tuning reference for A4 (default 440 Hz) |
| **Show all semitones** | When off, only natural notes are shown |
| **Harmonic series** | Draws lines at integer multiples of a fundamental frequency |
| **Fundamental Hz** | Root frequency for the harmonic series |
| **Harmonic count** | How many harmonics to display |
| **Custom frequencies** | Comma- or newline-separated Hz values for arbitrary lines |
| **Show labels** | Annotate grid lines with Hz or note names |

### Timing Grid

Horizontal lines marking moments in time (displayed vertically on the canvas).

| Setting | Description |
|---------|-------------|
| **Enabled** | Toggle the timing grid |
| **Mode** | **Interval** — fixed ms between lines. **BPM** — beat/bar grid derived from tempo. |
| **Interval (ms)** | Spacing in milliseconds (Interval mode) |
| **BPM** | Tempo in beats per minute (BPM mode) |
| **Beats per bar** | Time signature numerator |
| **Subdivisions** | How many subdivision lines per beat |
| **Offset (ms)** | Phase shift for the grid (useful if the downbeat doesn't start at t=0) |
| **Show labels** | Annotate beat/bar lines |
| **Bar / Beat / Subdiv colours** | Separate RGBA colours for bar lines, beat lines, and subdivision lines |

### Chord Overlay

A live frequency-grid preview for chords selected in the **Chord Generator** (see [Section 17](#17-chord-generator)). The overlay is independent of the Frequency Grid and can be enabled without it.

| Setting | Description |
|---------|-------------|
| **Enabled** | Toggle the chord overlay |
| **Notes colour** | RGBA colour for non-root chord tones |
| **Root colour** | RGBA colour for the root note (drawn with a heavier line) |
| **Show note labels** | Annotate each line with the note name and octave (e.g. "C4") |

> **Tip:** When you select a chord in the Chord Generator panel the overlay is enabled and updated automatically — you don't need to open the Overlay panel to see it.

### Snap to Grid

**View → Snap to Grid** is a toggle that causes all mouse-operated positioning to snap to the nearest active overlay grid line.

When enabled, the following operations snap automatically:

- **Paste placement** — dragging the floating paste quad
- **Selection repositioning** — moving or resizing a selection transform
- **Text tool** — drawing and repositioning text boxes
- **Curve tool nodes** — placing and dragging bezier nodes
- **Warp tool control points** — editing curve control points in the Warp tool
- **Layer transform handles** — translating and resizing layers interactively

Snap uses the **finest** active grid interval: subdivisions within BPM mode, individual notes or harmonics in the frequency grid, custom frequencies, etc.

> **Tip:** Enable the timing grid or frequency grid first (Ctrl+G), then toggle **Snap to Grid** on. If no grid lines are active, the toggle has no visible effect.
>
> **Note:** Snap currently applies in the default canvas orientation (r0) only.

---

## 17. Chord Generator

**Tools → Chord Generator** (`Ctrl+Shift+U`)

The Chord Generator creates chords or arpeggios and stamps them as a new MIDI layer. As you change the chord selection, the **Chord Overlay** in the Overlay panel updates live so you can see the notes on the frequency grid.

### Chord Section

| Control | Description |
|---------|-------------|
| **Root** | Root note: C through B |
| **Category** | Chord family: Triads · 6th · 7th · 9th · Extended / Added |
| **Chord** | Specific chord type within the selected category (36 types across all categories) |
| **Octave** | Octave of the root note (0–8; default 4) |
| **Notes** | Live read-only preview showing the MIDI note names of the selected chord |

### Mode

| Control | Description |
|---------|-------------|
| **Block Chord** | All chord notes are stamped simultaneously at column 0 |
| **Arpeggio** | Notes are sequenced one at a time according to the pattern settings below |

#### Block Chord settings

| Control | Description |
|---------|-------------|
| **Duration** | Note length in spectrogram columns |

#### Arpeggio settings

| Control | Description |
|---------|-------------|
| **Preset** | Note ordering pattern: Ascending · Descending · Up-Down · Down-Up · Alternating · Outside-In · Inside-Out · Random · Custom |
| **Custom** | Custom index sequence (e.g. `0,2,1,3`) when Preset is set to Custom |
| **Subdivision** | Note spacing as a musical subdivision: 1/1 · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 |
| **BPM** | Tempo in beats per minute; syncs with the project BPM and the Overlay panel timing grid |
| **Note dur.** | Duration of each note as a percentage of one subdivision step width |
| **Repeats** | Number of times to cycle through the full sequence |
| **Randomize** | Set Preset to Random and shuffle the note order |

### Instrument

| Control | Description |
|---------|-------------|
| **Program** | GM program number (0–127) used when rendering the MIDI layer |
| **Velocity** | Note velocity (1–127) |

### Stamping

Click **Stamp to MIDI Layer** to create a new layer named after the chord and mode. The notes are converted to spectrogram pixel rows using the project's frequency mapping and rendered using the same MIDI synthesis pipeline as imported MIDI files. The layer is immediately editable and re-renderable (e.g. via **Reload MIDI Programs**).

> **Note:** A project must be open and at least one layer must be loaded before stamping. The stamp always starts at column 0 (the beginning of the project).

### BPM synchronisation

The BPM spinbox in the Chord Generator, the BPM field in the Overlay panel's Timing Grid, and **Project Settings → BPM** are all kept in sync. Changing any one of them updates the others.

---

## 18. The Mind-Shot Editor

**Tools → Mind-Shot Editor** (`Ctrl+Shift+I`)

Mind-Shots are reusable brush stamps for the Mind-Shot tip. Each Mind-Shot stores a 4-page Sound Mind TIFF (amplitude L/R on pages 0–1, phase L/R on pages 2–3) alongside a greyscale kernel PNG and a metadata JSON file. When stamped, the amplitude and phase pages are applied to the canvas according to the **Amplitude pages** and **Phase pages** mode settings in the Tools panel (see [Section 6 — Tools Panel: Mind-Shot tip](#6-the-tools-panel)).

### Creating a Mind-Shot

Open the editor with **Tools → Mind-Shot Editor** (`Ctrl+Shift+I`). The top of the dialog contains four source buttons:

| Button | Source |
|--------|--------|
| **From selection** | Captures the current canvas selection as a 4-page Sound Mind TIFF; amplitude and phase data are preserved exactly as they appear in the project |
| **From image file…** | Opens a file picker for a PNG, JPEG, or other image; the image is converted to a Sound Mind TIFF via the standard channel-mapping pipeline (Yellow phase preset) |
| **From sound clip…** | Opens a file picker for a WAV, MP3, FLAC, or other audio file; the clip is encoded to a Sound Mind TIFF in the background using the project's current codec settings (sample rate, bins per octave, timestep, scale); a progress dialog appears and can be cancelled |
| **Record…** | Opens a recording dialog (see below); records audio directly from any connected input device and encodes it the same way as **From sound clip…** |

#### Recording from an input device

Clicking **Record…** opens a recording dialog with the following controls:

| Control | Description |
|---------|-------------|
| **Input device** | Dropdown listing all connected input devices; defaults to the system default input |
| **Level meter** | Live bar graph showing the RMS level of incoming audio while recording |
| **● Start Recording / ■ Stop Recording** | Toggle button; starts or stops capture; elapsed time (MM:SS.t) is shown while recording |
| **Use Recording** | Enabled after stopping; encodes the captured audio and opens the crop/resize preview |

After clicking **Use Recording** the recorded audio is encoded to a spectrogram swatch in a background thread (same progress dialog as **From sound clip…**). The crop and resize workflow is then identical to any other source type.

### Crop and Resize Workflow

After a source is loaded, a **source preview** appears above the canvas showing the full source image. A dashed yellow crop rectangle overlays the preview:

| Interaction | Effect |
|-------------|--------|
| Drag a **corner handle** | Resize the crop rectangle |
| Drag **inside** the rectangle | Move the rectangle |
| Drag **outside** the rectangle | Draw a new rectangle from scratch |
| Tiny click anywhere | Reset to the full source extent |

The **W** and **H** spinboxes below the preview set the output dimensions in pixels, independently of the crop selection. Set these before clicking **Apply crop**.

Click **Apply crop** to crop and resize the selected region and commit it to the swatch canvas. The source preview is then hidden and the canvas is ready for optional touch-up.

### Canvas Touch-Up

The swatch canvas below the source preview works the same as in previous versions: left-drag to paint, right-drag to erase, scroll to zoom. Use this step to refine details after cropping. The canvas always shows the amplitude pages in R/G (page 0 = red, page 1 = green).

### Saving a Mind-Shot

Click **Save** to write the Mind-Shot to the project's `instruments/` folder. Three files are created:

| File | Contents |
|------|----------|
| `<name>.png` | Greyscale kernel derived from the amplitude swatch |
| `<name>.tiff` | 4-page uint16 Sound Mind TIFF (amplitude L/R + phase L/R) |
| `<name>.json` | Metadata: `source_type`, `crop_rect`, `resize_hw`, `conversion_params` |

The original source file is also copied to `sources/instruments/` inside the project so the Mind-Shot can be regenerated or re-cropped later.

### Waveform Overview Strip

A 48 px waveform strip is displayed above the swatch canvas. It shows the per-column RMS amplitude of the swatch across both amplitude pages (pages 0 and 1), giving you an at-a-glance envelope view of the Mind-Shot content. Click or drag the strip to move the playback cursor (0–1 normalised position).

### Mind-Shot Size

Width and height spinboxes in the stamp controls accept values from 1 to **4 000 px**, allowing very large or very small Mind-Shot stamps. The aspect-ratio lock (**Constrain proportions**) is available for both axes.

### Fundamental Row

The **Fundamental row** control (visible in swatch mode, just above the brush controls) specifies which pixel row inside the swatch represents the Mind-Shot's fundamental frequency. This value is saved in the Mind-Shot's JSON metadata and used whenever the Mind-Shot is stamped via the MIDI brush.

**Visual indicator:** a teal horizontal line is overlaid on the swatch canvas at the current fundamental row, so you can see exactly where the pivot sits relative to the spectrogram content.

**Setting it:**

- **Click anywhere in the canvas** — a bare click (no drag) moves the fundamental-row line to that vertical position and updates the spinbox. This is the fastest way to align the line with a visible feature in the swatch.
- **Spinbox** — type a pixel row number or use the up/down arrows for fine adjustment. The line on the canvas updates immediately.
- Dragging (left-drag) still paints as usual; only a bare click (press and release without moving) repositions the fundamental row.

After applying a crop, the spinbox defaults to the vertical centre of the swatch (`H ÷ 2`).

When the MIDI brush paints a note at a target pitch row, the swatch is positioned so that the fundamental row aligns to that row. Because the NSGT spectrogram uses a log-frequency axis, this is an exact pitch-shift: the harmonics above and below the fundamental maintain their correct intervals regardless of which note is played.

The setting does not affect how the Mind-Shot is stamped by the regular Brush tool — it is only used by the MIDI brush. If not set (or if the Mind-Shot was saved before this feature existed), the vertical centre is used as the fallback.

### Amplitude and Phase Mode Controls

Once a Mind-Shot is loaded into the tool, two control groups in the **Mind-Shot tip** section of the Tools panel govern how its pages are applied:

**Amplitude pages (0 & 1)**

| Mode | Behaviour |
|------|-----------|
| Keep Mind-Shot amplitude | The Mind-Shot swatch's amplitude data is blitted directly to the canvas (default) |
| Use colour setting | The swatch's amplitude values act as an opacity mask; the tool's **R** and **G** colour values fill in through the mask |

**Phase pages (2 & 3)**

| Mode | Behaviour |
|------|-----------|
| Ignore | Phase pages on the canvas are left untouched under the stamp (default) |
| Keep Mind-Shot phase | The Mind-Shot's phase pages are blitted to the canvas |
| Use colour setting | The tool's **Phase R** and **Phase G** values fill in through the amplitude-derived mask |

### Sharing Mind-Shots Between Projects

The editor's library row (below the source buttons) contains three buttons for moving Mind-Shot definitions between projects and distributing them as files.

#### From project…

Opens a file picker for a `.smsproj` file. After selecting a project, a checkbox list shows every Mind-Shot definition found in that project. Check the Mind-Shots you want and click **OK**. The following files are copied from the source project into the current project's `instruments/` and `sources/instruments/` directories:

- `<name>.png` (kernel)
- `<name>.tiff` (swatch, if present)
- `<name>.json` (metadata, if present)
- Original source file under `sources/instruments/` (if recorded in the metadata and still present)

If a Mind-Shot with the same name already exists in the current project a numeric suffix is appended (`_2`, `_3`, …) so nothing is overwritten. The `source_path` field inside the copied JSON is updated to reflect the new location. After import, if exactly one Mind-Shot was selected it is loaded into the editor canvas for immediate use.

#### Export bundle…

Packages the currently named Mind-Shot (it must have been saved at least once) into a `.sminstr` file — a standard zip archive with the following structure:

```
instruments/<name>.png
instruments/<name>.tiff   (if present)
instruments/<name>.json   (if present)
sources/instruments/<name>.<ext>   (original source, if present)
```

Opens a save dialog defaulting to `<name>.sminstr`. The resulting file can be given to other users or imported into any Sound Mind Studio project.

#### Import bundle…

Opens a file picker for a `.sminstr` (or `.zip`) bundle. All Mind-Shots in the bundle are extracted into the current project's `instruments/` folder. Name collisions are resolved with numeric suffixes, and `source_path` references in JSON metadata are updated automatically. After import, if the bundle contained exactly one Mind-Shot it is loaded into the editor canvas.

### Using a Mind-Shot

1. Open the Mind-Shot Editor and create or load a Mind-Shot, then click **Save**.
2. Select the **Brush** tool and set **Tip** to **Mind-Shot** in the Tools panel.
3. Set **Instr Width** and **Instr Height** to scale the stamp at paint time.
4. Choose **Amplitude pages** and **Phase pages** modes as needed.
5. Paint on the canvas — the saved Mind-Shot is stamped at each stroke point.
6. The Curve tool also accepts the Mind-Shot stamp: set its paint mode to **Mind-Shot**.

---

## 20. Encoding & Decoding Audio

### Encode Audio → TIFF (`Ctrl+E`)

Converts a WAV, MP3, FLAC, or OGG file into a Sound Mind TIFF.

| Option | Default | Description |
|--------|---------|-------------|
| Sample rate | 44100 Hz | Target sample rate; input is resampled if necessary |
| Bins per octave | 75 | Frequency resolution. Higher = more pitch detail, larger files. |
| Timestep | 10.0 ms | Milliseconds per time-axis pixel |
| TIFF variant | stripped | `stripped` (fast, sequential) or `tiled` (random-access, better for long files) |
| Tile size | 256 px | Tile dimension (tiled variant only) |
| GPU | off | Use GPU acceleration (CUDA → OpenCL → CPU fallback) |

Files longer than 30 seconds are split into snippet TIFFs automatically.

> **Important:** Record the `bins per octave` value used when encoding. Decoding with a different value produces incorrect audio.

### Decode TIFF → Audio (`Ctrl+D`)

Converts the current TIFF back to a WAV file. The decode dialog asks for:

| Option | Description |
|--------|-------------|
| Bins per octave | Must match the value used during encoding. Run the `info` command or check the file metadata if unsure. |
| GPU | Use GPU acceleration |

Output is always a stereo WAV file.

### Decode Rendered → WAV (`Ctrl+Shift+D`)

Renders the project (if not already rendered) and decodes the Rendered layer to a WAV file in the project folder. The file is named after the project. The volume gain set via the **Adjust…** toolbar button is applied at this stage.

### Bake (`Ctrl+Shift+B` for projects; `Studio → Bake TIFF…` for standalone TIFFs)

**Bake** collapses the render → decode → re-encode pipeline into a single command and imports the result back into the current session.

#### Project bake

1. **Render** — the composite of all visible layers is rendered to the Rendered layer (skipped if nothing has changed since the last render).
2. **Decode** — the Rendered TIFF is decoded to audio (`<projectname>_baked.wav` in the project `media/` folder).
3. **Re-encode** — the audio is re-encoded back to a new Sound-Mind TIFF (`<projectname>_baked.tiff`) using the project's codec settings (sample rate, bins-per-octave, timestep).
4. **Import** — the baked TIFF is added as a new layer named **`<projectname> Baked`** with **Normal** blend mode at the top of the layer stack and the project is saved.

**Dirty-bit optimization** — a flag tracks whether any layer has changed since the last successful bake. If the flag is clear and `<projectname>_baked.tiff` already exists, steps 2 and 3 are skipped and the existing file is imported directly.

#### Standalone TIFF bake

When a single TIFF is open without a project (`Studio → Open TIFF…`), **Studio → Bake TIFF…** decodes the current TIFF to audio and re-encodes it as `<name>_baked.tiff` beside the original, then opens the result in the viewer.

#### When to use Bake

| Scenario | Why Bake helps |
|----------|----------------|
| You painted on a spectrogram and want a clean audio round-trip | Confirms exactly what the codec will reproduce |
| You want to flatten multiple layers into one for further editing | Bake collapses the composite into a single layer |
| You want a reference layer at the current mix state | Bake captures the mix at this moment; further edits are compared against it |

### Background Processing

Encoding and decoding run in background threads. Progress appears in the status bar. Editing remains available during processing.

---

## 19. MindGrains

MindGrains bring **live-canvas granular synthesis** to Sound Mind Studio. Instead of sampling from a pre-recorded audio file, each grain is extracted from the composited spectrogram layers that sit beneath a MindGrain layer in the stack — so the grain source evolves in real time as you edit the canvas.

### What is a MindGrain Layer?

A **MindGrain layer** (`layer_type = "mindgrain"`) holds no pixel data of its own. It defines:

- A **column slice** (`start_x` to `stop_x`) of the canvas to treat as a sample bank.
- Default **granular parameters** that the Brush tool's MindGrain tip will use unless overridden in the Modulation panel.

The layer is transparent during compositing — it acts purely as a source definition.

### Adding a MindGrain Layer

1. Open the Layers panel.
2. Click **+** → **Add MindGrain…**.
3. In the dialog:
   - Enter a **name**.
   - Set **Start column** and **Stop column** — the horizontal range of the canvas this MindGrain will sample from. The preview strip shows the selected region.
   - Configure the default granular parameters (see table below).
4. Click **OK**. The new layer appears above the currently active layer.

### MindGrain Parameters

| Parameter | Description |
|-----------|-------------|
| **Start / Stop column** | Bounds of the canvas slice used as the granular sample bank |
| **Scan position** | Normalised read-head position within the slice (0 = left edge, 1 = right edge) |
| **Scan width** | Fraction of the slice available for random grain extraction |
| **Spray X** | Per-stamp horizontal jitter of the read position (px) |
| **Spray Y** | Per-stamp vertical jitter of the read position (px) |
| **Grain duration** | Width of each extracted grain in spectrogram columns |
| **Envelope** | Amplitude window applied across the grain width (`hann`, `triangle`, `flat`, `fade_in`, `fade_out`, `ramp_up`, `ramp_down`) |
| **Direction** | `forward`, `reverse`, or `ping_pong` — reversal direction of the grain, resolved per stamp |
| **Pitch shift** | Default vertical row shift in semitones applied to the grain |
| **Time stretch** | Default horizontal scale of the grain before stamping |
| **Amplitude** | Default grain amplitude scaling factor |

All default parameters can be overridden per stroke via the Brush tool's **Modulation** section.

### Painting with MindGrains

1. Select the **Brush** tool.
2. In the **Tip** row, choose **MindGrain**.
3. Select the target MindGrain layer from the **Source:** dropdown.
4. Adjust granular modulation in the **Modulation** section (scatter, pitch shift, time stretch, etc.).
5. Paint on any normal layer — each stamp extracts and places a grain from the MindGrain buffer.

### The MindGrain Buffer

When you first paint, the buffer for a MindGrain layer is built by compositing all layers below it in the stack (restricted to the configured column slice). The buffer is stored as a uint16 (H × slice_width × 4) array matching the project's full canvas height.

The buffer is automatically **invalidated** whenever any layer below the MindGrain is modified. It rebuilds on the next stamp.

> **Tip:** place the MindGrain layer high in the stack so that rich, layered audio content contributes to the grain source.

---

## 21. Importing Media as Layers

### Import Wizard and Drag-and-Drop

All media types are imported through a single **Import Wizard** dialog, which can be opened in two ways:

- **Studio → Import…** (`Ctrl+Shift+I`) — opens the wizard with no pre-selection; choose a type and browse for files.
- **Drag and drop onto the canvas** — drag one or more files from Explorer / Finder directly onto the Sound Mind Studio window. The wizard opens automatically with the file type pre-detected and the files already loaded.

The wizard has four **Import type** radio buttons. Dropping a file auto-selects the right one based on the file extension:

| Extension | Detected type | Wizard page shown |
|-----------|---------------|-------------------|
| `.wav` `.mp3` `.flac` `.ogg` `.aiff` `.m4a` `.opus` | Audio | Layer vs MindShot choice |
| `.png` `.jpg` `.jpeg` `.bmp` `.tga` `.webp` | Image | Channel mapping + size mode |
| `.mid` `.midi` | MIDI | Information text (no options needed) |
| `.smsproj` | Project | Checkbox list of importable content |
| `.tiff` / `.tif` (Sound Mind format) | *(silent)* | No dialog — added directly as a layer |

You can drag **multiple files of the same type** at once; the wizard opens once for the whole group and applies your settings to all of them.

If you switch the Import type radio button manually, any pre-supplied file paths are cleared and you are prompted to browse again.

---

### Importing Audio Files

WAV, MP3 (requires librosa), FLAC (requires librosa), OGG (requires librosa).

The audio is copied to `sources/`, then encoded to one or more TIFFs in `media/` using the project's codec settings.

| Option | Description |
|--------|-------------|
| Snippet duration | If set, the audio is split into snippets of this duration (seconds). Each snippet becomes a separate layer, matching the project timeline. Leave blank to encode the whole file as one layer. |

### Importing MIDI Files

`.mid` and `.midi` files.

MIDI notes are stored as **editable note events** (a `MidiBrushOp`) on an empty layer, then rendered to the spectrogram using the active instrument profiles. This means:

- The imported layer immediately looks and sounds like a normal spectrogram layer.
- The original note events are preserved, so you can **reload** the layer after editing the profiles without re-importing the MIDI file.
- Individual notes appear in the Op Log and can be removed or copied.

**Instrument profiles:**

MIDI rendering reads instrument parameters from `midi_profiles.json` — the project-local copy in `media/` if it exists, otherwise the bundled default (covers all 128 GM programs and 47 standard drum notes). To customise any instrument, use **Project → MIDI Instrument Editor…**; see [Section 35 — MIDI Instrument Editor](#35-midi-instrument-editor) for full documentation. After saving, click **Reload MIDI Programs** in the Brush panel to re-render all MIDI layers.

When a custom profiles file is selected during import, it is copied into the project's media folder so the project remains self-contained.

**Workflow:**

1. **Ctrl+Shift+A** → select "From MIDI file (.mid, .midi)" → Browse to the MIDI file.
2. Choose **Single layer** or **One layer per MIDI program**.
3. To change the encoding mode or instrument profiles, click **▶ Advanced settings** and adjust as needed (defaults: Synth encoding, bundled profiles).
4. Click **OK** — synthesis begins in background.
5. When complete, the new layer(s) appear in the Layers panel.
6. Play back, paint, or filter the layer; **Project → Render Project** to render; **Ctrl+Shift+D** to decode.

> **Snippet splitting:** If the project has an existing background layer, MIDI layers are split into snippets of the same duration as that layer. This keeps all layers aligned on the project timeline.

> **Note:** MIDI drums use MIDI channel 9 (0-indexed). Each drum note maps to a frequency row based on its MIDI note number — this is a visual convention for spectrogram display, not a pitch representation. Drum layers are labelled "Drums" and kept separate when using per-program mode.

### Importing Image Files

PNG, JPG, BMP, and any format supported by Pillow.

The image is copied to `sources/`, then converted to a Sound Mind TIFF.

**Import size** controls how the image dimensions are handled during conversion. Five single-image modes are available, plus a **Sequential** mode that appears only when multiple files are selected:

| Mode | Height | Width | When to use |
|------|--------|-------|-------------|
| **Scale to canvas height, preserve aspect ratio** *(default)* | Project canvas height (bin count) | Scaled proportionally | Standard use — the layer fills the full frequency range and is width-proportional to the original |
| **Squash to canvas height, keep native width** | Project canvas height | Original image width | Use when the image width carries meaningful content (e.g. a timeline or pixel art) and you don't want horizontal compression |
| **Stretch to fill project canvas** | Project canvas height | Project canvas width | Forces the image to exactly match the project dimensions, ignoring aspect ratio; useful for backgrounds or textures. Requires a loaded project layer as the size reference |
| **Stretch width to project canvas width, preserve aspect ratio** | Scaled proportionally | Project canvas width | Scales the image to the project width; the height adjusts so the image is not horizontally distorted. Requires a loaded project layer |
| **Native image dimensions** | Original image height | Original image width | Import at full resolution — the layer may be taller or shorter than the project canvas |
| **Sequential** *(multi-file only)* | Project canvas height | Aspect-ratio scaled | Each image is placed end-to-end using a layer transform; see below |
| **Polar** *(single file only)* | Project canvas height | User-defined (default ≈ 2π × radius) | Un-warps a polar/flower-encoded image to rectangular; click **Set origin…** for the graphical picker |

> **Size reference:** the stretch and canvas-height modes derive their target dimensions from the first loaded project layer. If no project layer has been loaded yet, they fall back to aspect-ratio scale.

> **Polar import:** full documentation — coordinate convention, origin picker controls, and the arc-angle and output-width options — is in the [Sound Flowers](#sound-flowers) section above.

#### Sequential multi-image import

When **two or more image files** are selected in the file chooser, a **Sequential** radio button appears alongside the other size modes.

In Sequential mode each file is imported as a separate layer. Files are sorted **lexicographically** before processing, so if you select `a.png`, `c.png`, and `b.png` they are arranged left-to-right as `a → b → c`. Layers are added to the stack in this same order.

Each image is scaled to the project canvas height with its aspect ratio preserved. A `translate_x` layer transform is applied automatically so that each image's left edge abuts the right edge of the previous image:

- The first image starts at column 0.
- Each subsequent image's offset = sum of the widths of all preceding images.
- If the cumulative offset reaches or exceeds the project canvas width, the next image wraps back to column 0 (hard reset, not modulo — the remainder is discarded).
- The last image may extend past the right edge of the canvas; this is by design.

To adjust the position of any layer after import, use the **Layer Transform** tool (`T`).

When Sequential is not selected, multi-file import creates one layer per file, all starting at column 0 — equivalent to importing them one at a time.

> **Native dimensions note:** If the imported layer's height differs from the project canvas height, the compositing engine handles the mismatch gracefully: areas of the layer that fall outside the canvas are clipped, and areas within the canvas where the layer does not reach are treated as transparent (silence) for that layer. Non-standard heights work correctly but the layer will not fill the full frequency range unless a layer transform is used to scale it.

**Channel mapping** controls how the image's RGB channels map to the four TIFF pages. The default (**Yellow**) is correct for most image imports and is hidden under **▶ Advanced settings** in the Add Layer dialog. Three colour families are available, each in three variants:

**Yellow** — R→Amp L, G→Amp R:

| Preset | Page 0 (Amp L) | Page 1 (Amp R) | Pages 2 & 3 (Phase) |
|--------|---------------|---------------|---------------------|
| **Yellow** | R | G | B (both) |
| **Yellow pure** | R | G | zero |
| **Yellow alt** | (R+B)/2 | (G+B)/2 | zero |

**Purple** — R→Amp L, B→Amp R (Yellow with G↔B swapped):

| Preset | Page 0 (Amp L) | Page 1 (Amp R) | Pages 2 & 3 (Phase) |
|--------|---------------|---------------|---------------------|
| **Purple** | R | B | G (both) |
| **Purple pure** | R | B | zero |
| **Purple alt** | (R+G)/2 | (B+G)/2 | zero |

**Cyan** — B→Amp L, G→Amp R (Yellow with R↔B swapped):

| Preset | Page 0 (Amp L) | Page 1 (Amp R) | Pages 2 & 3 (Phase) |
|--------|---------------|---------------|---------------------|
| **Cyan** | B | G | R (both) |
| **Cyan pure** | B | G | zero |
| **Cyan alt** | (B+R)/2 | (G+R)/2 | zero |

The *pure* variants encode no phase (phase vocoder will reconstruct with zero phase). The *alt* variants blend the amplitude pages with the third image channel. The import families mirror the RGB visualisation families: a file imported with the default **Yellow** mapping displays in its original colours when viewed with the default **Yellow phase** RGB view preset — the blue channel is stored in the phase pages and recovered in the blue display channel.

**Project columns** — if set, wide images are split into chunks of this many columns, producing multiple layers.

### Importing Existing TIFFs

A TIFF file is copied into `media/` if it is not already there, with a numeric suffix added if a file with the same name already exists.

---

## 22. Converting Images to Audio

**Studio → Convert Image → TIFF** (`Ctrl+I`)

Converts any image file to a standalone Sound Mind TIFF. The image's pixel values become spectrogram amplitude data; decoding the resulting TIFF produces audio whose spectrum matches the image pattern.

The dialog defaults to sensible values (44100 Hz · 23 ms/px · stripped · Yellow mapping) and shows only a summary line. Click **▶ Advanced settings** to change any of the following:

| Option | Default | Description |
|--------|---------|-------------|
| Sample rate | 44100 Hz | Stored in the TIFF metadata |
| Timestep | 23 ms/px | Milliseconds per pixel — determines the implied audio duration |
| TIFF variant | stripped | `stripped` for clips; `tiled` for long files |
| Channel mapping | Yellow | How RGB → TIFF pages (Yellow / Purple / Cyan families — same presets as layer import) |

After conversion, decode the TIFF to hear what the image sounds like.

---

## 23. The Op Log

The op log is a persistent, append-only record of every significant editing operation performed on a project. It is stored inside the `.smsproj` file (as a JSON array) and is used by the **Remaster** and **Re-edit** features.

### What Gets Logged

#### Pixel ops

| Op type | Logged by |
|---------|-----------|
| **paint_stroke** | Any brush tool (Airbrush, Smudge, Stamp, Mind-Shot) |
| **fill_gradient** | Fill tool (Apply) |
| **curve** | Curve tool (Apply) |
| **paste_region** | Select tool (paste committed) |
| **clear_region** | Select tool (Cut or Delete) |
| **filter** | Any filter dialog (Apply) |
| **warp** | Warp tool (Apply) |
| **text** | Text tool (Apply) |

Each pixel op entry records the full set of parameters needed to reproduce it from scratch: layer ID, geometry, brush settings, gradient stops, filter parameters, etc. For paste operations, the swatch data is saved as a `.npy` file in the project's `ops/` folder.

#### Stack ops

| Op type | Logged by |
|---------|-----------|
| **layer_add** | Project → Add Layer |
| **layer_remove** | Deleting a normal layer |
| **layer_reorder** | Drag-reorder in the Layers panel |
| **layer_set_visible** | Visibility checkbox in the Layers panel |
| **layer_set_opacity** | Opacity slider in the Layers panel |
| **layer_set_blend_mode** | Blend mode combo in the Layers panel |
| **layer_rename** | Rename button (✎) |
| **layer_set_transform** | Transform editor (⊞) |
| **adjustment_layer_add** | Project → Add Adjustment Layer |
| **adjustment_layer_edit** | Edit… button on an adjustment layer row |
| **adjustment_layer_remove** | Deleting an adjustment layer |

Stack ops describe structural or property changes. They appear in the Op Log in amber and do not have an enable/disable checkbox (they cannot be selectively skipped during Remaster).

### Viewing the Op Log — the Op Log Dock

**View → Op Log** opens the Op Log dock, which shows all operations as a flat chronological list ordered from first to last. Each row has four columns:

| Column | Content |
|--------|---------|
| **#** | Sequential index (e.g. `▶ 042`) — `▶` marks the last-applied op (the op cursor position) |
| **Op · Layer** | A short description of the operation and the name of the layer it applies to |
| **Time** | Wall-clock timestamp of when the op was recorded |
| **On** | Enable/disable checkbox for pixel ops (absent for stack ops) |

- **Pixel ops** — have an enable/disable checkbox; unchecking skips the op during Remaster without deleting it.
- **Stack ops** — shown in amber; always applied during Remaster and cannot be individually disabled.
- **Future ops** — ops after the op cursor are greyed-out; they exist in the log but have not yet been applied (they appear after an undo that discards future ops).
- Double-click any re-editable pixel op to switch to its layer and reopen the editor with the original parameters.
- Click **Remaster…** to rebuild the active layer from the op log from scratch.

---

## 24. Re-edit Mode

Re-edit mode lets you go back to a previously applied operation and change its parameters. Instead of undoing and redoing everything, you adjust the original operation and remaster the layer from the log.

### Entering Re-edit Mode

1. Select the **Pick** tool.
2. Click on the canvas where you want to identify an operation.
3. **Double-click** the highlighted operation (or double-click any Pick selection).

The studio enters re-edit mode for that operation's type:

| Op type | Re-edit experience |
|---------|-------------------|
| **fill_gradient** | The Fill tool is activated with the original selection and gradient restored; all controls are editable |
| **curve** | The Curve tool is activated with the original path, gradient, and brush settings restored |
| **paste** | The paste is re-entered in floating mode with the original swatch; opacity and blend mode are restored |
| **filter** | The original filter dialog is reopened with its parameters pre-filled |
| **warp** | The Warp tool is activated with the original selection and path |
| **text** | The Text tool is activated with the original text, font, colour, and bounding-box corners restored |

### Making Changes

Edit the restored operation exactly as if you were creating it fresh — move nodes, change the gradient, adjust filter parameters, change the blend mode, etc.

### Committing Re-edits

Click **Apply** (for filters, curve, fill, warp) or press **Enter** (for paste). The entry in the op log is updated with the new parameters, and the layer is **remastered** — rebuilt from scratch by replaying all ops in order, with your changes incorporated.

### Cancelling

Press **Escape** or close the dialog to cancel. The op log is not modified.

> Re-edit mode always triggers a Remaster of the affected layer. For layers with many ops this may take a few seconds. Progress is shown in the status bar.

---

## 25. History Panel & Undo/Redo

### Undo / Redo

| Action | Shortcut |
|--------|----------|
| Undo | Ctrl+Z |
| Redo | Ctrl+Y |

The undo stack tracks all painting strokes, filter applications, cut/paste operations, layer property changes, and reordering. The stack holds up to **50 operations**. Brush strokes are stored as pixel deltas (before/after snapshots of the changed region), not full layer copies.

**What is undoable:**

- Brush strokes (Airbrush, Smudge, Stamp, Mind-Shot)
- Curve, Fill, Warp applications
- Cut, paste, delete operations
- Filter applications (including Preview/Cancel — Cancel itself is undoable)
- Layer property changes (opacity, blend mode, visibility, transform)
- Layer reordering
- Adjustment layer parameter edits
- Re-edit / Remaster results

### History Panel

**Ctrl+H** toggles the History panel. It shows recent paint strokes from the undo stack, with the most recent at the top. For a full chronological view of all operations including filters, fills, and layer changes, use the **Op Log** dock (see [Section 21](#21-the-op-log)).

### Op Log Dock

**View → Op Log** opens the Op Log dock, which shows the full project op log as a flat chronological list (see [Section 21](#21-the-op-log) for details). The Op Log complements the History panel: it shows the persistent semantic record of every significant operation, regardless of how many times the undo stack has been cleared or the project has been reopened. Double-clicking an entry automatically switches the active layer to match the op's layer.

---

## 26. Remaster

**Tools → Replay Op Log…** (`Ctrl+Shift+R`)

Remaster rebuilds one or all layers from scratch by replaying the op log. This is useful when:

- You re-edit an operation and want to see the cumulative result.
- A layer's TIFF has been corrupted or accidentally overwritten.
- You want to apply op log operations to a fresh or resized layer.

### How Remaster Works

1. For each layer being remastered, the layer's pixel data is reset to zeros.
2. All op log entries for that layer are replayed in order, using `apply_to_layer()` for each op.
3. Disabled ops (toggled off in the op log view) are skipped.
4. The layer's TIFF is written to disk when the remaster completes.

Progress is shown in the status bar. The operation runs in a background thread; editing is paused during remaster.

The **Remaster** dialog shows an info summary and a **▶ Codec settings** section (collapsed by default). To remaster with the current codec settings, click **Remaster…** without expanding. To change sample rate, bins per octave, timestep, or TIFF variant before rebuilding, expand the section first. Stroke geometry is preserved through codec changes — painted content stays at the correct audio time/frequency positions.

---

## 27. Project Settings

**Project → Project Settings**

The "Discard future ops" preference is shown directly. All codec parameters are under **▶ Codec settings** (collapsed by default).

| Setting | Description |
|---------|-------------|
| **Discard future ops** | Whether to confirm before discarding redo history when a new edit is made after undo (Ask / Always / Never). |
| *(▶ Codec settings)* | |
| **Sample rate** | Audio sample rate in Hz (default: 44100). Affects encoding of new audio layers and decoding of the rendered output. |
| **Bins per octave** | NSGT frequency resolution (default: 75). More bins = more pitch detail, larger files, slower encoding. Must be consistent across a project. At 75 bpo the spectrogram is 724 rows tall. |
| **Timestep (ms)** | Milliseconds per time-axis pixel (default: 10.0). Smaller = more time detail but wider images. |
| **Frequency scale** | Variable-Q (music-optimal zone-based bpo) or Log (uniform bpo across the full range). |
| **TIFF variant** | `stripped` — fast sequential access, good for short clips. `tiled` — 256×256 px tiles, enables fast random access for long files. |
| **Tile size** | Tile width/height for the tiled variant (default: 256 px). |

Changing codec settings does not re-encode existing layers. They apply to new audio imports and to the decode of the rendered output.

---

## 28. File Formats

### Sound Mind TIFF (`.tiff`)

The primary format. Four 16-bit grayscale pages, LZW-compressed. Height equals the NSGT bin count (724 px at the default 75 bins-per-octave); width varies with duration. Two variants:

- **Stripped** — horizontal strips. Fast to write and read sequentially. Best for clips up to ~30 seconds.
- **Tiled** — 256×256 px tiles. Enables random-access seeking. Best for full songs.

Metadata (sample rate, hop length, duration, codec version) is stored in the TIFF `ImageDescription` tag. See `SOUND_MIND_TIFF_SPEC.md` for the full format specification.

### Project File (`.smsproj`)

UTF-8 JSON. Stores layer list (names, paths, blend modes, opacity, transforms, source provenance) and the complete op log. TIFF paths are stored relative to the project file for portability.

### Mind-Shot Files

- `*.png` — kernel (grayscale float32 alpha mask)
- `*.tiff` — swatch (4-page uint16 Sound Mind TIFF: amplitude L/R + phase L/R), same basename as PNG
- `*.json` — metadata (source type, crop rect, resize dimensions, conversion params)

Stored in `instruments/` inside the project folder.

### Audio Formats

| Format | Input | Output |
|--------|-------|--------|
| WAV | Yes | Yes (decoded output) |
| MP3 | Yes (requires librosa) | No |
| FLAC | Yes (requires librosa) | No |
| OGG | Yes (requires librosa) | No |

### Image Formats

Input for Convert Image and layer import: PNG, JPG, BMP, and any format supported by Pillow.

---

## 29. Analysis Tools

The **Analysis** menu provides tools that measure and visualise properties of the active layer directly from the amplitude and phase pages — no decoding to audio is required.

---

### Loudness Meter (`Ctrl+Shift+L`)

**Analysis → Mastering & Sound Engineering → Loudness Meter**

The Loudness Meter is a floating dock that reports ITU-R BS.1770-4 loudness and dynamics metrics for the active layer. All values are derived directly from the amplitude pages (pages 0 and 1) — no codec round-trip is needed.

#### Opening the meter

Open it from **Analysis → Mastering & Sound Engineering → Loudness Meter** or press **Ctrl+Shift+L**. The dock appears on the right side of the window and can be floated, resized, or closed like any other panel.

#### Readouts

| Field | Description |
|-------|-------------|
| **Integrated LUFS** | BS.1770-4 K-weighted loudness over the analysed region (LUFS). The streaming compliance target for Spotify is −14 LUFS; Apple Music is −16 LUFS. |
| **Short-term LUFS** | K-weighted loudness over the most recent 3-second window of the analysed region. |
| **Momentary LUFS** | K-weighted loudness over the most recent 400 ms window. |
| **RMS L / R** | Root-mean-square amplitude for each channel expressed in dBFS. Unweighted. |
| **Peak L / R** | Maximum single pixel value in the region for each channel, expressed as dBFS. |
| **Crest L / R** | Peak dBFS minus RMS dBFS per channel — higher values indicate a more dynamic signal. |
| **PLR (dynamic range)** | Peak-to-Loudness Ratio: the gap between the peak level and integrated LUFS. A well-mastered track typically has a PLR between 8 and 14 dB. Over-compressed material has a low PLR. |
| **Duration** | Wall-clock length of the analysed column range. |

All LUFS values use the ITU-R BS.1770-4 K-weighting transfer function (high-shelf pre-filter + RLB high-pass) evaluated at each NSGT frequency bin's centre frequency. This is exact for log-spaced bins rather than the discrete-time approximation used by most loudness tools.

#### Buttons

**Analyse Full Layer** — measures the whole active layer from column 0 to the last column. Use this for a final loudness check before exporting.

**Analyse Selection** — measures only the column range of the current selection (enabled when a selection is active with the Select tool). Use this to check loudness in a specific segment or loop region.

#### Live playback updates

When the project is playing back, the meter updates every ~10 ms (one NSGT column). Short-term and momentary LUFS follow the playback cursor position. Integrated LUFS always shows the full layer value. This gives you a real-time dynamics display during preview — no action required.

#### How loudness is calculated

Sound Mind stores amplitude as 16-bit pixels. The conversion to dBFS is:

```
dBFS = (pixel / 65535) × 96 − 96
```

Zero → −96 dBFS (silence); 65535 → 0 dBFS (full scale). The loudness engine converts pixels to linear power, applies K-weighting per frequency row, sums L and R channels, and computes LUFS as:

```
LUFS = −0.691 + 10 × log₁₀(mean K-weighted power)
```

This follows BS.1770-4 exactly. The only approximation is that the NSGT bins are treated as having a constant gain equal to the K-weight at their centre frequency rather than integrating over the bin's full bandwidth — negligible for the log-spaced bins used by Sound Mind.

#### Streaming compliance targets

| Platform | Target integrated LUFS | Max true-peak |
|----------|------------------------|---------------|
| Spotify | −14 LUFS | −1 dBTP |
| Apple Music | −16 LUFS | −1 dBTP |
| YouTube | −14 LUFS | −1 dBTP |
| Tidal | −14 LUFS | −1 dBTP |
| Amazon Music | −14 LUFS | −2 dBTP |
| Broadcast (EBU R128) | −23 LUFS | −1 dBTP |

Note: Sound Mind reports **sample peak**, not true-peak (no inter-sample interpolation). True-peak can exceed sample peak by up to +3 dB on heavily limited material. Allow 1–2 dB of headroom below the platform limit when targeting from sample-peak readings.

---

---

### Pitch Tracker Overlay (`Ctrl+Shift+P`)

**Analysis → Vocal Coaching → Pitch Tracker Overlay**

The Pitch Tracker detects the fundamental frequency (F0) column-by-column from the left amplitude page and draws a continuous pitch curve directly on the spectrogram canvas. No decoding to audio is required.

#### Opening the tracker

**Analysis → Vocal Coaching → Pitch Tracker Overlay** or **Ctrl+Shift+P**. The dock appears on the right side and can be floated or docked like any panel.

#### Detection method

The tracker uses the **lowest-strong-partial** algorithm:

1. Scan each column's amplitude spectrum from the lowest frequency upward.
2. The first bin whose normalised amplitude exceeds the **Threshold** is the candidate fundamental.
3. Check bins at 2×, 3×, and 4× that frequency — if at least **Min harmonics** of these exceed the **Harmonic threshold**, the candidate is accepted as F0.
4. Columns where no candidate is confirmed are marked unvoiced (no curve point drawn).

This approach is robust on spectrogram data where partials are already isolated by the NSGT transform.

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Threshold** | 0.15 | Normalised amplitude (0–1) a bin must exceed to be a candidate F0. Raise to suppress noise; lower to track quiet singing. ≈0.15 → −16 dBFS. |
| **Harmonic threshold** | 0.05 | Amplitude required at harmonic multiples (2×, 3×, 4×) to confirm the fundamental. ≈0.05 → −26 dBFS. |
| **Min harmonics** | 1 | How many harmonic checks (of 3) must pass. Set to 0 to skip harmonic confirmation; set to 3 to require all three. |
| **F0 min / max** | 50 – 2000 Hz | Search range for the fundamental. Set to the expected vocal or instrument range to avoid octave errors. |
| **A4 reference** | 440.0 Hz | Reference tuning for note-name annotation. |
| **Show note labels** | On | Toggle note name + cents annotation on the canvas. |

#### Canvas overlay

The pitch curve is drawn as a coloured polyline on top of the spectrogram:

- **Green** — in tune (≤15 ¢ deviation from the nearest equal-temperament pitch)
- **Red** — sharp or flat (>15 ¢ deviation)

Note name labels (e.g. `A4`, `C#3 +12¢`) are drawn at ~80-pixel intervals along the curve. The curve is split wherever there is an unvoiced gap.

#### Dock readout (live during playback)

| Field | Description |
|-------|-------------|
| **Note** | Note name and octave at the current playback column |
| **Frequency** | F0 in Hz at the current column, or "unvoiced" |
| **Cents** | Deviation from the nearest equal-temperament pitch (−50 … +50 ¢) |
| **Voiced cols** | Number of columns where a pitch was detected / total columns analysed |

#### Buttons

**Detect Pitch** — runs the detector on the full active layer and draws the overlay. Detection time is proportional to image width; a 3-minute piece at 10 ms/column takes under a second.

**Clear** — removes the pitch curve from the canvas and resets the readout.

#### Interpreting the overlay

- A long unbroken green curve indicates a stable, in-tune tone.
- Short red segments indicate momentary pitch drift or ornaments.
- Long red sections suggest the singer or instrument is consistently sharp or flat relative to A4 = 440 Hz — try adjusting the A4 reference if the piece is in a different tuning.
- Gaps in the curve (unvoiced regions) correspond to silence, noise, or polyphonic material where no clear fundamental was found.
- The curve follows the lowest strong partial, so on chords or harmonically rich material it will track the bass note.

#### Multi-candidate mode

Enable the **Multi-candidate** group to detect up to N independent pitch hypotheses simultaneously — useful when the recording contains a second instrument or background vocal alongside the target voice, causing the tracker to jump between the two.

**How it works:**
For each column the detector collects *all* candidate fundamentals that pass the harmonic threshold, skipping any frequency that is a harmonic of an already-accepted candidate. Candidates are sorted by amplitude (strongest = Track 1). Up to *Max candidates* independent tracks are returned.

**On the canvas:**
Each enabled track is drawn in a distinct colour with its own polyline:

| Track | Colour | Style |
|-------|--------|-------|
| Track 1 (primary) | Green | Solid |
| Track 2 | Cyan | Dashed |
| Track 3 | Yellow | Dashed |
| Track 4 | Magenta | Dashed |
| Track 5 | Grey | Dashed |

Note labels (note name + cents) are shown only on the primary track.

**Discarding unwanted lines:**
In the dock, each track appears with a checkbox showing its voiced column count. Uncheck any track to hide it from the canvas and exclude it from the assembled primary result. The remaining enabled tracks are merged — for each column, the highest-priority (lowest track number) enabled candidate is used. This assembled result also feeds the Vibrato Analyzer and the Formant Display.

**Typical workflow for a noisy recording:**
1. Set **Max candidates** to 3 and click **Detect Pitch**.
2. Inspect the canvas — you will see up to 3 overlapping lines.
3. If Track 1 (green) is tracking the wrong voice (e.g. the accompaniment), uncheck it.
4. Track 2 (cyan) becomes the primary; note labels update accordingly.
5. Open the **Formant Display** dock and click **Detect Formants** — the formant tracking will use the new primary track as its F0 floor.

---

### Formant Display (`Ctrl+Shift+F`)

**Analysis → Vocal Coaching → Formant Display**

Tracks F1 (first formant) and F2 (second formant) column-by-column directly from the amplitude pages. F1 reflects the position of the jaw and tongue body; F2 reflects the front-to-back position of the tongue. Together they determine the vowel quality of a sung note — making the Formant Display useful for vowel coaching, phonation analysis, and language training.

#### Opening the dock

**Analysis → Vocal Coaching → Formant Display** or **Ctrl+Shift+F**. The dock opens on the right side.

#### Algorithm

1. For each column: average the L and R amplitude pages to obtain the amplitude spectrum.
2. Apply a **Gaussian smoothing** (configurable sigma) in the frequency direction to extract the *spectral envelope* — this blurs away individual harmonics and reveals the broad resonance peaks (formants).
3. Find local maxima (peaks) in the smoothed envelope.
4. **F1** = the highest-amplitude peak in the F1 search band that lies above the F0 floor.
5. **F2** = the highest-amplitude peak in the F2 search band, above F1.
6. Apply a running **median filter** across time (7-column window) to reduce column-to-column jitter.

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **F1 min / max** | 300 – 1200 Hz | Search band for F1. For soprano voices consider raising F1 max to 1500 Hz. |
| **F2 min / max** | 700 – 3500 Hz | Search band for F2. |
| **Smooth (rows)** | 80 rows | Gaussian smoothing width. Larger values give a smoother envelope but may merge nearby formants. 80 rows ≈ 1.07 octave at the default 75 bins-per-octave layout. |
| **Min amplitude** | 0.02 | Minimum normalised amplitude for a spectral peak to count as a formant. Raise if spurious detections appear in silent passages. |
| **Use pitch tracker as F0 floor** | On | When checked, uses the active Pitch Tracker primary result to set a per-column lower bound on the F1 search, preventing the fundamental from being confused with F1. For best results, run the Pitch Tracker first. |

#### Canvas overlay

Two coloured polylines are drawn across the spectrogram canvas:

| Colour | Formant | Frequency range |
|--------|---------|-----------------|
| **Coral / orange** | F1 | Jaw + tongue body position |
| **Sky-blue** | F2 | Front-to-back tongue position |

Both tracks are drawn with a dark shadow outline for contrast and are split wherever no formant was detected.

#### Vowel space scatter plot

The lower portion of the dock shows a vowel-space scatter plot — each voiced column contributes one point at its (F2, F1) coordinates, following the phonetics convention where:

- **X axis** — F2 (Hz), decreasing left → right (high F2 at the left = front vowels)
- **Y axis** — F1 (Hz), increasing downward (high F1 at the bottom = open vowels)
- **Colour** — time position (blue = early, red = late)

Approximate cardinal vowel targets are shown as faint reference labels (based on Peterson & Barney 1952 male averages — for female or child voices, expect F values shifted somewhat higher).

#### Dock readout

| Field | Description |
|-------|-------------|
| **F1 (at cursor)** | F1 Hz at the current playback column |
| **F2 (at cursor)** | F2 Hz at the current playback column |
| **Voiced cols** | Columns where both F1 was detected / total columns |

#### Tips

- **Run the Pitch Tracker first** and leave **Use pitch tracker as F0 floor** enabled. This prevents a dominant fundamental from being detected as F1 — common for low-voiced singers or instruments with a strong fundamental.
- **Soprano voices**: set F1 max to 1500 Hz and F2 max to 4000 Hz; formants can sit higher than standard settings.
- **Smooth value**: if F1 and F2 tracks look fragmented or noisy, increase Smooth (try 120–150 for cleaner signals). If F1 and F2 merge into one peak, decrease Smooth (try 50–60).
- **Vowel scatter**: a tight cluster in the plot indicates a consistent vowel; a diffuse cloud suggests the vowel is changing across the phrase — useful for checking vowel matching across a phrase.
- **Non-vocal material**: formant tracking is designed for single-voice signals. On complex polyphonic content the detected peaks may not correspond to meaningful formants.

---

### Vibrato Analyzer (`Ctrl+Shift+V`)

**Analysis → Vocal Coaching → Vibrato Analyzer**

The Vibrato Analyzer takes the pitch curve produced by the Pitch Tracker and characterises the periodic oscillation in each voiced segment. It reports rate, depth, onset delay, and regularity, and classifies every voiced column on the canvas.

#### Opening the analyzer

**Analysis → Vocal Coaching → Vibrato Analyzer** or **Ctrl+Shift+V**. The dock appears on the right side and can be floated or docked.

#### Workflow

1. Either run the **Pitch Tracker** first (click **Detect Pitch** there), then open the Vibrato Analyzer and click **Analyse** — or click **Detect & Analyse** in the Vibrato Analyzer dock to run pitch detection automatically.
2. The canvas gains a coloured band below the pitch curve classifying each region.
3. The dock readout shows global metrics and any flags.

#### Algorithm

1. The F0 curve (Hz) is converted to cents relative to the median voiced F0.
2. A running-median smoothing window (0.2 s default) removes slow pitch drift, leaving the vibrato oscillation.
3. An FFT of the gap-removed voiced signal identifies the dominant oscillation rate in the configured rate range (default 3–10 Hz).
4. A sliding window (1 s default) computes local depth and regularity for per-column classification.
5. Regularity is measured as autocorrelation coherence at the dominant period lag (0 = random, 1 = perfectly periodic).

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Min depth** | 10 ¢ | Half peak-to-peak amplitude below which a window is classified as straight tone rather than vibrato. |
| **Max depth** | 150 ¢ | Depth above this raises the excessive-depth flag. |
| **Min regularity** | 0.50 | Autocorrelation coherence below which a window is classified as erratic rather than vibrato. |
| **Rate min / max** | 3 – 10 Hz | Search range for the dominant vibrato rate in the FFT. Classical/opera vibrato is typically 5–7 Hz. |

#### Metric readouts

| Field | Description |
|-------|-------------|
| **Rate** | Dominant oscillation frequency (Hz) over the voiced region. Classical vibrato: 5–7 Hz; tremolo: < 4 Hz; flutter: > 8 Hz. |
| **Depth** | Half peak-to-peak amplitude in cents (mean over voiced windows). Normal vibrato: 25–75 ¢. |
| **Onset delay** | Seconds from the first voiced column until the first vibrato-classified column. |
| **Regularity** | Autocorrelation coherence at the dominant rate (0–1). Trained singers typically score > 0.7. |
| **Reference F0** | Median fundamental frequency of the voiced region (Hz). |
| **Vibrato cols** | Number and percentage of voiced columns classified as vibrato. |

#### Canvas overlay

A coloured band is drawn just below the pitch curve for each voiced run:

| Colour | Classification | Meaning |
|--------|---------------|---------|
| **Green** | Vibrato | Periodic oscillation at the detected rate, depth ≥ min, regularity ≥ min |
| **Blue-grey** | Straight tone | Depth below the minimum threshold — little or no oscillation |
| **Orange** | Erratic | Depth meets the threshold but oscillation is irregular (regularity below min) |

The rate (in Hz) is labelled at the start of each vibrato run; straight and erratic runs are labelled `str` and `err`.

#### Mini oscillation chart

The dock includes a small waveform display showing the vibrato signal (cents deviation from the smoothed pitch trend) over the full analysed region. The horizontal dashed lines mark ± min-depth. The trace is coloured by the same classification scheme as the canvas band.

#### Flags

| Flag | Condition | Typical interpretation |
|------|-----------|----------------------|
| ⚠ No vibrato detected | No column reached the min depth | Straight-tone technique, or threshold too high for the signal level |
| ⚠ Excessive depth | Mean depth > max depth | Very wide vibrato — may sound unstable or out of control |
| ⚠ Erratic periodicity | Mean regularity < min regularity | Unsteady oscillation — could indicate technique issues or pitch instability |

#### Interpreting results

- **Consistent green band** → well-established vibrato throughout the note.
- **Blue at onset, then green** → normal: straight attack followed by vibrato. Check the **Onset delay** value.
- **Orange runs** → erratic sections — zoom in on those columns and listen; they may correspond to breath breaks, register transitions, or loss of support.
- **Rate < 4 Hz** → slow tremolo rather than vibrato; common in early music or as a stylistic choice.
- **Rate > 8 Hz** → fast flutter; can indicate tension in the voice.
- **Depth < 10 ¢** throughout → straight-tone technique (countertenors, early music, some folk styles) — lower the min-depth threshold if you want to analyse sub-semitone oscillations.

---

---

### Key / Scale Detector (`Ctrl+Shift+Y`)

**Analysis → Music Production → Key / Scale Detector**

The Key / Scale Detector computes a **chromagram** — 12-bin spectral energy integrated per semitone — and compares it against Krumhansl-Schmuckler key profiles to rank the most likely musical key and mode. It works directly on amplitude pixels with no audio decode.

#### Opening the detector

**Analysis → Music Production → Key / Scale Detector** or **Ctrl+Shift+Y**. The dock opens, immediately analyses the full layer, and displays the result.

#### Algorithm

1. Each image row's centre frequency is converted to a MIDI pitch class (0 = C … 11 = B) using the configured A4 reference.
2. Spectral power for that row is accumulated into the corresponding chromagram bin (L+R channels averaged).
3. The 12-bin chromagram is normalised to sum = 1.
4. Pearson correlation is computed against every profile in the key library (84 profiles total: 24 major/minor, 12 harmonic minor, 60 church modes).
5. Keys are ranked descending by correlation; the gap between the top two scores forms the confidence metric.

#### Key library

| Category | Keys included |
|----------|--------------|
| **Major** | All 12 roots (C, C#, D … B) |
| **Natural minor** | All 12 roots |
| **Harmonic minor** | All 12 roots (raised 7th degree emphasised) |
| **Dorian** | All 12 roots |
| **Phrygian** | All 12 roots |
| **Lydian** | All 12 roots |
| **Mixolydian** | All 12 roots |
| **Locrian** | All 12 roots |

#### Dock controls

| Control | Description |
|---------|-------------|
| **A4 reference** | Tuning reference in Hz. Default 440 Hz. Set to 415 Hz for baroque pitch, 432 Hz for alternative tuning, 442 Hz for some European orchestras. |
| **Analyse Full Layer** | Analyses all columns of the active layer. |
| **Analyse Selection** | Analyses only the columns spanned by the current rectangular selection — useful for checking a verse, chorus, or modulation in isolation. |

#### Readout

**Best key** — the top-ranked key, shown large at the top of the dock.

**Confidence** — normalised gap between the 1st and 2nd correlations (0–100%). High confidence (> 60%) means the chromagram strongly resembles one profile; low confidence indicates ambiguity (modal, atonal, or modulating material).

**Top candidates** — the five highest-ranked keys with their raw Pearson correlation (−1 … +1). A correlation above 0.9 is a strong match; below 0.7 is weakly indicative.

**Chromagram bar chart** — 12 bars labelled C through B; bar height = normalised energy in that pitch class. The root of the best key is highlighted in amber; sharp/flat degrees in blue-grey.

#### Interpreting results

- **Clear winner, high confidence** → the piece is firmly in one key throughout the analysed region.
- **Major and minor of the same root** competing in the top two → the piece mixes parallel modes, or uses the natural minor / aeolian scale which shares most pitches with the relative major.
- **Church mode in the top candidates** → modal material. Compare with the relative major/minor: Dorian on D and G major have identical pitch content, so correlations will be close.
- **Low confidence across the board** → chromatic, atonal, or modulating material; try **Analyse Selection** on shorter segments to find local tonality.
- **Wrong key on a transposing instrument recording** → adjust A4 reference if the recording is at a non-standard tuning.

---

### Phase Correlation Meter (`Ctrl+Shift+M`)

**Analysis → Mastering & Sound Engineering → Phase Correlation Meter**

Measures how well the left and right channels agree in phase at every time-frequency bin. The result is a standard **phase correlation coefficient** (the same quantity shown by the goniometer "phase correlation" meter in professional DAWs) ranging from −1 to +1:

| Value | Meaning |
|-------|---------|
| **+1** | L and R are perfectly in phase — they add at full level in mono (+6 dB). |
| **0** | L and R are 90° apart — mono level is unchanged. Pure stereo width. |
| **−1** | L and R are perfectly out of phase — they cancel completely in mono. A critical problem. |

Sound Mind is the only spectrogram editor that stores phase as pixels (pages 2 and 3), making this analysis possible without decoding.

#### Opening the meter

**Analysis → Mastering & Sound Engineering → Phase Correlation Meter** or **Ctrl+Shift+M**. The dock opens and automatically analyses the full active layer.

#### Algorithm

For every amplitude-phase quadruplet `(A_L, A_R, φ_L, φ_R)` at each time-frequency bin:

1. Compute `cos(Δφ) = cos(φ_L)·cos(φ_R) + sin(φ_L)·sin(φ_R)` — avoids wrapping artefacts.
2. Weight by `A_L · A_R` so that loud bins dominate the aggregate.
3. Aggregate:  `C = Σ(A_L · A_R · cos(Δφ)) / √(Σ A_L² · Σ A_R²)`

The same formula is applied per column (for the timeseries) and per octave band (for the breakdown table).

#### Canvas overlay

Two overlays are drawn on the spectrogram:

| Overlay | Description |
|---------|-------------|
| **Problem column markers** | Semi-transparent red vertical lines over every column whose per-column correlation is below the threshold. |
| **Correlation strip** | A 16 px horizontal bar at the bottom of the viewport — green for in-phase columns, white for neutral, red for cancellation columns. |

#### Dock readouts

| Readout | Description |
|---------|-------------|
| **Aggregate correlation** | Large number (−1.000 … +1.000), colour-coded green/amber/red. |
| **Verdict** | SAFE (> 0.5) · CAUTION (0 – 0.5) · DANGER (< 0) |
| **Verdict detail** | One-line plain-English explanation. |
| **At cursor** | Per-column correlation for the current playback position. Updates live. |
| **Correlation chart** | Per-column correlation plotted over time. Green fill above zero, red fill below; amber cursor line. |
| **Per-octave band table** | Sub-bass · Bass · Low-mid · Mid · High-mid · Air — correlation for each frequency range. |

#### Dock controls

| Control | Description |
|---------|-------------|
| **Flag threshold** | Per-column correlation below this value is marked on the canvas. Default 0.0 flags any column with net cancellation. Set to −0.3 to flag only severe problems. |
| **Analyse Full Layer** | Analyses all columns of the active layer. |
| **Analyse Selection** | Analyses only the columns covered by the current rectangular selection. |
| **Clear** | Removes the canvas overlay and resets all readouts. |

#### Interpreting results

- **SAFE, correlation > 0.8** — excellent mono compatibility; wide stereo is achieved through genuine amplitude differences between channels (pan pot stereo or mid/side encoding).
- **CAUTION, correlation 0 – 0.5** — typical of wide stereo recordings; the mix will narrow in mono but not significantly cancel. Acceptable for most delivery formats.
- **DANGER, correlation < 0** — net phase cancellation. Common causes:
  - One channel was accidentally inverted (flip the polarity of the L or R amplitude page with a Curves filter: linear negative).
  - An out-of-phase microphone or source in the session.
  - Heavy use of stereo widening plugins that introduce artificial phase spread.
  - Phase-painting in Sound Mind with incompatible L/R phase gradients.
- **Low-frequency cancellation only (Sub-bass / Bass band is red)** — the most audible form. Kick and bass guitar will disappear in mono. Apply a mid/side high-pass on the side channel or use the Select + Curves filter to reduce phase spread below 250 Hz.
- **High-frequency cancellation only (Air band is red)** — less audible in mono but common with some room mic setups. Usually acceptable.

#### Notes on layers without phase data

If the active layer was imported from an image or created by drawing (rather than encoding audio), it may have fewer than 4 pages and no phase information. The meter will display a warning. Phase data is only available in layers encoded by Sound Mind Codec.

---

### Clipping & Saturation Detector (`Ctrl+Shift+C`)

**Analysis → Mastering & Sound Engineering → Clipping & Saturation Detector**

Scans for pixels at full scale (65535 = 0 dBFS). Any such pixel represents a sample that has been clipped or saturated; the decoder reproduces it at exactly 0 dBFS, which is indistinguishable from true clipping.

The detector operates on the **rendered (composited) TIFF** by default so that blend-mode interactions between layers are accounted for. It also supports scanning a single layer directly.

#### Opening the detector

**Analysis → Mastering & Sound Engineering → Clipping & Saturation Detector** or **Ctrl+Shift+C**.

#### Canvas overlay

Clipped columns are marked with semi-transparent vertical lines on the spectrogram canvas:

| Colour | Meaning |
|--------|---------|
| Red | Both L and R channels clipped in this column |
| Orange | Left channel only |
| Yellow | Right channel only |

The markers remain visible until you click **Clear** or change the active layer.

#### Dock controls

| Control | Description |
|---------|-------------|
| **Analyse Rendered** | Renders all visible layers into a composite TIFF, then scans it. Use this for a final quality-control check that accounts for blend-mode summation between layers. |
| **Analyse Active Layer** | Scans the active layer directly without rendering — useful for checking a single layer in isolation. |
| **Clear** | Removes the canvas overlay and resets the readout. |

#### Readout

| Field | Description |
|-------|-------------|
| **Clipped columns** | Total number of distinct columns containing at least one clipped pixel (L, R, or both) |
| **L + R** | Columns where both channels are simultaneously clipped |
| **L only / R only** | Columns where only one channel exceeds full scale |
| **Clipped fraction** | Percentage of total columns that contain clipping |
| **First clip** | Timestamp (seconds, or m:ss.ff) of the earliest clipped column |
| **Last clip** | Timestamp of the latest clipped column |
| **Clipped px (L/R)** | Total count of 65535-valued pixels in each amplitude page |

#### Interpreting results

- **is_clean / 0 clipped columns** — the rendered output is within the codec ceiling. No action needed.
- **Isolated clipped columns** — short transients have hit 0 dBFS. Apply a limiter or reduce the gain of the loudest layer. The first/last timestamps locate the problem region.
- **Large clipped fraction (> 1%)** — sustained saturation, likely from an over-driven layer or additive blend exceeding full scale. Reduce layer opacity or overall level.
- **L-only or R-only clipping** — asymmetric saturation; check per-channel levels. A large imbalance can cause mono-compatibility issues after limiting.
- **Clipping introduced by render but not present in individual layers** — occurs when two layers combine via Add or Screen blend to exceed full scale. Reduce the opacity of one of the contributing layers or change the blend mode.

---

### Edit Artifact Detector (`Ctrl+Shift+E`)

**Analysis → Unique Opportunities → Edit Artifact Detector**

Every op recorded in the op log has a defined temporal extent (a column range). The Edit Artifact Detector measures the normalised RMS amplitude discontinuity at the left and right boundaries of each op and flags any boundary where the step exceeds the configured threshold. Such steps become clicks or zipper noise when the image is decoded to audio.

The companion **Smooth Boundary** repair applies a Hann (raised-cosine) half-window crossfade taper over a configurable number of columns at each flagged boundary. The result is pushed to the undo stack as a standard `PaintOp`, so it can be undone with **Ctrl+Z** like any other edit.

#### Opening the detector

**Analysis → Unique Opportunities → Edit Artifact Detector** or **Ctrl+Shift+E**.

#### Algorithm

For each op in the project's op log that has a defined column range:

1. **Left boundary** at column `x0` — compare amplitude (pages 0 and 1) in column `x0 − 1` (just outside the op) vs column `x0` (first inside). The discontinuity is the RMS of all per-row differences, normalised by 65535.
2. **Right boundary** at column `x1` — compare column `x1 − 1` (last inside) vs column `x1` (just outside).
3. If the discontinuity ≥ threshold, the boundary is **flagged**.

Op types scanned: `fill_gradient`, `cut_delete`, `filter` (with rect), `warp`, `paste`, `curve`.

#### Canvas overlay

Flagged boundaries are marked with **solid amber vertical lines** on the spectrogram, labelled with a small "L" or "R" to show which edge of the op. Clean (below-threshold) boundaries are shown as dim grey dashed lines.

The overlay remains until you click **Clear** or change the active layer.

#### Dock controls

| Control | Description |
|---------|-------------|
| **Threshold** | Normalised RMS discontinuity cutoff (0.01–0.30). Default 0.05 ≈ a step of ~3280/65535 averaged across all rows and both channels. Lower = more sensitive; higher = only large jumps. |
| **Kernel width** | Number of columns over which the Hann taper is applied during smoothing (2–64). Larger values create a longer, more gradual crossfade but consume more of the op content near the boundary. |
| **Analyse Active Layer** | Scans the op log for the active layer's `layer_id` and measures every boundary. |
| **Smooth All Flagged** | Applies the Hann crossfade taper at all currently flagged boundaries. |
| **Smooth Selected** | Applies the taper at the single boundary selected in the list. |
| **Clear** | Removes the canvas overlay and resets the list. |

#### Boundary list

Each row in the list shows: flagged star (★), op type, side (left/right), column index, timestamp, and discontinuity percentage. Amber rows are flagged; grey rows are clean.

#### The Hann crossfade taper

The repair does not require the pre-op pixel state. Instead it tapers the edited region itself:

- **Left boundary** (at column `x0`): columns `x0` … `x0 + kw − 1` are blended from the amplitude of column `x0 − 1` (the outside reference) toward the unmodified inside amplitude, using a rising Hann window `w[i] = 0.5 × (1 − cos(π·i/(kw−1)))`.
- **Right boundary** (at column `x1`): columns `x1 − kw` … `x1 − 1` are blended from inside amplitude toward the amplitude of column `x1` (outside reference) using a falling Hann window.

The blended result replaces the pixel values in-place. Phase pages (2 and 3) are not touched.

#### Interpreting results

- **0 flagged boundaries** → all op edges transition smoothly; the layer should decode without click artefacts.
- **Isolated flagged boundary** → one op was applied to a region that had a silent (zero) neighbour. Smooth the boundary with a kernel of 4–8 columns.
- **Many flagged boundaries** → the layer has many abrupt edits. Consider using the **Fill** tool with a gradient that fades to black at the edges, rather than hard-rectangle fills.
- **Smoothing introduces a new artefact** → the kernel is too wide and is blending across another op boundary. Reduce kernel width and smooth selectively.
- **Boundary at column 0 or W** — image edge boundaries are not measurable and are skipped automatically.

---

### Transient Marker (`Ctrl+Shift+T`)

**Analysis → Music Production → Transient Marker**

Detects rapid amplitude onsets (transients) in the spectrogram using a **spectral-flux onset detection function (ODF)**. Each detected transient is stamped with an amber vertical tick mark on the canvas. The full timestamp list can be exported for use in external tools (DAWs, Audacity, Python scripts).

#### How it works

1. **Energy computation** — the selected amplitude pages are converted from uint16 pixel values to linear power.
2. **Frequency-band filtering** — only rows whose centre frequency falls within the configured **Freq low / Freq high** range contribute to the ODF.
3. **Spectral flux ODF** — the half-wave rectified first difference of energy is summed across all active rows per column:

       flux[c] = Σ_rows  max(0,  E[row, c] − E[row, c−1])

   This produces a scalar ODF value per column that is large wherever energy is rising rapidly (i.e., at an onset).

4. **Adaptive threshold** — a local mean of the ODF over the **avg_window** (40 columns by default) is computed and multiplied by the **Threshold ×** parameter. Only ODF values that exceed this local mean × multiplier are considered as candidates.
5. **Peak picking** — each candidate column must be the maximum value in a neighbourhood of ±3 columns.
6. **Minimum inter-onset interval** — a greedy forward pass enforces the **Min interval** between consecutive detections; conflicts within the same interval are resolved in favour of the higher ODF peak.
7. **BPM estimation** — the median inter-onset interval (IOI) is converted to BPM. Requires ≥ 4 transients.

#### Opening the dock

**Analysis → Music Production → Transient Marker** or **Ctrl+Shift+T**. The dock opens on the right side and can be floated or docked.

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| Threshold × | 1.5 | Adaptive threshold multiplier. Higher = fewer but stronger detections. |
| Min interval | 0.05 s | Minimum gap between consecutive transients. |
| Freq low | 20 Hz | Lower frequency bound for spectral flux accumulation. |
| Freq high | 16000 Hz | Upper frequency bound for spectral flux accumulation. |
| Channel | Both (L+R) | Which amplitude page(s) to use. L+R averages both channels. |

#### Results

- **Transients** — count of detected onsets.
- **BPM est.** — estimated tempo from the median inter-onset interval.
- **Timestamp list** — scrollable list showing the index and time in seconds of each transient.

#### Canvas overlay

Each detected transient is marked with a thin amber vertical line spanning the full height of the viewport, with a brighter amber cap at the top for visibility at small zoom levels. The overlay is cleared automatically when you switch to a different layer.

#### Export

| Button | Format | Details |
|--------|--------|---------|
| Save CSV… | CSV | `index,time_s,col` — one row per transient |
| Save plain text… | TXT | `index  time_s` — one line per transient |
| Save Audacity labels… | TXT | Tab-separated `start  end  label` — import directly into Audacity via File → Import → Labels |
| Copy to clipboard | — | Same format as plain text |

#### Controls

- **Analyse Full Layer** — runs the ODF over the entire layer width.
- **Analyse Selection** — runs the ODF over the current rectangular selection only (requires the Select tool with an active selection).
- **Clear** — removes the overlay from the canvas and resets the dock.

#### Tips

- **Kick drum detection**: set Freq low = 20 Hz, Freq high = 250 Hz to focus on the kick's fundamental energy. Bass and sub-bass frequency rows dominate the flux.
- **Snare detection**: set Freq low = 1000 Hz, Freq high = 8000 Hz to pick up the snare crack.
- **Full-mix transient detection**: leave Freq low/high at defaults (20–16000 Hz) for a broadband onset function.
- **Too many false positives**: raise Threshold × (try 2.0–3.0) and/or increase Min interval.
- **Missing soft transients**: lower Threshold × (try 1.0–1.2) and check that the correct channel is selected.
- **BPM is wrong**: the median IOI estimator works best when the tempo is steady. For rubato or irregular rhythms, use the CSV export and compute BPM externally.

---

### Frequency Masking Detector (`Ctrl+Shift+Q`)

**Analysis → Music Production → Frequency Masking Detector**

Identifies time-frequency regions where two layers compete simultaneously. When two sounds occupy the same frequency bin at the same moment, the quieter one is perceptually masked by the louder one — the listener cannot distinguish it regardless of its level. This tool makes that overlap visible so you can act on it with targeted EQ, compression, or sidechain processing.

#### How it works

For each time-frequency bin the detector:

1. Converts the uint16 amplitude pixel to a linear amplitude value (0 dBFS = 1.0).
2. Marks the bin as *active* on a layer if its amplitude meets or exceeds the **Threshold (dBFS)** setting (default −40 dBFS).
3. Computes masking severity as `min(amp_a, amp_b)` for bins where **both** layers are active, and zero elsewhere.

The severity value equals the amplitude of the potentially-masked (weaker) signal in the overlap region. Full-scale on both layers gives severity 1.0; one layer silent gives severity 0.0. The result is normalised to [0, 1] for display.

#### Opening the dock

**Analysis → Music Production → Frequency Masking Detector** or **Ctrl+Shift+Q**. The dock opens on the right side and can be floated or docked.

#### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Layer A** | (first loaded layer) | The first layer to compare |
| **Layer B** | (second loaded layer) | The second layer to compare |
| **Threshold (dBFS)** | −40 dB | Minimum amplitude for a bin to be counted as active. Raise to ignore quiet background; lower to catch very soft material. |

Click **Refresh layers** to update the layer dropdowns after loading new layers.

#### Canvas overlay

After analysis a semi-transparent heatmap is drawn on top of the spectrogram canvas. The colour at each pixel encodes the masking severity at that time-frequency bin:

- **Transparent** — no masking (at least one layer is below threshold)
- **Yellow** — mild masking (both layers active, one much quieter than the other)
- **Orange/red** — heavy masking (both layers at similar, high amplitude)

The overlay scales automatically with zoom and is erased when the dock is closed or the layer selection changes.

#### Dock readouts

| Readout | Description |
|---------|-------------|
| **Verdict** | CLEAN / MILD / MODERATE / HEAVY — colour-coded summary label |
| **Masked fraction** | Proportion of active bins where both layers overlap |
| **Mean severity** | Average masking severity across all masked bins (0–1) |
| **Worst band** | The perceptual octave band with the highest mean masking severity |

#### Verdict thresholds

| Verdict | Masked fraction | Interpretation |
|---------|-----------------|----------------|
| **CLEAN** | < 5% | Minimal overlap — layers are spectrally well-separated |
| **MILD** | 5–20% | Some overlap — minor masking unlikely to be noticed |
| **MODERATE** | 20–40% | Significant overlap — EQ or compression recommended |
| **HEAVY** | ≥ 40% | Heavy overlap — strong masking; layers will compete audibly |

#### Per-band severity table

The six rows in the band table show mean masking severity for the standard perceptual octave bands:

| Band | Range |
|------|-------|
| Sub-bass | 20–80 Hz |
| Bass | 80–250 Hz |
| Low-mid | 250–800 Hz |
| Mid | 800–2500 Hz |
| High-mid | 2500–8000 Hz |
| Air | 8000+ Hz |

Each row is colour-coded green (low), yellow (mild), orange (moderate), or red (heavy) to make the worst band immediately obvious.

#### Per-column severity chart

A bar chart below the band table shows the mean masking severity for each time column across the analysed region. Tall bars flag moments of peak spectral overlap — useful for identifying the exact phrase or beat where masking is worst.

#### Tips

- **Kick and bass**: set Layer A = kick drum, Layer B = bass guitar. A red Bass band means classic low-end masking — apply a gentle high-pass on the bass around 60–80 Hz or use sidechain compression.
- **Lead vocal and pad**: check the Mid and Low-mid bands. If both are heavy, carve 2–4 dB from the pad in the vocal's fundamental range.
- **Raise the threshold** (e.g. to −30 dB) to see only the most energetic overlaps. Lower it (e.g. to −60 dB) to catch quiet spectral build-up.
- **The overlay is qualitative**: the heatmap is drawn in raw image-row coordinates and is most accurate in Log scale mode. In Mel or Linear mode there may be slight frequency-band misalignment, but the dock readouts are always computed from the full data regardless of display scale.

---

### Stereo Width Analyzer (`Ctrl+Shift+W`)

**Analysis → Mastering & Sound Engineering → Stereo Width Analyzer**

Measures the stereo width of the active layer column-by-column and per octave band by computing the mid/side energy balance directly from the amplitude pages — no decoding is required.

#### How it works

For each column the analyzer:

1. Reads L = page 0 (amplitude left) and R = page 1 (amplitude right), normalised to 0–1.
2. Computes `mid = (L + R) / 2` and `side = |L − R| / 2` for every frequency row.
3. Computes RMS energy for mid and side: `E_mid = √mean(mid²)`, `E_side = √mean(side²)`.
4. Stereo width ratio: `w = E_side / (E_mid + E_side)`.
   - 0 = perfect mono (L equals R at every frequency)
   - 1 = fully anti-correlated (maximum stereo, very poor mono compatibility)
   - ≈ 0.5 = typical well-produced stereo mix
5. Per-octave-band widths are computed by restricting the row range to each frequency band.

#### Opening the dock

**Analysis → Mastering & Sound Engineering → Stereo Width Analyzer** or **Ctrl+Shift+W**. The dock opens and automatically analyses the full active layer.

#### Dock readouts

| Readout | Description |
|---------|-------------|
| **Width** | Aggregate width ratio across the entire analysed region (0.000–1.000) |
| **Verdict** | MONO / NARROW / BALANCED / WIDE / VERY WIDE |
| **Detail** | One-line description of the verdict |
| **At cursor** | Per-column width at the current playback position |
| **Chart** | Per-column width timeseries (red = narrow, green = balanced, blue = wide) |
| **Per-octave band table** | Width ratio for each of the six perceptual octave bands |

#### Verdict thresholds

| Verdict | Width range | Interpretation |
|---------|------------|----------------|
| **MONO** | < 0.10 | L and R channels are nearly identical |
| **NARROW** | 0.10–0.25 | Mostly mono with slight stereo differences |
| **BALANCED** | 0.25–0.55 | Typical balanced stereo image |
| **WIDE** | 0.55–0.75 | Wide stereo — check mono compatibility |
| **VERY WIDE** | > 0.75 | Extreme width — expect significant mono cancellation |

#### Canvas overlay

A 14 px colour-coded strip is drawn at the bottom of the spectrogram canvas after analysis:

- **Red** — narrow / mono columns
- **Amber/green** — balanced stereo
- **Blue** — wide columns

The strip scales with the view zoom and is erased when the dock is closed or the layer changes.

#### Tips

- Use **Analyse Selection** to check width within a specific phrase or passage.
- A **very high width in the sub-bass** (below 250 Hz) is a common mastering problem — low-frequency side content disappears in mono and causes phase problems. Apply a low-frequency M/S EQ or a mono-maker below 80–120 Hz.
- The per-octave-band table lets you spot which frequency region drives the width — for example, a drum overhead with reverb will typically show high width in the Air band but narrower in Bass.

---

### Stereo Imager / Goniometer (`Ctrl+Shift+G`)

**Analysis → Music Production → Stereo Imager / Goniometer**

A classic Lissajous goniometer display that plots the mid/side content of the current playback column as a scatter of frequency-bin points. Updated live during playback to give a real-time view of the stereo image.

#### Goniometer axes

The display uses the standard professional audio convention:

| Axis | Formula | Interpretation |
|------|---------|----------------|
| **Vertical (M)** | (L + R) / √2 | Mid energy — a perfectly mono signal points straight up |
| **Horizontal (S)** | (L − R) / √2 | Side energy — stereo width is spread along the X axis |

Each frequency bin of the selected column(s) is plotted as one dot. The points fade across a configurable persistence window (default 5 columns), giving the characteristic Lissajous figure used in professional meters.

- **Tall, narrow ellipse** → narrow stereo, good mono compatibility
- **Wide ellipse, nearly horizontal** → very wide / out-of-phase content, poor mono compatibility
- **Circle** → balanced stereo image across all frequencies

#### Opening the dock

**Analysis → Music Production → Stereo Imager / Goniometer** or **Ctrl+Shift+G**. The dock opens and begins reading data from the active layer immediately.

#### Controls

| Control | Description |
|---------|-------------|
| **Persistence** | Number of adjacent columns shown simultaneously (1–20). Higher values give a smoother, more stable figure. |
| **Column** | Column index for the Snapshot function |
| **Snapshot Column** | Freeze the display at the chosen column for detailed inspection |
| **Clear** | Reset the goniometer |

#### Live readouts

| Readout | Description |
|---------|-------------|
| **Correlation** | Amplitude-weighted correlation between L and R in the current window. Range −1 (fully out of phase) to +1 (perfect mono). |
| **Width** | Side / (mid + side) energy ratio for the current window (same formula as the Stereo Width Analyzer). |

#### Tips

- During mastering, leave the goniometer open and scan through the file — watch for moments where the figure collapses or swings hard to one side.
- If correlation drops below 0 in the low frequencies, apply a mono-maker or low-frequency M/S EQ.
- Use **Snapshot Column** to freeze a specific moment (e.g. the snare hit) and compare the figure with adjacent quieter regions.

---

### Spectral Comparison (A/B)

**Analysis → Music Production → Spectral Comparison (A/B)**

The Spectral Comparison dock overlays the averaged spectra of two sources — any combination of layers or time ranges — as continuous line plots on a shared frequency/amplitude canvas.  A colour-coded fill region makes it immediately clear which frequencies are louder in source A vs source B.  This is the primary tool for comparing a mix against a reference master without leaving the studio.

#### The chart

- **Y axis** — frequency (logarithmic, Hz), high frequencies at the top.
- **X axis** — RMS amplitude in dBFS (−96 … 0).
- **Cyan line** — averaged spectrum for Source A.
- **Orange line** — averaged spectrum for Source B.
- **Green fill** — frequency regions where Source A is louder than B.
- **Red fill** — frequency regions where Source B is louder than A.
- **Legend** — source labels shown in the top-left corner of the chart.

#### Per-band table

Below the chart, a table shows the per-octave-band summary:

| Column | Description |
|--------|-------------|
| A dBFS | RMS energy for Source A in that band |
| B dBFS | RMS energy for Source B in that band |
| Δ A−B | Difference (positive = A louder, negative = B louder) |

Bands where A and B are within ±1 dB are shown in grey; bands where A is louder are tinted cyan; bands where B is louder are tinted orange.

#### Opening the dock

**Analysis → Music Production → Spectral Comparison (A/B)**.  The dock opens with the layer combos pre-populated from the current project.

#### Source A / Source B selectors

Each source has two controls:

| Control | Options | Description |
|---------|---------|-------------|
| Layer | Any project layer / Rendered Composite | Which spectrogram to read |
| Range | Full Layer / Active Selection | Whether to average the full layer or only the current selection |

To compare two different time ranges within the same layer: select the same layer for both A and B, then draw a selection for the region you want to assign to one source.  Use "Active Selection" for that source and "Full Layer" for the other.

#### Workflow: match a mix to a reference master

1. Import the reference master as an additional layer (Project → Add Layer → Import Image or drag a TIFF into the project).
2. Open **Analysis → Music Production → Spectral Comparison (A/B)**.
3. Set **Source A** → your mix layer, **Source B** → the reference layer.
4. Click **Analyse** — the chart and band table appear immediately.
5. Red fill regions indicate bands where the reference master is louder; green fill regions indicate bands where your mix is louder.
6. Use the band table to identify specific octave bands that need adjustment.
7. Apply **Filters → Curves** (or **Filters → Equaliser**) to the mix layer to bring the balance closer to the reference.
8. Re-analyse to verify the correction.
9. Click **Export CSV…** to save per-row A/B/delta data for session notes.

---

### Phase Anomaly Scanner (`Ctrl+Shift+H`)

**Analysis → Unique Opportunities → Phase Anomaly Scanner**

The NSGT codec reconstructs audio from amplitude and phase data stored in the TIFF. Reconstruction assumes phase continuity between adjacent time columns — a sudden phase jump at column boundary manifests as a click or pop on decode. The Phase Anomaly Scanner detects these discontinuities before they reach the listener.

#### How it works

Phase is stored as uint16 on pages 2 (L) and 3 (R), where 0 maps to −π and 65535 maps to +π radians. For every adjacent column pair *(c, c+1)* the scanner computes the wrapped phase difference per frequency bin:

```
Δφ = wrap(φ[c+1] − φ[c])   where wrap puts the result in [−π, +π]
```

Any bin where `|Δφ| > threshold` is flagged. The result is aggregated per column (count of flagged bins).

#### Canvas overlay

- **Violet vertical lines** at flagged columns. Brighter magenta for severe columns (>10 % of bins flagged).
- **Intensity strip** (12 px, bottom of viewport): colour-coded by anomaly density — dark for mild, bright violet/magenta for severe.

#### Controls

| Control | Description |
|---------|-------------|
| **Threshold** | Phase jump magnitude (radians) above which a bin is flagged. Default π/2 ≈ 1.57 rad. Lower = more sensitive. |
| **Channel** | Both L + R / L only / R only — which phase pages to scan. |
| **Source** | Active Layer or Rendered Composite (renders all visible layers first). |
| **Analyse Full Layer** | Scan the entire layer width. |
| **Analyse Selection** | Scan only the current selection column range. |
| **Clear** | Remove the overlay from the canvas. |

#### Result readout

| Field | Description |
|-------|-------------|
| Flagged columns | Number of columns with at least one anomalous bin. |
| Anomalous bins | Total count of (row, page) bins exceeding the threshold. |
| First / Last anomaly | Timestamp (seconds) and column index of the first and last flagged column. |

#### Repairing anomalies

Use the **Heal brush** to blend the flagged columns into their surroundings, or repaint the phase values manually using the Airbrush tool targeting pages 2/3.

---

### Before/After Spectrum Diff

**Analysis → Unique Opportunities → Before/After Spectrum Diff**

Every editing operation that changes pixels (paint strokes, filters, warps, fills) stores a `PixelDelta` — a sparse record of every changed pixel's before and after value. The Spectrum Diff dock reads this delta and computes the per-frequency-bin mean amplitude change in dB, turning each edit into a measurable EQ curve.

#### How it works

For each amplitude page (L = page 0, R = page 1) in the delta:

1. Group changed pixels by image row (= frequency bin).
2. Compute mean amplitude before and after the edit for each row.
3. Convert both to dBFS and take the difference: `diff_dB = after_dBFS − before_dBFS`.

A positive diff means the edit added energy at that frequency; a negative diff means it reduced energy.

#### Chart

A horizontal bar chart is drawn with frequency bins displayed low-to-high (bass at the bottom, treble at the top, matching the spectrogram orientation). Reference frequency lines are drawn at 100 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz, and 16 kHz.

- **Green bars** → gain (edit added energy at that frequency)
- **Red bars** → cut (edit removed energy at that frequency)

The horizontal scale is symmetric around 0 dB, spanning ±max_abs_diff.

#### Controls

| Control | Description |
|---------|-------------|
| **Operation** drop-down | Lists all undo-stack ops with amplitude pixel data, newest first. |
| **Analyse Last Op** | One-click: automatically selects and analyses the most recent amplitude-changing op. |
| **Analyse Selected** | Analyse whichever op is selected in the drop-down. |
| **Refresh op list** | Reload the drop-down from the current undo stack (useful after a series of edits). |
| **Clear** | Clear the chart and readout. |
| **Export CSV…** | Save the full diff table (row, frequency_hz, diff_db, before_db, after_db) to CSV. |

#### Result readout

| Field | Description |
|-------|-------------|
| Max gain | Largest positive dB difference across all frequency bins. |
| Max cut | Largest negative dB difference (most energy removed). |
| Changed columns | Number of distinct time columns touched by this op. |
| Changed bins | Total number of (row, page) amplitude pixels changed. |

#### Workflow: EQ-by-painting with feedback

1. Select the Airbrush tool and paint at a target frequency.
2. Open **Analysis → Unique Opportunities → Before/After Spectrum Diff**.
3. Click **Analyse Last Op** to see the per-frequency effect of the stroke.
4. Adjust brush amplitude, size, or position and repeat.

This makes it possible to perform precise, measurable frequency boosts and cuts by painting directly on the spectrogram.

---

### Spectral Balance Meter (`Ctrl+Shift+Z`)

**Analysis → Mastering & Sound Engineering → Spectral Balance Meter**

The Spectral Balance Meter splits the full frequency range into 7 perceptual bands and reports the RMS energy in each one as a vertical bar chart. Unlike the Frequency Activity Heatmap (which shows the full per-row spectral shape), the Spectral Balance Meter gives you a fast at-a-glance read on whether the overall tonal balance of a mix matches a professional reference target.

#### Perceptual bands

| Band | Frequency range | Typical content |
|------|-----------------|-----------------|
| Sub-bass | 20–60 Hz | Kick transient body, sub-bass synths |
| Bass | 60–250 Hz | Bass guitar, bass synths, low-end warmth |
| Low-mid | 250–500 Hz | Guitar body, piano fundamentals, muddiness |
| Mid | 500 Hz–2 kHz | Vocals, snare, guitar presence |
| High-mid | 2–4 kHz | Attack, intelligibility, harshness zone |
| Presence | 4–6 kHz | Vocal presence, string brightness |
| Air | 6–20 kHz | Cymbals, breath, top-end shimmer |

#### The chart

Bars represent the **normalised** RMS energy of each band — that is, the raw dBFS level with the overall mean subtracted, so you see the spectral *shape* rather than the absolute level. A flat spectrum (equal energy per band) shows all bars at 0 dB.

When a reference curve is selected, a white tick mark is drawn across each bar at the target level for that band. The bar is coloured:

| Colour | Meaning |
|--------|---------|
| Green | Within ±2 dB of the reference target |
| Yellow | 2–5 dB from the reference target |
| Red | More than 5 dB from the reference target |

#### Reference curves

Select a curve from the **Reference** drop-down in the Options section:

| Curve | Description |
|-------|-------------|
| None | No reference — bars show normalised shape only |
| Pink Noise | Flat (equal energy per octave band); a useful starting point for acoustic balance |
| Pop / Rock master | Elevated sub-bass and bass, scooped low-mid and mid, boosted presence and air — typical commercial loudness profile |
| Broadcast (EBU R128) | Flat mid with gentle presence lift; designed for speech intelligibility on broadcast |
| Classical / Orchestral | Mid-forward with natural roll-off at the extremes |

#### Opening the meter

**Analysis → Mastering & Sound Engineering → Spectral Balance Meter** or **Ctrl+Shift+Z**. The dock opens and automatically analyses the full active layer.

#### Options

| Option | Description |
|--------|-------------|
| Source | *Active Layer* or *Rendered Composite* (requires a prior render) |
| Channel | L, R, or L+R avg |
| Reference | Reference curve to compare against (see table above) |

#### Mastering workflow

1. Press **Ctrl+Shift+Z** to open the Spectral Balance Meter.
2. Set **Source** → *Rendered Composite* to analyse the full mix.
3. Set **Reference** → *Pop / Rock master* (or whichever target suits the genre).
4. Click **Analyse Full Layer** — the bar chart appears in seconds.
5. Red bars indicate bands significantly above or below the target; use **Filters → Equaliser** (or paint directly on the amplitude pages) to bring them into range.
6. Re-render and re-analyse to verify the correction.
7. Click **Export CSV…** to save the band data for session notes.

---

### Frequency Activity Heatmap (`Ctrl+Shift+X`)

**Analysis → Unique Opportunities → Frequency Activity Heatmap**

Compresses the spectrogram horizontally into a single per-row spectral signature. Every frequency row's amplitude is averaged across all selected time columns and displayed as a horizontal bar chart — making the overall spectral character of the piece immediately visible at a glance.

#### How it works

For each frequency row:

1. Extract all amplitude values (dBFS) across the selected time range.
2. Convert to linear power, compute the mean and peak power.
3. Convert back to dBFS: the bar length shows the power-averaged amplitude.

Power-averaging is used rather than dB-averaging because it gives the perceptually correct result — a row that is mostly silent but occasionally peaks loud should have a low average level, and power-averaging captures this.

#### Chart

A horizontal bar chart is drawn with frequency on the Y axis (log-scale, bass at the bottom, treble at top, matching the spectrogram). Each bar is coloured using the same amplitude colour map as the spectrogram (deep blue → cyan → yellow → red). Octave reference lines are drawn at A0–A8.

- **Filled bars** → average amplitude (power-averaged dBFS).
- **White tick marks** → peak amplitude per row (toggle with *Show peak markers* checkbox).

The X axis spans −96 dBFS to 0 dBFS. A faint reference line is drawn at −40 dBFS as a useful noise-floor guide.

#### Controls

| Control | Description |
|---------|-------------|
| **Source** | *Active Layer* — uses the currently selected layer. *Rendered Composite* — renders all visible layers and analyses the composite. |
| **Channel** | *L+R avg* — average of left and right amplitude pages. *L* — left channel only. *R* — right channel only. |
| **Show peak markers** | Toggle the secondary white peak-amplitude tick marks. |
| **Analyse Full Layer** | Analyse all time columns in the selected source. |
| **Analyse Selection** | Analyse only the columns covered by the current rectangular selection. Falls back to full layer if no selection is active. |
| **Clear** | Remove the chart data and reset the display. |
| **Export CSV…** | Save the full per-row table (row, frequency_hz, avg_db, peak_db) to CSV. |

#### Per-octave band table

The dock shows a summary table with seven octave bands:

| Band | Frequency range |
|------|----------------|
| Sub-bass | 20–60 Hz |
| Bass | 60–250 Hz |
| Low-mid | 250–500 Hz |
| Mid | 500 Hz–2 kHz |
| High-mid | 2–4 kHz |
| Presence | 4–6 kHz |
| Brilliance | 6–20 kHz |

Each row shows `avg / peak` in dBFS. Levels above −20 dBFS are highlighted amber; −40 to −20 dBFS are green; below −40 dBFS are grey (near the noise floor).

#### Mastering workflow

1. **Ctrl+Shift+X** — open the Frequency Activity Heatmap.
2. Set **Source** → *Rendered Composite* to analyse the full mix.
3. Click **Analyse Full Layer** — the bar chart appears in seconds.
4. Check the bar chart:
   - A healthy mix typically shows a smooth roll-off from bass toward treble.
   - Flat or rising treble may indicate excessive brightness or harshness.
   - A gap in the mid-range may indicate hollowness.
5. Check the per-octave summary table for numerical confirmation.
6. Make EQ corrections (Filters → Curves on the relevant layer), re-render, and re-analyse.

---

### Instantaneous Frequency Overlay (`Ctrl+Shift+J`)

**Analysis → Unique Opportunities → Instantaneous Frequency Overlay**

The NSGT codec stores the raw complex phase at every time-frequency bin in pages 2 (L) and 3 (R) of the spectrogram TIFF. The **time-derivative of that phase** is the instantaneous frequency — the true local pitch of the content at that pixel, not the nominal centre frequency of the bin. This tool makes that deviation visible as a colour-coded canvas overlay and summarises pitch stability across the selection.

#### Algorithm

For each pair of adjacent columns at row `r`, column `c`:

1. Extract phase `φ[r, c]` from the uint16 phase page: `φ = (px / 65535) × 2π − π` (range `[−π, +π]`).
2. Forward difference: `Δφ = φ[r, c+1] − φ[r, c]`.
3. Wrap to `[−π, π]` to handle phase-cycle crossings.
4. Instantaneous frequency: `IF_hz = Δφ / (2π × timestep_s)`.
5. Deviation from the bin centre: `dev_st = 12 × log₂(IF_hz / f_nominal[r])`.
6. Mask: bins where amplitude is below the configured threshold, or where `|dev_st| > 6` (phase noise), are excluded (NaN).

Result columns are aligned to the *later* column of each pair, so the result for column `c` reflects the phase change between `c−1` and `c`.

#### Canvas overlay

The deviation at each pixel is mapped to a colour:

- **Red** — instantaneous frequency is *above* the bin centre (content is sharper than the nominal pitch).
- **Blue** — instantaneous frequency is *below* the bin centre (content is flatter).
- Opacity scales with the magnitude of the deviation; pixels within ±0.1 st of the bin centre are nearly transparent.

The overlay is rendered at 70% opacity so the amplitude content beneath remains visible. Toggle it on and off with the **Show overlay on canvas** checkbox without re-running the analysis.

#### Dock controls

| Control | Description |
|---------|-------------|
| **Amp. threshold** | Bins whose amplitude (normalised per row) is below this fraction are masked to suppress phase noise in silent regions. Default: 0.04 (4%). |
| **Colour scale** | Semitone deviation that corresponds to full colour saturation in the overlay. Default: 1.0 st. Increase for content with heavy vibrato; decrease to make subtle drift visible. |
| **Show overlay on canvas** | Toggle the canvas overlay on/off without clearing the result. |
| **Analyse** | Run the analysis on the current layer (or current selection if one is active). |
| **Clear** | Hide the overlay and discard the result. |

#### Chart

The mini chart below the info banner shows the **mean absolute deviation per column** across all voiced (non-masked) bins in that column:

- Bars are coloured green (stable, < 0.5 × scale_st) through yellow to red (drifty, ≥ scale_st).
- A white line connects the column means.
- Grey dashed reference lines mark 0.5 st and 1.0 st.
- During playback the chart cursor tracks the current position.

#### Statistics

| Field | Description |
|-------|-------------|
| **Median \|dev\|** | Median absolute deviation across all voiced bins in the selection. |
| **95th pct \|dev\|** | 95th-percentile absolute deviation — the deviation below which 95% of voiced bins fall. Useful as an objective colour-scale setting. |

#### Verdict

| Verdict | Median |dev| |
|---------|----------------|
| **STABLE** | < 0.05 st |
| **SLIGHT DRIFT** | 0.05–0.20 st |
| **MODERATE DRIFT** | 0.20–0.50 st |
| **HIGH INSTABILITY** | ≥ 0.50 st |

#### Interpretation guide

- **STABLE with blue/red overlay visible** — the codec phase is well-behaved; the overlay reveals fine microtonality in the recording (singer's natural pitch drift, vibrato onset).
- **STABLE with very little overlay** — nearly perfectly stationary tones (synthesised sources, tuned instruments on sustain).
- **MODERATE DRIFT, symmetric red/blue** — classic vibrato; the mean deviation near zero means the pitch oscillates equally above and below the bin centre.
- **HIGH INSTABILITY in silence** — phase noise; reduce the amplitude threshold to mask more of it.
- **Systematic red or blue in a region** — content was pitch-shifted by an amount that does not align with the bin grid; the shift lands between two bin centres, causing a constant-sign deviation.

#### Workflow: verifying a pitch-painted region

1. Select the painted columns with the **Select** tool.
2. **Analysis → Unique Opportunities → Instantaneous Frequency Overlay** (`Ctrl+Shift+J`).
3. The dock opens and analyses the selection automatically.
4. Check the verdict — **STABLE** confirms the phase is consistent with the target pitch.
5. A **MODERATE DRIFT** or **HIGH INSTABILITY** verdict suggests phase noise or a blend-mode interaction that has disrupted phase continuity; use the Phase Anomaly Scanner (`Ctrl+Shift+H`) to locate specific problem columns.

#### Workflow: distinguishing vibrato from phase noise

1. Open the IF Overlay on the vocal layer.
2. Set **Amp. threshold** to 0.10 or higher to aggressively suppress silent bins.
3. If the chart shows regular oscillation (alternating red and blue columns at a consistent rate) — that is genuine vibrato. The Vibrato Analyzer (`Ctrl+Shift+V`) will characterise it further.
4. If the chart shows random red/blue flicker with high variance — that is phase noise, not content. Check whether the recording was made with a very low amplitude (the phase is unreliable at low amplitudes).

---

### Criticality Meter (`Ctrl+Alt+C`)

**Analysis → Criticality → Criticality Meter…**

The Criticality Meter measures where each time column of a spectrogram sits on the axis between perfect order and pure chaos. Systems — and sounds — are most expressive and information-rich near the midpoint of that axis: not rigid enough to be a pure tone, not random enough to be noise. The dock surfaces this as two per-column metrics and paints the result as a colour-coded overlay directly on the canvas.

#### Metrics

**H_n — Normalised Shannon entropy (primary)**

For each time column, the amplitude values across all frequency bins are treated as a probability distribution. Shannon entropy measures how evenly energy is spread:

- **H_n ≈ 0** — energy concentrated in very few bins: a pure tone, a single harmonic, or silence. The column is frozen / too ordered (amber).
- **H_n ≈ 1** — energy spread uniformly across all bins: broadband noise. The column is too chaotic (violet).
- **H_n ∈ [0.40, 0.72]** — the critical band: the range where natural speech, musical instruments, and expressive sound tend to sit (green).

**ρ — Lag-1 autocorrelation (secondary)**

The Pearson correlation between H_n[t] and H_n[t+1] over a rolling 64-column window. It measures temporal rigidity: how much the entropy stays the same from one column to the next.

- **ρ → 1** — entropy barely changes column-to-column: temporally rigid (amber in ρ mode).
- **ρ → 0 or negative** — entropy fluctuates wildly: erratic (violet in ρ mode).
- **ρ ∈ [0.30, 0.80]** — the critical band for autocorrelation (green in ρ mode).

#### Canvas overlay

The overlay paints a colour tint over the spectrogram: amber where entropy is too low, green in the critical band, violet where entropy is too high.

| Control | Effect |
|---------|--------|
| **Show overlay** | Toggle the overlay on/off |
| **Overlay metric** | Switch between H_n (default) and ρ; re-maps colours without re-running analysis |
| **Compact strip** | Off = full-canvas semi-transparent tint (default); On = 12 px colour strip at the bottom of the canvas |
| **Opacity** | 0–100% (default 40%); adjusts the tint strength without re-running analysis |

Small tick marks appear at the very top of the canvas for extreme outliers: amber for H_n < 0.20 (very frozen) and violet for H_n > 0.90 (very chaotic).

#### Thresholds

| Control | Default | Meaning |
|---------|---------|---------|
| **H_n ordered** | 0.40 | H_n below this → amber (too ordered) |
| **H_n chaotic** | 0.72 | H_n above this → violet (too chaotic) |
| **ρ ordered** | 0.80 | ρ above this → amber (too rigid) in ρ overlay mode |
| **ρ chaotic** | 0.30 | ρ below this → violet (too erratic) in ρ overlay mode |

Changing any spinbox instantly re-maps the overlay colours from the cached result — no re-analysis needed.

**Calibrate to Selection** — draw a selection with the Select tool over a region whose character you want to treat as "critical," then click this button. It computes mean ± 0.5 SD of H_n within the selection and sets the H_n ordered/chaotic thresholds accordingly, letting you define the critical band relative to your material.

#### Running the analysis

1. Set **Channel** (Both L+R / L only / R only) and **Source** (Active Layer / Rendered Composite).
2. Click **Analyse Full** to analyse the entire layer, or draw a selection first and click **Analyse Selection** to limit the scan to a column range.
3. The verdict label shows the dominant zone (CRITICAL / MIXED / ORDERED / CHAOTIC) and the aggregate H_n and ρ values.
4. The per-column chart below the verdict shows the H_n or ρ curve with dashed threshold lines. During playback the chart cursor and the live readout label update for each column in real time.

**Clear** removes the result and overlay. **Export CSV** saves `col_index`, `time_s`, `H_n`, `rho`, `zone` for all analysed columns.

#### Workflow: diagnosing over-processed audio

1. Open a layer that sounds dull or overly compressed.
2. Run **Analyse Full**.
3. If the verdict is ORDERED and the canvas is mostly amber, the layer has been over-smoothed — high-frequency content has been reduced to the point where most columns look like tones, not natural audio. The Frequency Activity Heatmap (`Ctrl+Shift+X`) will confirm which bands are missing.
4. Raise H_n ordered threshold until the amber reaches only the truly silent or sinusoidal regions.

#### Workflow: checking a painted spectrogram section

1. Select the painted columns with the **Select** tool.
2. **Analysis → Criticality → Criticality Meter…** (`Ctrl+Alt+C`).
3. Click **Analyse Selection**.
4. If the section reads ORDERED (too amber), the painting has created overly uniform energy — try varying amplitude across the frequency axis. If it reads CHAOTIC (too violet), the painting is essentially noise and may not decode to anything recognisable — reduce high-frequency random content or apply the Denoise filter.
5. Use **Calibrate to Selection** on a reference region from a natural recording to set the target critical band for your material.

---

### Generators (`Ctrl+Alt+A`)

**Analysis → Criticality → Generators…**

The Generators dock contains three distinct synthesis engines selectable via the **Engine** radio buttons at the top of the panel. The CA and Reservoir engines share the Iterations, Seed, Region, Channel, Phase, and Preview/Generate controls. The L-System engine replaces Iterations with grammar and depth controls.

---

#### Engine: Cellular Automaton

The CA engine simulates a 2D Coupled Map Lattice (CML) — a continuous-state cellular automaton. The same Criticality (λ) slider that anchors the Order and Chaos brushes controls this engine: λ = 0.5 (r ≈ 3.57) targets the same critical band tracked by the Criticality Meter.

**How it works**

State: a 2D grid X[i, j] ∈ [0, 1]. At each time step:

```
X_new[i,j] = (1 − ε) · f(X[i,j]) + (ε / 4) · Σ f(X[neighbour])
```

where `f(x) = r · x · (1 − x)` is the logistic map, ε is coupling, and neighbours are the four cardinal cells (periodic boundary). The final state maps to uint16 amplitude (`X * 65535`).

**CA Parameters**

| Control | Description |
|---------|-------------|
| **Criticality (λ, 0–1)** | Maps to `r = 2.0 + λ · 2.0`. λ=0 → fixed-point; λ=0.5 → critical edge-of-chaos; λ=1 → full chaos. |
| **Coupling (ε, 0.00–0.80)** | Spatial coupling between cells. Low → noisy independent texture; high → coherent spatial structure. Default: 0.30. |
| **Init pattern** | Uniform random (default), Sparse random, Sine wave, Gaussian centre. |

---

#### Engine: Reservoir Lattice

The Reservoir Lattice engine runs a recurrent neural network on a 2D grid. Unlike training-based Echo State Networks, there is no readout layer: the final lattice state is the pixels directly. The key parameter is the spectral radius ρ(W) of the sparse weight matrix — the literal edge-of-chaos control, analogous to λ in the CA engine.

**How it works**

A H × W grid of nodes is connected by a sparse random weight matrix W (Moore neighbourhood, Uniform[−1, 1] weights). The matrix is rescaled so that its spectral radius (estimated via 30-step power iteration) equals the target ρ exactly. Starting from an initial state, the lattice evolves for *iterations* steps:

```
x(t+1) = tanh(W @ x(t))
```

The final state `|tanh(activation)| × 65535` is written as uint16 pixels. L and R channels share the same W but use independent initial states (consecutive seeds), producing structurally related yet naturally decorrelated stereo.

**Reservoir Parameters**

| Control | Description |
|---------|-------------|
| **Spectral radius (ρ, 0.00–2.00)** | Edge-of-chaos parameter. ρ < 0.9 → Ordered (fading, convergent); ρ ∈ [0.9, 1.1] → Critical (complex patterns); ρ > 1.1 → Chaotic (explosive, saturated). Default: 1.00. |
| **Neighbourhood r (1–4)** | Moore neighbourhood radius. r=1 → 3×3 (8 neighbours); r=2 → 5×5 (24); r=3 → 7×7 (48); r=4 → 9×9 (80). Larger r creates longer-range spatial coherence. Default: 2. |
| **Initial state** | **Random noise** — uniform [−1, 1] (default). **Gaussian impulse** — hot-spot at grid centre, all others near-zero; produces radially propagating wave-like patterns. **From active layer** — amplitude page of the active layer rescaled to [−1, 1]; seeds the lattice from existing audio content. |

---

#### Shared controls

| Control | Description |
|---------|-------------|
| **Iterations (10–500)** | Lattice update steps before sampling. More iterations drive the system deeper into its attractor. Default: 50. |
| **Seed + Randomise** | 32-bit RNG seed for fully deterministic output. |
| **Region** | Whole layer (default), Active selection, or Selection columns only. |
| **Channel** | L, R, or Both (default). |
| **Also generate phase** | Runs additional lattice pass(es) (seeds +2/+3) to populate phase pages 2/3. Unchecked leaves phase untouched. |
| **Preview** | Renders a fast downsampled overlay (≤ 64 × 64 internal grid for Reservoir; ≤ 256 × 256 for CA) without creating a layer. |
| **Clear Preview** | Removes the preview overlay. |
| **Generate** | Runs full generation in a background thread and stamps the result into a new layer (screen blend mode). CA layers are named "CA Generator"; Reservoir layers are named "Reservoir Lattice"; L-System layers are named "L-System". |

#### Output

The operation is pushed to the undo stack and recorded as a `CAGeneratorOp` (with `engine="ca"`, `engine="reservoir"`, or `engine="lsystem"`), storing only semantic parameters — no pixel data. Remaster replays are fully lossless.

#### Tips

**CA engine:**
- λ = 0.50 produces the richest texture. λ < 0.30 → convergence (mostly silence). λ > 0.80 → broadband noise.
- **Sparse random + high Coupling + λ ≈ 0.55** → organic worm-like structures that decode to evolving drones.
- **Gaussian centre** biases energy toward mid-frequencies and mid-time; useful for seeding centred melodic forms.

**Reservoir engine:**
- Start at ρ = 1.00 (Critical zone) and adjust from there. Small increases (ρ = 1.05–1.10) often give the most complex spatial patterns before the system saturates.
- **Gaussian impulse** seed with low neighbourhood r (1–2) creates concentric ring-like amplitude structures.
- **From active layer** effectively treats the existing audio content as a resonator excitation — the lattice "rings" from that initial state. Combine with an avalanche or CA layer underneath for layered complexity.
- Run the **Criticality Meter** after generating to verify the output lands in the critical band (H_n 0.40–0.72, green zone).
- Combine with the **Order** or **Chaos** brush to locally adjust regions of the generated layer.

---

#### Engine: L-System

An L-System (Lindenmayer System) is a formal grammar that grows a branching string by iteratively replacing each symbol with a production rule. A turtle interpreter then walks the string, and each step paints amplitude (and optionally phase) directly onto the spectrogram canvas.

**Grammar alphabet**

| Symbol | Meaning |
|--------|---------|
| `F` | Move forward one step, draw a segment |
| `G` | Move forward one step, no draw |
| `+` | Turn left by the turn angle |
| `-` | Turn right by the turn angle |
| `[` | Push position/angle onto the stack (start a branch) |
| `]` | Pop position/angle from the stack (return to branch point) |
| `\|` | U-turn (rotate 180°) |

All other symbols (like `X`, `A`, `B`) are non-terminal variables — they expand via production rules but produce no drawing action.

**Presets**

| Preset | Axiom | Default angle | Character |
|--------|-------|---------------|-----------|
| Plant | X | 25° | Organic bifurcating tree |
| Fern | X | 20° | Pinnate fern frond |
| Koch Curve | F | 90° | Fractal snowflake / self-similar spikes |
| Dragon Curve | FX | 90° | Space-filling recursive folds |
| Sierpinski Triangle | A | 60° | Triadic self-similar lattice |
| Hilbert Curve | A | 90° | Dense space-filling Z-path |

**Three projection modes**

| Mode | Canvas origin | X displacement → | Y displacement → | Best for |
|------|--------------|-------------------|-------------------|----------|
| **Pitch-Time Trace** | Left-centre (col=0, row=H/2) | Time (column) | Pitch (row) | Plant, Fern — vertical structures |
| **Heading-Frequency** | — (fills full width) | Accumulated distance → column | Current heading (sin) → row | Koch, Dragon — horizontal sweeps |
| **Polar Bloom** | Canvas centre | Centred column | Centred row | Sierpinski, Hilbert — radially symmetric forms |

In **Heading-Frequency** mode, heading 90° (turtle facing up) maps to row 0 (high frequency); heading 270° (facing down) maps to row H−1 (low frequency). The entire path length is always mapped to fill the full canvas width.

**Branch falloff and phase noise**

Amplitude decays geometrically with branch depth: `amplitude(depth) = falloff ^ depth`. At the default 0.75, the trunk is at full amplitude, first branches at 75%, second branches at 56%, and so on. This mirrors the way acoustic energy dissipates through a branching structure.

Phase noise per level (when *Also generate phase* is checked) adds `depth × phase_noise_per_level` radians of phase spread, so deep branches become progressively less phase-coherent — producing natural-sounding spatial dispersion in the decoded stereo field.

**Randomization zones**

| Range | Zone | Effect |
|-------|------|--------|
| 0–34% | Ordered | Rigid geometry; perfect self-similarity |
| 34–67% | Critical | Angle and step jitter; natural organic variation |
| 67–100% | Chaotic | Strong jitter + branch skipping; loose fractal structure |

**Custom Grammar**

Check the **Custom Grammar** box to define your own axiom and up to four production rules. The **Validate** button estimates how many drawable segments the grammar produces at the current depth — use this before generating to avoid very long renders. Auto depth reduction still applies (capped at 500 000 segments).

**L-System Parameters**

| Control | Description |
|---------|-------------|
| **Preset** | Six built-in grammars. Selecting a preset auto-fills the turn angle. Disabled when Custom Grammar is checked. |
| **Depth (1–8)** | Recursion depth. Auto-reduced if estimated segments exceed 500 000. |
| **Turn angle (0.5°–180°)** | Base rotation per `+`/`-` command. |
| **Step length (1–200 px)** | Forward distance per `F` command in canvas pixels. |
| **Randomization (0–100%)** | Controls angle jitter, step jitter, and (above 60%) branch-skip probability. |
| **Projection** | Pitch-Time Trace, Heading-Frequency, or Polar Bloom (see table above). |
| **Branch falloff (0.50–1.00)** | Amplitude decay per branch depth. Default 0.75. |
| **Phase noise/level (0–π rad)** | Phase noise added per branch depth when generating phase. Default π/4. |

**Tips**

- **Plant + Pitch-Time + depth 5–6** is the most musically useful combination: the branching structure creates a fractal melody line radiating upward from the tonic row, with harmonic content in the branches.
- **Koch + Polar Bloom + depth 4** produces a snowflake centred on the canvas; very dense at high depths — use Branch falloff 0.60–0.70 to reduce midrange congestion.
- **Hilbert + Pitch-Time + depth 5** creates a dense space-filling pattern that covers the canvas almost uniformly. Reduce step length to 2–4 px to keep the structure within the canvas bounds.
- **20–40% Randomization** (Critical zone) gives the most musically alive results: the basic form is still recognisable but has natural variation that decodes to richer timbre.
- The L-System engine does not use the Iterations control — the number of drawing steps is determined purely by the grammar depth and string length.

---

### Reservoir Stream (`Ctrl+Alt+T`)

**Analysis → Criticality → Reservoir Stream…**

The Reservoir Stream generator uses the same recurrent lattice dynamics as the Snapshot Reservoir engine (inside the Generators dock), but runs in a fundamentally different mode: rather than taking the final 2D state as a static texture, it reads out one column of the lattice at every update step, so each canvas column becomes the frequency profile of the lattice at that moment in time.

This produces **genuinely time-varying spectral content** — the output is an evolving spectrogram, not a static image. Near the critical zone (ρ ≈ 1), the lattice sustains long resonance tails and slowly shifting drones. In the chaotic zone (ρ > 1.1) it churns rapidly, producing dense evolving noise.

#### How it works

Internal lattice: H × 64 (H = min(canvas height, 512), fixed 64 context columns). The generator runs exactly as many steps as the target canvas is wide:

```
for t in 0 … W_canvas:
    state = tanh(W @ state)           # full H×64 lattice update
    canvas_column[t] = |state[0::64]| # column-0 readout: every 64th element
```

The `0::64` stride selects elements at positions 0, 64, 128, … in the flat state vector — the first column of the H × 64 lattice after reshaping. This is the only column exported; the remaining 63 context columns serve as hidden state that shapes the temporal dynamics.

The spectral radius ρ(W) is enforced exactly via power-iteration rescaling, identical to Snapshot mode.

#### Controls

| Control | Description |
|---------|-------------|
| **Spectral radius (ρ, 0.00–2.00)** | ρ < 0.9 → Ordered (decaying, convergent resonances). ρ ∈ [0.9, 1.1] → Critical (sustained, slowly evolving texture). ρ > 1.1 → Chaotic (rapidly churning). Default: 1.00. |
| **Neighbourhood r (1–4)** | Moore neighbourhood radius of the weight matrix. Larger r creates longer-range spatial coupling and richer harmonic content. Default: 2. |
| **Initial state** | **Random noise** — uniform [−1, 1] (default). **Gaussian impulse** — hot-spot at grid centre; decays outward in time as a resonant ring-down. **From active layer** — seeds from the active layer's amplitude page. Note: not Remaster-safe; replays fall back to Random noise. |
| **Seed + Randomise** | 32-bit RNG seed; fully deterministic for Random and Gaussian impulse modes. |
| **Region** | Whole layer, Active selection, or Selection columns only. |
| **Channel** | L, R, or Both (default). Stereo channels share W but start from independent states (seeds S and S+1). |
| **Also generate phase** | Runs additional lattice passes (seeds +2/+3) to populate phase pages. |
| **Preview** | Runs ≤ 512 steps at ≤ 64 internal rows, upsampled to canvas size — fast enough to audition different ρ values and seed modes before committing. |
| **Clear Preview** | Removes the preview overlay. |
| **Generate** | Runs the full temporal stream (one step per canvas column) in a background thread. Stamps result as a new **"Reservoir Stream"** layer with screen blend mode. |

#### Key differences from Snapshot Reservoir (Generators dock)

| | Snapshot | Temporal Stream |
|--|---------|-----------------|
| Steps | User-set Iterations (10–500) | Canvas width (automatic) |
| Lattice | H × W (2D spatial) | H × 64 (temporal context) |
| Readout | Final 2D state = pixels | Column-0 per step = one canvas column |
| Output character | Static texture | Evolving, time-varying |
| Dock | Generators | Reservoir Stream |

#### Tips

- **ρ = 0.95–1.05** is the sweet spot for musical content: resonances decay slowly and blend naturally. Push to 1.10–1.20 for energetic, continuously churning textures.
- **Gaussian impulse** with ρ ≈ 0.95 and r = 1 creates a clean exponential ring-down — the frequency content determined by the weight structure, not random noise.
- Because the step count equals the canvas width, long pieces (thousands of columns) are fully supported but take proportionally longer to generate. The background thread keeps the UI responsive.
- Combine with the Snapshot Reservoir layer underneath: the static texture provides harmonic density and the streaming layer provides temporal movement.
- Run the **Criticality Meter** after generating — the time axis is real now, so the H_n trajectory across the canvas reflects the lattice's temporal evolution.

---

### Expectation / Surprise Overlay (`Ctrl+Alt+S`)

**Analysis → Criticality → Expectation / Surprise Overlay…**

The Expectation / Surprise Overlay measures **temporal predictability** — how expected each moment is given what has happened recently in the piece. It is the music-structural sibling of the Criticality Meter:

- **Criticality Meter** answers: *"Is this patch at the edge of chaos?"* — a local, spatial question about the frequency distribution within one column.
- **Expectation / Surprise** answers: *"Did you expect this to happen now?"* — a temporal, sequential question about how much the piece has prepared the listener for this moment.

A repeating noise pattern can score high criticality (locally disordered) yet low surprise (the listener has heard it many times). A sudden shift to a distant key in a smooth melody can score low criticality (locally tonal) yet high surprise.

#### How it works

**Feature extraction:** each column is reduced to a single symbol in {0 … K−1} by computing its spectral centroid (amplitude-weighted mean frequency row) and quantizing it into K equal-width bins across the frequency range (default K = 8).

**Markov model:** a sliding window of W preceding symbols (default W = 512 columns, roughly 3–5 seconds) maintains a first-order transition frequency table T[prev][curr]. Laplace +1 smoothing prevents zero-probability transitions.

**Surprisal:**

```
S(t) = −log₂( P(symbol(t) | symbol(t−1)) ) / log₂(K)
```

- S = 0: the transition has occurred many times recently — fully expected.
- S = 1: the transition has never been seen in the window — maximally surprising.

The first column of every analysis window starts at S = 0 (no history yet).

#### Zones and overlay

| Zone | Colour | Default range |
|------|--------|---------------|
| Predictable | Blue | S < 0.33 |
| Interesting | Green | 0.33 ≤ S ≤ 0.67 |
| Surprising | Orange | S > 0.67 |

The full-canvas tint, compact strip, and tick-mark options follow the same layout as the Criticality Meter. Orange tick marks appear at the top of the canvas for extremely surprising columns (S > 0.90).

#### Controls

| Control | Description |
|---------|-------------|
| **Show overlay** | Enable/disable the canvas tint |
| **Compact strip (12 px)** | Show a 12-px strip at the bottom instead of a full tint |
| **Opacity** | Overlay opacity (0–100%, default 35%) |
| **Channel** | Both L+R (default), L only, R only |
| **Source** | Active Layer or Rendered Composite |
| **Window** | Markov history in columns (32–4096, default 512) |
| **Alphabet K** | Number of spectral-centroid bins (2–32, default 8) |
| **Low / High** | Zone threshold spinboxes; changing either instantly re-maps the overlay |
| **Analyse Full / Selection** | Runs analysis in a background thread |
| **Clear** | Removes the overlay and chart data |
| **Export CSV** | Saves `col_index`, `time_s`, `surprise`, `symbol`, `zone` |

#### Tips

- A section that stays blue (predictable) for many seconds is where the piece "flatlines" — the listener is no longer surprised. Consider applying the **Chaos** brush or the **Avalanche** brush there to inject structural novelty.
- The green "interesting" band between 0.33 and 0.67 is the compositional sweet spot — frequent enough to feel coherent, rare enough to feel alive.
- Combine with the **Criticality Meter**: columns that are simultaneously green on both overlays (critical H_n AND intermediate surprisal) are the richest compositional moments — locally complex and temporally well-prepared.
- Increase **Window** (e.g. 1024–2048) to measure long-range predictability (verse–chorus structure); decrease it (128–256) to measure phrase-level expectation.
- Increase **K** for a finer-grained model of spectral centroid movement; decrease to K=4 or K=2 for very coarse structural tracking.

---

### Robust Pattern Match (`Ctrl+Alt+P`)

**Analysis → Criticality → Robust Pattern Match…**

The Robust Pattern Match dock lets you locate all regions in a spectrogram that sound like a chosen reference passage — surviving pitch transposition and minor tempo variation.

#### Workflow

1. **Define the reference:** draw a selection (marquee or lasso) over the canvas region you want to find elsewhere.
2. **Set Reference:** click **Set Reference from Selection** in the dock. The dock freezes a spectral fingerprint of that region and shows a band-energy heatmap preview.
3. **Configure parameters** (see Controls below).
4. **Find Matches:** click **Find Matches** to search the whole layer, or **Find in Selection** to restrict to a narrower column range.
5. **Navigate results:** click any row in the results list to scroll the canvas to that match.

#### How it works

Each column is reduced to a 32-bin **log-band energy vector**: the frequency axis (already log-spaced in the NSGT representation) is divided into 32 equal-row bins and the mean amplitude per bin is computed, then L1-normalised. This makes the descriptor amplitude-invariant and captures spectral shape independent of loudness.

**Coarse search** slides the reference fingerprint across the search space at hop-4, trying circular band shifts for pitch compensation (a pitch shift of ≈3 semitones ≈ 1 band shift). The minimum-distance shift is recorded per position.

**Fine DTW:** the top-20 coarse candidates are refined with Sakoe-Chiba constrained Dynamic Time Warping, which allows local time-axis warping (frame repetitions / skips) within a configurable band. This provides the time-stretch tolerance.

**Non-overlapping selection:** the best match is accepted, then all positions overlapping it by >50% are suppressed; the process repeats until max matches or no candidates remain.

**Optional criticality matching:** checking **Include criticality profile (H_n)** appends a 0.5×-weighted normalised Shannon entropy value (H_n) per column as a 33rd feature. This finds regions with both similar spectral shape *and* similar order/chaos character — useful for matching sections with the same textural quality as the reference.

#### Canvas overlay

Accepted matches appear as **horizontal span bars** (8 px) across the top of the canvas:

| Colour | Confidence |
|--------|------------|
| Green | ≥ 0.80 |
| Amber | 0.50 – 0.79 |
| Orange | < 0.50 |

A rank number is shown above each bar. The reference region is outlined with a **dashed cyan border**.

#### Controls

| Control | Description |
|---------|-------------|
| **Set Reference from Selection** | Freeze the current selection as the search fingerprint |
| **Channel** | Both L+R (default), L only, R only |
| **Source** | Active Layer or Rendered Composite |
| **Pitch tolerance** | Maximum pitch shift tried (0–12 semitones, default 3). 0 = exact pitch only; 12 = up to one octave |
| **Time warp** | Sakoe-Chiba band as a fraction of reference length (0–0.40, default 0.15). Controls how much local tempo variation is tolerated |
| **Match threshold** | Maximum normalised DTW distance to report a match (0.0–1.0, default 0.25). Lower = stricter |
| **Max matches** | Maximum non-overlapping results returned (1–50, default 10) |
| **Include criticality profile (H_n)** | Add H_n as an extra matching dimension. Off by default — see note above |
| **Show match spans on canvas** | Toggle the canvas span-bar overlay |
| **Opacity** | Overlay opacity (0–100%, default 70%) |
| **Find Matches** | Search the whole layer in a background thread |
| **Find in Selection** | Restrict the search to the current canvas selection |
| **Clear** | Remove overlay, results list, and stored reference |
| **Export CSV** | Saves `rank`, `col_start`, `col_end`, `time_start_s`, `time_end_s`, `confidence`, `pitch_offset_bands` |

#### Tips

- Start with a short, characteristic reference (1–4 seconds). Very long references increase DTW computation time and may over-specify the match.
- If matches feel too loose, decrease **Match threshold** (0.15–0.20 is strict). If the expected passage is not found, increase it toward 0.40.
- Use **Include criticality profile** to find recurrences that share both the spectral shape *and* the noise/tone texture of the reference. Uncheck it to find transposed or re-textured versions of the same melodic or rhythmic content.
- The **pitch offset bands** column in the results list (and CSV) shows how many band positions were shifted to achieve the match; divide by ≈3 to get the approximate semitone shift.
- For pitch-shifted recurrences, enable pitch tolerance and check the **pitch offset bands** in the results to understand the transposition.

---

## 30. Keyboard Shortcuts

### File

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open TIFF |
| Ctrl+S | Save TIFF (Studio menu, standalone mode) |
| Ctrl+Shift+S | Save TIFF As (Studio menu, standalone mode) |
| Ctrl+E | Encode Audio → TIFF |
| Ctrl+D | Decode TIFF → Audio |
| Ctrl+I | Convert Image → TIFF |
| Ctrl+Q | Quit |

### Project

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+N | New Project |
| Ctrl+Shift+O | Open Project |
| Ctrl+S | Save Project |
| Ctrl+Shift+S | Save Project As |
| Ctrl+M | Edit Metadata… |
| Ctrl+Shift+A | Add Layer |
| — | Render Project |
| Ctrl+Shift+D | Decode Rendered → WAV |
| Ctrl+Shift+B | Bake Project |
| Ctrl+Shift+R | Replay Op Log |

### Edit

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+C | Copy selection |
| Ctrl+X | Cut selection |
| Ctrl+V | Paste |
| Delete | Delete selection (fill with silence) |
| Ctrl+M | Curves filter (also Edit Metadata… in the Project menu when a project is loaded) |
| Ctrl+Shift+K | Custom Convolve filter |

### View

| Shortcut | Action |
|----------|--------|
| Ctrl++ | Zoom in |
| Ctrl+- | Zoom out |
| Ctrl+0 | Fit in window |
| Ctrl+Shift+0 | Fit height |
| Ctrl+Alt+0 | Reset zoom to 1:1 |
| Ctrl+] | Zoom time axis wider |
| Ctrl+[ | Zoom time axis narrower |
| Ctrl+Shift+] | Zoom frequency axis in |
| Ctrl+Shift+[ | Zoom frequency axis out |
| Ctrl+L | Toggle Layers panel |
| Ctrl+G | Toggle Overlay panel |
| Ctrl+H | Toggle History panel |
| Ctrl+W | Toggle MindWaves panel |
| Ctrl+Alt+W | Toggle Waveform View panel |
| Ctrl+Alt+F | Toggle Sound Flower (polar view) |
| Ctrl+B | Toggle Live Mode panel |

### Page / Display

| Shortcut | Action |
|----------|--------|
| Ctrl+1 | Page 0 — Amplitude L |
| Ctrl+2 | Page 1 — Amplitude R |
| Ctrl+3 | Page 2 — Phase L |
| Ctrl+4 | Page 3 — Phase R |
| Ctrl+5 | RGB composite view |

### Analysis

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+L | Open Loudness Meter |
| Ctrl+Shift+M | Open Phase Correlation Meter |
| Ctrl+Shift+P | Open Pitch Tracker Overlay |
| Ctrl+Shift+V | Open Vibrato Analyzer |
| Ctrl+Shift+Y | Open Key / Scale Detector |
| Ctrl+Shift+C | Open Clipping & Saturation Detector |
| Ctrl+Shift+E | Open Edit Artifact Detector |
| Ctrl+Shift+T | Open Transient Marker |
| Ctrl+Shift+Q | Open Frequency Masking Detector |
| Ctrl+Shift+W | Open Stereo Width Analyzer |
| Ctrl+Shift+G | Open Stereo Imager / Goniometer |
| Ctrl+Shift+F | Open Formant Display |
| Ctrl+Shift+H | Open Phase Anomaly Scanner |
| Ctrl+Shift+X | Open Frequency Activity Heatmap |
| Ctrl+Shift+Z | Open Spectral Balance Meter |
| Ctrl+Shift+J | Open Instantaneous Frequency Overlay |
| Ctrl+Alt+C | Open Criticality Meter |
| Ctrl+Alt+A | Open Generators (CA / Reservoir / L-System) |
| Ctrl+Alt+T | Open Reservoir Stream |
| Ctrl+Alt+S | Open Expectation / Surprise Overlay |
| Ctrl+Alt+P | Open Robust Pattern Match |

### Tools Panel

| Shortcut | Action |
|----------|--------|
| Ctrl+Shift+I | Open Mind-Shot Editor |
| Ctrl+Shift+U | Open Chord Generator |
| M | Activate Measure tool |
| Ctrl+Alt+C | Open Criticality Meter (auto-pushes overlay when Order or Chaos is active) |

### Canvas Interactions

| Shortcut | Action |
|----------|--------|
| Space (tap) | Play / Pause |
| Space (hold + drag) | Pan temporarily |
| Middle-mouse drag | Pan |
| Ctrl+Scroll | Zoom uniformly |
| Ctrl+Shift+Scroll | Zoom time axis only |
| Ctrl+Alt+Scroll | Zoom frequency axis only |
| Alt+click | Set clone source (Stamp tool) |
| Click+drag (Measure) | Measure distance between two points |
| Enter / double-click | Commit floating paste (Select tool in paste mode) |
| Escape | Cancel paste / deselect Pick / cancel Warp |
| Delete | Delete selected Curve node (Curve tool) / delete selection (Select tool) |
| Alt+drag handle | Free-corner perspective (Select tool) |
| Shift+drag handle | Proportional resize / skew (Select tool) |
| Double-click segment | Insert Curve node |
| Double-click node | Delete Curve node |
| Alt+drag Bézier handle | Independent handle movement (corner mode) |

---

## 31. Common Workflows

### Check loudness for streaming compliance

1. Open the project and ensure the correct layer is active
2. **Analysis → Mastering & Sound Engineering → Loudness Meter** (or **Ctrl+Shift+L**)
3. Click **Analyse Full Layer** — the integrated LUFS, peak, and PLR are displayed immediately
4. Compare Integrated LUFS against the target (−14 LUFS for Spotify/YouTube, −16 LUFS for Apple Music)
5. If too loud, use **Filters → Curves** to reduce overall amplitude, then re-analyse
6. Use **Analyse Selection** to check a specific chorus or verse region in isolation

### Monitor dynamics during playback

1. Open the Loudness Meter (**Ctrl+Shift+L**)
2. Press **▶ Play** — the short-term and momentary LUFS readouts track the playback cursor live
3. Watch the **PLR** and **Crest** values to gauge how compressed the material sounds in context
4. Pause or stop; the meter retains the last displayed values

### Check pitch accuracy of a vocal recording

1. Open the project and activate the vocal layer
2. **Analysis → Vocal Coaching → Pitch Tracker Overlay** (or **Ctrl+Shift+P**)
3. Set **F0 min/max** to the expected vocal range (e.g. 80–800 Hz for a typical voice)
4. Click **Detect Pitch** — a green/red curve appears on the canvas
5. Green segments are in tune; red segments are sharp or flat
6. Note the cents labels to identify systematic tuning issues
7. Press **▶ Play** and watch the live **Note / Frequency / Cents** readout in the dock

### Characterise vibrato on a vocal take

1. Open the project and activate the vocal layer
2. **Analysis → Vocal Coaching → Vibrato Analyzer** (**Ctrl+Shift+V**)
3. Click **Detect & Analyse** — pitch detection and vibrato analysis run automatically
4. Check **Rate** (5–7 Hz = classical vibrato), **Depth** (25–75 ¢ = healthy), **Regularity** (> 0.7 = controlled)
5. Look at the canvas band: green = vibrato, blue = straight, orange = erratic
6. If erratic sections appear, zoom in to those columns and compare with the recording notes

### Compare vibrato before and after coaching

1. Add the "before" recording as Layer 1 and the "after" recording as Layer 2
2. Activate Layer 1 → **Ctrl+Shift+V** → **Detect & Analyse** → note Rate, Depth, Regularity
3. Click **Clear**, then activate Layer 2 → **Detect & Analyse** → compare metrics
4. The improvement in Regularity and the normalisation of Depth toward the target range shows quantifiable progress

### Check mono compatibility before export

1. Open the project and activate the target layer
2. **Analysis → Mastering & Sound Engineering → Phase Correlation Meter** (**Ctrl+Shift+M**)
3. The aggregate correlation and verdict appear immediately — green = SAFE, amber = CAUTION, red = DANGER
4. Check the **per-octave band table**: a red Sub-bass or Bass row means the kick and bass guitar will cancel in mono — fix this first
5. Scroll through the **correlation strip** at the bottom of the spectrogram; red columns identify the most problematic moments
6. Press **▶ Play** and watch the **At cursor** readout to spot when correlation drops during playback
7. If the verdict is DANGER: check whether one amplitude page needs polarity inversion (Filters → Curves, pull the curve to a negative slope on the affected page)

### Fix phase cancellation in a specific frequency band

1. Run the Phase Correlation Meter (**Ctrl+Shift+M**) and identify the band with negative correlation in the band table
2. Use the **Select** tool to draw a selection over the frequency range of the problem band (e.g., 20–250 Hz for Sub-bass + Bass)
3. Open **Filters → Curves**, set scope to the appropriate channel (L only or R only), and apply a gentle gain reduction — this reduces the amplitude of the problematic channel in that band, reducing the cancellation energy
4. Re-analyse to confirm the band correlation improved

### Check the rendered mix for clipping before export

1. Finish editing all layers and set visibility as desired
2. **Analysis → Mastering & Sound Engineering → Clipping & Saturation Detector** (or **Ctrl+Shift+C**)
3. Click **Analyse Rendered** — the studio renders all visible layers and scans the composite
4. If the result is "No clipping detected", the mix is within the codec ceiling — safe to decode
5. If clipping is found, the canvas shows red/orange/yellow markers at the affected columns; note the **First clip** and **Last clip** timestamps to locate the problem
6. To fix: reduce the opacity of the loudest layer, apply a Curves filter to lower the peak, or change Add blend modes to Normal/Screen on the loudest layers
7. Re-render and re-analyse to confirm the fix

### Diagnose L-only or R-only clipping

1. Open the Clipping Detector (**Ctrl+Shift+C**) and click **Analyse Rendered**
2. If the dock shows a high count for **L only** or **R only** but few in **L + R**, the mix is panned asymmetrically into saturation
3. Activate the loudest left- or right-weighted layer, apply **Filters → Channel Balance** to reduce gain on the clipping channel
4. Re-analyse to verify the fix

### Detect and repair edit boundary clicks

1. Open the project and activate the layer that may have click artefacts
2. **Analysis → Unique Opportunities → Edit Artifact Detector** (**Ctrl+Shift+E**)
3. Click **Analyse Active Layer** — the list populates with every op boundary; amber rows are flagged
4. Listen to the decoded audio at a flagged timestamp to confirm the click is audible
5. Click **Smooth All Flagged** to apply the default 8-column Hann crossfade taper to all amber boundaries
6. Press **Ctrl+Z** to undo if the result is unsatisfactory; reduce kernel width and try again
7. Re-run **Analyse Active Layer** to verify all amber rows are now grey

### Selectively repair a single boundary

1. Run the Edit Artifact Detector and identify the specific amber boundary in the list
2. Click that row to select it
3. Adjust **Kernel width** to match the context (4 columns = subtle, 16 = more gradual)
4. Click **Smooth Selected** — only that one boundary is repaired
5. Re-run **Analyse Active Layer** to confirm the discontinuity dropped below the threshold

### Identify the key and mode of a recording

1. Open the project and activate the target layer
2. **Analysis → Music Production → Key / Scale Detector** (**Ctrl+Shift+Y**)
3. The result appears immediately — note the **Best key** and **Confidence**
4. If confidence is low (< 50%), use the Select tool to draw a selection over a stable section and click **Analyse Selection**
5. Check the chromagram: the tallest bar should match the root; the pattern of tall vs short bars reveals the mode

### Find where a piece modulates

1. Open the Key / Scale Detector (**Ctrl+Shift+Y**)
2. Draw a selection with the Select tool over the first section → click **Analyse Selection** → note the key
3. Move the selection to the next section → click **Analyse Selection** → compare
4. Repeat to map the modulation structure

### Detect transients and export a beat grid

1. Open the project and activate the target layer
2. **Analysis → Music Production → Transient Marker** (or **Ctrl+Shift+T**)
3. Set **Freq low = 20 Hz** and **Freq high = 250 Hz** to focus on kick-drum energy (or leave at defaults for full-mix detection)
4. Click **Analyse Full Layer** — amber tick marks appear at each transient; the BPM estimate is shown in the dock
5. Review the timestamp list; if false positives appear, raise **Threshold ×** and re-analyse
6. Click **Save CSV…** or **Save Audacity labels…** to export the timestamp list for use in a DAW or sampler

### Estimate the BPM of a rhythmically regular track

1. Open the Transient Marker (**Ctrl+Shift+T**)
2. Set **Freq low = 20 Hz**, **Freq high = 250 Hz** (kick band), **Min interval = 0.10 s**
3. Click **Analyse Full Layer** — if the track has a steady kick, the **BPM est.** field updates immediately
4. If the estimate looks wrong, check the timestamp list: a large cluster at the start (silence → audio transition) or an isolated soft section may skew the median
5. Draw a selection over a clean rhythmic region and click **Analyse Selection** for a more reliable estimate

### Check kick/bass masking

1. Open a project with at least two layers — one containing the kick drum and one the bass guitar
2. **Analysis → Music Production → Frequency Masking Detector** (**Ctrl+Shift+Q**)
3. In the dock, set **Layer A** to the kick drum layer and **Layer B** to the bass guitar layer
4. Click **Analyse** — the heatmap appears on the canvas and the verdict is shown immediately
5. Check the **Bass (80–250 Hz)** row in the band table; if it shows orange or red, there is significant kick/bass overlap in the core frequency range
6. Recommended fix: apply a gentle high-pass to the bass guitar (Filters → Curves on the bass layer, reduce amplitude below 60–80 Hz) or reduce the kick's sub energy and re-analyse
7. Re-run **Analyse** to verify the Bass band severity has dropped

### Identify frequency congestion between two layers

1. Open the Frequency Masking Detector (**Ctrl+Shift+Q**)
2. Select the two layers you want to compare from the Layer A / Layer B dropdowns
3. Click **Analyse** and note the **Worst band** label — this is the perceptual band where masking is most severe
4. Zoom into the canvas heatmap over a red/orange region to find the exact time-frequency neighbourhood causing congestion
5. Use **Filters → Curves** on the quieter of the two layers to cut 2–4 dB in that band, creating space for the other layer
6. Re-run **Analyse** to verify the worst-band severity has improved

### Track a voice buried in a noisy mix using multi-candidate pitch detection

1. Open the project and activate the target layer (the mixed or noisy recording)
2. **Analysis → Vocal Coaching → Pitch Tracker Overlay** (**Ctrl+Shift+P**)
3. Enable the **Multi-candidate** group and set **Max candidates** to 3
4. Click **Detect Pitch** — three overlapping pitch curves appear on the canvas (green solid, cyan dashed, yellow dashed)
5. Identify which track follows the voice: the vocal line usually shows a smooth, slowly-moving curve; the instrument may show rapid or erratic jumps
6. Uncheck the non-vocal tracks in the dock — the canvas updates to show only the selected tracks, and the assembled primary result uses the best remaining candidate per column
7. Open the **Formant Display** (**Ctrl+Shift+F**) and click **Detect Formants** — the F1/F2 bands appear, guided by the assembled pitch track

### Map vowel quality across a vocal phrase

1. Open the project and activate the vocal layer
2. Run the **Pitch Tracker** (**Ctrl+Shift+P**) → **Detect Pitch** to establish the F0 floor
3. Open the **Formant Display** (**Ctrl+Shift+F**)
4. Click **Detect Formants** — coral (F1) and sky-blue (F2) bands appear on the canvas
5. Inspect the vowel-space scatter plot: clusters near the top-left (high F2, low F1) correspond to front vowels (/i/, /e/); clusters near the bottom-right (low F2, high F1) correspond to open back vowels (/a/, /o/)
6. A phrase with consistent vowels will show a tight cluster; a spread-out cloud indicates vowel inconsistency across repetitions
7. Zoom into a region with colour-shifted scatter points to identify which phrase section has different vowel placement

### Diagnose an out-of-tune passage

1. Run **Detect Pitch** on the layer
2. Zoom into a red section of the curve using **Ctrl+Scroll**
3. Read the cents label — e.g. "A4 +32¢" means the sung note is 32 cents sharp of A4
4. If the whole piece reads sharp (e.g. consistently +20 ¢), the recording may use A4 = 442 Hz tuning — adjust **A4 reference** to 442 Hz and re-detect

### Reduce volume in a specific frequency range

1. Open or import the TIFF
2. Draw a selection (Select tool) around the frequency band to reduce
3. **Filters → Curves** — pull the curve down
4. Click Apply, decode to verify

### Draw a melodic line

1. Select the **Curve** tool; left-click to place nodes along the pitch/time path
2. Right-click or double-click to finish placing; drag nodes and handles to refine
3. Set Interval to 1–5 px (smaller = smoother stroke)
4. Adjust the gradient if you want dynamics to vary along the line
5. Click **Apply**

### Fade audio to silence over a region

1. Select the **Fill** tool; drag a selection over the region
2. Gradient: t=0 → R=1, G=1, R op=1, G op=1; t=1 → R=0, G=0, R op=1, G op=1
3. Direction = 0° (rightward fade) or 180° (leftward)
4. Click **Apply**

### Crossfade between two regions

1. Fill tool, selection over the end of region A: gradient from full (t=0) to silent (t=1), direction 0°
2. Apply; draw a new selection over the start of region B: gradient from silent (t=0) to full (t=1), direction 0°
3. Apply

### Copy and reposition a sound

1. Select tool; draw selection; Ctrl+C; Ctrl+V
2. Drag the paste region to its new time/frequency position
3. Adjust blend mode or opacity in the Tools panel if needed
4. Enter to commit

### Mix two sounds

1. **Project → New Project**; **Project → Add Layer** → first audio; **Project → Add Layer** → second audio
2. Set the top layer's blend mode to **Add** and adjust both layers' opacity to balance
3. **Project → Render Project** → **Project → Decode Rendered → WAV**

### Create a pitched Mind-Shot patch along a time grid

1. Mind-Shot Editor — paint a frequency-domain shape for the Mind-Shot
2. Curve tool; Paint mode = **Mind-Shot**
3. Draw a horizontal line (two nodes)
4. Interval = 500 ms (for 120 BPM eighth notes), unit = ms
5. Click **Apply** — Mind-Shot stamps at even rhythmic intervals

### Bend a frequency upward over time (pitch bend)

1. Select tool; draw a selection around the region to bend
2. Switch to the **Warp** tool; draw a rising curve over the selection
3. Set Axis = **Frequency**
4. Apply — the selection's content is warped upward in pitch

### Re-edit a curve stroke you painted earlier

1. Pick tool; click near the stroke; double-click to enter re-edit
2. The Curve tool reopens with the original path and gradient restored
3. Adjust nodes, gradient, interval as needed
4. Click **Apply** — the layer is remasted with the change

### Stamp a rhythmic pattern along a time axis

1. Mind-Shot Editor → create a patch with a specific spectral shape
2. Curve tool → Paint mode = Mind-Shot → horizontal line → interval in ms
3. Apply

### Add vibrato to a sustained tone

1. Select tool → draw a selection around the sustained note
2. Warp tool → draw a gentle sine-wave-like curve above the selection
3. Axis = Frequency
4. Apply

### Create a sound from an image

1. **Studio → Convert Image → TIFF** (or Add Layer → image file in a project)
2. Choose the channel mapping (Yellow: R→Amp L, G→Amp R, B→phase — or Purple/Cyan variants)
3. Decode to audio to hear the image

### Clean up background noise

1. **Filters → Denoise** with a moderate Strength (0.3–0.5)
2. **Filters → Median Blur** (Size 3) for any remaining speckle
3. Compare with Ctrl+Z / Ctrl+Y; decode to verify

### Balance a stereo recording

1. Open the TIFF; switch to RGB view (Ctrl+5)
2. **Filters → Channel Balance** — adjust the slider until the display looks symmetric
3. Decode to audio to verify

### Remaster a layer after re-editing ops

1. **Edit → Remaster** — rebuilds all layers from their op logs
2. Disabled ops in the op log are skipped during remaster
3. Check the History panel to see what was replayed

### Check the length of a musical phrase or silence gap

1. Select the **Measure** tool from the Navigate category in the Tools panel (or press **M**)
2. Click and drag from the start of the phrase (or gap) to the end
3. Read the horizontal readout: `M:SS.mmm` gives the exact duration; if a BPM grid is set, the beat count tells you the rhythmic length
4. Compare against the project tempo to verify the phrase is aligned to the grid

### Verify an interval between two notes

1. Select the **Measure** tool
2. Click and drag from the centre of the lower note's frequency row to the centre of the upper note's row (align horizontally so Δx ≈ 0)
3. Read the vertical readout: the semitone count tells you the exact interval — 12 st = 1 octave, 7 st = a perfect fifth, etc.

### Check stereo width for streaming / mono compatibility

1. Open or import the TIFF and ensure the correct layer is active
2. **Analysis → Mastering & Sound Engineering → Stereo Width Analyzer** (or **Ctrl+Shift+W**)
3. Review the aggregate **Width** readout and verdict
4. Check the per-octave band table — a "VERY WIDE" reading in Sub or Bass means mono cancellation in the low end
5. If sub-bass width is high, apply a mono-maker (e.g. low-pass the side channel below 80 Hz using Filters → Custom Convolve) and re-analyse to confirm
6. Use **Analyse Selection** to compare width between the chorus and verse

### Identify stereo phase problems with the goniometer during playback

1. Decode the rendered TIFF to a WAV file, then re-encode it to a TIFF to get the composite
2. **Analysis → Music Production → Stereo Imager / Goniometer** (or **Ctrl+Shift+G**)
3. Press **▶ Play** — the goniometer figure updates live as the playback cursor advances
4. Watch for: (a) the figure collapsing to a vertical line (mono / narrow), (b) the figure tilting hard left or right (one channel louder), (c) points appearing in the lower half (out-of-phase content → mono cancellation)
5. When a problem moment appears, click **Snapshot Column** to freeze the goniometer there
6. Note the playback position and zoom into that region on the canvas to identify the source

### Start a Live Mode session

1. Open a project that has been saved at least once (Live Mode requires a saved project so it has a media directory for temp files)
2. Set the project loop duration and codec settings as desired — the Live layer will use these
3. **View → Live Mode** (or **Ctrl+B**) to open the Live panel
4. Select the input device from the **Device** dropdown; click ↺ to refresh if the device is not listed
5. Choose **Mono** or **Stereo** depending on your input signal
6. Click **● LIVE** — a "Live" layer is created automatically if it does not exist, and the first recording loop starts
7. Watch the **◷ Loop delay** indicator; green (1) means the pipeline is keeping up, yellow (2) or red (≥3) means the pipeline is slower than the loop duration
8. To save the session to disk, tick **Save session to:** before clicking LIVE and choose a directory
9. Click **■ Stop** when finished — playback completes the current loop, then stops; the "Live" layer retains its last captured spectrogram

### Reduce Live Mode loop delay

1. Check the current delay in the **◷ Loop delay** indicator in the Live panel
2. If delay ≥ 2, open **Project Settings** and reduce the loop duration (shorter loops → less encode/decode work per cycle)
3. Alternatively, reduce the number of visible layers — each additional visible layer adds compositing time
4. Enable GPU acceleration for the codec if a CUDA or OpenCL device is available (speeds up encode and decode)
5. After adjusting, start a new Live session and check whether the delay has decreased to 1

---

## 32. Live Mode

Live Mode turns Sound Mind Studio into a real-time synthesis instrument: audio from a microphone, synthesiser, or any system input device is encoded as a spectrogram, composited with the existing project layers, decoded back to audio, and played at the next loop boundary — continuously, without gaps.

### Prerequisites

- The project must be **saved** before starting Live Mode. Live Mode stores temporary audio files in the project's media directory.
- The input device must be listed in the **Device** dropdown. Use the ↺ button to refresh if a device was connected after the panel was opened.

### Opening the Live panel

**View → Live Mode** (or **Ctrl+B**). The panel appears as a dockable window on the right side of the studio. It can be floated or re-docked like any other panel.

### The Live panel

| Element | Description |
|---------|-------------|
| **● LIVE** | Starts the session. Turns red when active. Clicking while active triggers a clean stop. |
| **■ Stop** | Stops the session after the current recording loop completes (no abrupt cutoff). |
| **◷ Loop delay** | How many loops behind the live input the output currently is. Green = 1 (best), yellow = 2, red = 3 or more. |
| **Device** | Input device selector. All system audio input devices are listed; ↺ refreshes the list. |
| **Channels** | Mono (1 channel) or Stereo (2 channels). Must match the input signal. |
| **Loop** | Derived from the project settings. Shows the recording duration per cycle. |
| **In** | Input level meter — reflects the amplitude of the current recording. Decays automatically between peaks. |
| **Save session to** | When ticked, each input buffer and decoded output is saved as a numbered WAV file in the chosen directory. The path can be typed or selected via the **…** button. |
| **Status** | Running status line: loop count, pipeline progress, or error messages. |

### The signal chain

```
System input device
        │  (records one project-length loop at a time)
        ▼
  _LivePipelineWorker (background thread)
        │
        ├─ 1. Encode input WAV → spectrogram (NSGT)
        │
        ├─ 2. Write encoded data → "Live" layer image_data (in memory)
        │
        ├─ 3. Composite all visible layers → single TIFF array
        │
        └─ 4. Decode composite → output WAV
                │
                ▼
          QAudioSink (continuous PCM stream) → audio output device
```

At each loop boundary, recording switches to a new buffer immediately (so there is no gap in capture), and the completed buffer enters the pipeline. While the pipeline is running, the previous output loops seamlessly. When the pipeline finishes, the output file replaces the current playback at the next boundary.

### The "Live" layer

When a Live session starts, the studio looks for a layer named exactly `"Live"` in the project. If it does not exist, a blank layer of the correct project dimensions is created and added at the top of the stack. This layer is updated in memory on every loop — no TIFF file is written during the session. When the session ends, the layer holds the spectrogram of the last recorded input; saving the project persists it as a regular TIFF.

The "Live" layer participates in compositing the same way as any other layer: its blend mode and opacity can be changed at any time in the Layers panel, and it interacts with all other layers. This makes it possible to, for example, set the Live layer to **Screen** blend mode and overlay it on top of a backing track.

### Loop delay

The loop delay (shown in the **◷ Loop delay** indicator) is the number of recording cycles between capturing audio and hearing the result:

| Delay | Meaning | Typical cause |
|-------|---------|---------------|
| 1 | Pipeline completed within one loop | Fast CPU or short loop duration |
| 2 | Pipeline took 1–2 loops | Moderate CPU load or longer loop |
| 3+ | Pipeline is significantly slower than the loop | Many layers, long loop, slow CPU |

The minimum achievable delay is 1 (you always hear what was recorded one loop ago — this is a fundamental consequence of the encode→render→decode pipeline). A delay of 1 is indistinguishable from 1 loop of analogue latency.

**To reduce loop delay:**

- Shorten the project loop duration (Project Settings)
- Reduce the number of visible layers
- Enable GPU acceleration (CUDA or OpenCL)

### Session saving

When "Save session to:" is enabled and a directory is chosen, files are written as:

```
session_dir/in_000001.wav    ← raw microphone input, loop 1
session_dir/out_000001.wav   ← decoded pipeline output, loop 1
session_dir/in_000002.wav    ← raw input, loop 2
session_dir/out_000002.wav   ← decoded output, loop 2
…
```

Files are numbered sequentially from `000001` and are never overwritten. The raw inputs are useful for re-processing a captured improvisation at higher quality settings later. The outputs are the exact audio that was played during the session.

### Temp files

During a session, Sound Mind stores intermediate scratch files in `<project media dir>/_live_tmp/`. Each loop produces:
- `rec_NNNNNN.wav` — the raw recording for that slot
- `out_NNNNNN.wav` — the decoded output for that slot

Files more than five slots back are deleted automatically as the session progresses. The `_live_tmp/` directory is left in place after the session ends but its contents can be safely deleted.

### Stopping a session

Click **■ Stop**. The current recording loop finishes (no abrupt cut), then playback stops. The "Live" layer retains its last captured spectrogram in memory. Save the project to persist it. The session object is torn down cleanly and all background threads exit.

Closing the main window while a Live session is active triggers a safe stop first — the window will wait for the pipeline to finish before closing.

### Performance notes

- **Encode time** is proportional to loop duration × number of frequency bins. At default codec settings (75 bpo, 10 ms/pixel), a 4-second loop encodes in approximately 200–400 ms on a modern CPU.
- **Decode time** is the dominant factor. Decoding the full composite (which may include many layers) typically takes 500 ms – 2 s at default settings. Enable GPU acceleration to reduce this significantly.
- **Compositing** is fast (numpy array operations) and is rarely the bottleneck.
- **Thread safety**: the pipeline worker updates the "Live" layer's `image_data` from a background thread. This is the same pattern used by the pyramid composite worker elsewhere in the studio; the GIL provides sufficient protection for the numpy array assignment.

### Playback fidelity

Playback uses `QAudioSink` with a continuous PCM ring buffer (`LivePCMDevice`). The sink runs without interruption: when a new decoded loop arrives it is appended to the queue, and the device transitions to it seamlessly at the byte level. There is no audible gap at loop boundaries.

---

## 33. Canvas Right-Click Menu

Right-clicking anywhere on the spectrogram canvas opens a small popup menu containing shortcuts to any action in the studio. The menu is fully customisable — each user can configure it with the actions they reach for most often.

### Default actions

Out of the box the menu contains:

- Zoom In / Zoom Out
- Fit in Window / Fill Screen / Fit Height
- Layers Panel / Overlay Configuration / Waveform View (panel toggles)
- Loudness Meter / Criticality Meter (analysis shortcuts)

### Opening the configuration dialog

**Studio → Configure Canvas Menu…**

The dialog has two panels:

- **Available actions (left)** — a categorised tree of every action that can appear in the menu, spanning five categories: **Zoom**, **View** (panel toggles), **Tools** (paint tool switches), **Filters**, and **Analysis**.
- **Configured menu (right)** — the ordered list that will appear when you right-click.

### Building your menu

| Action | How |
|--------|-----|
| Add an action | Select it in the left tree; click **Add →** or double-click |
| Add a separator | Click **Add Separator** |
| Remove an item | Select it in the right list; click **← Remove** |
| Reorder items | Drag-and-drop within the right list, or use **▲ Up** / **▼ Down** |

Click **OK** to apply; **Cancel** to discard changes.

### Saving as part of a profile

The canvas menu configuration is one of the ten saveable components in a Studio Profile. Tick **Canvas Right-Click Menu** in the profile dialog to capture or restore the current configuration as part of any user or project profile. See [Studio Profiles](#34-studio-profiles).

---

## 34. Studio Profiles

A **Studio Profile** is a named snapshot of any combination of studio configuration that can be saved once and applied to any project at any time. Profiles are ideal for switching between different working contexts (composition, mixing, mastering, live performance) or for sharing a consistent studio setup across a team.

Open the dialog via **Studio → Studio Profile…**.

---

### Components

Every profile can capture up to ten independent components. Each component has its own checkbox in the dialog, so you can apply only the parts you want.

| Component | What it captures |
|-----------|-----------------|
| **Overlay Grid Configuration** | The complete state of the Overlay panel: timing grid (mode, BPM, bar/beat colours), frequency grid (notes, reference Hz, harmonics, custom frequencies), and chord overlay. |
| **Panel Layout** | Which docks are open or closed, their positions, sizes, and tab groupings. Restored via Qt's `saveState` / `restoreState` mechanism. |
| **Display / RGB Mapping** | Display mode (single-page or RGB composite), the active RGB channel-mapping preset, and light-mode on/off. |
| **Zoom & Scroll Position** | The horizontal and vertical zoom levels and scroll-bar positions at the time of saving. |
| **MindShot Instruments** | All instrument files in the project are copied into the profile directory (`mindshots/`) so they travel with the profile. |
| **MindWave Definitions** | All named MindWave configurations are serialised as JSON and stored in the profile. |
| **MIDI Configuration** | The project's `midi_profiles.json` — all custom MIDI instrument and channel mappings. |
| **Canvas Right-Click Menu** | The ordered list of actions and separators configured in the canvas context menu. |
| **Waveform View** | Whether the Waveform View panel is open and which display mode is active (Composite / All Layers / Active Layer). |
| **Analysis Tools** | Which of the 22 analysis docks are open or closed when the profile is saved. |

---

### Scopes

Profiles live in one of three scopes:

| Scope | Storage location | Visibility |
|-------|-----------------|------------|
| **Built-in** | Compiled into the application | Always available; read-only |
| **User** | `~/.soundmind/profiles/<slug>/` | Available to every project on this machine |
| **Project** | `<project_folder>/profiles/<slug>/` | Available only when that project is open; travels with the project |

The profile selector combo groups profiles by scope, with a separator between each group. The scope of the currently selected profile is shown below the combo.

---

### Factory profiles

Three read-only built-in profiles are always present:

| Name | Overlay | Display |
|------|---------|---------|
| **Composition** | Note frequency grid + chord overlay enabled | RGB / Yellow Phase |
| **Analysis** | Timing grid + all-semitones frequency grid | Single-page |
| **Mastering** | BPM timing grid + harmonic series overlay | RGB / Yellow Balanced |

---

### Applying a profile

1. Open **Studio → Studio Profile…**.
2. Select a profile from the drop-down.
3. The checkboxes reflect which components were captured when the profile was saved. Uncheck any component you do not want to apply.
4. Click **Apply**.

**Non-destructive behaviour:** MindShots and MindWaves are applied **additively** — instruments and wave definitions that already exist in the project are kept; only missing ones are added from the profile. This means applying a profile will never break a project that uses instruments or waves not included in the profile.

Overlay, display mode, zoom, and panel layout are fully replaced by the profile values.

---

### Saving a profile

#### Updating an existing profile (Save)

1. Select the profile you want to update.
2. Adjust the component checkboxes to include what you want to capture.
3. Click **Save**.

The dialog captures the current studio state for every checked component and writes it back to the same profile directory. Built-in profiles cannot be saved.

#### Creating a new profile (Save As…)

1. Click **Save As…**.
2. Enter a name for the new profile.
3. Choose a **scope** — User (shared across all projects) or Project (embedded in the current project folder).
4. Click OK.

The profile is created immediately and selected in the combo.

---

### Renaming and deleting profiles

- **Rename…** — renames the display name of the selected user or project profile. The profile directory slug is not changed (only `profile.json` is updated).
- **Delete** — permanently removes the profile directory from disk. A confirmation dialog is shown first. Built-in profiles cannot be deleted.

---

### Tradition Presets

The profile selector separates built-in profiles into two groups:

- **Workflow Presets** — the factory profiles (Composition, Analysis, Mastering) optimised for typical tasks.
- **Tradition Presets** — 22 world musical tradition profiles, each pre-configuring the overlay frequency grid and the MIDI instrument filter for a specific tonal framework.

Applying a tradition preset does two things:

1. **Loads a custom frequency grid** into the Overlay panel. The grid lines reflect the scale degrees, shruti positions, maqam tones, or overtone partials specific to that tradition — including microtonal intervals that fall between standard 12-TET semitones.
2. **Sets the Tradition filter** in the MIDI section of the Tools panel. The MIDI program list is narrowed to the instruments commonly associated with the tradition, so you do not have to scroll through all 128 GM programs.

Neither change affects the project's audio content — they only configure the reference grid and the instrument filter for your painting session.

#### Available tradition presets

| Preset | Pitch system | Key intervals |
|--------|-------------|---------------|
| **Western Classical** | 12-TET chromatic | All 12 semitones |
| **Western Early Music** | Just intonation (Ptolemaic) | Pure major 3rd (327 Hz), pure 5th (392 Hz) |
| **Chinese Han** | Pentatonic cycle-of-5ths | 5 pitches ≈ major pentatonic |
| **Hindustani Classical** | 22-shruti JI | Full shruti grid Sa → Kakali Ni |
| **Carnatic Classical** | 22-shruti JI | 29th melakarta (Shankarabharanam) |
| **Japanese Traditional** | In + Yo pentatonic | Semitone pair (C–C#), bright Yo scale |
| **Arabic Maqam** | Maqam Rast (neutral intervals) | Neutral 2nd (285 Hz), neutral 6th (429 Hz) |
| **Turkish Makam** | 53-comma Holdrian | Segah at 327 Hz (17 commas) |
| **Persian Dastgah** | Dastgah Shur (koron) | Koron Re at 276 Hz (≈ 90 ¢) |
| **Gamelan Pelog** | 7-tone unequal | Large gap at degree 3–5 |
| **Gamelan Slendro** | 5-TET | 5 equally-spaced tones |
| **Sub-Saharan African** | Mbira JI + 7-TET Chopi | Neutral 2nd (150 ¢), neutral 6th (850 ¢) |
| **Byzantine Chant** | 72-moria Echos I | Narrow 2nd (Pa, ≈ 90 ¢) |
| **Ancient Greek** | Enharmonic genus | Quarter-tone pair + major 3rd leap |
| **Jewish / Klezmer** | Freygish (Phrygian dominant) | Aug. 2nd D♭–E; minor 2nd C–D♭ |
| **Balkan** | Altered Phrygian | Two augmented 2nds |
| **Celtic / Irish** | Dorian | Natural 6th in minor context |
| **Blues** | Blues hexatonic | Tritone blue note (F#) |
| **Jazz** | Bebop dominant | Added B♮ passing tone |
| **Overtone Singing** | Harmonic series (E2) | H7 (−31 ¢), H11 (+51 ¢) |
| **Andean** | Near-equidistant pentatonic | 5 tones ≈ 240 ¢ apart |
| **Native American** | Major pentatonic | C D E G A |
| **Australian Aboriginal** | Harmonic series (A1) | Drone + 8 upper partials |

For detailed cultural and historical background, tuning formulas, rhythmic structures, and SMS usage guidance for every tradition, see **[Musical Traditions](MUSICAL_TRADITIONS.md)**.

---

### Overlay grid presets

The Overlay panel provides several one-click preset loaders to speed up common configurations.

#### Custom frequency presets

The **Custom Frequencies** section has a preset combo at the top. Selecting an entry loads that set of frequencies into the text field; the combo resets to a placeholder afterwards so you can select the same preset again.

| Preset | Frequencies |
|--------|------------|
| **Solfeggio (Chakra)** | 396 Hz (Liberation), 417 Hz (Change), 528 Hz (DNA Repair / Transformation), 639 Hz (Connection), 741 Hz (Awakening), 852 Hz (Intuition) |
| **Extended Solfeggio** | 13 frequencies from 174 Hz (Foundation) to 2172 Hz (Transcendence) |
| **DNA Repair (Horowitz 528 Hz)** | 528 Hz — Leonard Horowitz's DNA repair frequency |
| **Cosmic Octave / Om (Cousto)** | 136.1 Hz Om resonance (Cousto) plus octave harmonics at 272.2 Hz and 544.4 Hz |
| **Gamma (40 Hz)** | 40 Hz — gamma brainwave entrainment frequency |

#### A4 tuning reference presets

The **Notes** section has a preset combo next to the A4 reference spinbox. Selecting an entry sets the spinbox value immediately.

| Preset | A4 Hz | Notes |
|--------|-------|-------|
| **ISO 16 / Concert pitch** | 440.0 | International standard since 1939 |
| **Verdi / Philosophical pitch** | 432.0 | Based on C=256 Hz; advocated by Giuseppe Verdi |
| **Paris Diapason Normale** | 435.0 | French standard adopted 1859 |
| **Handel's tuning fork** | 422.5 | Measured from Handel's surviving tuning fork |
| **Baroque pitch** | 415.0 | Common Baroque performance pitch (A=415) |

---

## 35. MIDI Instrument Editor

**Project → MIDI Instrument Editor…**

The MIDI Instrument Editor is the central place to customise how MIDI notes are rendered into the spectrogram. Every note painted with the Brush (MIDI tip), imported from a `.mid` file, or stamped by the Chord Generator uses a **synthesis profile**. Profiles control the harmonic content, envelope shape, broadband noise, and optional sample source for each instrument. Changes take effect immediately after saving and clicking **Reload MIDI Programs**.

---

### Overview

The editor window has three regions:

| Region | Purpose |
|--------|---------|
| **Left: profile tree** | Navigates all melodic program ranges and individual drum note profiles |
| **Centre: profile editor** | Edits the selected profile's synthesis parameters |
| **Bottom right: preview** | Renders and plays the selected note using the current parameters |

---

### The Profile Tree

The tree has two top-level sections:

**Melodic Instruments** — each entry represents a contiguous range of GM program numbers, e.g. "Pianos (0–7)" or "Strings (40–47)". Selecting an entry loads it into the editor.

**Drum Notes** — individual entries for specific MIDI note numbers (mapped to General MIDI drum names, e.g. "38: Acoustic Snare"), plus a **Default** entry that applies to any drum note not individually listed.

#### Break Out Programs

When a melodic range spans more than one program number, the **Break Out Programs…** button below the tree becomes active. Clicking it opens a small dialog:

```
Break Out from Programs 0–7
─────────────────────────────────────────
Enter programs or ranges to break out:

  [ 4-6, 7                            ]

  ┌──────────────┐  ┌─────────┐
  │  Break Out   │  │ Cancel  │
  └──────────────┘  └─────────┘
```

Enter any combination of single program numbers and ranges (e.g. `4-6, 7`). On confirming, the original range is split:

- The specified programs become new independent entries (copies of the original profile).
- The remaining programs become one or more new entries covering the complement (e.g. if you break 4–6 and 7 from 0–7, a third entry for 0–3 is created).
- All resulting entries inherit the original profile and can be edited independently from that point.

This is useful when a factory range groups instruments that have similar synthesis but you want to assign a MindShot or different harmonic set to a specific program (e.g. separate a honky-tonk piano sound from the rest of the piano family).

---

### Profile Editor

The scrollable editor panel on the right shows all parameters for the selected profile.

#### Description

A free-text label displayed in the profile tree. Update it after breaking out sub-ranges to keep the tree readable.

#### ADSR Envelope

The ADSR section shows an interactive envelope diagram above four numeric spinboxes.

```
  1.0 ┤     /\
      │    /  \___________
  S   │   /               \
      │  /                 \
  0.0 ┤ /         A   D  S  R
         ──────────────────────▶ time
```

**Dragging the diagram handles:**

| Handle | Controls | How to drag |
|--------|----------|-------------|
| **Attack peak** (top of rise) | `attack_ms` | Left/right changes the attack time |
| **Decay end** (shoulder after peak) | `decay_ms` and `sustain_ratio` | Left/right changes decay time; up/down changes sustain level |
| **Release end** (tail end) | `release_ms` | Left/right changes the release time |

The sustain segment length between decay end and release end is computed automatically as a proportion of attack + decay; it is not directly draggable but updates as the other times change.

Hovering near a handle highlights it; the cursor changes to a move cursor. Precise values can always be typed into the spinboxes below the diagram.

| Field | Range | Default | Description |
|-------|-------|---------|-------------|
| **Attack** | 0–20 000 ms | 10 ms | Time to reach peak amplitude |
| **Decay** | 0–20 000 ms | 50 ms | Time to fall from peak to sustain level |
| **Sustain (0–1)** | 0.0–1.0 | 0.7 | Fraction of peak amplitude held during the note |
| **Release** | 0–20 000 ms | 80 ms | Time to fall from sustain to silence after note-off |

#### Harmonics

The harmonics panel shows a bar chart of all 16 partial amplitudes above the numeric spinboxes.

**Using the bar chart:**
- **Click** anywhere in the chart to set that partial's amplitude to the y position (0 = bottom = 0.0, top = 5.0).
- **Drag** left/right across bars to set multiple partials in one gesture.
- The current hover bar is highlighted in lighter blue.

**Using the spinboxes:**
The 16 spinboxes below the chart accept direct numeric entry (range 0.0–5.0, step 0.05). Spinner buttons are hidden to save space — click the field and type directly, or use the mouse wheel.

Partial 1 is the fundamental; partial 2 is one octave above; partial 3 is a fifth above that; and so on. Setting a partial to 0.0 skips it entirely during synthesis.

**Tips:**
- A single partial at 1.0 (all others 0) produces a pure sine wave — useful as a foundation for MindShot-based instruments.
- Approximate a sawtooth wave with `[1, 0.5, 0.33, 0.25, 0.2, ...]` (harmonic series amplitude).
- Approximate a square wave with `[1, 0, 0.33, 0, 0.2, 0, 0.14, ...]` (odd harmonics only).
- Large values (> 1.0) are valid and boost that partial above the fundamental.

#### Noise Bands

Each row in the table defines a broadband noise component added on top of the harmonic synthesis:

| Column | Description |
|--------|-------------|
| **Lo Hz** | Lower frequency bound of the noise band |
| **Hi Hz** | Upper frequency bound of the noise band |
| **Amplitude** | Relative amplitude of the noise (0 = none, 1 = same level as fundamental, higher values boost the noise further) |

Use noise bands to model:
- Breath noise in flutes and reeds (mid-frequency band, low amplitude ~0.05).
- Bow noise on strings (low-frequency band, very low amplitude ~0.02).
- Cymbal shimmer (full-frequency, moderate amplitude ~0.3).
- Percussion attack click (broad band, moderate amplitude, short envelope).

Add a row with **Add Band**; remove selected rows with **Remove Selected**.

#### Sound Source Override

The bottom of the profile editor exposes two optional fields that replace the harmonic + noise-band synthesis with a sample source.

| Field | What it does |
|-------|--------------|
| **MindShot** | Name of a Mind-Shot swatch (without file extension) in the project's `instruments/` folder. When set, the swatch is stamped for every note in this profile range instead of synthesising harmonics. The swatch is scaled horizontally to match the note duration, and vertically positioned so the Mind-Shot's **fundamental row** aligns to the target pitch row, allowing correct pitch-shifting for non-centred swatches. Amplitude pages are scaled by velocity × ADSR. |
| **MindGrain** | Name of a MindGrain layer in the current project. When set, a grain extracted from that layer's live canvas buffer is stamped. This takes priority over MindShot if both are set. |

When a project is open, both fields show a dropdown pre-populated with available names (Mind-Shots from `instruments/`, MindGrain layers from the project). Both fields are also freely editable — type any name manually if the project is not yet saved or you are preparing a profile for a different project.

**Priority order:** MindGrain → MindShot → harmonic synthesis. If the referenced source is unavailable (layer not found, swatch file missing), the next priority level is used automatically.

> **Setting up a MindShot source:**
> 1. Open the Mind-Shot Editor and create or import the swatch you want to use.
> 2. Set the **Fundamental row** so pitch placement is accurate.
> 3. Note the swatch name (shown at the top of the editor).
> 4. In the MIDI Instrument Editor, type that name (or select it from the dropdown) in the **MindShot** field.
> 5. Save to project and click **Reload MIDI Programs**.

---

### Spectrogram Preview

The preview panel at the bottom right shows the selected note rendered as a spectrogram using the current profile.

#### Controls

| Control | Description |
|---------|-------------|
| **MIDI spinbox** | MIDI note number for the preview (0–127; 60 = C4 / middle C) |
| **Vel spinbox** | MIDI velocity for the preview (1–127) |
| **Dur spinbox** | Note duration in seconds (0.2–8.0 s) |
| **⟳ Refresh** | Re-render the spectrogram from scratch using the current parameters. The preview refreshes automatically after a short delay whenever any profile parameter changes. |
| **▶ Play** | Decode the preview spectrogram via the NSGT path and play back the result. This is the same decode path used when playing the project, so what you hear is exactly what will appear in the exported audio. |
| **■ Stop** | Stop playback immediately. Also cancels a decode in progress. |
| **− / + / Fit** | Zoom out, zoom in, and reset to fit the full spectrogram in the view. |
| **☀** | Toggle light mode (greyscale, white background) vs dark mode (fire colormap, black background). |

#### Reading the spectrogram

The spectrogram displays the frequency–time representation of the note. The horizontal axis is time in milliseconds from the note start; the vertical axis is frequency in Hz, increasing upward (log scale).

- **Hz axis (left):** tick marks at octave landmarks — 55 Hz, 110 Hz, 220 Hz, 440 Hz, 880 Hz, 1760 Hz, 3520 Hz.
- **ms axis (bottom):** tick spacing adapts to the current zoom level and uses round numbers (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 ms, etc.).

In dark mode, silence is black, quiet components are dark red, mid-level energy is orange-yellow, and the loudest components are white. In light mode, silence is white and energy is black — useful if you want to print or export a screenshot.

#### Zoom and pan

- **Mouse wheel** — zoom in or out, centred on the cursor position.
- **Left-drag** — pan when zoomed in beyond fit level.
- The **−**, **+**, and **Fit** buttons perform the same zoom operations at fixed steps.
- Zoom range: 1× (fit) to 8×.

#### Decoding indicator

When **▶ Play** is clicked, the NSGT decode runs in a background thread so the UI remains responsive. The Play button shows `⏳ Decoding…` and is temporarily disabled. The status label below the spectrogram reads `Decoding audio via NSGT…` during the decode and `Playing.` once playback begins. If the decode fails, an error dialog is shown and the button reverts.

---

### Saving and Applying Changes

| Action | How |
|--------|-----|
| **Save to Project** | Writes `midi_profiles.json` to the project's `media/` folder and reloads the profile cache. Click **Reload MIDI Programs** in the Brush panel to re-render all MIDI layers. |
| **Export JSON…** | Saves the entire profile set to any JSON file — useful for backing up or sharing between projects. |
| **Import JSON…** | Replaces all profiles in the editor with those from a JSON file. The imported profiles are not saved to the project until you click **Save to Project**. |
| **Discard** | Closes the editor without saving. All unsaved edits are discarded. |

> **Re-render all layers:** after saving profiles, click **Reload MIDI Programs** in the Tools panel (visible when the Brush tool is active with the MIDI tip selected, or in the MIDI tip panel). This re-renders every `MidiBrushOp` in the project — imported MIDI files, Chord Generator layers, and hand-painted notes — using the new profiles.

---

### JSON Profile Format Reference

For advanced use or batch editing, profiles can be edited as JSON. The project's profiles file is at `<project>/media/midi_profiles.json`.

**Top-level structure:**

```json
{
  "melodic_ranges": [ ... ],
  "drum_note_profiles": { "38": { ... }, "42": { ... }, ... },
  "drum_default": { ... }
}
```

**Profile entry fields:**

| Field | Type | Description |
|-------|------|-------------|
| `"description"` | string | Label shown in the editor tree |
| `"lo"` | int | Lowest GM program number in this range (melodic only) |
| `"hi"` | int | Highest GM program number in this range (melodic only) |
| `"harmonics"` | float[] | Amplitudes for partials 1, 2, … 16; trailing zeros may be omitted |
| `"attack_ms"` | float | Attack time in milliseconds |
| `"decay_ms"` | float | Decay time in milliseconds |
| `"sustain_ratio"` | float | Sustain level as a fraction 0.0–1.0 |
| `"release_ms"` | float | Release time in milliseconds |
| `"noise_bands"` | `[[lo, hi, amp], ...]` | Broadband noise bands (Hz bounds + relative amplitude) |
| `"mindshot"` | string \| null | Mind-Shot swatch name (overrides harmonic synthesis) |
| `"mindgrain"` | string \| null | MindGrain layer name (overrides mindshot and harmonics) |

> **Backward compatibility:** `"instrument"` is accepted as a synonym for `"mindshot"` for profiles created before v1.3.1.

---

## 35. Video Export

**Project → Export Video…** (`Ctrl+Shift+O`) encodes the current spectrogram canvas as a video file with a synchronised audio track. The audio is always freshly decoded from the project TIFF — no pre-rendered WAV file is required.

### Opening the dialog

The action is available whenever at least one layer is loaded in the project. For multi-layer projects the audio is decoded from the **rendered composite** TIFF (Project → Render Project first). For single-layer projects the active layer's TIFF is used directly.

### Format & Resolution

| Setting | Options |
|---------|---------|
| **Format** | MP4/H.264 · MP4/H.265 · WebM/VP9 · MOV/H.264 |
| **Resolution** | 1080p (1920×1080) · 4K UHD (3840×2160) · 1440p · 720p · TikTok/Shorts 1080 (1080×1920) · TikTok/Shorts 720 (720×1280) · Square 1080 (1080×1080) · Custom |
| **Frame rate** | 24 fps · 30 fps · 60 fps |
| **Quality** | Low · Medium · High (mapped to codec-appropriate CRF values) |

When **Custom** resolution is selected, width and height spinboxes appear. Changing the format automatically updates the output file extension.

### Animation modes

#### Whole Canvas

The entire spectrogram is visible for the full duration of the video. A two-pass (dark shadow + bright gold) indicator line sweeps along the time axis from left to right, reaching the right edge precisely when the audio ends.

In **polar mode** (Sound Flower active), the full polar canvas is shown fixed; the indicator is a ray from the flower centre that rotates clockwise from 12 o'clock, matching the in-studio playback overlay.

#### Scrolling Canvas

Only a configurable **time window** of the canvas is visible at any instant. The window pans so that the playback indicator travels from the left edge to the right edge over the full track duration.

- **Window duration** — set with the `m:ss` time picker (default 30 s). Narrower windows emphasise local detail; the full canvas width is the maximum useful setting.
- **Show playback indicator** — uncheck to produce a clean pan with no indicator line.

In **polar mode**, the scrolling canvas behaviour changes: the indicator is fixed at 12 o'clock and the entire canvas rotates counter-clockwise. One full rotation coincides with the end of the track. The window duration setting has no effect in polar scrolling mode.

### Colour and coordinate fidelity

The video uses whatever **coordinate system** (rectangular or polar), **colour mapping**, and **light/dark mode** are active in the Studio at the moment you click Export. No re-render or settings change is needed.

The canvas is **letterboxed** to fit the target resolution: it is scaled with aspect ratio preserved, and any remaining area is filled with the background colour (black in dark mode, white in light mode).

### Progress and cancellation

A progress dialog shows the current phase:

1. Decoding audio (TIFF → WAV)
2. Reading audio (WAV → PCM)
3. Encoding frames (renders and encodes each frame)
4. Finalising (flush and close container)

Click **Cancel** at any point to abort. The partial output file is deleted automatically on cancellation or error.

### Opening after export

Tick **Open file after export** to launch the finished video in the system's default player when encoding completes.

### Dependency note

Video export requires **PyAV** (`av >= 13.0`), which is installed automatically as part of Sound Mind Studio. PyAV bundles the FFmpeg libraries as a self-contained Python wheel — no separate FFmpeg installation is needed.
