# Sound Mind Studio - Development Roadmap

Status: **first draft.** This sequences `sound-mind-design.md`'s feature set into a series of concrete, always-working Studio versions, from the current empty-window bring-up (`v0.0.0.1`) to a feature-complete `v1.0.0.0`. Expect this to be revised as work proceeds and real effort/complexity becomes clearer — it's a plan to work from, not a schedule to hold to.

## Versioning

`vX.Y.Z.W`, per your instruction:

- **X** stays `0` until the Studio is ready for its first actual release. It becomes `1` at `v1.0.0.0` and this roadmap doesn't go past that point.
- **Y** increments when a breaking change is made to one of the main project file formats (the Project file, the Pool file, or the Stream file). Introducing a format for the first time isn't itself a breaking change - there's nothing yet to break - so Y stays put the first time each format appears; it only moves when an *already-established* format changes incompatibly afterward. Adding a new, separate file type (a portable `.smwave` or `.sminst`, say) doesn't bump Y either, since it doesn't break an existing format - that's a Z-level feature.
- **Z** increments for a feature release - one of the milestones below.
- **W** increments every time a new binary should be built and released, including point releases with no new feature between them.
- **Reset convention** (standard, not explicitly stated in your instruction - flagging the assumption): bumping Y resets Z and W to 0; bumping Z resets W to 0. W never resets anything, since nothing is smaller than it.

Exactly *when* Y will bump can't be predicted precisely this far out - it depends on what actually turns out to need breaking. Milestones below where a Y bump is likely are flagged; treat the rest as probably-Y=0 until proven otherwise.

`v0.0.0.1` (already shipped) was the toolchain bring-up: empty window, no real feature yet - Z and Y both still 0.

## Sequencing principles

1. **Stream before Pool.** Per the design doc, the Studio operates in Stream mode by default; Pool is a deliberate, manual, higher-fidelity step layered on top. Building the fast path first means every milestone from early on has real audio going in and out, rather than waiting for the much harder lossless codec to be finished first.
2. **Every milestone is a working Studio**, not a library-only checkpoint. Each one below ends with something you can actually open, do a thing in, and see/hear the result of.
3. **Dependency order, not design-doc reading order.** The sequence below follows what each capability actually needs to exist first, which isn't the same order the design doc presents things in.
4. **Open questions get resolved where the work that needs them happens**, not all up front. Each milestone that depends on one of `sound-mind-architecture.md`'s Decisions Needed / Deferred Decisions says so.

## Explicit non-goals for this roadmap

- **Sound Mind VST is not on this path.** Per the design doc, it's deferred until the standalone Studio is fully operational - that means after `v1.0.0.0`, not before it.
- **Collaboration/multi-user** stays out of scope throughout, per the design doc's resolved decision.
- **A final Project file schema** is not a pre-1.0 requirement - the design doc explicitly treats that as a near-beta concern. Expect Y to move once, deliberately, around there.

---

## Phase 1 - Foundation

### v0.Y.1.1 - Project & Canvas

A real `Project` (settings, an ordered layer list, the operation log) that loads and saves as the JSON project file sketched in the architecture doc, and a canvas that renders the (still-empty) Background layer. No codec yet - the canvas can show a placeholder/solid raster.

**Demo:** open the Studio, see a canvas backed by a real, saved-and-reloadable project file.

### v0.Y.2.1 - Stream Codec

The Stream codec itself: encode a short audio clip into a Stream-format layer, decode it back to audio. This is where the NSGT/FFT backend Decision Needed gets resolved for real (the permissive-FFT-backend direction already scoped in the architecture doc) and the Stream file container format decision gets settled.

**Demo:** a round-trip test - audio in, Stream-encoded, decoded, audibly the same clip back out.

**Likely Y bump:** first time the Stream file format is exercised for real; whether it needs to change again once real content is flowing through it is the open question.

### v0.Y.3.1 - Import & Display

Real audio and image import as new layers, using the Stream codec above; a first, minimal Color Mapping (single-page grayscale plus a basic RGB composite).

**Demo:** import a WAV or an image, see it rendered on the canvas as a real layer, not a placeholder.

### v0.Y.4.1 - Playback

Transport controls, Stream-mode decode of the live composite, audio output.

**Demo:** import something, press play, hear it.

---

## Phase 2 - Painting & Editing

### v0.Y.5.1 - Basic Painting

A plain procedural brush (tip shape + falloff, no harmonic model yet) painting into a layer's amplitude as logged `PaintOperation`s; undo/redo via the operation log (first real exercise of the `supersedes` mechanism).

**Demo:** paint a stroke, hear the difference on playback, undo it.

### v0.Y.6.1 - Selection & Fill

Rectangle, Lasso, and Wand selection with boolean combination; cut/copy/paste; the Gradient model; Fill.

**Demo:** select a region, cut it, paste it elsewhere, fill another region with a gradient.

### v0.Y.7.1 - Paths & Grids

The Path (Bézier) tool with node placement/editing and Path Gradient; Overlay Grids (frequency and timing) and Snap to Grid, including pitch quantising.

**Demo:** draw a precise, grid-snapped melodic line.

### v0.Y.8.1 - Filter Layers

The Filter layer type, a first concrete filter set (blur family, sharpen, tone curve, frequency-axis gradient), and the Equalizer special layer made functional.

**Demo:** add an EQ layer, reshape frequency balance, hear it.

---

## Phase 3 - Expressive Tools

### v0.Y.9.1 - MindWaves v1

