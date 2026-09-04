# Sound Mind - the Design Document

# Vision

Sound Mind is a creative toolkit for working with audio as a visual medium. It encodes sound as a spectrogram image — a picture you can paint, layer, filter, and transform — then reconstructs audio from the result with perfect fidelity.

The project spans a codec library, a command-line tool, and a full graphical studio. Together they let you see, touch, and reshape sound in ways that traditional audio editors cannot.

In principle, Sound Mind creates sound from images, or images from sound. More precisely, when given a sound, it creates a spectrograph which uniquely represents or encodes that sound. Inversely, when given an image, it creates a unique sound of which spectrograph approximates that image. Converting from sound to image and back to sound is deterministic and perceptually lossless. Sound Mind is the physical/mathematical bidirectional bridge between a sound and its characteristic spectrograph, or between an image and its characteristic sound. Based on this foundation, the Studio is a sound engineering laboratory, a digital audio workstation, a painter's attic and a musician's instrument, all at once.

# Philosophy

## The Sound Mind Symbol

The Sound Mind symbol is composed of five interwoven forms, each reflecting a principle that guided the design of every tool in this collection:

**The Möbius Strip** — A surface with one side and no boundary. It stands for nonduality: the indivisibility of sound and image, of creator and creation. Audio and spectrogram are not two things but one continuous surface — encode and decode are two directions along the same loop, unified and unbroken. The Pool transformation is seamless and lossless because there is, in the deepest sense, no transformation at all.

**The Infinity Symbol (∞)** — Recurrence, eternity, perpetuity. A spectrogram is never finished — it is a field of perpetual becoming. Every composition can be layered further, transformed again, heard anew. There is no final state, only an endless unfolding of creative possibility.

**The Diamond** — The UML decision symbol, and the emblem of choice kept permanently open. But the diamond is also hardness, brilliance, and ground: the clarity of structure, the resilience of a form that does not yield. Sound Mind Studio's non-destructive pipeline — unlimited undo, revisable layers, blend modes that never commit — embodies the explicate order: every decision made explicit, every path remaining traversable. It is angular and masculine, enveloped by the Heart.

**The Heart** — Love, compassion, and life. The heart carries rhythm — the pulse underlying all music — and within it: reflection, symmetry, complexity. The studio is built for intuitive, compassionate making: tools that answer to feeling as much as measurement. It is curvaceous and feminine, enveloping the Diamond. The heart is a reminder of strength in expression, and of why we create at all.

**Phi (φ)** — More than proportion. Phi stands for consciousness itself: the integrated information of IIT, the universal consciousness field of Strømme's theory. The mel scale encodes the implicate order of mind into the geometry of the canvas — frequency mapped to pixel so that equal vertical distance is equal perceived distance, the piano keyboard aligning cleanly to the vertical axis. The proportions are not a convenience; they are the shape of awareness made visible.

## Design Principles

| Principle | Symbol | How it is expressed |
|-----------|--------|---------------------|
| **Nonduality** | Möbius Strip | The Pool codec is mathematically lossless, the Stream codec immediate. Audio and spectrogram are one continuous surface — encode → edit → decode leaves no seam, no artefact, no break |
| **Consciousness** | Phi | The mel-scaled frequency axis encodes the implicate order of perception into the canvas geometry. Equal vertical distance = equal perceived pitch; the piano keyboard maps cleanly to the vertical axis |
| **Explicate order** | Diamond | Non-destructive layers, blend modes, unlimited undo, and per-layer transforms make every decision explicit and every path revisable |
| **Life** | Heart | Painting tools designed for intuitive, expressive sculpting of sound — rhythm, symmetry, and complexity in service of feeling |
| **Eternity** | Infinity | Multi-layer compositing, cross-layer cloning, image-to-audio conversion — no ceiling on creative scope, no final state |

# Concepts

## What is a Spectrogram?

A spectrogram is a 2D representation of audio:

- **Horizontal axis** — time (left to right)
- **Vertical axis** — frequency (low at the bottom, high at the top)
- **Pixel brightness** — loudness at that frequency and moment in time
- **Color** — stereo balance (left channel is one color, right channel another) 

## Frequency Scale

Frequencies are encoded on a **logarithmic scale** by default, which is close (but not identical) to the **mel scale** that matches human hearing.

## Time Scale

Every horizontal coordinate corresponds to a few milliseconds (N). That is, the pixels in column X represents the sound energy distribution over time [X\*N,X\*(N+1)).

