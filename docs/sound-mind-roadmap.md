# Sound Mind Studio - Development Roadmap

Status: **first draft, revised once.** This sequences `sound-mind-design.md`'s feature set into a series of concrete, always-working Studio versions, from the current empty-window bring-up (`v0.0.0.1`) to a feature-complete `v1.0.0.0`. Expect this to be revised further as work proceeds and real effort/complexity becomes clearer — it's a plan to work from, not a schedule to hold to.

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

1. **Full I/O and real-time infrastructure comes right after Foundation, before any creative feature.** Pool codec, Export, Live Mode, and Record all sit in Phase 2, immediately after the basic Project/Stream/Import/Playback loop exists - deliberately not creative-feature-first. This gets the hardest, most performance-sensitive paths (a full lossless codec round trip, real-time capture, file export) working and measurable as early as possible, so every later phase has a real, working performance baseline to check against, rather than a promise that it'll be fine once painting and filters and MindWaves are layered on. From Phase 3 onward, re-confirming that baseline still holds - both correctness and the ~100 ms / ~250 ms latency targets - is part of finishing each milestone, not a separate later pass.
2. **Stream before Pool**, still. Per the design doc, the Studio operates in Stream mode by default; Pool is a deliberate, manual, higher-fidelity step layered on top. Stream comes first within Phase 1 so Pool (Phase 2) has a working fast path, and an FFT backend decision, to build on.
3. **Every milestone is a working Studio**, not a library-only checkpoint. Each one below ends with something you can actually open, do a thing in, and see/hear the result of.
4. **Dependency order, not design-doc reading order.** The sequence below follows what each capability actually needs to exist first, which isn't the same order the design doc presents things in. A few milestones below get a *simpler* first pass than their eventual design-doc scope, specifically because they're now scheduled before something they'd otherwise lean on (Live Mode before MindWaves, Record before Mind Shots) - each says so, and says which later milestone comes back to finish the job.
5. **Open questions get resolved where the work that needs them happens**, not all up front. Each milestone that depends on one of `sound-mind-architecture.md`'s Decisions Needed / Deferred Decisions says so.
6. **`.5` phases get inserted between numbered phases as UI/workflow checkpoints, not planned in advance.** `Phase 2.5` (added after `v0.0.8.1`) is the first: a review of the legacy Studio's UI surfaced patterns (a persistent start screen, a real project-creation wizard, a fixed-length loop-pedal Live Mode) worth adopting deliberately once enough of the underlying engine existed to make that concrete, rather than guessing at UI needs from Phase 1. Numbered phases keep their names and don't get renumbered when this happens - only the `Z` values of whatever came after the insertion point shift up to make room. Expect more of these (`Phase 3.5`, etc.) at similar junctures, not just this one.

## Explicit non-goals for this roadmap

- **Sound Mind VST is not on this path.** Per the design doc, it's deferred until the standalone Studio is fully operational - that means after `v1.0.0.0`, not before it.
- **Collaboration/multi-user** stays out of scope throughout, per the design doc's resolved decision.
- **A final Project file schema** is not a pre-1.0 requirement - the design doc explicitly treats that as a near-beta concern. Expect Y to move once, deliberately, around there.

## Packaging & Installers

Not its own Z-milestone - this is infrastructure, like the release workflow it extends, and can slot in whenever is convenient rather than waiting for a specific phase:

- **A basic installer, early.** Once the release workflow's `windeployqt` packaging step exists (already does, as of `v0.0.0.1`), adding a minimal installer alongside the existing zip is low effort - CMake's built-in CPack can drive one from the same file list. Doing this early, rather than right before `v1.0.0.0`, is the same reasoning as moving Pool/Export/Live/Record into Phase 2: validate the packaging pipeline while the app is still simple, so it isn't a surprise at the end.
- **Real installer polish belongs in Phase 6**, alongside Performance Validation & Hardening: code signing (needs a purchased certificate - a cost/logistics item, not just engineering), Start Menu shortcuts and icons, in-place upgrade handling across versions, file-type association for the Project file, license/EULA text.
- **Tooling choice not yet made.** CPack can drive either WiX (a proper MSI, more Windows-native) or NSIS (simpler, more common, a bespoke EXE installer) - a new third-party dependency either way, which `CLAUDE.md` flags as something to discuss before adding, not to pick unilaterally.

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