The core generator types (periodic, envelope, stepped/noise, spatial, a first fractal field), superposition, and binding to layer opacity and filter parameters (the direct-vs-shape distinction).

**Demo:** bind a sine MindWave to a layer's opacity; watch and hear it pulse.

### v0.Y.10.1 - Sound Mind Instruments

The harmonic-series + inharmonicity + noise + body-resonance + ADSR instrument model; the canvas-space vs. operation-relative MindWave binding-coordinate-frame choice, since that's specifically about how a paint operation (an instrument note, in particular) binds to a MindWave.

**Demo:** paint with an instrument voice that actually sounds like a plausible physical source.

### v0.Y.11.1 - Mind Shots & Mind Grains

Capture-and-stamp static samples; live-reference dynamic grains from a source layer.

**Demo:** capture a moment as a Mind Shot and restamp it; link a Mind Grain to a source layer and watch it change live as the source does.

### v0.Y.12.1 - Composer Mode

The DAW-style track view: each layer as a track, operations drawn as boxes via `Operation::bounds()`, retiming/moving an operation between layers via the `supersedes` mechanism, the three track background styles.

**Demo:** arrange a multi-layer piece in the track view; move a stamped note to a different layer without repainting it.

### v0.Y.13.1 - MindWaves v2

Field operators (Warp, Reduce), drawn-shape and step-grid generator types, and the continuous shape/skew/character controls.

**Demo:** a MindWave built from a hand-drawn Path, reduced to a plain time-varying control signal.

### v0.Y.14.1 - Chords/Arpeggiator/Sequencer

The Chord Generator and the generalized notation-driven sequence it's built on, targeting any paintable tip. Resolves the sequence-notation Deferred Decision (validating the ABC-notation direction, or picking an alternative).

**Demo:** stamp a chord progression, then re-voice and re-time it without repainting.

---

## Phase 4 - Generative & Analytical

### v0.Y.15.1 - Generators

Lattice, fractal, and streaming procedural content generators, sharing the Order/Chaos criticality axis.

**Demo:** generate a fractal melodic texture as a new layer, tuned from rigid to chaotic.

### v0.Y.16.1 - Analysis Tools v1

A first useful cross-section across all five categories (loudness/mastering, pitch/vocal, stereo/phase, spectral health, criticality/pattern) - not every meter the legacy version had, but at least one representative of each.

**Demo:** check integrated loudness and stereo correlation on a real mix.

### v0.Y.17.1 - Sound Flower

Polar canvas view, and polar-form image import.

**Demo:** toggle Sound Flower view while painting and keep working without switching tools.

---

## Phase 5 - Fidelity & Real-Time

### v0.Y.18.1 - Pool Codec

The lossless NSGT round-trip and the finalized Pool file format (reusing the FFT-backend groundwork from the Stream codec milestone); the "Pool" action itself (hide-not-delete of raw layers/operations, per the non-destructive-processing principle).

**Demo:** pool a painted layer and confirm a lossless round trip.

**Likely Y bump:** this is the milestone most likely to force a real Project-format change, once a Pool file actually needs referencing from project data.

### v0.Y.19.1 - Export

Audio export (Pool-mode primary, a quick Stream-mode bounce for scratch use, compressed formats from day one); video export of the canvas synced to audio.

**Demo:** export a finished piece to a compressed audio format and to an MP4 with the spectrogram animation.

### v0.Y.20.1 - Live Mode

Continuous Stream-mode real-time capture, the "Live" layer compositing into the rest of the project, operation-relative MindWave binding exercised for genuine per-note retrigger feel.

**Demo:** sing or play into a microphone and hear it shaped by the project in real time.

### v0.Y.21.1 - Record

One-shot input-device capture into a new layer or a Mind Shot, distinct from continuous Live.

**Demo:** record a take directly into a new layer.

---

## Phase 6 - Interchange & Polish

### v0.Y.22.1 - Portable Resources

Standalone `.smwave` and `.sminst` files; cross-project import of layers, Mind Shots, MindWaves, and Sound Mind Instruments. Resolves the Mind Grain portability Deferred Decision one way or the other.

**Demo:** export an instrument from one project, import it cleanly into another.

### v0.Y.23.1 - Performance Validation & Hardening

Validate the ~100 ms / ~250 ms latency targets for real, on both this Arm64 machine and actual desktop Nvidia/AMD hardware (the Adreno-isn't-representative caveat from `tech-stack-decisions.md` finally gets addressed, not just flagged - needs real desktop GPU access, which is a dependency outside pure coding). Tablet and MIDI-controller input, if not already picked up incidentally. A full pass reconciling Doxygen output, `sound-mind-architecture.md`, and the test suite against each other end to end, per `CLAUDE.md`'s documentation policy.

**Demo:** the full design-doc feature set, exercised together, meeting the latency targets on real desktop GPU hardware.

---

## v1.0.0.0 - First real release

X becomes `1`. Feature-complete relative to `sound-mind-design.md`; every Decision Needed and Deferred Decision in the architecture doc is either resolved or explicitly, deliberately carried forward as known future work; the DX12 GPU compute path is validated on real desktop hardware, not just this laptop's Adreno GPU.

## After v1.0.0.0 (not part of this roadmap)

Sound Mind VST (the Stream effect plugin, the Player instrument plugin, and the Studio as a third-party plugin host) - deferred until this point by the design doc's own decision, picked up as its own roadmap once v1.0.0.0 ships.