## What Editing Does

- **Painting amplitude pixels brighter** makes that frequency louder at that moment in time.
- **Painting amplitude pixels darker** (toward zero) makes it quieter or silent.
- **Editing phase pixels** changes the waveform shape at that time-frequency bin — altering the perceived timbre or creating pitch-shift effects without changing loudness.
- **Applying filters** reshapes the spectral content across regions or the whole layer.
- **Compositing layers** blends multiple sounds together, with blend modes controlling how they interact.

# Codec and Format

Sound Mind Codec has two operating modes: "stream" and "pool"

## Pool Mode

The Pool codec mode is for high-fidelity, off-line processing. It employs the Non-Static Gabor Transform to encode sound into spectrograms, and to decode spectrograms into sound. A Sound Mind Pool file is the encoded spectrogram. NSGT is inversible, so once a sound is encoded, it can be decoded and encoded endlessly without loss of fidelity. Similarly, once an image is decoded, it can be encoded and decoded endlessly without further loss of image quality.

This is basically the same as what was developed in the prototype Sound Mind project. See CODEC_DETAILS.md (copied from the prototype project) for details - this may require modifications for the new Sound Mind version.

### Sound Mind Pool Format

The Sound Mind Pool Format is a special TIFF file. See SOUND_MIND_TIFF_SPEC.md (copied from the prototype project) for details - this may require modifications for the new Sound Mind version.

## Stream Mode

The Stream codec mode is for fast processing, suitable for use in live performances, in electronic instruments, or for immediate feedback in the Studio. It employs non-inversible transforms that can run in real-time, either on the CPU or on a GPU. These transforms are configured and tuned to correspond as closely as possible to the Pool mode, while optimized for speed and portability.

### Sound Mind Stream Format

A Sound Mind Stream file is an archive containing a source (sound or image file, channel, or stream) and associated metadata needed for a Sound Mind device (e.g. the Studio or a Sound Mind VST plugin) to process it.

# Sound Mind Studio

The Sound Mind Studio is the visual audio editor. Primarily a Digital Audio Workstation, the Studio is also a full-featured image editing program, and bridges the gap between manipulating images and sound, or between the visual arts and music.

The Studio operates within Sound Mind Projects files, and can import and export sound and images of all common formats. It can also export to video, and Sound Mind Projects can be exported and shared in whole or in part.

## Sound Mind Projects

Sound Mind Projects are records of editing steps performed in the Studio, the imported media on which those edits are made, and the generated output.

## Pool vs Stream

In order to enable an easy and smooth user experience in the Studio, the Studio will primarily operate in Stream Mode. That means, after importing media into the project, all image and audio operations will produce immediate feedback.

However, this breaks the "nondual" principle behind Sound Mind, so Pool processing is available as a manual step. To minimize the auditory and visual difference between Pool and Stream output, audio will be always be imported in Pool mode, with the resulting Pool file converted to a light-weight Stream copy.

In the Studio context, "Stream Mode" also means faster image processing - rather than processing and rendering large and clunky 4-layer TIFF (Pool format) files when painting, the Studio will use regular RGB images for immediate visual feedback. One consequence is that all channel-specific phase data that would be encoded in the 4-layer TIFF is lost in RGB.

Finally, to close the loop to nonduality, the user also has the option to "pool" an imported image or a painted layer - this performs a Pool export to audio, followed by a Pool import. Subsequent exports and imports will not affect the image or the sound.

## Non-destructive Processing

Operations in Sound Mind Projects are fully non-destructive. Importing a sound file copies the media to a project subdirectory and performs the encoding to an image. Painting in that image adds a "paint event" that exactly describes the paintbrush used and the path it followed, without modifying the image. This "paint event" can be copied, moved, modified, or removed.

When performing a Pool export, all the operations are applied to the Pool-format media, rather than the lighter Stream media. Pool-mode operations are all fully phase-aware, for optimal audio reconstruction.

## Layers

Sound Mind Projects are composed of multiple layers, which are composited to a visual whole before decoding to sound. Each layer contains metadata describing the operations applied to it, as well as the latest rendered result.

### Special Layers

Locked at the bottom is the Background Layer. It cannot be made transparent, nor can it be hidden, removed, or re-ordered. It can be painted on like any other layer.

Locked at the top is the Equalizer Layer. It can not be re-ordered. It is a Filter layer of Equalizer type.

