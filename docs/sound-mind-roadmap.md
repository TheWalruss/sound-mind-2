# Sound Mind Studio - Development Roadmap

Status: **first draft, revised once.** This sequences `sound-mind-design.md`'s feature set into a series of concrete, always-working Studio versions, from the current empty-window bring-up (`v0.0.0.1`) to a feature-complete `v1.0.0.0`. Expect this to be revised further as work proceeds and real effort/complexity becomes clearer — it's a plan to work from, not a schedule to hold to.

**Status legend:** ✅ marks a milestone actually implemented, tested, and merged - not just planned. 🔜 marks what's next in line, in order. An unmarked milestone hasn't been started yet. This tracks real, current progress, not the roadmap's own planning history - a milestone implemented out of its listed order (Visual Identity, `v0.Y.16.1`) is still marked ✅ once it's actually done.

## Versioning

`vX.Y.Z.W`, per your instruction:

- **X** stays `0` until the Studio is ready for its first actual release. It becomes `1` at `v1.0.0.0` and this roadmap doesn't go past that point.
- **Y** increments when a breaking change is made to one of the main project file formats (the Project file, the Pool file, or the Stream file). Introducing a format for the first time isn't itself a breaking change - there's nothing yet to break - so Y stays put the first time each format appears; it only moves when an *already-established* format changes incompatibly afterward. Adding a new, separate file type (a portable `.smwave` or `.sminst`, say) doesn't bump Y either, since it doesn't break an existing format - that's a Z-level feature.
- **Z** increments for a feature release - one of the milestones below.
- **W** increments every time a new binary should be built and released, including point releases with no new feature between them.
- **Reset convention** (standard, not explicitly stated in your instruction - flagging the assumption): bumping Y resets Z and W to 0; bumping Z resets W to 0. W never resets anything, since nothing is smaller than it.

Exactly *when* Y will bump can't be predicted precisely this far out - it depends on what actually turns out to need breaking. Milestones below where a Y bump is likely are flagged; treat the rest as probably-Y=0 until proven otherwise.

`v0.0.0.1` (already shipped) was the toolchain bring-up: empty window, no real feature yet - Z and Y both still 0.

**Note on this roadmap's own `Z` numbering versus actually-shipped binary versions:** this document's `v0.Y.N.1` labels are planning slots in this roadmap's own sequence, not a promise every milestone ships as that exact binary version - a milestone can (and has) shipped out of its listed order (see Visual Identity, `v0.Y.16.1`, "implemented out of order" in its own entry), and the real, shipped version numbers recorded in `CHANGELOG.md` advance strictly by actual build order instead. Treat this roadmap's numbers as "where a milestone sits in the plan," not as a literal version-history log.

## Sequencing principles

1. **Full I/O and real-time infrastructure comes right after Foundation, before any creative feature.** Pool codec, Export, Live Mode, and Record all sit in Phase 2, immediately after the basic Project/Stream/Import/Playback loop exists - deliberately not creative-feature-first. This gets the hardest, most performance-sensitive paths (a full lossless codec round trip, real-time capture, file export) working and measurable as early as possible, so every later phase has a real, working performance baseline to check against, rather than a promise that it'll be fine once painting and filters and MindWaves are layered on. From Phase 3 onward, re-confirming that baseline still holds - both correctness and the ~100 ms / ~250 ms latency targets - is part of finishing each milestone, not a separate later pass.
2. **Stream before Pool**, still. Per the design doc, the Studio operates in Stream mode by default; Pool is a deliberate, manual, higher-fidelity step layered on top. Stream comes first within Phase 1 so Pool (Phase 2) has a working fast path, and an FFT backend decision, to build on.
3. **Every milestone is a working Studio**, not a library-only checkpoint. Each one below ends with something you can actually open, do a thing in, and see/hear the result of.
4. **Dependency order, not design-doc reading order.** The sequence below follows what each capability actually needs to exist first, which isn't the same order the design doc presents things in. A few milestones below get a *simpler* first pass than their eventual design-doc scope, specifically because they're now scheduled before something they'd otherwise lean on (Live Mode before MindWaves, Record before Mind Shots) - each says so, and says which later milestone comes back to finish the job.
5. **Open questions get resolved where the work that needs them happens**, not all up front. Each milestone that depends on one of `sound-mind-architecture.md`'s Decisions Needed / Deferred Decisions says so.
6. **`.5` phases get inserted between numbered phases as UI/workflow checkpoints, not planned in advance.** `Phase 2.5` (added after `v0.0.8.1`) is the first: a review of the legacy Studio's UI surfaced patterns (a persistent start screen, a real project-creation wizard, a fixed-length loop-pedal Live Mode) worth adopting deliberately once enough of the underlying engine existed to make that concrete, rather than guessing at UI needs from Phase 1. Numbered phases keep their names and don't get renumbered when this happens - only the `Z` values of whatever came after the insertion point shift up to make room. Expect more of these (`Phase 3.5`, etc.) at similar junctures, not just this one.
7. **Every phase ends with a "Refactor & Clean Up" milestone**, added once all of a phase's real feature work exists to clean up after - not planned in detail in advance, since what actually needs cleaning up only becomes clear once the phase's real code exists. Purely internal: code quality, structure, testability, and decomposition/separation-of-concerns work that makes the *next* phase faster and cheaper to build, especially for Claude Code working in this codebase - never new user-facing behavior, and never expected to bump Y.

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

### v0.Y.1.1 - Project & Canvas ✅

A real `Project` (settings, an ordered layer list, the operation log) that loads and saves as the JSON project file sketched in the architecture doc, and a canvas that renders the (still-empty) Background layer. No codec yet - the canvas can show a placeholder/solid raster.

**Demo:** open the Studio, see a canvas backed by a real, saved-and-reloadable project file.

### v0.Y.2.1 - Stream Codec ✅

The Stream codec itself: encode a short audio clip into a Stream-format layer, decode it back to audio. This is where the NSGT/FFT backend Decision Needed gets resolved for real (the permissive-FFT-backend direction already scoped in the architecture doc) and the Stream file container format decision gets settled.

**Demo:** a round-trip test - audio in, Stream-encoded, decoded, audibly the same clip back out.

**Likely Y bump:** first time the Stream file format is exercised for real; whether it needs to change again once real content is flowing through it is the open question.

### v0.Y.3.1 - Import & Display ✅

Real audio and image import as new layers, using the Stream codec above; a first, minimal Color Mapping (single-page grayscale plus a basic RGB composite).

**Demo:** import a WAV or an image, see it rendered on the canvas as a real layer, not a placeholder.

### v0.Y.4.1 - Playback ✅

