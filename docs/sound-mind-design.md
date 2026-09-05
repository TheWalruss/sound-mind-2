# Sound Mind - the Design Document

# Vision

Sound Mind is a creative toolkit for working with audio as a visual medium. It encodes sound as a spectrogram image — a picture you can paint, layer, filter, and transform — then reconstructs audio from the result with perfect fidelity.

The project spans a codec library and a full graphical studio. Together they let you see, touch, and reshape sound in ways that traditional audio editors cannot.

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

The working target for that immediacy is roughly 100 ms from a paint or filter action to updated graphical feedback, and roughly 250 ms to updated audio feedback. These are aspirational targets to validate empirically once a real Stream pipeline exists, not guarantees — faster is welcome, and slower is still usable.

### Sound Mind Stream Format

A Sound Mind Stream file is an archive containing a source (sound or image file, channel, or stream) and associated metadata needed for a Sound Mind device (e.g. the Studio or a Sound Mind VST plugin) to process it.

# Sound Mind Studio

The Sound Mind Studio is the visual audio editor. Primarily a Digital Audio Workstation, the Studio is also a full-featured image editing program, and bridges the gap between manipulating images and sound, or between the visual arts and music.

The Studio operates within Sound Mind Projects files, and can import and export sound and images of all common formats. It can also export to video, and Sound Mind Projects can be exported and shared in whole or in part.

## Sound Mind Projects

Sound Mind Projects are records of editing steps performed in the Studio, the imported media on which those edits are made, and the generated output.

A project is stored as a project file alongside a folder holding its media, sources, and cached renders — not a single opaque archive. A formal, versioned schema for that file is a near-beta concern rather than a day-one one; early on, the format is free to change as the design settles. The design also assumes a single user editing a project at a time — sharing or merging a project between multiple people is out of scope for now.

## Pool vs Stream

In order to enable an easy and smooth user experience in the Studio, the Studio will primarily operate in Stream Mode. That means, after importing media into the project, all image and audio operations will produce immediate feedback.

However, this breaks the "nondual" principle behind Sound Mind, so Pool processing is available as a manual step. To minimize the auditory and visual difference between Pool and Stream output, audio will be always be imported in Pool mode, with the resulting Pool file converted to a light-weight Stream copy.

In the Studio context, "Stream Mode" also means faster image processing - rather than processing and rendering large and clunky 4-layer TIFF (Pool format) files when painting, the Studio will use regular RGB images for immediate visual feedback. One consequence is that all channel-specific phase data that would be encoded in the 4-layer TIFF is lost in RGB.

Finally, to close the loop to nonduality, the user also has the option to "pool" an imported image or a painted layer - this performs a Pool export to audio, followed by a Pool import. Subsequent exports and imports will not affect the image or the sound.

## Non-destructive Processing

Operations in Sound Mind Projects are fully non-destructive. Importing a sound file copies the media to a project subdirectory and performs the encoding to an image. Painting in that image adds a "paint event" that exactly describes the paintbrush used and the path it followed, without modifying the image. This "paint event" can be copied, moved, modified, or removed.

A project's real content is therefore closer to a scene graph — objects, events, and operations referencing source data — than a stack of raster images. The raster pixel data a layer currently shows is a cached, replayable *result* of that graph, not the source of truth: it's buffered during editing and written to disk at convenient points (saving the project, switching the active layer, and similar moments) purely so the whole history doesn't need replaying from scratch every time.

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