### Layer Types

There are two layer types - Normal and Filter. 

#### Normal Layer

This is what can be painted on.

#### Filter Layer

This layer composits the selected beneath it (by default: all the layers down to and including the next-lower filter layer), respecting their blend modes and opacities, applies a certain filter operation (sharpen, denoise, speckle, gradient fill, custom convolve, displace, etc.) to the composite, and renders the result.

Refer to "### Adjustment Layers" in legacy/USER_GUIDE.md, but only as a guide. I think most of these filters are useful and fun, but I want to add more ways to add more interesting noise and distortion sources.

## Low Frequency Oscillations (MindWaves)

The Studio has an LFO engine enabling the user to define arbitrary LFO's for use as MindWaves. A **MindWave** is a named parametric waveform that produces a per-pixel scalar field in [0, 1] across the canvas. 

Refer to section 13 "MindWaves" in legacy/USER_GUIDE.md, but only to get the general idea. In this version of the Studio, I will revamp the way MindWaves are defined - it shall be easier and more powerful to generate some really interesting patterns.

### Filter parameters

See "MindWave-Bound Parameters" in legacy/USER_GUIDE.md. We actually want all possible filter parameters to behave like the **Offset Distance, Offset Angle, and Rotate Colors Angle** parameters, not as an opacity setting. Needs refinement.

### Layer opacity

Linking a MindWave to a layer multiplies the layer's effective opacity by that field at every pixel during compositing — areas where the waveform evaluates to 1.0 contribute at full opacity; areas where it evaluates to 0.0 are fully transparent. Any value in between produces partial blending

### Brush parameters

Much like "Filter parameters". Also needs refinement.


### LFO functions 
See "Supported Adjustment Filters" in legacy/USER_GUIDE.md. Needs refinement.


## Paths

Draws a segmented cubic Bézier curve and stamps brush marks along its length. Ideal for melodic lines, precise spectral contours, and rhythmically-timed patterns.

### Phase 1 — Placing Nodes

- **Left-click** to place a node. A preview shows the path as you move.
- **Right-click** or **double-click** to stop placing and enter edit mode.
- The **corner/smooth toggle button** (see Node Controls below) sets the type of node placed on each click *before* you click — no need to convert nodes retroactively.

### Phase 2 — Editing

| Action | Effect |
|--------|--------|
| Drag a node | Move it; handles follow |
| Drag a handle | Adjust curvature. Opposite handle mirrors by default (smooth) |
| Alt+drag a handle | Move one handle independently, creating a corner |
| Double-click a segment | Insert a new node at that position |
| Double-click a node | Delete that node |
| Click a node | Select it and show node controls in the Tools panel |
| Delete key | Delete the selected node |

### Node Controls

The **Corner ↔ Smooth** button in the Paths section of the Tools panel serves two roles:

| State | Button label | Click action |
|-------|-------------|--------------|
| No node selected | **Place: Corner** or **Place: Smooth** | Flips the placement default — subsequently placed nodes use the new type |
| Node selected | **→ Smooth** or **→ Corner** | Converts that node to the opposite type and updates the placement default to match |

**Smooth** nodes have Bézier handles: dragging them curves the path and the opposite handle mirrors to maintain a smooth tangent. **Corner** nodes have no handles; the path meets at a sharp kink.

- **Delete Node** — removes the selected node.

### Path Gradient