Transport controls, Stream-mode decode of the composite, audio output via JUCE (linked in for real for the first time - see `sound-mind-architecture.md`'s Decisions Made for the JUCE tier this settled on).

**Simpler first pass than the design doc's eventual scope:** decode happens once, into a fixed buffer, when Play is pressed - the audio callback just reads sequentially from it (allocation-free by construction, satisfying `CLAUDE.md`'s non-negotiable real-time constraint). This isn't "live" in the edit-reactive, no-separate-render-step sense the design doc ultimately describes, but nothing generates a live edit to react to yet (Painting doesn't exist until Phase 3) - so building that now would be solving a problem that doesn't exist yet. The design doc's actual "always-current" model is needed for real by Live Mode (`v0.Y.8.1`), which inherently requires continuous incremental decode anyway (it's processing a continuously-arriving live input signal) - that's where it gets built, and Playback picks it up once it exists.

**Demo:** import something, press play, hear it.

### v0.Y.5.1 - Refactor & Clean Up

A dedicated pass over everything Phase 1 (Project & Canvas, Stream Codec, Import & Display, Playback) added, purely for code quality and maintainability - no new user-facing behavior. Look for: classes/files that grew too large or took on too many responsibilities and would benefit from decomposition/separation of concerns; duplicated logic worth factoring into a shared helper; test coverage gaps surfaced while building this phase's own features; anything that reads harder to navigate than it needs to - specifically because a smaller, better-organized codebase is faster and cheaper to work in on every later phase, especially for Claude Code, not just a one-time tidiness pass.

**Demo:** the full regression suite still passes, unchanged in behavior - no demo of its own, since success here means the *next* phase is easier to build, not that anything new is visible.

**No Y bump expected** - refactoring, by definition, doesn't change behavior or the project file format; a genuine breaking cleanup found along the way would be flagged explicitly, not folded in silently here.

---

## Phase 2 - I/O Infrastructure & Performance Baseline

Everything in this phase is, in one way or another, getting data into or out of the Studio at full fidelity or in real time - the parts most likely to be expensive, and the parts every later creative feature will eventually have to coexist with performance-wise. Building it now, while the project is still simple, means there's something concrete to measure against from here on.

### v0.Y.6.1 - Pool Codec ✅

The near-lossless NSGT round-trip and the finalized Pool file format, reusing the FFT-backend groundwork from the Stream codec milestone; a first "Pool" action. Nothing has been painted yet at this point in the roadmap, so pooling works on imported content.

**Simpler first pass than the design doc's eventual scope, on purpose:** pooling replaces a layer's content in place (re-deriving a fresh Stream copy from the pooled result) rather than the design doc's "hide-not-delete, recorded as an undoable operation" behavior - that needs the first concrete `Operation` subtype, which arrives with Basic Painting, not here. Revisit Pooling once that exists.

**Demo:** pool an imported layer and confirm a near-lossless round trip (measured via correlation against the original, since a practical NSGT implementation's rectangular-storage step - interpolating onto a common pixel grid - is never mathematically bit-exact, matching how the wider NSGT literature and the legacy codec both use "lossless" in practice, not as an absolute claim).

**No Y bump after all.** Predicted "likely" above, but the actual addition (a `pool/layer_<id>.smpool` reference, mirroring how `media/` was added for Stream in `v0.0.3.1`) is additive, not breaking - nothing in an existing project file's shape changes. Stayed `v0.0.5.1`, not `v0.1.0.1`.

### v0.Y.7.1 - Export ✅

Audio export (Pool-mode primary, a quick Stream-mode bounce for scratch use, compressed formats from day one); video export of the canvas synced to audio. ffmpeg joins the dependency set here (MP3 encoding and MP4 muxing/video/audio - JUCE's own MP3 writer turned out to be an unimplemented stub, and ffmpeg was already the only practical MP4-muxing option in vcpkg) - see `sound-mind-architecture.md`'s Decisions Made for the codec/linking-mode reasoning.

**Demo:** export the imported/pooled audio to a compressed format and to an MP4 with the spectrogram animation.

**No Y bump.** Same reasoning as `v0.0.5.1`: `sound-mind-codec` gains new export functions and two new dependencies, but nothing about the project file format or any existing public API changes - additive, not breaking. Stayed `v0.0.6.1`, not `v0.1.0.1`.

### v0.Y.8.1 - Live Mode ✅

Continuous Stream-mode real-time capture and the "Live" layer compositing into the rest of the project. The design doc's operation-relative MindWave binding (for a genuine per-note retrigger feel) isn't available yet - MindWaves and Sound Mind Instruments don't exist until Phase 4 - so this first pass is plain continuous capture-composite-output; Live Mode gets revisited once those land to add it.

**Also where Playback's (`v0.Y.4.1`) deferred work lands.** Playback's first pass decodes once into a fixed buffer rather than the design doc's eventual always-current, no-separate-render-step model, since nothing existed yet to make the composite actually change during playback. Live Mode has no such luxury - a continuously-arriving live input signal *requires* continuous incremental Stream decode/composite by its very nature - so this is where that general real-time pipeline actually gets built. Playback should be revisited afterward to pick it up, so edits made during playback (once Painting exists in Phase 3) are reflected live too, closing the gap to the design doc's original description.

**Demo:** feed a live input signal in and hear it composited with the rest of the project in real time.

**Confirmed scope, narrower than "composited with the rest of the project" on two points, both asked before implementing:** real multi-layer audio mixing doesn't exist anywhere in the codebase yet - not even Playback does it (it plays only the single topmost layer with content) - so building it for Live Mode alone, ahead of Playback, was explicitly declined; this first pass's output is the live input alone, round-tripped through the Stream codec, not mixed with any other layer. Real compositing (and Playback's own deferred always-current model, above) both remain open follow-up work, now that a real continuous pipeline exists to build them on. The captured layer's spectrogram *does* visibly grow on the canvas in real time (confirmed in scope) - a UI-thread timer polls the growing Stream image and repaints, satisfying the design doc's "sound and image are one continuous surface" principle even though the audio-mixing half of "composited" is deferred. See `docs/sound-mind-architecture.md`'s Decisions Made for the real-time handoff mechanism (lock-free ring buffers) this settled, and its own "known limitation" note on why decoding isn't yet incremental.

**No Y bump.** `sound_mind::codec::StreamIncrementalEncoder` and `sound_mind::core::LiveEngine` are new, additive capabilities - nothing about the project file format or any existing public API changed. Stayed `v0.0.7.1`, not `v0.1.0.1`.

### v0.Y.9.1 - Record ✅

One-shot input-device capture into a new layer. Mind Shots don't exist yet (Phase 4), so capturing directly to one isn't available in this first pass; Record gets revisited once Mind Shots land to add that option.

**Demo:** record a take directly into a new layer.

**Confirmed scope, mirroring Playback's and Live Mode's own precedent:** the design doc's "choose the input device (with rescan)" and "set an input gain" are both deferred as UI affordances layered on top of a working capture pipeline, same as Playback deferred an output-device picker and Live Mode deferred an input-device picker before it. Mutually exclusive with Playback and Live Mode (see `docs/sound-mind-architecture.md`'s Decisions Made) - starting Recording stops Playback outright, but refuses to start against a running Live Mode session (and vice versa) rather than surprise-stopping it. `RecordEngine` is deliberately simpler than `LiveEngine`: no incremental encoder, no output/decode side, no background worker thread - see the architecture doc for why a UI-thread timer drain is enough here.

**No Y bump.** `sound_mind::core::RecordEngine` is a new, additive capability - no project file format or existing public API changed. Stayed `v0.0.8.1`, not `v0.1.0.1`.

### v0.Y.10.1 - Refactor & Clean Up

A dedicated pass over everything Phase 2 (Pool Codec, Export, Live Mode, Record) added, same purpose and scope as `v0.Y.5.1`'s entry - purely code quality/structure/testability, no new user-facing behavior. Particularly worth a look here: `LiveEngine`/`RecordEngine`/`PlaybackEngine`'s device-I/O and ring-buffer plumbing, which grew somewhat organically across three milestones building on each other - a natural point to check for logic that should already be shared rather than duplicated per engine.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

---

## Phase 2.5 - UI Foundations

Inserted after the fact (following `v0.0.8.1`), informed by a review of the legacy Studio's UI - not to replicate it, but because several of its patterns (a persistent start screen instead of a splash, a real project-creation wizard, a fixed-length loop-pedal Live Mode, a Layers panel worth adapting rather than inventing from scratch, and a distinct brand identity worth carrying into this rewrite) turned out worth adopting deliberately rather than backing into later. Phase numbers stay stable as anchors; a `.5` phase can be inserted between any two numbered phases whenever a similar UI/workflow checkpoint is worth taking before more creative features land on top of it - this is not expected to be the last one.

### v0.Y.11.1 - Landing Page ✅

A persistent start screen - not a splash, no timed/loading behavior - shown as the Studio's central widget until a project is created or opened, then swapped for the real canvas and not shown again until there's no project open. Informed by the legacy Welcome panel's shape (a quick-actions column plus a Recent Projects list) without carrying over everything it had - no standalone-file actions (there's no standalone-TIFF concept to open here, see `v0.Y.12.1`), and richer polish like a startup-profile selector or favorite directories is deferred until there's a real profile/preferences concept to hang it on.

**Demo:** launch the Studio with no project open, see the landing page; create or open a project and land in the real editor.

**Implemented as `sound_mind::studio::LandingPage`** (a `QStackedWidget` alternates it with `CanvasWidget` as `MainWindow`'s central widget) **and `sound_mind::studio::RecentProjects`** (an ini-format `QSettings`-backed most-recently-used list, capped at 10, missing files filtered out on read - see `docs/sound-mind-architecture.md`'s Decisions Made #15 for the storage-format choice). "Not shown again until there's no project open" is accurate as far as it goes in this pass - nothing yet *returns* to a no-project state (that's `v0.Y.12.1`'s Project Lifecycle work), so in practice the Landing Page is simply shown once, at launch, until the first New/Open succeeds.

**No Y bump.** `LandingPage` and `RecentProjects` are new, additive UI/settings capabilities - no project file format changed.

### v0.Y.12.1 - Project Lifecycle ✅

Real unsaved-changes guards on every path that can discard work - New, Open, and switching projects, not just window Close (the only one that currently has one). Every project switch - new, open, or otherwise - fully clears all in-memory buffers, caches, and panel-local state first, so nothing from the previous project (a layer's decoded audio, a Live/Loop layer's partially-captured content, a panel showing stale data) can leak into or be confused with the next one. `MainWindow::setProject()` is the audit point: everything it doesn't already reset when a new `Project` comes in is a gap to close here.

**Demo:** open project A, do some work, switch to project B without saving - get prompted; confirm nothing from A is visible or audible in B afterward.

**Correction, found while implementing:** "not just window Close (the only one that currently has one)" was wrong - `MainWindow` had no `closeEvent()` override, and no unsaved-changes tracking anywhere, at all, before this milestone. The underlying intent (guard *every* discarding path, not add guards to some) still held and is what got built; the roadmap's assumption about prior state didn't match the actual code, worth recording rather than quietly not mentioning.

**Implemented** with a plain `hasUnsavedChanges()` flag (`MainWindow::hasUnsavedChanges_`) - marked on every layer-content mutation that exists today (import, Pool, Live Mode starting, Recording adding a layer), cleared by a successful save or by `setProject()` - rather than diffing against the operation log's replay, since no operation type actually logs these mutations yet (that starts with real `Operation` subtypes in Phase 3's Basic Painting milestone, per `docs/sound-mind-architecture.md`'s cross-check with the Doxygen docs). A real `QMessageBox` (Save/Discard/Cancel) guards `newProject()`/`openProject()`/the Recent Projects click handler/`closeEvent()` whenever `hasUnsavedChanges()` is true; `openProjectAt()` itself stays deliberately unguarded there (its established "non-prompting, testable" contract - see its own docs), with each of its interactive callers guarding before calling it instead.

**A separate, non-modal guard for Live Mode/Recording**, on top of the unsaved-changes prompt: `newProject()`, `openProject()`, `openProjectAt()`, and `closeEvent()` all *refuse outright* (a status-bar message, no dialog) while either is active, rather than either prompting about it generically or letting a switch silently discard an in-progress hardware capture. Matches this codebase's already-established "refuse rather than surprise-stop" philosophy for Live Mode/Recording's own mutual exclusion (`docs/sound-mind-architecture.md`'s Decisions Made #14) - extended here, not invented fresh. See Decisions Made #17 for why this is a separate mechanism from the unsaved-changes prompt rather than folded into it.

**No Y bump.** `hasUnsavedChanges_` is in-memory only, never serialized - no project file format changed.

### v0.Y.13.1 - Create Project Wizard ✅

Replaces today's no-dialog `newProject()` (an in-memory default project, nothing asked) with a real creation wizard: name, save location, and duration always visible; sample rate, frequency range, bin count, and timestep hidden behind an "Advanced" disclosure, defaulted sensibly for anyone who never opens it. Matches the legacy pattern confirmed worth keeping in `v0.Y.12.1`: a project becomes a real file on disk at creation time (the wizard's completion *is* the first save), not an in-memory thing that only becomes a file on first explicit save.

**Demo:** create a new project through the wizard using only the always-visible fields; create another using the Advanced section to pick a non-default sample rate.

**Implemented as `sound_mind::studio::CreateProjectWizard`** (a `QDialog`; `MainWindow::createProjectAt()` is the testable core it drives, mirroring the `openProject()`/`openProjectAt()` split), **plus real wiring, confirmed before starting:** a project's `ProjectSettings` now actually drives `sound_mind::codec::encode()`/`fromRgbImage()` at every direct call site (`importAudioFile()`, `importImageFile()`, Recording's post-capture encode), via the new `sound_mind::core::streamCodecConfigFor()` bridge - previously all three silently used a hardcoded codec default regardless of what a project's settings said, a real, pre-existing gap surfaced while scoping this milestone. `ProjectSettings` gained `binCount`/`minFrequencyHz`/`maxFrequencyHz` to have somewhere to hold the Advanced fields; deserializes them leniently (falling back to their defaults) rather than requiring them, so a project file saved before this milestone still loads. `LiveEngine`'s own construction-time config is deliberately *not* part of this wiring - see `docs/sound-mind-architecture.md`'s Decisions Made #18 for why.

**No Y bump.** The new `ProjectSettings` fields are read leniently (see above) - an old project file isn't broken by their absence, so this stays additive rather than a breaking format change.

### v0.Y.14.1 - Loop Mode (renamed from Live Mode) ✅

Live Mode (`v0.0.7.1`) is renamed **Loop Mode** (`LiveEngine`/`LiveLayer`-style names follow suit) and reimplemented as the fixed-length loop pedal the legacy Studio's own "Live Mode" actually was, confirmed as the right model rather than continuous streaming: loop length derived from the project's own duration (not user-adjustable); each loop, in sequence - record the input for that loop's duration, encode it into the Loop layer, decode it, queue the result for playback during the *next* loop - with a measured, displayed loop delay (in whole loops, not milliseconds), since a slower-than-real-time pipeline falls behind by whole loop iterations, not a fixed latency.

**Reduced scope, confirmed before implementing:** no compositing with the rest of the project after all - each loop plays back only the current topmost layer with content (the just-recorded Loop layer, or whatever else happens to already be on top - a recorded take, an imported clip, anything), the same "topmost layer" convention Playback (`v0.Y.4.1`) and Record (`v0.Y.9.1`) already use. This removes the real multi-layer audio mixing dependency `v0.0.7.1`'s notes originally expected this milestone to finally force - Loop Mode no longer needs it, and real compositing stays deferred to whenever Playback's own still-outstanding "always-current" revisit (noted back in `v0.0.7.1`/`v0.0.4.1`) actually happens.

**New capability, not present in the legacy version:** a "Keep looping" checkbox. Checked, subsequent loop iterations replay the last-captured Loop layer's content unchanged instead of recording over it again each cycle - so a captured take can keep looping hands-free without re-arming capture every time. Unchecked (the default, matching the legacy behavior) is the record-every-loop behavior described above.

**Demo:** start Loop Mode, record a few seconds; hear it loop back on the next cycle with a visible loop-delay indicator. Check "Keep looping" and confirm playback keeps looping the captured take without recording over it, until unchecked.

**No Y bump after all.** Predicted "likely" above on the assumption compositing would touch the project file format; dropping that requirement removes the reason - nothing about the format changes here.

**Implemented as `sound_mind::core::LoopEngine`** (renamed/reimplemented from `LiveEngine`, in `sound-mind-core` - `MainWindow::loopEngine_`/`toggleLoopMode()`/`updateLoopLayer()`/`setKeepLooping()` follow the same rename in `sound-mind-studio`). Each completed loop is encoded/decoded via the same whole-buffer `sound_mind::codec::encode()`/`decode()` any import or Recording already uses - not `StreamIncrementalEncoder` - since a loop is a fixed, bounded buffer, not a continuously-growing stream; this is a bigger departure from the original `LiveEngine`'s incremental-decode design than "renamed" alone suggests. Playback reads from one of two pre-allocated, fixed-length buffers, swapped by the background worker and picked up by the audio thread only at *its own* loop boundary (never mid-loop, to avoid an audible splice).

**One real, structural latency clarification, confirmed acceptable rather than engineered away:** the opening paragraph's "queue the result for playback during the *next* loop" turned out to slightly overstate it once actually built - a loop's audio can't begin encoding until its own capture finishes, and the playback cursor only checks for a newly-published result once per loop it itself plays through, so the earliest a captured loop is actually heard is playback loop `N + 2`, not `N + 1`, even when the worker keeps up perfectly. `LoopEngine::loopsBehind()` tracks something additive on top of that fixed baseline (whether the worker has *also* fallen further behind) - `0` means only the baseline applies, not that there's no latency at all. A true zero-extra-latency design would need capture and playback to run deliberately out of phase with each other - future work if this baseline turns out to matter in practice, not part of this milestone.

**Resolves the construction-time-config gap `v0.Y.13.1`'s docs flagged as this milestone's job:** `loopEngine_` is no longer a single `MainWindow` member built once, before any project exists, with a hardcoded default config - it's a `std::unique_ptr`, `nullptr` until the first `setProject()` call, then (re)constructed there from the *current* project's own `streamCodecConfigFor()` config and its duration in samples (`canvasWidth * hopLength`).

**Demo, as actually implemented:** the "Keep Looping" checkbox lives in the transport toolbar next to the renamed "Loop" button; the loop-delay indicator is a status-bar message (`"Looping... (N loops behind)"` once `loopsBehind() > 0`), matching every other non-modal progress indicator in this codebase rather than a dedicated widget. **Superseded by `v0.Y.18.1`**: the "Keep Looping" checkbox later moved from the toolbar into a dedicated Loop panel - see that milestone's own entry.

**Fixed in manual testing, before push:** `toggleLoopMode()` unconditionally created a brand-new "Loop Input" layer on every start, rather than reusing one already in the project - stopping and restarting (or reopening a project that already captured a loop) piled up duplicate "Loop Input" layers instead of continuing to build on the same one. Worse, since the newest layer is always topmost and starts with no content, `findTopmostRender()` would render *nothing at all* until that new layer's own first loop finished (up to a whole project-duration's wait, in silence, with no earlier layer's content showing through) - easily read as "Loop Mode isn't capturing anything," especially on a project where the Loop layer was the only one with real content. Fixed by having `toggleLoopMode()` search the current project for an existing Normal layer named "Loop Input" and reuse its id if found, only creating a new one otherwise - so a restart's canvas shows the previous session's last-captured content immediately, rather than going blank again while a new one is captured.

**A second round of manual testing found the remaining half of that same confusion**: even for a genuinely *new* "Loop Input" layer (nothing to reuse), the canvas correctly had nothing real to show yet - but a silent, unchanged canvas for up to a whole project-duration's wait still reads as "not working," not as "correctly empty." New **`LoopEngine::emptyImage()`**: a silent, correctly-dimensioned placeholder Stream image (a real `encode()` of a zero-filled buffer, at the engine's own config/loop length), given to a brand-new "Loop Input" layer immediately on start, so the canvas shows an empty spectrogram right away instead of nothing at all. A reused layer keeps its real previous content untouched, per the fix above.

### v0.Y.15.1 - Layers Panel ✅

A dockable panel listing the project's layer stack, adapted from the legacy Studio's Layers panel (drag-handle reorder, per-row visibility toggle, name, opacity, add/delete) rather than designed from scratch - but scoped down to what the current, deliberately minimal `sound_mind::core::Layer` model actually supports. The legacy panel's blend-mode combo, MindWave-link combo, and transform controls are all left out of this first pass, since none of those concepts exist in the engine yet (blend modes and MindWaves both arrive in Phase 3/4) - adding placeholder UI for features that don't do anything yet isn't worth it.

**Real dependency:** `Layer` gains a `visible` flag, which doesn't exist today, so the panel's visibility toggle has something real to control. Toggling a layer hidden changes what "topmost layer with content" logic (Playback, Record, Loop Mode above) treats as being on top - skipping hidden layers - rather than changing anything about mixing, since real multi-layer compositing is still separately deferred.

**Row design, adapted from the legacy panel:** drag handle for reordering, replaced with a lock icon (no drag, no delete) for the fixed-position `Background` and `Equalizer` `LayerType`s; visibility toggle; name (double-click to rename); a small type tag for non-`Normal` layers; an opacity slider (the model already has `opacity()`, just never had UI for it); a delete button, hidden for the two locked types. No settings/gear button yet - deferred until there's a blend mode or MindWave link to put behind it, unlike the legacy panel's floating config window.

**Demo:** open a project with several layers, reorder them by dragging, hide one, rename another, adjust an opacity slider, delete a fourth.

**Implemented as `sound_mind::studio::LayersPanel`** (a `QDockWidget`, hidden until a project exists), **`Project::removeLayer()`/`reorderLayers()`** (new - per `layers()`'s own docs, membership/order changes go through dedicated methods, not the mutable vector directly), and **`MainWindow::toggleLayerVisibility()`/`setLayerOpacity()`/`renameLayer()`+`renameLayerTo()`/`deleteLayer()`/`reorderLayers()`** (the last four mirroring the openProject()/openProjectAt() interactive-vs-testable split where a real dialog's involved - only renaming needs one, via `QInputDialog`).

**One item narrowed from the opening paragraph's "add/delete" mention:** no "+ Add Layer" button this pass, despite the general adapted-from-legacy list naming "add" - the detailed Row design bullet above and the demo line never actually called for one, and Painting doesn't exist yet (Phase 3) to make a blank layer meaningful to add. Left out rather than building placeholder UI for it, matching this milestone's own stated philosophy about the settings/gear button.

**Reordering's drag validation, confirmed while implementing:** a drag that would displace `Background`/`Equalizer` from their fixed position is rejected - `LayersPanel` snaps its own display back to the last known-good order rather than emitting the reorder, and `MainWindow::reorderLayers()`/`Project::reorderLayers()` both independently re-validate too (defense in depth, the same pattern `setProject()`'s engine-stopping already established). The drag-and-drop mechanics themselves aren't covered by an automated test - matching this codebase's existing precedent for anything that fundamentally needs a real, interactive gesture (modal dialogs, real file pickers) - confirmed manually instead.