Every layer-shaping operation available as a one-shot edit (see [Painting](#painting) and [Selection](#selection)) is also available as a Filter layer, so it can be applied non-destructively to a running composite instead of baked into a single layer. Broadly, they fall into a few families:

- **Blur & focus** — softening (uniform, edge-preserving, or directional) and its opposite, sharpening, for smoothing rough edges or tightening a transient.
- **Noise & distortion** — adding or removing speckle noise, and denoising a noisy composite. This is the family most worth growing: candidates beyond that classic pair include bit-depth/quantisation crush, granular texture noise, feedback-style resonant distortion, and spectral wavefolding — anything that introduces controlled, characterful noise or distortion rather than just cleaning it up.
- **Geometric** — displacing pixels from a shifted source position, and cycling amplitude and phase content between each other.
- **Tonal** — a tone curve, channel balance between left and right, and inversion.
- **Spectral shaping** — a frequency-axis gradient (the basis of the Equalizer layer, see [Special Layers](#special-layers)), and an arbitrary convolution kernel for custom spectral or temporal responses.
- **Space** — a spectral reverberation, extending a sound through time the way a physical space would.

Every parameter across every one of these families can be bound to a MindWave (see [Filter parameters](#filter-parameters)), which is what makes them genuinely spatial tools rather than uniform, whole-canvas effects.

## Low Frequency Oscillations (MindWaves)

A **MindWave** is a named parametric waveform that produces a per-pixel scalar field in [0, 1] across the canvas — a spatial "LFO" that varies over time (horizontal axis), frequency (vertical axis), or both. Any numeric parameter that shapes a layer, a filter, or a brush stroke can be bound to a MindWave instead of a fixed value, replacing a single number with a field that varies across the canvas.

Rather than a long flat menu of named pattern types, MindWaves are meant to be built from a small set of primitives that compose:

- a handful of **generator types** — periodic, envelope, stepped/noise, spatial, and fractal (see [MindWave Functions](#mindwave-functions));
- **superposition**, combining any two or more waves with a chosen blend;
- and, recursively, a generator's own parameters — its period, its phase, its centre — can themselves be bound to another MindWave.

That last point is what makes the system more powerful without growing the list of named functions: a sine whose period is itself driven by a slow noise field behaves like nothing in a flat catalogue of presets, without needing to be one. Complexity comes from composing a few simple, well-understood pieces rather than from adding more of them.

### Layer opacity

Linking a MindWave to a layer multiplies the layer's effective opacity by that field at every pixel during compositing — areas where the waveform evaluates to 1.0 contribute at full opacity; areas where it evaluates to 0.0 are fully transparent. Any value in between produces partial blending.

### Filter parameters

Any numeric parameter of a Filter layer can be bound to a MindWave in the same way. The bound value becomes the *ceiling* the parameter reaches where the wave is brightest; where the wave is dark, the parameter falls toward its unfiltered baseline. This makes a filter spatially selective — sharpening only the loud transients, blurring only the trailing edge of a note, cutting only one frequency band.

Two kinds of parameters behave differently under this binding, and the distinction matters more than the mechanism:

- **Direct parameters** — a displacement distance, a displacement angle, a hue-rotation angle, and similar values that describe *where* something is sampled from or *how* it is oriented — are evaluated exactly per pixel. The MindWave literally is the parameter's value at that point.
- **Shape parameters** — a blur radius, a reverb size, a convolution kernel, and other values that describe the *shape* of an operation rather than a single per-pixel number — cannot vary continuously per pixel without recomputing the whole operation at every point. These are approximated: the filter runs once at its configured strength, and the MindWave blends the filtered and unfiltered result together. The approximation is indistinguishable from the exact version for anything that behaves linearly, and close enough elsewhere to be musically useful.

**Offset distance**, **offset angle**, and **hue-rotation angle** are examples of direct parameters — every other filter parameter should be treated as a shape parameter unless the same direct reasoning clearly applies. All filter parameters are meant to work this way, so a filter's behaviour when MindWave-bound never needs a special case in the design: it is either exact by nature, or a blended approximation, and the experience of using it is identical either way — any parameter's fixed value can become a field.

### Brush parameters

The same binding applies to brush parameters — size, opacity, color/intensity, and the direct parameters described above (offset, angle) all accept a MindWave in place of a fixed value, with the same direct-vs-shape distinction. This lets a single stroke vary its own strength, width, or hue as it crosses the canvas, driven by the same waveform library used for layers and filters.

### MindWave Functions

A MindWave's shape comes from one of a small family of generator types:

- **Periodic waveforms** — sine, triangle, square, sawtooth, and pulse cycles along one axis, for rhythmic breathing envelopes or regular fades.
- **Envelope shapes** — exponential decay, a decaying oscillation, and an S-curve transition, for one-shot fades and thresholds rather than repeating cycles.
- **Stepped and noise fields** — a quantised staircase, and smooth (Gaussian-filtered) or fractal noise, for organic or mechanical texture rather than a clean curve.
- **Spatial patterns** — ripples, checkerboards, cellular (Voronoi-like) blobs, and domain-warped noise, evaluated across both axes at once rather than projected from a single one.
- **Fractal fields** — a branching structure (see [Generators](#generators)) evaluated as an opacity field instead of stamped as pixels, so a layer or filter can fade in and out along the shape of a fractal rather than a wave.

Any two or more MindWaves combine into a new one by **superposition** — layering waves together the same way layers themselves composite, with a chosen blend (multiply, add, min, max, average) determining how each one folds into the running result. Combined with the recursive parameter binding described above, superposition is how genuinely complex, interference-like patterns are built from a small number of simple, well-understood pieces.

## Paths

A **Path** is a segmented cubic Bézier curve that a brush stamps along its length, rather than following the raw mouse motion. It is ideal for melodic lines, precise spectral contours, and rhythmically-timed patterns that would be hard to draw freehand.

### Placing and Editing

A path is built in two phases: first its nodes are placed one at a time, previewing the path as it grows; then, once placing ends, the same path is entered into an edit phase, where any node can be moved, converted, inserted, or deleted, and any handle can be dragged to reshape the path around it.

Every node is one of two types:

- **Smooth** nodes carry a pair of Bézier handles. Moving one handle curves the path on that side, and the opposite handle mirrors it by default to keep the tangent smooth through the node — though the two can also be detached to meet at a controlled corner instead.
- **Corner** nodes have no handles; the path simply meets at a sharp kink.

The type used for newly placed nodes is a standing default that can be flipped at any time, and an existing node can also be converted from one type to the other after the fact, which updates that default to match. This means a whole path can be placed quickly with one node type, then selectively broken at just the nodes that need a corner instead.

### Path Gradient

A full [gradient](#gradients) controls color and opacity along the path, from its start (t=0) to its end (t=1).

## Gradients

A **gradient** controls how paint color and opacity vary along a Path, across a filled selection, or across the frequency axis of an Equalizer filter. The same gradient model is shared by every tool and filter that needs one, rather than each having its own.

### Stops

A gradient is defined by an ordered set of **stops** along a normalized position from t=0 (the start — the beginning of a path, or one edge of a fill) to t=1 (the end). There are always at least two stops, at t=0 and t=1; additional stops can be inserted anywhere in between, and any stop but the two endpoints can be removed. Between stops, all values interpolate linearly, so a gradient with only two stops is a simple ramp, and additional stops sculpt more complex shapes.

### Stop Values

Each stop carries independent intensity and opacity for the left and right channels:

- **Left / right intensity** — the amplitude target that channel's paint reaches at this stop.
- **Left / right opacity** — how strongly that target is written at this stop; zero leaves existing content untouched regardless of intensity.
- **Link channels** — an optional mode where editing the left channel's values mirrors them to the right, for gradients meant to affect both channels identically.

New gradients start fully transparent (zero intensity and opacity at both stops), so a freshly created gradient has no visible effect until values are deliberately set — nothing is painted by accident.

### Examples

**Uniform color** — both stops share the same intensity and opacity; the path or fill paints at one consistent color and strength throughout.

**Fade to silence** — full intensity and opacity at t=0, dropping to zero intensity at t=1; the painted result fades out along the path or fill direction.

**One channel only** — with channels unlinked, one channel carries full intensity and opacity at every stop while the other stays at zero; only that channel is affected, end to end.

**Opacity envelope** — intensity held constant across all stops while opacity varies; the target color never changes but the strength with which it's applied does, shaping the dynamics of a note without changing its timbre.

## Painting

Painting is how sound is sculpted directly: every brush writes into the amplitude and/or phase of the active layer, and every stroke is recorded as a non-destructive operation rather than a pixel change (see [Non-destructive Processing](#non-destructive-processing)). A stroke can either follow the raw motion of the cursor, or be stamped along a previously-drawn [Path](#paths) for precise, repeatable placement — the same brush and gradient settings apply either way.

A mouse or trackpad is the baseline input device; support for a pressure-sensitive tablet is a desirable addition, not a hard requirement.

### Procedural Brushes

A brush's **tip** defines the geometric footprint stamped at each paint event — a shape from a shared library spanning basic fills (circle, square, diamond, triangle), lines and line compounds (single strokes, crosses, star patterns), corners and arcs, and procedurally scattered textures (dot spatter and dapple patterns), all sharing a common falloff (edge softness) control. Tip shape is independent of tone: it governs *where* a stamp lands, not *what it sounds like*.

### Sound Mind Instruments

A **Sound Mind Instrument** is a small parametric sound model that can be painted with directly, saved, named, and shared between projects — the same way a Mind Shot is, but generative rather than sampled. Where a brush stroke implies a pitch (from where it lands on the frequency axis), a Sound Mind Instrument synthesizes a stamp around that pitch from:

- a **harmonic series** above the fundamental, with adjustable strength per harmonic and an **inharmonicity** stretch so the overtones can drift sharp of a pure integer series, the way a real vibrating body does;
- a **noise** component, for breath, bow, and attack texture that a pure harmonic stack lacks;
- a simple **body resonance**, a formant-like emphasis that colors the harmonic content the way an instrument's resonant chamber would;
- an **ADSR envelope** shaping the attack, sustain, and release of each note over time.

Together these give a single instrument definition a wide range of plausible characters, tunable continuously rather than chosen from a fixed list — and, like any other brush parameter, every one of these can be bound to a [MindWave](#low-frequency-oscillations-mindwaves) for organic variation from note to note. Sound Mind Instruments are what [MIDI import](#import) and the [Chords/Arpeggiator/Sequencer](#chordsarpeggiatorsequencer) render through — a note event, wherever it comes from, is a pitch and a timing that any Sound Mind Instrument (or indeed any other paintable tip: a plain procedural brush, a Mind Shot, a Mind Grain) can be asked to paint.

### Mind Shots

Static samples. A Mind Shot captures a selection from any layer at a point in time and stores it permanently. That sample can then be used as a brush stamp on any layer, and always paints back exactly as it was when captured, independent of later changes to its source.

### Mind Grains

Dynamic samples. A Mind Grain stores a *reference* to a selection on a layer, rather than a captured copy, and can be used as a brush stamp only on layers above its source. Each time the stamp is rendered, a fresh grain is drawn live from the referenced selection — so moving or repainting the selection, or editing the source layer, changes the stamp on every layer that uses it.

### Order/Chaos

Pushes the painted region toward spectral order or spectral chaos — concentrating a region's energy into fewer, stronger, coherent components, or dispersing it into broader, more random ones. Used together, they let a passage be pushed toward, or held at, the critical balance between rigid and noisy that most musically alive sound occupies.

### Smudge

Pushes pixels along the stroke direction, stretching and blending sound in time and frequency.

### Heal

Temporal blur — replaces the painted region with a blend of its neighbours in time, for erasing a stray mark or defect without disturbing the surrounding texture.

### Soften

Radial blur — softens a region uniformly in every direction, rather than along a single axis.

### Clone

Copies pixels from a source point to the cursor as you paint, offset consistently as the stroke moves.

### Selection

A selection scopes an operation — cut, copy, paste, delete, or fill — to a specific region of a layer rather than the whole thing.

#### Rectangle

A rubber-band rectangular selection with resize and rotate handles.

#### Lasso

Freehand polygon selection, for tracing an irregular boundary — the body of a single note isolated from its neighbours, or a diagonal frequency smear — that a rectangle can't follow.

#### Wand

Flood-fill-selects connected pixels by amplitude similarity from a chosen point. Optionally harmonics-aware, extending the selection to a note's overtone rows along with its fundamental, rather than only the pixels immediately touching it.

#### Warp

Warp a selection along a Path curve, in time or frequency.

#### Fill

Fills the selected region with a color or gradient (see [Gradients](#gradients)), confined exactly to the selection's boundary.

## Overlay Grids

Overlay grids are visual reference lines drawn on the canvas — they never affect encoding, decoding, or any stored pixel data. They exist purely to make time and frequency legible while painting.

### Frequency Grid

Vertical reference lines marking specific frequencies, built from any combination of:

- a **note grid**, anchored to a tuning reference (concert pitch at 440 Hz by default, with common alternate historical and philosophical tunings available as presets);
- a **harmonic series** grid, at integer multiples of a chosen fundamental;
- **custom frequencies**, entered directly or loaded from a preset set (e.g. commonly-cited reference-tone frequency sets some users like to work against).

The same grid can also drive **pitch quantising** while painting — snapping a stroke's pitch to the nearest active grid line — which is what lets a brush stay musically in tune to an arbitrary tuning or scale, rather than only equal temperament.

### Timing Grid

Horizontal reference lines marking moments in time, either at a fixed interval or as a beat/bar grid derived from a project tempo, with subdivisions down to the finest rhythmic value in use.

### Chord Overlay

When a chord or arpeggio is selected in the Chord Generator (see [Chords/Arpeggiator/Sequencer](#chordsarpeggiatorsequencer)), its notes are drawn live on the frequency axis — independent of, and in addition to, the general frequency grid — so the notes are visible on the canvas before or while they're stamped.

### Snap to Grid

An optional mode where any position a tool would otherwise place freely — a pasted region, a selection, a text box, a Path node, a layer transform handle — snaps instead to the nearest line of whichever active grid is finest at that point. With no grid active, snapping has nothing to snap to and behaves as if it were off.

## Generators

Generators seed spectral material algorithmically, as an alternative starting point to painting by hand or importing a recording. A generated result becomes an ordinary layer, is fully paintable and filterable afterward, and can be blended with existing material the same as any other source.

All generators share the same order/chaos control used by the [Order/Chaos](#orderchaos) brush: pushed toward order, a generator settles into simple, rigid, repetitive structure; pushed toward chaos, it produces broadband, unstructured noise; near the middle, it produces the rich, complex-but-coherent texture most generators are actually used for. Three families of generator sit on that same spectrum:

- **Lattice generators** — a grid of coupled cells evolving under simple local rules, producing an organic, self-similar texture across the whole canvas.
- **Fractal generators** — a branching grammar (the same kind behind fractal [MindWave functions](#mindwave-functions)) grows a tree-like structure directly into amplitude and phase, well suited to melodic or harmonic shapes that branch and vary rather than repeat.
- **Streaming generators** — rather than a static texture, these read out a genuinely time-varying signal, evolving continuously across the full duration of the layer instead of settling into one fixed pattern.

Every generator run is deterministic from its seed: replaying it later reproduces its result bit-for-bit, on the same Sound Mind Studio version and target architecture, consistent with the project's non-destructive, replayable operation log. Across different architectures (Arm64 versus x64), only a perceptual match — "sounds the same" — is guaranteed, not a bit-identical one. The same guarantee applies to any other seeded randomness in the Studio, such as the Chaos brush.

## Analysis

Analysis tools read properties of a layer directly from its amplitude and phase data — no decoding to audio is required, and nothing they show ever modifies the layer. The legacy version accumulated a long list of narrow, single-purpose meters; here they're organized around what a music producer, editor, or musician is actually trying to check, with the set of tools in each category free to grow or shrink as that need does:

- **Loudness & mastering** — integrated and momentary loudness, peak level, and dynamic range, referenced against common streaming and broadcast targets.
- **Pitch & vocal coaching** — fundamental-frequency tracking, vibrato characterization, formant content, and key/scale detection.
- **Stereo & phase** — correlation, stereo width, and a goniometer view, for catching mono-compatibility and phase-cancellation problems before they reach a mix.
- **Spectral health** — clipping and saturation, edit-boundary artifacts, transient detection, and masking between overlapping layers or frequency bands.
- **Criticality & pattern** — the same order/chaos measure that drives the [Order/Chaos](#orderchaos) brush and the [Generators](#generators), plus tools that flag unexpected (novel) moments or find repeated motifs across a layer.

Any of these can run once over a whole layer or a selection, and several can update live during playback, so mixing and coaching decisions can be made by ear and by eye at the same time.

## Color Mapping

Sound Mind's underlying data has no color of its own — it is amplitude and phase for two channels. Color only enters wherever that data has to appear on, or come from, an RGB canvas: importing an image, encoding a sound onto a visual canvas, or rendering a Pool file for viewing. All three moments need the same underlying answer — which of the two channels' amplitude and phase map to which of red, green, and blue — so they should share a single, simple mapping rather than each inventing its own.

A minimal version of that mapping needs only: which channel drives red, which drives green, and what (if anything — it can be left empty) drives blue. Left amplitude and right amplitude should always occupy two of the three; the third is the interesting choice, and is where phase, a blend of both channels, or nothing at all can go. This single small decision, made once and reused everywhere color is involved, is what should replace the larger preset catalogue the legacy version accumulated.

This two-channel, amplitude-and-phase model is the working assumption for the format, not a hard architectural ceiling — extending it to more channels (surround, ambisonics) is conceivable in principle. But visualizing and painting even two channels already uses most of what a three-channel color image can comfortably show, so there's no pressure to generalize beyond it before it's actually needed.

## Polar Coordinates (Sound Flower)

A **Sound Flower** is a polar rendering of the same spectrogram data as the standard flat view — never a separate copy. Time wraps around the ring (the start of the layer at twelve o'clock, advancing clockwise) and frequency radiates outward from the centre (lowest at the centre, highest at the outer edge), so the result reads like a mandala whose rings encode pitch and whose rotation encodes time.

Because the two views share the same data, switching between them changes only how the canvas is drawn and how a click maps to a pixel — every paint and selection tool works identically in either view, with clicks and drags inverse-mapped back to the same rectangular coordinates the flat view would have used.

A source image that already looks like a flower — a spectrogram exported in polar form from elsewhere, or any image whose content radiates from a centre point — can be imported directly: given the image's centre, sampling radius, and arc, it is unwrapped back into an ordinary rectangular layer, the same as any other image import.

## Chords/Arpeggiator/Sequencer

A **sequence** is a written description of a series of notes or frequencies over time — pitch and timing, in a compact notation rather than placed by hand — that generates paint events instead of a fixed instrument's audio. Anything paintable can be the instrument a sequence plays through: a plain procedural brush, a [Sound Mind Instrument](#sound-mind-instruments), a Mind Shot, or a Mind Grain.

The Chord Generator is the common, structured case built on top of this: choose a root note, a chord category (triads, sixths, sevenths, ninths, extended/added tones), and a specific voicing, and either sound every note together (**block chord**) or sequence them one at a time (**arpeggio**) in a chosen order and rhythm. Its notes preview live on the frequency grid (see [Chord Overlay](#overlay-grids)) before anything is committed to the canvas.

Beyond chords, the same underlying notation lets a sequence be written directly — any series of notes or frequencies with their own timing — so a melodic line, a rhythmic pattern, or an arpeggio that doesn't fit a named chord shape can all be generated the same way a chord is. Because the result is paint events rather than baked pixels, a generated sequence stays editable afterward: re-voice it, re-time it, or point it at a different instrument, without repainting it from scratch.

The intent is to reuse an existing, established text notation for this rather than invent a new one. ABC notation is a reasonable starting candidate — compact, plain-text, and already expressive enough for pitch, duration, and key — but it needs validating against Sound Mind's actual needs (arbitrary frequencies, not just named pitches; the codec's own timing resolution) before being committed to.

Driving a sequence live from a connected MIDI controller, in addition to writing or painting it, is a desirable addition, not a hard requirement.

## Composer Mode

Composer Mode switches the canvas into a traditional DAW-style track view, alongside — not instead of — the direct spectrogram canvas. Each visible layer becomes a track, and every track has one of three background styles:

- **Clean** — a blank background.
- **Amplitude** — the track's total amplitude across all frequencies, shown as grayscale or as vertical stacks.
- **Thumbnail** — the actual painted layer, squashed vertically to fit the track.

Every paint operation is rendered on its track as a box. Composer Mode is where those boxes get arranged: re-order or retime them, copy or move them between layers, and adjust the gain, equalizer, or other settings of an individual object, a whole track, or a section of a track, all without leaving the timeline view.

## Record

Recording captures audio directly from a connected input device into a new layer: choose the input device (with the option to rescan for newly connected ones), set an input gain, and start or stop the capture. Once captured, the audio is encoded into the layer exactly as any other imported audio would be.

## Playback

Playback provides standard transport — play, pause, and stop — over the current composite, an output device selection (with the option to rescan), and a volume control. Because Stream mode is built for immediate feedback, playback reflects the project as it currently stands, edits included, without a separate render step.

## Live

Live turns the Studio into a real-time instrument or effect: a live input signal — microphone, instrument, or synthesizer — is continuously encoded into a dedicated layer, composited with the rest of the project exactly like any other layer (including its own blend mode and opacity, and any MindWave or filter applied to it), and streamed back out. Because Stream mode's transforms are designed to run in real time, this is a genuinely continuous signal path rather than a record-then-playback loop — the same low-latency path that underlies [Sound Mind VST](#sound-mind-vst).

## Import

Audio and images can enter a project two ways: as a new layer of their own, or pasted as an object into an existing layer at a chosen position — the same distinction as painting a stamp, just sourced from a file instead of a brush.

MIDI files import as a new layer too, but rather than becoming a fixed audio rendering, each MIDI program/instrument in the file is mapped to a **paint preset** — any brush tip, Mind Shot, or [Sound Mind Instrument](#sound-mind-instruments) — and every MIDI note becomes an individual paint event using that preset. The imported layer is therefore fully editable afterward: change the preset a program maps to, and every note that used it repaints through the new one.

A project's shareable resources — layers, Mind Shots, MindWaves, and Sound Mind Instruments — can also be imported directly from another Sound Mind Project, rather than only from raw media files, so a library built up in one project can be reused in another.

At minimum, format coverage should match the legacy version: WAV, MP3, FLAC, OGG, AIFF, M4A, and Opus for audio; PNG, JPEG, BMP, TGA, and WebP for images; standard MIDI files. Any licensing constraint that applied to the legacy version (historically a concern for MP3 in particular, though its core patents have since expired) needs re-checking against whichever C++ libraries are actually used, rather than being assumed resolved.

## Export

Exporting audio is best done in Pool mode: the full phase-aware round trip gives the most faithful possible reconstruction, at the cost of the time a Pool encode takes (see [Pool vs Stream](#pool-vs-stream)). A quicker Stream-mode bounce remains available for scratch exports where speed matters more than fidelity. Unlike the legacy version, which only ever exported WAV, compressed audio output (at minimum MP3 and OGG/FLAC) should be supported from the start.

The Studio can also export the spectrogram canvas itself as a video, synchronised to the decoded audio — either the whole canvas visible for the full duration with a moving playback indicator, or a scrolling window that pans to keep the indicator in view. The video reflects whatever coordinate system (flat or [Sound Flower](#polar-coordinates-sound-flower)), color mapping, and light/dark mode are active in the Studio at export time, so what's exported always matches what's on screen.

## Pool

**Pooling** a layer or an entire project performs a full round trip through the lossless Pool codec and replaces the working Stream copy with the result — closing the loop back to the nondual principle that sound and image are one continuous surface, at the cost of the time a full Pool encode/decode takes.

Pooling never discards anything: consistent with [non-destructive processing](#non-destructive-processing), the original raw layers and operations are hidden rather than deleted, the pooled result is inserted in their place, and the pooling step itself is recorded as an operation that can be undone like any other.

# Sound Mind VST

*Deferred: this section records the intended direction, but implementation should wait until the standalone Studio is fully operational.*

Sound Mind's real-time Stream codec is well suited to living inside a conventional plugin host, independently of the standalone Studio. Two plugin roles follow from the same underlying pipeline:

## Sound Mind Stream (effect plugin)

A lightweight effect plugin that puts the Stream codec directly in a host's signal chain: incoming audio is encoded to a spectrogram, a small set of Sound Mind operations is applied (a handful of filters, MindWave-driven modulation, Order/Chaos sculpting), and the result is decoded back to audio, all within the host's real-time audio callback. It exposes a focused slice of the Studio's processing — not the full editing surface — as an ordinary insert effect.

## Sound Mind Player (instrument plugin)

An instrument plugin hosting a compact version of the Studio itself inside a plugin window: a project can be built or loaded there, and notes from the host trigger and pitch whatever paintable material is assigned to them — a Sound Mind Instrument, a Mind Shot, a Mind Grain source, or a plain procedural brush voice — the way a conventional sampler or synthesizer would, but built from a spectrogram rather than a waveform or oscillator.

## The Studio as a plugin host

The standalone Studio can also load third-party plugins itself, on a layer or across the whole project — an external EQ, compressor, or synthesizer used as a source or a processing step alongside Sound Mind's own tools. This keeps Sound Mind interoperable with an existing plugin-based workflow, rather than a closed system that only talks to itself.

Both plugin roles only ever use the Stream codec on the audio thread — Pool's higher-fidelity round trip stays a manual, non-real-time step (see [Pool](#pool)), consistent with the requirement that real-time audio paths never touch anything that can allocate, block, or run for an unbounded time.

---

# Deferred Decisions

Points intentionally punted rather than resolved — not blocking the move to architecture, but worth tracking:

1. **Project file schema.** A formal, versioned schema for the project file is a near-beta concern; the format is free to change during design and early development.
2. **Sound Mind VST scope.** Which plugin formats to target (VST3, AU, both) and how that interacts with JUCE's licensing tiers, is shelved until the standalone Studio is fully operational.
3. **New noise/distortion filters.** Specific additional filter types for the Filter Layer's noise & distortion family, beyond the candidates already noted, can be expanded later as ideas arise.
4. **Sequence notation.** Whether ABC notation (or another existing standard) actually covers Sound Mind's needs — arbitrary frequencies, the codec's own timing resolution — needs validating before committing to it.
5. **Real-time performance targets.** The ~100 ms graphical / ~250 ms audio feedback targets are aspirational; they need validating against real measurements once a Stream-mode pipeline exists.
6. **External format licensing.** Format and licensing constraints (MP3 in particular) need re-verifying against the specific C++ libraries chosen, rather than assuming the legacy version's constraints still apply.