Transport controls, Stream-mode decode of the composite, audio output via JUCE (linked in for real for the first time - see `sound-mind-architecture.md`'s Decisions Made for the JUCE tier this settled on).

**Simpler first pass than the design doc's eventual scope:** decode happens once, into a fixed buffer, when Play is pressed - the audio callback just reads sequentially from it (allocation-free by construction, satisfying `CLAUDE.md`'s non-negotiable real-time constraint). This isn't "live" in the edit-reactive, no-separate-render-step sense the design doc ultimately describes, but nothing generates a live edit to react to yet (Painting doesn't exist until Phase 3) - so building that now would be solving a problem that doesn't exist yet. The design doc's actual "always-current" model is needed for real by Live Mode (`v0.Y.7.1`), which inherently requires continuous incremental decode anyway (it's processing a continuously-arriving live input signal) - that's where it gets built, and Playback picks it up once it exists.

**Demo:** import something, press play, hear it.

---

## Phase 2 - I/O Infrastructure & Performance Baseline

Everything in this phase is, in one way or another, getting data into or out of the Studio at full fidelity or in real time - the parts most likely to be expensive, and the parts every later creative feature will eventually have to coexist with performance-wise. Building it now, while the project is still simple, means there's something concrete to measure against from here on.

### v0.Y.5.1 - Pool Codec

The near-lossless NSGT round-trip and the finalized Pool file format, reusing the FFT-backend groundwork from the Stream codec milestone; a first "Pool" action. Nothing has been painted yet at this point in the roadmap, so pooling works on imported content.

**Simpler first pass than the design doc's eventual scope, on purpose:** pooling replaces a layer's content in place (re-deriving a fresh Stream copy from the pooled result) rather than the design doc's "hide-not-delete, recorded as an undoable operation" behavior - that needs the first concrete `Operation` subtype, which arrives with Basic Painting, not here. Revisit Pooling once that exists.

**Demo:** pool an imported layer and confirm a near-lossless round trip (measured via correlation against the original, since a practical NSGT implementation's rectangular-storage step - interpolating onto a common pixel grid - is never mathematically bit-exact, matching how the wider NSGT literature and the legacy codec both use "lossless" in practice, not as an absolute claim).

**No Y bump after all.** Predicted "likely" above, but the actual addition (a `pool/layer_<id>.smpool` reference, mirroring how `media/` was added for Stream in `v0.0.3.1`) is additive, not breaking - nothing in an existing project file's shape changes. Stayed `v0.0.5.1`, not `v0.1.0.1`.

### v0.Y.6.1 - Export

Audio export (Pool-mode primary, a quick Stream-mode bounce for scratch use, compressed formats from day one); video export of the canvas synced to audio. ffmpeg joins the dependency set here (MP3 encoding and MP4 muxing/video/audio - JUCE's own MP3 writer turned out to be an unimplemented stub, and ffmpeg was already the only practical MP4-muxing option in vcpkg) - see `sound-mind-architecture.md`'s Decisions Made for the codec/linking-mode reasoning.

**Demo:** export the imported/pooled audio to a compressed format and to an MP4 with the spectrogram animation.

**No Y bump.** Same reasoning as `v0.0.5.1`: `sound-mind-codec` gains new export functions and two new dependencies, but nothing about the project file format or any existing public API changes - additive, not breaking. Stayed `v0.0.6.1`, not `v0.1.0.1`.

### v0.Y.7.1 - Live Mode

Continuous Stream-mode real-time capture and the "Live" layer compositing into the rest of the project. The design doc's operation-relative MindWave binding (for a genuine per-note retrigger feel) isn't available yet - MindWaves and Sound Mind Instruments don't exist until Phase 4 - so this first pass is plain continuous capture-composite-output; Live Mode gets revisited once those land to add it.

**Also where Playback's (`v0.Y.4.1`) deferred work lands.** Playback's first pass decodes once into a fixed buffer rather than the design doc's eventual always-current, no-separate-render-step model, since nothing existed yet to make the composite actually change during playback. Live Mode has no such luxury - a continuously-arriving live input signal *requires* continuous incremental Stream decode/composite by its very nature - so this is where that general real-time pipeline actually gets built. Playback should be revisited afterward to pick it up, so edits made during playback (once Painting exists in Phase 3) are reflected live too, closing the gap to the design doc's original description.

**Demo:** feed a live input signal in and hear it composited with the rest of the project in real time.

**Confirmed scope, narrower than "composited with the rest of the project" on two points, both asked before implementing:** real multi-layer audio mixing doesn't exist anywhere in the codebase yet - not even Playback does it (it plays only the single topmost layer with content) - so building it for Live Mode alone, ahead of Playback, was explicitly declined; this first pass's output is the live input alone, round-tripped through the Stream codec, not mixed with any other layer. Real compositing (and Playback's own deferred always-current model, above) both remain open follow-up work, now that a real continuous pipeline exists to build them on. The captured layer's spectrogram *does* visibly grow on the canvas in real time (confirmed in scope) - a UI-thread timer polls the growing Stream image and repaints, satisfying the design doc's "sound and image are one continuous surface" principle even though the audio-mixing half of "composited" is deferred. See `docs/sound-mind-architecture.md`'s Decisions Made for the real-time handoff mechanism (lock-free ring buffers) this settled, and its own "known limitation" note on why decoding isn't yet incremental.

**No Y bump.** `sound_mind::codec::StreamIncrementalEncoder` and `sound_mind::core::LiveEngine` are new, additive capabilities - nothing about the project file format or any existing public API changed. Stayed `v0.0.7.1`, not `v0.1.0.1`.

### v0.Y.8.1 - Record

One-shot input-device capture into a new layer. Mind Shots don't exist yet (Phase 4), so capturing directly to one isn't available in this first pass; Record gets revisited once Mind Shots land to add that option.

**Demo:** record a take directly into a new layer.

**Confirmed scope, mirroring Playback's and Live Mode's own precedent:** the design doc's "choose the input device (with rescan)" and "set an input gain" are both deferred as UI affordances layered on top of a working capture pipeline, same as Playback deferred an output-device picker and Live Mode deferred an input-device picker before it. Mutually exclusive with Playback and Live Mode (see `docs/sound-mind-architecture.md`'s Decisions Made) - starting Recording stops Playback outright, but refuses to start against a running Live Mode session (and vice versa) rather than surprise-stopping it. `RecordEngine` is deliberately simpler than `LiveEngine`: no incremental encoder, no output/decode side, no background worker thread - see the architecture doc for why a UI-thread timer drain is enough here.

**No Y bump.** `sound_mind::core::RecordEngine` is a new, additive capability - no project file format or existing public API changed. Stayed `v0.0.8.1`, not `v0.1.0.1`.

---

## Phase 2.5 - UI Foundations

Inserted after the fact (following `v0.0.8.1`), informed by a review of the legacy Studio's UI - not to replicate it, but because several of its patterns (a persistent start screen instead of a splash, a real project-creation wizard, a fixed-length loop-pedal Live Mode, a Layers panel worth adapting rather than inventing from scratch, and a distinct brand identity worth carrying into this rewrite) turned out worth adopting deliberately rather than backing into later. Phase numbers stay stable as anchors; a `.5` phase can be inserted between any two numbered phases whenever a similar UI/workflow checkpoint is worth taking before more creative features land on top of it - this is not expected to be the last one.

### v0.Y.9.1 - Landing Page

A persistent start screen - not a splash, no timed/loading behavior - shown as the Studio's central widget until a project is created or opened, then swapped for the real canvas and not shown again until there's no project open. Informed by the legacy Welcome panel's shape (a quick-actions column plus a Recent Projects list) without carrying over everything it had - no standalone-file actions (there's no standalone-TIFF concept to open here, see `v0.Y.10.1`), and richer polish like a startup-profile selector or favorite directories is deferred until there's a real profile/preferences concept to hang it on.

**Demo:** launch the Studio with no project open, see the landing page; create or open a project and land in the real editor.

### v0.Y.10.1 - Project Lifecycle

Real unsaved-changes guards on every path that can discard work - New, Open, and switching projects, not just window Close (the only one that currently has one). Every project switch - new, open, or otherwise - fully clears all in-memory buffers, caches, and panel-local state first, so nothing from the previous project (a layer's decoded audio, a Live/Loop layer's partially-captured content, a panel showing stale data) can leak into or be confused with the next one. `MainWindow::setProject()` is the audit point: everything it doesn't already reset when a new `Project` comes in is a gap to close here.

**Demo:** open project A, do some work, switch to project B without saving - get prompted; confirm nothing from A is visible or audible in B afterward.

### v0.Y.11.1 - Create Project Wizard

Replaces today's no-dialog `newProject()` (an in-memory default project, nothing asked) with a real creation wizard: name, save location, and duration always visible; sample rate, frequency range, bin count, and timestep hidden behind an "Advanced" disclosure, defaulted sensibly for anyone who never opens it. Matches the legacy pattern confirmed worth keeping in `v0.Y.10.1`: a project becomes a real file on disk at creation time (the wizard's completion *is* the first save), not an in-memory thing that only becomes a file on first explicit save.

**Demo:** create a new project through the wizard using only the always-visible fields; create another using the Advanced section to pick a non-default sample rate.

### v0.Y.12.1 - Loop Mode (renamed from Live Mode)

Live Mode (`v0.0.7.1`) is renamed **Loop Mode** (`LiveEngine`/`LiveLayer`-style names follow suit) and reimplemented as the fixed-length loop pedal the legacy Studio's own "Live Mode" actually was, confirmed as the right model rather than continuous streaming: loop length derived from the project's own duration (not user-adjustable); each loop, in sequence - record the input for that loop's duration, encode it into the Loop layer, decode it, queue the result for playback during the *next* loop - with a measured, displayed loop delay (in whole loops, not milliseconds), since a slower-than-real-time pipeline falls behind by whole loop iterations, not a fixed latency.

**Reduced scope, confirmed before implementing:** no compositing with the rest of the project after all - each loop plays back only the current topmost layer with content (the just-recorded Loop layer, or whatever else happens to already be on top - a recorded take, an imported clip, anything), the same "topmost layer" convention Playback (`v0.Y.4.1`) and Record (`v0.Y.8.1`) already use. This removes the real multi-layer audio mixing dependency `v0.0.7.1`'s notes originally expected this milestone to finally force - Loop Mode no longer needs it, and real compositing stays deferred to whenever Playback's own still-outstanding "always-current" revisit (noted back in `v0.0.7.1`/`v0.0.4.1`) actually happens.

**New capability, not present in the legacy version:** a "Keep looping" checkbox. Checked, subsequent loop iterations replay the last-captured Loop layer's content unchanged instead of recording over it again each cycle - so a captured take can keep looping hands-free without re-arming capture every time. Unchecked (the default, matching the legacy behavior) is the record-every-loop behavior described above.

**Demo:** start Loop Mode, record a few seconds; hear it loop back on the next cycle with a visible loop-delay indicator. Check "Keep looping" and confirm playback keeps looping the captured take without recording over it, until unchecked.

**No Y bump after all.** Predicted "likely" above on the assumption compositing would touch the project file format; dropping that requirement removes the reason - nothing about the format changes here.

### v0.Y.13.1 - Layers Panel

A dockable panel listing the project's layer stack, adapted from the legacy Studio's Layers panel (drag-handle reorder, per-row visibility toggle, name, opacity, add/delete) rather than designed from scratch - but scoped down to what the current, deliberately minimal `sound_mind::core::Layer` model actually supports. The legacy panel's blend-mode combo, MindWave-link combo, and transform controls are all left out of this first pass, since none of those concepts exist in the engine yet (blend modes and MindWaves both arrive in Phase 3/4) - adding placeholder UI for features that don't do anything yet isn't worth it.

**Real dependency:** `Layer` gains a `visible` flag, which doesn't exist today, so the panel's visibility toggle has something real to control. Toggling a layer hidden changes what "topmost layer with content" logic (Playback, Record, Loop Mode above) treats as being on top - skipping hidden layers - rather than changing anything about mixing, since real multi-layer compositing is still separately deferred.

**Row design, adapted from the legacy panel:** drag handle for reordering, replaced with a lock icon (no drag, no delete) for the fixed-position `Background` and `Equalizer` `LayerType`s; visibility toggle; name (double-click to rename); a small type tag for non-`Normal` layers; an opacity slider (the model already has `opacity()`, just never had UI for it); a delete button, hidden for the two locked types. No settings/gear button yet - deferred until there's a blend mode or MindWave link to put behind it, unlike the legacy panel's floating config window.

**Demo:** open a project with several layers, reorder them by dragging, hide one, rename another, adjust an opacity slider, delete a fourth.

**No Y bump.** Adding `visible` to the in-memory `Layer` model is additive to the project file's per-layer JSON, not a change to an already-established field.

### v0.Y.14.1 - Visual Identity

A lumped UI-polish milestone, per explicit go-ahead to bundle smaller UI changes into one point release rather than spreading them across several:

- The legacy Studio's app icon (`ChooseAgainIcon.ico`) and its large companion image (`ChooseAgainLarge.png`) carried over as this project's own icon - wired as the real window/taskbar/executable icon (there is none today; the Studio currently runs under Qt's generic default icon).
- The whole Studio UI restyled with the legacy documentation's brand palette - deep orange `#DD4B00` to amber gold `#FEC100` gradient accents on a dark/slate ground, per `docs/stylesheets/extra.css` in the legacy repo - applied as one app-wide Qt stylesheet rather than per-widget styling (there is no styling of any kind applied today; every widget renders in Qt's default Fusion look).
- The generated Doxygen HTML output (`docs/generated`) given a matching `HTML_EXTRA_STYLESHEET`, currently unset, so the code documentation reads as the same product rather than a stock Doxygen theme.

**Demo:** launch the Studio and see the branded icon and palette throughout, including the new Layers panel; open the generated Doxygen docs and see the same palette applied there too.

**No Y bump.** Purely visual - no project file, public API, or codec format is touched.

---

## Phase 3 - Painting & Editing

### v0.Y.15.1 - Basic Painting

A plain procedural brush (tip shape + falloff, no harmonic model yet) painting into a layer's amplitude as logged `PaintOperation`s; undo/redo via the operation log (first real exercise of the `supersedes` mechanism). Also: re-confirm Phase 2's Pool/Export/Loop/Record pipeline still works, and still meets its performance targets, with real painted content flowing through it for the first time.

**Demo:** paint a stroke, hear the difference on playback, undo it - then pool and export the result.

### v0.Y.16.1 - Selection & Fill

Rectangle, Lasso, and Wand selection with boolean combination; cut/copy/paste; the Gradient model; Fill.

**Demo:** select a region, cut it, paste it elsewhere, fill another region with a gradient.

### v0.Y.17.1 - Paths & Grids

The Path (Bézier) tool with node placement/editing and Path Gradient; Overlay Grids (frequency and timing) and Snap to Grid, including pitch quantising.

**Demo:** draw a precise, grid-snapped melodic line.

### v0.Y.18.1 - Filter Layers

The Filter layer type, a first concrete filter set (blur family, sharpen, tone curve, frequency-axis gradient), and the Equalizer special layer made functional.

**Demo:** add an EQ layer, reshape frequency balance, hear it.

---

## Phase 4 - Expressive Tools

### v0.Y.19.1 - MindWaves v1

The core generator types (periodic, envelope, stepped/noise, spatial, a first fractal field), superposition, and binding to layer opacity and filter parameters (the direct-vs-shape distinction).

**Demo:** bind a sine MindWave to a layer's opacity; watch and hear it pulse.

### v0.Y.20.1 - Sound Mind Instruments

The harmonic-series + inharmonicity + noise + body-resonance + ADSR instrument model; the canvas-space vs. operation-relative MindWave binding-coordinate-frame choice, since that's specifically about how a paint operation (an instrument note, in particular) binds to a MindWave. Also: revisit Loop Mode (Phase 2.5) to add the operation-relative retrigger feel this unlocks.

**Demo:** paint with an instrument voice that actually sounds like a plausible physical source; feed the same instrument through Loop Mode and hear it retrigger per note.

### v0.Y.21.1 - Mind Shots & Mind Grains

Capture-and-stamp static samples; live-reference dynamic grains from a source layer. Also: revisit Record (Phase 2) to add capture-directly-to-a-Mind-Shot.

**Demo:** capture a moment as a Mind Shot and restamp it; link a Mind Grain to a source layer and watch it change live as the source does.

### v0.Y.22.1 - Composer Mode

The DAW-style track view: each layer as a track, operations drawn as boxes via `Operation::bounds()`, retiming/moving an operation between layers via the `supersedes` mechanism, the three track background styles.

**Demo:** arrange a multi-layer piece in the track view; move a stamped note to a different layer without repainting it.

### v0.Y.23.1 - MindWaves v2

Field operators (Warp, Reduce), drawn-shape and step-grid generator types, and the continuous shape/skew/character controls.

**Demo:** a MindWave built from a hand-drawn Path, reduced to a plain time-varying control signal.

### v0.Y.24.1 - Chords/Arpeggiator/Sequencer

The Chord Generator and the generalized notation-driven sequence it's built on, targeting any paintable tip. Resolves the sequence-notation Deferred Decision (validating the ABC-notation direction, or picking an alternative).

**Demo:** stamp a chord progression, then re-voice and re-time it without repainting.

---

## Phase 5 - Generative & Analytical

### v0.Y.25.1 - Generators

Lattice, fractal, and streaming procedural content generators, sharing the Order/Chaos criticality axis.

**Demo:** generate a fractal melodic texture as a new layer, tuned from rigid to chaotic.

### v0.Y.26.1 - Analysis Tools v1

A first useful cross-section across all five categories (loudness/mastering, pitch/vocal, stereo/phase, spectral health, criticality/pattern) - not every meter the legacy version had, but at least one representative of each.

**Demo:** check integrated loudness and stereo correlation on a real mix.

### v0.Y.27.1 - Sound Flower

Polar canvas view, and polar-form image import.

**Demo:** toggle Sound Flower view while painting and keep working without switching tools.

---

## Phase 6 - Interchange & Polish

### v0.Y.28.1 - Portable Resources

Standalone `.smwave` and `.sminst` files; cross-project import of layers, Mind Shots, MindWaves, and Sound Mind Instruments. Resolves the Mind Grain portability Deferred Decision one way or the other.

**Demo:** export an instrument from one project, import it cleanly into another.

### v0.Y.29.1 - Performance Validation & Hardening

By now Phase 2's I/O pipeline has been re-checked at the end of every phase; this milestone is the capstone, not the first look. Validate the ~100 ms / ~250 ms latency targets for real, on both this Arm64 machine and actual desktop Nvidia/AMD hardware (the Adreno-isn't-representative caveat from `tech-stack-decisions.md` finally gets addressed properly - needs real desktop GPU access, which is a dependency outside pure coding). Tablet and MIDI-controller input, if not already picked up incidentally. A full pass reconciling Doxygen output, `sound-mind-architecture.md`, and the test suite against each other end to end, per `CLAUDE.md`'s documentation policy.

**Demo:** the full design-doc feature set, exercised together, meeting the latency targets on real desktop GPU hardware.

---

## v1.0.0.0 - First real release

X becomes `1`. Feature-complete relative to `sound-mind-design.md`; every Decision Needed and Deferred Decision in the architecture doc is either resolved or explicitly, deliberately carried forward as known future work; the DX12 GPU compute path is validated on real desktop hardware, not just this laptop's Adreno GPU.

## After v1.0.0.0 (not part of this roadmap)

Sound Mind VST (the Stream effect plugin, the Player instrument plugin, and the Studio as a third-party plugin host) - deferred until this point by the design doc's own decision, picked up as its own roadmap once v1.0.0.0 ships.