**No Y bump**, confirmed: `Layer::visible` deserializes leniently (defaults to `true` if absent, the same treatment `ProjectSettings`' own new fields got in `v0.Y.13.1`) - a project file saved before this milestone still loads.

### v0.Y.16.1 - Visual Identity ✅

A lumped UI-polish milestone, per explicit go-ahead to bundle smaller UI changes into one point release rather than spreading them across several:

- The legacy Studio's app icon (`ChooseAgainIcon.ico`) and its large companion image (`ChooseAgainLarge.png`) carried over as this project's own icon - wired as the real window/taskbar/executable icon (there is none today; the Studio currently runs under Qt's generic default icon).
- The whole Studio UI restyled with the legacy documentation's brand palette - deep orange `#DD4B00` to amber gold `#FEC100` gradient accents on a dark/slate ground, per `docs/stylesheets/extra.css` in the legacy repo - applied as one app-wide Qt stylesheet rather than per-widget styling (there is no styling of any kind applied today; every widget renders in Qt's default Fusion look).
- The generated Doxygen HTML output (`docs/generated`) given a matching `HTML_EXTRA_STYLESHEET`, currently unset, so the code documentation reads as the same product rather than a stock Doxygen theme.

**Demo:** launch the Studio and see the branded icon and palette throughout, including the new Layers panel; open the generated Doxygen docs and see the same palette applied there too.

**Implemented out of order, ahead of `v0.Y.12.1`-`v0.Y.15.1`** (Project Lifecycle, Create Project Wizard, Loop Mode, Layers Panel - none built yet): confirmed explicitly before starting. The "including the new Layers panel" half of the demo above doesn't yet apply - there's no Layers panel to see it on - but the app-wide QSS (`sound_mind::studio::theme::studioStyleSheet()`) styles `QDockWidget`/`QDockWidget::title` pre-emptively, so it needs no revisiting once that milestone lands. `ChooseAgainIcon.ico` is used only as `sound-mind-studio`'s native Win32 executable resource (`resources/app.rc`, read by the RC compiler at build time); `ChooseAgainLarge.png` covers both `QApplication`/`MainWindow`'s runtime window icon and the Landing Page's header logo (both via Qt's resource system, `assets/app.qrc`) - splitting the two files this way, rather than loading the `.ico` through Qt too, avoids an otherwise-pointless runtime dependency on Qt's `qico` imageformat plugin. See `docs/sound-mind-architecture.md`'s Decisions Made #16 for the fixed-theme-not-a-toggle scope note and a real static-library resource-linking gotcha hit and fixed along the way.