A full gradient editor (see [Section 9](#9-the-gradient-editor)) controls color and opacity along the path. t=0 is the start of the path; t=1 is the end.

## Gradients

The gradient editor appears in the Tools panel when the **Path** or **Fill** tool is active, and in the **Gradient Fill** filter dialog. It controls how paint color and opacity vary along the path, across the filled region, or across the frequency axis.

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

## Painting

Color, brush type, modulation settings, paths

### Paths

"Apply to existing path" or "Paint new Path"

### Procedural brushes

#### Brush tips + harmonics
Refer to legacy/USER_GUIDE.md "Brush Tip Shapes". Refine.

#### adsr+harmonic+noise+body-resonance+inharmonicity

Refer to legacy/USER_GUIDE.md "35. MIDI Instrument Editor". However, this will not be referred to as "MIDI instruments", but rather "Sound Mind instruments". Refine.

#### Mind Shots

Static samples. Sample a "Selection" from any layer at a point in time, and store that sample. That sample can be used as a brush stamp in any layer and remain exactly as it was at the time it was created.

#### Mind Grains

Dynamic samples. Sample a "Selection" from any layer, and store that reference. That reference can be used as a brush stamp in any HIGHER layer. When rendering the higher layer, the rendering engine will copy a new sample from the selection - move or modify the selection, or make a change in the source layer, and the Mind Grain stamp will be altered.

#### Order/Chaos

Pushes the painted region toward spectral order/chaos (reduces H_n)

#### Smudge

Pushes pixels along the stroke direction

#### Heal

Temporal blur

#### Soften

Radial blur

#### Clone

Clone brush; copies pixels from a source point to the cursor

### Selection

#### Rectangle

Rubber-band selection with transform handles

#### Wand

Click to flood-fill-select pixels by amplitude similarity. Optionally, it can be harmonics-aware.

#### Lasso

Freehand polygon selection

#### Warp

Warp a selection along a Path curve in time or frequency

#### Fill

This tool takes a selection and applies a color or gradient to the canvas, just inside the selection.

## Overlay Grids

Refer to legacy/USER_GUIDE.md "16. Overlays & Grids". Could use refinement. 

Refer to legacy/USER_GUIDE.md "Snap to Grid". Could use refinement. 

## Generators

Refer to legacy/USER_GUIDE.md  "Generators (`Ctrl+Alt+A`)". Definitely needs refinement.

## Analysis

Refer to legacy/USER_GUIDE.md "29. Analysis Tools". Probably many of these analysis tools can be streamlined, some can be culled, and there are other tools that could be useful to add. 

Focus here should be for music producers, editors, and musicians.

Refine.

## Color Mapping

When importing images, encoding sounds, and when rendering Sound Mind Pools on an RGB canvas, the stereo and phase channels must map to R G and B values.

Refer to legacy/USER_GUIDE.md "RGB Mode". I think I want to change this up. Definitely refine.

## Polar Coordinates (Sound Flower)

Refer to legacy/USER_GUIDE.md "Sound Flowers". Could use refinement. 

## Chords/Arpeggiator/Sequencer

Refer to legacy/USER_GUIDE.md "Chord generator". It will not create MIDI events, but any kind of paint event, configured by the user. Needs refinement.

The user shall be able to write any sequence of notes or frequencies, with a notation for timing etc., and also generate paint events in this fashion.

## Composer Mode

This switches the paint canvas into a traditional DAW track panel.

Each visible layer is a track.
There are three track background options:
 * Clean - the track has a blank background
 * Amplitude - the track shows the total amplitude over all frequencies, as grayscale or as vertical stacks.
 * Thumbnail - the track background is the actual painted layer, but squished vertically to fit to the track

Project objects are rendered on the track as boxes.

The Composer Mode allows the user to easily re-arrange objects, copy or move them between layers, adjust timing, etc. Also adjusting the gain, equalizer, etc., of individual objects, tracks, or track sections is easy in this mode.


## Record

There's a Record panel allowing the user to record to a new layer, with a start/stop recording button, input device selector with refresh button, and input gain control.

Refine.

## Playback

There's a playback control with start/pause/stop buttons, an output device selector with refresh button, and a volume control.

Refine.

## Live

Refer to legacy/USER_GUIDE.md. See "32. Live Mode". Refine.

## Import

Images and sound files as new layers.

MIDI files as new layers, mapping each MIDI program/instrument to a paint preset (any kind of brush tip, MindShot, or Sound Mind Instrument) and creating a paint object for each MIDI event.

Images and sound files as pasted objects in an existing layer.

Layers from other Sound Mind Projects.

Mind Shots from other Sound Mind Projects.

Mind Waves from other Sound Mind Projects.

Sound Mind Instruments from other Sound Mind Projects.

Refine.

## Export

Exporting sound is best done in Pool mode, to enable full phase-aware processing for the best possible (re)construction of audio signals.

See legacy/USER_GUIDE.md "35. Video Export" regarding video export. Refine.

## Pool

"Pool" a layer or the entire project - this performs a "pool export" of the input media and then imports the results to replace the original input. 

In effect, the input is not replaced (that would break the non-destruction principle), but the original ("raw") input layers are hidden and the "pooled" input layers are inserted into the project in their stead.

Also paint operations etc are "pooled" - the original ops are kept but made hidden, and the "pooling" operation is recorded in the op log.

Could use refinement. 

# Sound Mind VST