**No Y bump.** Purely visual - no project file, public API, or codec format is touched.

### v0.Y.17.1 - Drag & Drop Import ✅

Dropping files onto the main window imports them, similar to the legacy Studio's own `dragEnterEvent`/`dropEvent`/`_route_dropped_files` handling - routed by extension, reusing the File menu's existing import paths rather than adding a separate code path for it. Narrower than the legacy routing table: no MIDI, no standalone-TIFF silent import, no per-drop import wizard - none of those concepts exist in this codebase yet, or (TIFF) never carried over as their own standalone-file idea in `v0.Y.12.1`'s notes.

**Routing:** `.wav` calls `importAudioFile()`; the image extensions `importImageFile()` already accepts (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`, `.webp`) call `importImageFile()`; `.smproj` calls `openProjectAt()` - subject to the same unsaved-changes-confirmation and Live-Mode/Recording-in-progress refusal those already enforce today (a drop is not a back door around guards a menu click has to respect). Unrecognized extensions are silently ignored, not an error - a stray file dropped by accident shouldn't force a dialog onto the screen.

**Multiple dropped files:** each recognized file is routed through its normal single-file import method in order. Unlike the legacy Studio's batching into one `_show_import_wizard(paths, ...)` call per type, there's no batch-import UI here to hand a list to, so this stays a plain per-file loop rather than something warranting a dedicated wizard of its own.

**Demo:** drag a `.wav` onto the canvas and see a new layer appear; with unsaved changes in the current project, drag a `.smproj` onto the window and confirm the same discard-changes prompt Open Project already shows, rather than silently losing the change.

**No Y bump expected** - no project-file-format change; this is a new entry point onto import/open methods that already exist.

**Implemented as `MainWindow::dragEnterEvent()`/`dropEvent()`** (thin `QWidget` overrides, accepting any drag carrying at least one local file URL) **plus a new testable core, `handleDroppedFiles()`** - the actual per-extension routing, split out the same way every other interactive/testable pair in this codebase is, since nothing can simulate a real OS-level drag gesture headlessly. `.wav` routes to `importAudioFile()` (every snippet, no picker, by design - a drop is a quick action, not the File menu's own richer flow); the image extensions route to `importImageFile()` with `ImageScalePickerDialog::Mode::RescaleToFitProject` (the same default that picker itself pre-selects, applied directly rather than shown as a dialog); `.smproj` routes to `openProjectAt()`, guarded by `confirmDiscardUnsavedChanges()` first, exactly matching the existing Recent Projects click handler's own pattern.

**One deliberate deviation from the File menu's own failure handling, confirmed while implementing:** a recognized file that fails to import/open reports it via the status bar (non-modal), not a blocking `QMessageBox`, unlike `importAudio()`/`importImage()`/`openProject()`. Two reasons: a multi-file drop shouldn't stop and demand attention partway through over one bad file, and - just as importantly - it keeps `handleDroppedFiles()` itself unconditionally headless-testable, the same reasoning every other testable core in this codebase already follows. The `.smproj` + genuine unsaved changes case is the one remaining real dialog, and it's the identical, already-accepted exception `confirmDiscardUnsavedChanges()`'s own callers all share.

### v0.Y.18.1 - Transport Panels ✅

Record, Loop, and a new Playback toolbar button each open their own dockable panel in the right sidebar - adapted from the legacy Studio's separate `_build_record_dock`/`_build_live_dock`/`_build_playback_dock`, replacing today's plain toolbar toggle buttons (`v0.0.4.1`/`v0.Y.14.1`/`v0.0.8.1`) with real per-engine surfaces to put controls on, rather than growing the transport toolbar itself indefinitely.

**Real input/output device selection, finally**: named as deferred scope at Playback (`v0.0.4.1`), Live/Loop Mode (`v0.0.7.1`/`v0.Y.14.1`), and Record (`v0.0.8.1`) alike - every engine has used whatever the system's default device happened to be, with no picker anywhere. Lands here, on these panels themselves (Loop/Record share an input picker; Playback gets an output picker), not a separate preferences dialog.

**Output volume control, allowed above "100%"**: a real gain boost past unity, not just an attenuator down to silence - on the Playback panel.

**"Keep Looping" moves into the Loop panel**, out of the transport toolbar checkbox `v0.Y.14.1` added it to as a confirmed stopgap location, pending this milestone.

**Scroll bars** on any panel whose content (device picker, volume/Keep Looping controls, etc.) exceeds the dock's available height, rather than clipping content or forcing the dock wider than the window.

**Demo:** open the Loop panel, pick a different input device, and check "Keep Looping" there instead of the toolbar; open the Playback panel and push its volume control past 100%.

**No Y bump expected** - a UI/device-selection milestone; whether a chosen device gets persisted anywhere, and how, is left to this milestone's own implementation to resolve.

**Two scope questions confirmed before implementing:** **(1)** matching the legacy Studio's own dock panels exactly, Play/Pause/Stop/Loop/Record all moved fully into the new panels - not just device pickers/volume/Keep Looping as this entry's own opening paragraph could be read either way on. The transport toolbar's three remaining actions (`playbackPanel_->toggleViewAction()`/`recordPanel_->toggleViewAction()`/`loopPanel_->toggleViewAction()`) are pure show/hide toggles for those docks, nothing more. **(2)** Device/volume choices are session-only for this pass, confirmed - they reset to system defaults on every launch; persisting them (`QSettings`, matching `RecentProjects`) is deferred, not part of this milestone.

**Implemented as `sound_mind::studio::LoopPanel`/`RecordPanel`/`PlaybackPanel`** (three new `QDockWidget`s, each wrapped in a `QScrollArea` for the confirmed scroll-bar requirement), plus new device-selection API on all three engines: `PlaybackEngine::availableOutputDeviceNames()`/`setPreferredOutputDevice()`/`currentOutputDeviceName()` (switches immediately - its device is open for the engine's whole lifetime, unlike the other two) and `setVolume()`/`volume()` (clamped to `[0, kMaxVolume]`, `2.0` - a real 200% ceiling, not just unity); `LoopEngine`/`RecordEngine` both get `availableInputDeviceNames()`/`setPreferredInputDevice()` (`LoopEngine` also `availableOutputDeviceNames()`/`setPreferredOutputDevice()`) - a preference set on either only takes effect on their *next* start(), since (unlike Playback) they only open a device inside start() at all. All three share a new `sound_mind::core::availableAudioDeviceNames()` free function for the actual JUCE device-enumeration dance, rather than tripling it.

**A real JUCE quirk, confirmed while testing**: `AudioDeviceManager::setAudioDeviceSetup()` only actually validates a device name once the manager's device types have been populated at least once (via `getAvailableDeviceTypes()`/`scanForDevices()`) - calling it as the very first thing ever done on a brand-new engine trivially "succeeds" against a name that was never checked against anything. Never an issue in practice (a real device picker always calls `availableOutputDeviceNames()` to populate itself first), but a real gotcha the tests had to work around explicitly - see `PlaybackEngine::setPreferredOutputDevice()`'s own docs.

**Each device combo's first entry is "(System Default)"**, mapped to an empty device name - the same empty-string-means-default convention the engines themselves already use, so a picker never needs a special "no selection" state.

### v0.Y.19.1 - Audio Import Snippets ✅

When importing audio, cut the input into segments exactly the project's own duration (the same `canvasWidth * hopLength` loop length `v0.Y.14.1`'s Loop Mode already derives) and import each snippet as its own layer, numbered in sequence - matching the legacy Studio's own `_start_layer_import()`/`_on_layer_import_done()` behavior (`name_0000`, `name_0001`, ...) for audio longer than the project canvas.

**Going beyond the legacy version, per explicit instruction**: a panel lists every resulting snippet - a numbered row per snippet, each showing its timespan within the source audio and a checkbox - so the user can import only a chosen subset rather than all-or-nothing, plus a "select all" checkbox. The legacy Studio always imported every snippet with no picker at all.

**Demo:** import an audio file longer than the project's own duration into a short project; see it listed as numbered snippets with their timespans in the picker, pick a handful, and see only those become layers.

**No Y bump expected** - a new import-time behavior; doesn't touch the project file format itself.

**Implemented as `sound_mind::studio::AudioSnippetPickerDialog`** (a modal `QDialog`, not a persistent dock panel - "panel" in the opening scope note above meant the picker's own row list, not a `QDockWidget`) plus three new `MainWindow` methods: `audioSnippetsForFile()` (the testable, no-dialog analysis step - computes the split without importing anything), `importAudioSnippets()` (imports a specific, caller-given set of snippet indices), and `importAudioFile()` itself, now a thin wrapper requesting every snippet `audioSnippetsForFile()` reports - preserving its exact pre-existing single-layer behavior and naming for audio no longer than the project (the common case), and headless-safe for tests either way, per the same interactive/testable split `openProject()`/`openProjectAt()` established. `importAudio()` (the interactive slot) skips the dialog entirely when there's only one snippet - it only ever appears when there's an actual choice to make.

**Layer naming, confirmed while implementing:** with more than one snippet, a layer is named `"<stem>_NNNN"` using its *original* position in the full split (zero-padded to four digits) - not renumbered sequentially among just the imported subset - so a layer's name still tells you where it came from even if some snippets were skipped. Requested indices are de-duplicated and imported in ascending position order regardless of the order they were requested in, so a picker's checked order never affects layer order.

### v0.Y.20.1 - Image Import Scaling ✅

When importing an image, offer a choice of how it's resized to the project's canvas dimensions, presented before the import proceeds:

- **Rescale to fit project (default)** - both axes stretched to the project's exact width/height, independent of the source image's own aspect ratio. Matches the legacy Studio's `stretch_fill` mode.
- **Scale vertically to fit project, keep horizontal resolution** - height changes to match the project's bin count; width stays the source image's own native pixel width. Matches legacy's `height_only`.
- **Scale horizontally to fit project, keep vertical resolution** - the width-first mirror of the option above; height stays native. Not present in the legacy Studio - its own `stretch_width` mode always also scales height proportionally, unlike this one.
- **Scale vertically to fit project, rescale horizontal in proportion** - height changes to match the project's bin count; width scales proportionally, preserving the source's aspect ratio. Matches legacy's `aspect` mode (its actual default there).
- **Keep native resolution** - no rescaling at all. Matches legacy's `native`.

**Narrower than the legacy version**: legacy's `stretch_width` (width-fit plus proportional height - no equivalent among the five options above) and `polar` (a polar-to-rectangular unwrap tied to its own view) modes aren't carried over - the latter belongs with Sound Flower, once that view exists, not general image import.

**Demo:** import a portrait-oriented photo with each of the five modes in turn and see the resulting layer's dimensions differ accordingly.

**No Y bump expected** - an import-time behavior change only.

**Implemented as `sound_mind::studio::ImageScalePickerDialog`** (a modal `QDialog`, five radio buttons, "Rescale to fit project" pre-selected) plus a `mode` parameter added to `MainWindow::importImageFile()` and a new private `scaleImageForImport()` helper that does the actual `QImage::scaled()` call per mode. Unlike Audio Import Snippets' picker, this dialog is shown unconditionally by `importImage()` - there's no "trivial, skip it" case the way a short audio file has only one snippet; an image import always has a real scaling choice to make.

**A real, pre-existing gap surfaced while scoping this milestone**: before this pass, `importImageFile()`/`fromRgbImage()` never resized anything at all - "Keep Native Resolution" was every import's *only* actual behavior, silently. This milestone is what first makes the other four modes possible, not just exposes a choice that already existed.

**`ScaleVerticalProportional`'s width is computed by hand, not via `QImage::scaled()`'s own aspect-ratio modes**: `Qt::KeepAspectRatio` fits *within* a bounding box rather than hitting an exact height, so the proportional width is computed directly (`sourceWidth * canvasHeight / sourceHeight`, rounded) and then applied via `Qt::IgnoreAspectRatio` - guaranteeing the exact, documented result rather than whatever Qt's own fitting logic happens to produce.

### v0.Y.21.1 - Layer Time Alignment ✅

Two new per-layer transform controls: horizontal translation (shifts a layer's content earlier/later in time, for lining up audio between layers) and horizontal rescaling (stretches/compresses a layer's own timeline, for matching timing between layers) - adapted from the legacy Studio's per-layer transform, narrowed per explicit instruction.

**Narrower than the legacy version, confirmed**: the legacy Studio's full affine transform (independent X/Y scale, rotation, vertical translation) is not needed here - only the two horizontal-axis controls above; nothing in this codebase's current scope needs vertical repositioning or rotation of a layer's spectrogram.

**Demo:** import two audio clips as separate layers slightly out of sync; nudge one layer's horizontal translation until they line up; rescale one layer's timeline to match the other's.

**No Y bump expected** - a new, additive per-layer field (translation/rescale offsets), deserialized leniently like every other optional field added so far - a project file saved before this milestone still loads.

**Implemented as:** `Layer` gains `translationColumns()` (a raw `int64_t` spectrogram-column count, not seconds - no sample-rate/hop-length conversion needed at render time) and `rescaleFactor()` (a plain `double` ratio, `1.0` = unrescaled, matching `opacity()`'s own unclamped-float precedent) - both confirmed with the user before implementing, along with a third decision: `renderLayer()` (see architecture.md's Decision #25) now always renders onto a `canvasWidth`-wide window rather than at a layer's own native content width, applying rescale then translation before padding/cropping to fit. `LayersPanel` gained the two spin box controls the "Demo" above calls for, wired live (editing either repaints the canvas immediately - unlike `opacitySlider`, whose edits currently have no visible effect, since real multi-layer blending doesn't exist yet).

**Narrower than stated above, in one more respect**: the transform is visual/spectrogram-only - it does not affect `decodeLayerForExport()` or playback audio in any way, since real multi-layer audio mixing still doesn't exist anywhere in the codebase (Playback/Loop Mode/Record all still only ever play the single topmost layer with content, unmixed - the same deferral `v0.Y.8.1`'s Live Mode and `v0.Y.14.1`'s Loop Mode notes already describe). "Lining up" and "matching timing" are demonstrated visually (toggling layer visibility to compare), not by ear.

### v0.Y.22.1 - Image Sequence Import ✅

When importing multiple images at once, an "import as sequence" option applies `v0.Y.20.1`'s "scale vertically to fit project, rescale horizontal in proportion" mode to each one, and automatically places each subsequent image's layer immediately after the previous one in time via `v0.Y.21.1`'s horizontal translation control - matching the legacy Studio's own cumulative-offset placement for multi-image imports.

**Demo:** select five images at once, check "import as sequence", and see five layers laid out end-to-end in time, each scaled to the project's own bin count.

**No Y bump expected.** Depends on `v0.Y.20.1`/`v0.Y.21.1` already existing - sequenced after both per Sequencing principle #4 (dependency order), matching the order these five points were given in.

**Implemented as:** checked against the legacy Studio's own `import_wizard.py`/`app.py` before implementing (this codebase's `../sound-mind/` reference), which settled several points the roadmap text above left open, each confirmed with the user: (1) "Import as sequence" is a checkbox in `ImageScalePickerDialog` (not a 6th `Mode` value), shown only when `MainWindow::importImage()`'s now-multi-select file dialog returned more than one file, and checking it disables the five mode radios (a sequence import always applies `ScaleVerticalProportional`, ignoring whatever radio was previously selected); (2) the running horizontal offset **wraps back to column `0`** once it reaches the project's own `canvasWidth`, matching the legacy Studio's own behavior exactly rather than just letting later layers extend past it (which `renderLayer()` would crop anyway, per architecture.md's Decision #25); (3) files are sorted by path before sequencing, regardless of the file dialog's own selection order - deterministic, and the natural choice for numbered frame sequences; (4) `MainWindow::importImageFiles()` (the new multi-file testable core `importImage()` delegates to, sitting between it and the existing single-file `importImageFile()`) succeeds if at least one file imported, matching `importAudioSnippets()`'s and `handleDroppedFiles()`'s own "don't let one bad file block everything" precedent.

### v0.Y.23.1 - Refactor & Clean Up

A dedicated pass over everything Phase 2.5 (UI Foundations - Landing Page through Image Sequence Import) added, same purpose and scope as `v0.Y.5.1`'s entry. This phase grew the largest and most UI-heavy so far, spanning eight real milestones plus this one - particularly worth a look: `MainWindow`'s own size/complexity (it has accumulated a lot of direct responsibility across every one of this phase's milestones) and whether any of it is now ready to decompose into smaller, more focused controllers or panels-owning-more-of-their-own-logic, the way `LayersPanel`/`LoopPanel`/`RecordPanel`/`PlaybackPanel` already own their own presentation.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

**Implemented as:** two installments (`v0.0.22.1`, `v0.0.23.1` - see `docs/sound-mind-architecture.md`'s Decisions Made #31/#32), each with its own tests-first pass and dedicated test file: `PlaybackController` (owns the `PlaybackEngine` and its position-polling timer) and `sound_mind::studio::import_export` (free functions - audio snippet splitting/importing, image importing, layer audio/video export). Both extracted out of `MainWindow` with `MainWindow`'s own methods kept as thin, exact-signature delegating bodies. Loop Mode, Recording, and project-lifecycle decomposition were scoped out explicitly (confirmed with the user each time) and were **not** ultimately pursued further in this pass - `MainWindow` still owns them directly. The phase is closed here regardless: the two extractions done were the highest-value/lowest-risk ones identified, `MainWindow`'s size is meaningfully reduced (2844 → 2544 combined header+source lines), and further decomposition of the remaining clusters isn't blocking Phase 3 - it can be picked up as its own pass later if `MainWindow`'s size becomes a problem again. A real, unrelated pre-existing test regression (`changingARealRowsOpacitySliderDoesNotCrash()`, stale since the Background-layer-controls fix) and a real test-suite performance bug (`MainWindow` opening real audio devices on every construction, regardless of what a test needed) were both found and fixed along the way - see `v0.0.23.2`'s `CHANGELOG.md` entry.

---

## Phase 3 - Painting & Editing

### v0.Y.24.1 - Basic Painting ✅

A plain procedural brush (tip shape + falloff, no harmonic model yet) painting into a layer's amplitude as logged `PaintOperation`s; undo/redo via the operation log (first real exercise of the `supersedes` mechanism). Also: re-confirm Phase 2's Pool/Export/Loop/Record pipeline still works, and still meets its performance targets, with real painted content flowing through it for the first time.

**Demo:** paint a stroke, hear the difference on playback, undo it - then pool and export the result.

**Implemented as** ten separately-tested installments (`v0.0.24.1` through `v0.0.24.10`, see `CHANGELOG.md`/`docs/sound-mind-architecture.md`'s Decisions #34-45 for each one's own rationale), going beyond the roadmap sketch above per explicit user direction at the milestone's outset: the Core data model (`Path`/`Gradient`/`ToolConfiguration`), the procedural brush DSP, `PaintController`'s session orchestration, real canvas mouse painting with a live preview, a full dockable Tool Configuration Panel (tip shape, falloff, size, a Color swatch for stereo balance, opacity, and the "Show bounding boxes"/"Show path geometry" overlays) rather than just the roadmap's bare "plain procedural brush", a "+ Add Layer" button, a status-bar cursor position readout, the Background layer made genuinely paintable, and - the final piece - Pick (select/move/modify-via-panel/delete a painted stroke, all as new, non-destructive `PaintOperation`s superseding the one acted on). Still open, and explicitly out of this milestone's own scope: the Tool Configuration Wizard, every paintbrush type past Procedural, the non-brush painting tools (Smudge, Order/Chaos, Heal, Soften, Clone), and manual Path node/handle editing (deferred to `v0.Y.26.1`'s own Path tool, which Pick's own "modify" will extend to reuse once it exists).

### v0.Y.25.1 - Selection & Fill ✅

Rectangle, Lasso, and Wand selection with boolean combination; cut/copy/paste; the Gradient model; Fill.

**Demo:** select a region, cut it, paste it elsewhere, fill another region with a gradient.

**Implemented as** two separately-tested installments (`v0.0.25.1`-`v0.0.25.2`, see `CHANGELOG.md`/`docs/sound-mind-architecture.md`'s Decisions #46-47), per explicit user-confirmed scope at the milestone's outset: Rectangle selection, Fill (color/gradient, confined exactly to the selection), and Cut/Copy/Paste (independently tracking a selection's source layer, what Cut clears, and Paste's destination layer as three potentially-different layers). The demo above is fully reachable with this scope. Still open, and explicitly out of this milestone's own confirmed scope: Lasso, Wand, and boolean combination between selections (`FillOperation`'s own `bounds()` stays a plain `TimeFrequencyRect` until one of these needs a real mask/region representation), and Rectangle's own rotate handle.

### v0.Y.26.1 - Paths & Grids

The Path (Bézier) tool with node placement/editing and Path Gradient; Overlay Grids (frequency and timing) and Snap to Grid, including pitch quantising.

**Demo:** draw a precise, grid-snapped melodic line.

### v0.Y.27.1 - Multi-layer Compositing

**Inserted ahead of Filter Layers, reordering this phase's remaining milestones** (confirmed with the user rather than assumed): Filter Layers' own definition - "composites the layers beneath it... applies a filter... renders the result" - requires real multi-layer compositing to exist first, and it never has. This is also the same long-deferred gap Live Mode (`v0.Y.8.1`), Loop Mode (`v0.Y.14.1`), and Layers Panel (`v0.Y.15.1`) each explicitly postponed in turn ("real multi-layer audio mixing doesn't exist anywhere in the codebase yet - not even Playback does it") - every one of those milestones' own notes point here.

**Normal compositing is audio-style mixing, not image-style alpha-over** (confirmed with the user - an earlier draft of this entry had this backwards): each visible layer's own amplitude/phase converts to a complex value per bin, scaled by that layer's own opacity as a linear gain, and every layer sums together - the way multiple simultaneous sounds actually combine. A fully-opaque top layer never mutes what's beneath it, unlike image alpha-over. This is the one "blend mode" any concrete need calls for so far; the design doc names "blend modes" throughout but never enumerates a fuller list, so anything beyond Normal is real, undesigned scope, deferred until something actually needs it (most likely alongside MindWave-bound blending in Phase 4).

**Extends past the canvas display to every "topmost layer with content" call site** (confirmed with the user): Playback, Loop Mode, and Record all currently play/decode only the single topmost layer, not a real mix - all three switch to decoding the real composite instead. **Playback's timing model stays one-shot** (decodes once into a fixed buffer when Play is pressed, as it already does) - just fed by the real composite instead of one layer's content. The design doc's separate "always-current, no separate render step" model (edits heard live *during* playback) stays its own deferred follow-up, same as it's been since `v0.Y.4.1`/`v0.Y.8.1`.

**Demo:** stack two painted layers at different opacities and hear them actually mixed together on playback, not just the topmost one sounding.

### v0.Y.28.1 - Filter Layers

The Filter layer type, a first concrete filter set, and the Equalizer special layer made functional - built on top of `v0.Y.27.1`'s own real compositor rather than needing to invent one itself.

**Filter set for this pass** (confirmed with the user): all three of the design doc's "Blur & focus" family - uniform, edge-preserving, and directional blur - plus sharpen, a tone curve, and a frequency-axis gradient (the basis of the Equalizer layer). The design doc's other Filter Layer families (Noise & distortion, Geometric, Space) are explicitly out of scope for this pass, left for a later filter-set expansion. **CPU-only**, matching every DSP feature built so far (painting, Pool codec) - `docs/tech-stack-decisions.md` designates DirectX 12 Compute for this kind of image/graphics work, but standing that pipeline up is deferred to a dedicated performance pass once profiling shows an actual need, not built speculatively here. MindWave parameter binding (`docs/sound-mind-design.md`'s "Filter parameters") stays out of scope too, same reasoning as every other Phase 3 milestone - MindWaves don't exist until Phase 4.

**The Equalizer layer is created for new projects only** - an older, already-saved project simply has none; no retroactive migration injects one on load. The two kinds of project are both left in a valid, if different, shape rather than rewriting old project files on open.

**Demo:** add an EQ layer, reshape frequency balance, hear it.

### v0.Y.29.1 - Refactor & Clean Up

A dedicated pass over everything Phase 3 (Basic Painting, Selection & Fill, Paths & Grids, Multi-layer Compositing, Filter Layers) added, same purpose and scope as `v0.Y.5.1`'s entry. This phase introduces the operation log's first real `Operation` subtypes and the `supersedes` mechanism's first real exercise - worth specifically checking that the paint/selection/path/filter tool implementations share what they should (common brush/stroke/selection-mask plumbing) rather than each having independently reinvented it.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

---

## Phase 4 - Expressive Tools

### v0.Y.30.1 - MindWaves v1

The core generator types (periodic, envelope, stepped/noise, spatial, a first fractal field), superposition, and binding to layer opacity and filter parameters (the direct-vs-shape distinction).

**Demo:** bind a sine MindWave to a layer's opacity; watch and hear it pulse.

### v0.Y.31.1 - Sound Mind Instruments

The harmonic-series + inharmonicity + noise + body-resonance + ADSR instrument model; the canvas-space vs. operation-relative MindWave binding-coordinate-frame choice, since that's specifically about how a paint operation (an instrument note, in particular) binds to a MindWave. Also: revisit Loop Mode (Phase 2.5) to add the operation-relative retrigger feel this unlocks.

**Demo:** paint with an instrument voice that actually sounds like a plausible physical source; feed the same instrument through Loop Mode and hear it retrigger per note.

### v0.Y.32.1 - Mind Shots & Mind Grains

Capture-and-stamp static samples; live-reference dynamic grains from a source layer. Also: revisit Record (Phase 2) to add capture-directly-to-a-Mind-Shot.

**Demo:** capture a moment as a Mind Shot and restamp it; link a Mind Grain to a source layer and watch it change live as the source does.

### v0.Y.33.1 - Composer Mode

The DAW-style track view: each layer as a track, operations drawn as boxes via `Operation::bounds()`, retiming/moving an operation between layers via the `supersedes` mechanism, the three track background styles.

**Demo:** arrange a multi-layer piece in the track view; move a stamped note to a different layer without repainting it.

### v0.Y.34.1 - MindWaves v2

Field operators (Warp, Reduce), drawn-shape and step-grid generator types, and the continuous shape/skew/character controls.

**Demo:** a MindWave built from a hand-drawn Path, reduced to a plain time-varying control signal.

### v0.Y.35.1 - Chords/Arpeggiator/Sequencer

The Chord Generator and the generalized notation-driven sequence it's built on, targeting any paintable tip. Resolves the sequence-notation Deferred Decision (validating the ABC-notation direction, or picking an alternative).

**Demo:** stamp a chord progression, then re-voice and re-time it without repainting.

### v0.Y.36.1 - Loop Mode Live Preview

A live-updating preview of the *currently capturing* loop, rendered incrementally as it's captured - restoring the visual behavior the original Live Mode (`v0.0.7.1`) had before Loop Mode's fixed-length redesign (`v0.Y.14.1`) replaced it. Today, the canvas only updates once a whole loop finishes - a real, silent wait as long as the project's own duration (see `v0.Y.14.1`'s own "Fixed in manual testing" note on how confusing that first wait already reads, even with a placeholder image now covering the very first activation).

**A second, parallel pipeline - not a change to what's actually played back.** LoopEngine's own whole-buffer encode/decode-per-loop design (confirmed, `v0.Y.14.1`) stays exactly as-is for the audio that's actually heard - this milestone is purely about what's *rendered on the canvas* while a loop is still being captured. A `sound_mind::codec::StreamIncrementalEncoder` instance (the same one the original Live Mode used, still present in `sound-mind-codec`, unused since the `v0.Y.14.1` rewrite) tracks just the current loop's progress, reset at every loop boundary rather than growing across a whole session - sidestepping the original Live Mode's own "cost grows with session length" limitation by construction, since a loop is always bounded.

**Distinct from `v0.Y.31.1`'s own "revisit Loop Mode" note**: that one is about *audio* - a per-note, operation-relative retrigger feel, once Sound Mind Instruments exists. This one is purely visual - what the canvas shows while a loop is in progress - and doesn't depend on Instruments existing first.

**Demo:** start Loop Mode and watch the spectrogram grow continuously *during* the current loop, the same way the original Live Mode used to, rather than jumping once per completed loop.

**No Y bump expected** - a rendering-only addition; the project file format, and what's actually captured/played back, are unchanged.

### v0.Y.37.1 - Refactor & Clean Up

A dedicated pass over everything Phase 4 (Expressive Tools - MindWaves v1/v2, Sound Mind Instruments, Mind Shots & Mind Grains, Composer Mode, Chords/Arpeggiator/Sequencer, Loop Mode Live Preview) added, same purpose and scope as `v0.Y.5.1`'s entry. This is the largest phase in the whole roadmap - a strong candidate for the biggest structural payoff of any of these cleanup milestones, particularly around the MindWave binding machinery (used by opacity, filter parameters, and instrument notes alike by this point) and Composer Mode's operation-to-track bookkeeping.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

---

## Phase 5 - Generative & Analytical

### v0.Y.38.1 - Generators

Lattice, fractal, and streaming procedural content generators, sharing the Order/Chaos criticality axis.

**Demo:** generate a fractal melodic texture as a new layer, tuned from rigid to chaotic.

### v0.Y.39.1 - Analysis Tools v1

A first useful cross-section across all five categories (loudness/mastering, pitch/vocal, stereo/phase, spectral health, criticality/pattern) - not every meter the legacy version had, but at least one representative of each.

**Demo:** check integrated loudness and stereo correlation on a real mix.

### v0.Y.40.1 - Sound Flower

Polar canvas view, and polar-form image import.

**Demo:** toggle Sound Flower view while painting and keep working without switching tools.

### v0.Y.41.1 - Refactor & Clean Up

A dedicated pass over everything Phase 5 (Generators, Analysis Tools v1, Sound Flower) added, same purpose and scope as `v0.Y.5.1`'s entry.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

---

## Phase 6 - Interchange & Polish

### v0.Y.42.1 - Portable Resources

Standalone `.smwave` and `.sminst` files; cross-project import of layers, Mind Shots, MindWaves, and Sound Mind Instruments. Resolves the Mind Grain portability Deferred Decision one way or the other.

**Demo:** export an instrument from one project, import it cleanly into another.

### v0.Y.43.1 - Performance Validation & Hardening

By now Phase 2's I/O pipeline has been re-checked at the end of every phase; this milestone is the capstone, not the first look. Validate the ~100 ms / ~250 ms latency targets for real, on both this Arm64 machine and actual desktop Nvidia/AMD hardware (the Adreno-isn't-representative caveat from `tech-stack-decisions.md` finally gets addressed properly - needs real desktop GPU access, which is a dependency outside pure coding). Tablet and MIDI-controller input, if not already picked up incidentally. A full pass reconciling Doxygen output, `sound-mind-architecture.md`, and the test suite against each other end to end, per `CLAUDE.md`'s documentation policy.

**Demo:** the full design-doc feature set, exercised together, meeting the latency targets on real desktop GPU hardware.

### v0.Y.44.1 - Refactor & Clean Up

A dedicated pass over everything Phase 6 (Portable Resources, Performance Validation & Hardening) added, same purpose and scope as `v0.Y.5.1`'s entry - and, by extension, the last general cleanup pass before `v1.0.0.0` itself. Real overlap with `v0.Y.43.1`'s own "full pass reconciling Doxygen output, architecture.md, and the test suite" - that milestone already covers documentation/test consistency end to end, so this one's own scope is specifically the code structure/decomposition half Sequencing principle #7 describes, not a duplicate documentation pass.

**Demo:** the full regression suite still passes, unchanged in behavior.

**No Y bump expected.**

---

## v1.0.0.0 - First real release

X becomes `1`. Feature-complete relative to `sound-mind-design.md`; every Decision Needed and Deferred Decision in the architecture doc is either resolved or explicitly, deliberately carried forward as known future work; the DX12 GPU compute path is validated on real desktop hardware, not just this laptop's Adreno GPU.

## After v1.0.0.0 (not part of this roadmap)

Sound Mind VST (the Stream effect plugin, the Player instrument plugin, and the Studio as a third-party plugin host) - deferred until this point by the design doc's own decision, picked up as its own roadmap once v1.0.0.0 ships.
