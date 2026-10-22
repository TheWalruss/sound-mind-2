# Sound Mind Codec — Design and Implementation Description

Status: accurate as of `v0.1.11.1`, with the Pool codec's removal (`v0.1.46.1`, architecture Decision #271) folded in. Written by reading `sound-mind-codec`'s actual source directly (not from memory of the design docs alone) - every claim below traces to a specific file. Companion to the CSV reports `sound-mind-codec-eval` produces (see that tool's own Doxygen docs under `sound-mind-codec-eval/`) - this document describes *what the codec does*; that tool measures *how well it does it* against real audio.

This is **not** `docs/legacy/CODEC_DETAILS.md`. That document describes the previous Python prototype's NSGT-library-based codec (`grrrr/nsgt`), kept only as a "lessons learned" reference per `CLAUDE.md`. The current C++ codec (`sound-mind-codec`) was implemented from scratch (`docs/sound-mind-architecture.md`'s Decision #8) and differs from the legacy implementation in several concrete, consequential ways this document calls out explicitly wherever they matter.

## Contents

1. [One Codec, One Config Struct](#1-one-codec-one-config-struct)
2. [The Transforms](#2-the-transforms)
3. [Window Functions](#3-window-functions)
4. [Axis Scaling](#4-axis-scaling)
5. [Bit Depth, by Representation](#5-bit-depth-by-representation)
6. [Pre-Processing and Post-Processing](#6-pre-processing-and-post-processing)
7. [What the Codec Does *Not* Do](#7-what-the-codec-does-not-do)
8. [CPU vs. GPU](#8-cpu-vs-gpu)
9. [On-Disk File Formats](#9-on-disk-file-formats)
10. [Known Fidelity Reference Point](#10-known-fidelity-reference-point)
11. [Real-World Evaluation Findings (2026-10-09)](#11-real-world-evaluation-findings-2026-10-09)
12. [The Pool Codec, Removed (2026-10-22)](#12-the-pool-codec-removed-2026-10-22)

---

## 1. One Codec, One Config Struct

The codec's parameters are one struct, `StreamCodecConfig` (`stream_codec.h`):

| Field | Default | Meaning |
|---|---|---|
| `sampleRateHz` | 44100 | Overwritten by `encode()` with the actual `AudioBuffer`'s own sample rate - a caller-supplied value here is only ever a hint. |
| `hopLength` | 441 | Samples per time-axis column (~10 ms at 44100 Hz). |
| `binCount` | 512 | Number of frequency bins - the image height. |
| `minFrequencyHz` | 20.0 | Lower edge of the encoded frequency range. |
| `maxFrequencyHz` | 16000.0 | Upper edge - clamped to Nyquist (`sampleRateHz/2`) if it would otherwise exceed it. |

Deliberately independent of `sound_mind::core::ProjectSettings` (`stream_codec.h`'s own docs) - `sound-mind-codec` has no project model and must stay usable standalone; `sound_mind::core::streamCodecConfigFor()` is the one-way bridge from a project's settings to this struct.

**`sound_mind::codec::StreamImage`** (`stream_codec.h`) is the codec's in-memory result: independent left/right amplitude (dB), plus one *shared* phase channel (not independent left/right phase), as `float` row-major `[bin][frame]` planes - see [Section 5](#5-bit-depth-by-representation) for how that differs from the display's bit depth.

## 2. The Transforms

### Stream: a fixed-window STFT

`encode()`/`decode()` (`stream_codec.cpp`, sharing per-frame primitives with the live/incremental `StreamIncrementalEncoder` via the private `stream_frame_codec.h`):

1. A mono/mid downmix (`0.5*(left+right)`) is computed once, alongside the independent left/right channels.
2. For each hop-spaced analysis frame (`fftSize = hopLength * 4`, i.e. a fixed 75% overlap): extract `fftSize` samples (zero-filled past either end of the signal - see [Section 6](#6-pre-processing-and-post-processing)), apply a Hann window, forward real FFT (PocketFFT) for all three signals (left, right, mid).
3. For each of `binCount` **log-spaced** target bins, the corresponding fractional linear-FFT-bin index is linearly interpolated out of the frame's own linear spectrum (magnitude directly; phase via cosine/sine decomposition first, to avoid wraparound - see `SpectrumSample`/`sampleSpectrum()`). Left and right amplitude are stored in dB; the mid signal's phase becomes the one shared phase value for that cell.
4. Decode runs the same process in reverse per frame (log-bin-indexed stored data resampled back to the linear FFT grid, inverse FFT, Hann-windowed again, and overlap-added with the standard window-sum-squared normalization) - a conventional analysis/synthesis STFT, nothing non-standard about the reconstruction math itself.

**This is a genuinely lossy approximation**, by design and clearly documented as such (`StreamImage`'s own class docs): reconstructing left and right from one shared phase is only exact when the two channels' true phase already agree (mono-ish content); genuinely stereo material trades some fidelity for this three-plane budget. Stream exists for speed, not for bit-exactness.

## 3. Window Functions

**Exactly one window function is used anywhere in this codec: a periodic Hann window**, `0.5 - 0.5*cos(2*pi*i/N)`, as a *time-domain* analysis/synthesis window applied to each `fftSize`-sample frame before/after the FFT - the conventional STFT role.

No other window (Blackman, Hamming, Kaiser, etc.) is implemented anywhere in the codec. The legacy Python codec's own CPU-fallback STFT backend used the same Hann choice at the same 75% overlap ratio (`docs/legacy/CODEC_DETAILS.md`'s `VulkanSTFTBackend`) - Stream's `fftSizeFor()` explicitly cites that precedent in its own code comment.

## 4. Axis Scaling

### Horizontal (time)

Uniform and linear: each column represents exactly `hopLength` samples (`hopLength / sampleRateHz` seconds - 441/44100 = 10 ms by default). `frameCount = ceil(numSamples / hopLength)`.

### Vertical (frequency)

**Purely logarithmic - not mel, not octave-aligned, not variable-Q.** `centerFrequencyHz(bin) = minFrequencyHz * exp(t * log(maxFrequencyHz / minFrequencyHz))`, where `t = bin / (binCount - 1)` ranges linearly over `[0, 1]`. This is `docs/sound-mind-design.md`'s "Frequency Scale" concept's *default* ("close, but not identical, to mel") - but as of this writing, **it is also the only scale the implementation offers**. `StreamCodecConfig` has no `scale`/mel/octave/variable-Q field at all; the legacy codec's `'log'`/`'mel'`/`'oct'`/`'varq'` options (`docs/legacy/CODEC_DETAILS.md` Section 8) have no counterpart here yet. Bin 0 (in-memory storage) is always the *lowest* frequency; the display convention flips this so row 0 reads as the *highest* frequency (bass at the bottom, treble at the top - `color_mapping.cpp`'s `height - 1 - y` convention), matching how a spectrogram is conventionally displayed.

## 5. Bit Depth, by Representation

There is no single answer to "what bit depth are the pixels" - it depends on which representation is meant:

| Representation | Amplitude | Phase | Notes |
|---|---|---|---|
| **In-memory** (`StreamImage`) | 32-bit float, **unclamped**, raw dB | 32-bit float radians | The canonical representation every Studio operation (painting, filters, compositing) actually reads/writes. |
| **Stream on-disk file** (custom binary) | 32-bit float, **no quantization at all** | 32-bit float | A raw dump of the in-memory planes (`stream_file.cpp`) - no bit-depth reduction. |
| **RGB display** (`toRgbImage()`, `color_mapping.cpp`) | 8-bit per channel, dB clamped to [-96, 0] | 8-bit (phase, where used as the blue channel) | Purely for on-screen rendering (`QImage`-compatible) - never read back into the codec's in-memory representation except via the deliberate image-import round trip (`fromRgbImage()`). |

So: **the in-memory data the Studio actually operates on is 32-bit float, unclamped**, and so is what it saves. 8-bit quantization only happens at the RGB-display boundary, which painting, filtering and compositing never cross.

## 6. Pre-Processing and Post-Processing

This section answers the specific questions the legacy codec's own documentation (`docs/legacy/CODEC_DETAILS.md`) raises, for the *current* C++ codec. All three were implemented for Stream (input-level normalization was later dropped again, `v0.1.33.1`, see below).

### Equal-loudness / perceptual weighting (A-weighting, ISO 226, or similar)

**Implemented** (`aWeightingDb()`, `stream_frame_codec.h`): the standard IEC 61672 A-weighting curve, normalized to `0` dB at 1 kHz, added to every stored dB value per bin during encode and subtracted back out per bin during decode - cosmetic, never affecting the reconstructed audio, exactly the legacy codec's own Pool-only mechanism (`docs/legacy/CODEC_DETAILS.md` Section 3.7), applied to Stream. Resolved `docs/sound-mind-architecture.md`'s former *Decisions Needed* #4. ISO 226 (full equal-loudness contours) was considered and passed on - it's level-dependent, needing a calibrated absolute-SPL reference this all-relative-dBFS system has no equivalent of, whereas A-weighting is frequency-only and was already the legacy codec's own choice.

### Input-level normalization (so quiet/dark content stays audible, loud/bright content doesn't clip)

**Update, `v0.1.33.1` (architecture Decision #232): Stream no longer normalizes.** Stream's stored magnitudes are now absolute dBFS - each bin has `20*log10(fftSize/4)` subtracted, the magnitude a full-scale sine's own bin reads with this Hann window, so a full-scale tone reads about 0 dB at any hop length - and `StreamImage` no longer carries `inputNormalizationScale`. Normalizing each encode toward 0.95 peak made every clip display near the top of the range regardless of its real level, so canvas brightness didn't track loudness, and the live `StreamIncrementalEncoder` (which never normalized) sat on a different scale from imported content. The paragraph below describes the original design.

**Originally implemented**, but deliberately **not** the legacy codec's own one-way behavior. The legacy codec normalized every encode's input to a peak of 0.95 before transforming (`docs/legacy/CODEC_DETAILS.md` Section 3.1) and never restored the original level on decode - fine for its own calling pattern (one normalize per whole-file import), but this codebase's `encode()` is called per-paint-stroke/per-layer/per-live-buffer, where blindly normalizing every call would make every quiet and every loud layer converge on the same brightness, destroying the relative dynamics multi-layer compositing depends on. That implementation (`StreamImage` gained `inputNormalizationScale`) was **fully reversible**: `scale = (peak > 0) ? 0.95/peak : 1.0` (one combined left+right peak, not independent per-channel scales, to preserve stereo balance) is applied before transforming and carried on the returned image; `decode()` divided the final reconstructed audio by that same scale before returning, so the decoded level always matches the original input exactly, regardless of what the stored image's own internal dynamic-range usage needed.

### Start/end boundary handling (removing encoding artifacts at clip edges)

**Implemented.** The legacy codec's own technique (`docs/legacy/CODEC_DETAILS.md` Section 3.2): reflection-pad the signal at both ends (mirroring without repeating the boundary sample) by one low-frequency bin's own period before transforming, so the longest analysis windows see a smooth continuation instead of a hard edge. `extractFrame()` (`stream_frame_codec.h`) reflects into any out-of-range sample within `computePadSamples()`'s own pad length, instead of a hard zero-fill - entirely local to frame extraction, invisible to `decode()`, with no effect on frame count or timing. `StreamIncrementalEncoder` is deliberately excluded (always `padSamples=0`) - a live, causally-arriving stream has no future samples to reflect around. (The Pool codec's own attempt was deferred after two failures - Decision #205 - and is moot since its removal.)

### Snippet/boundary continuity for long files split into pieces

Not applicable yet - the current codec has no snippet-splitting mechanism at all (unlike the legacy codec's `encode_snippets()`, `docs/legacy/CODEC_DETAILS.md` Section 4). Every encode is a single whole-buffer operation today.

### Overlap-add normalization

**Implemented**, and this *is* the standard, necessary kind of post-processing a windowed analysis/synthesis transform needs regardless of any perceptual/safety concern: `decode()` divides its accumulated reconstruction by the summed squared window (floored at a small epsilon to avoid dividing by near-zero at a signal's very start/end) - the conventional WOLA (weighted overlap-add) normalization, not something specific to Sound Mind.

### Amplitude-to-dB floor

`amplitudeToDb()` (`stream_frame_codec.h`) floors the linear magnitude at `1e-7` before taking `20*log10(...)`, i.e. a soft floor around -140 dBFS, purely to avoid `-inf`/`NaN` for an exactly-silent bin - not a perceptual or safety choice, just numerical hygiene.

## 7. What the Codec Does *Not* Do

Collected in one place for a quick scan against the legacy codec's own feature list:

- No snippet-splitting for long files.
- No mel, octave-aligned, or variable-Q frequency scale - log only (Section 4).
- No GPU-accelerated encode/decode path (Section 8).

None of these are regressions from a working feature that was removed - the current codec was built from scratch, and each of these hasn't been reached yet. They're listed here precisely so that absence is a documented, deliberate fact rather than something a future reader has to rediscover by reading source.

## 8. CPU vs. GPU

**Multithreaded since `v0.1.41.2`:** Stream `encode()`/`decode()` split their frames into fixed 64-frame blocks run by `sound_mind::codec::parallelFor()` (`parallel.h`). Encode blocks write disjoint cells. Decode blocks overlap-add into their own buffers, which are then summed into the output in block order, so the result is bit-identical whatever the thread count. On the 12-core dev machine, a 180 s decode went from 1856 to 335 ms and an encode from 3859 to 500 ms.

**Trig removed from the per-bin loops in `v0.1.41.3`:**
- **Decode:** takes each stored log bin's cosine and sine once per frame, then normalizes the interpolated vector. That replaces two cos/sin pairs per linear bin, an `atan2`, and two `std::polar` calls, since polar(a, atan2(s, c)) = a·(c, s)/|(c, s)|. A vanishing vector reads as angle 0, as `atan2(0, 0)` did. The per-linear-bin interpolation positions are precomputed once per decode, and `dbToAmplitude()` is one `exp()`.
- **`sampleSpectrum()`** (shared by `encode()` and `StreamIncrementalEncoder`, which therefore still agree exactly) takes the phase vector as Re z/|z|, Im z/|z|, instead of `cos(arg z)`, `sin(arg z)`.
- **Result:** at 180 s (with `v0.1.41.2`'s threads), decode went from 335 to 214 ms and encode from 500 to 263 ms; 8.7x and 14.7x against the single-threaded baseline. Every codec test passes unchanged.

**A GPU Stream codec was assessed and not built (`v0.1.41.3`):**
- Decoding 180 s now takes ~0.21 s, about 840x real time.
- A DirectX 12 path would upload the image (512 bins x 18 000 frames x 3 planes, ~110 MB of floats) and read back ~60 MB of samples per 180 s. On the dev machine's integrated Adreno, that alone is a large share of what's left.
- It would also need a hand-written FFT shader matching PocketFFT to the codec tests' tolerances (no new FFT dependency without asking).
- Revisit if profiling ever shows decode dominating again (very long projects, or much finer hop lengths).

**The codec is CPU-only, in both directions, as of this writing.** Checked directly: nothing in `sound-mind-codec` references `sound_mind::gpu::ComputeDevice` or any DirectX 12 type. `StreamCodecConfig`'s own docs and `encode()`'s own Doxygen comment say so explicitly ("CPU-only for now... the not-yet-prototyped GPU path").

This is easy to conflate with a different, already-shipped GPU capability: `sound-mind-gpu`'s compute shaders (`mix_amplitude_phase_signal.hlsl` and friends) *do* accelerate GPU work in this codebase, but for `sound-mind-core`'s **compositor** - blending already-encoded layers' amplitude/phase together - not for the codec's own audio-to-spectrogram or spectrogram-to-audio transform. A project with GPU acceleration enabled gets a faster *composite*; encoding a new import or decoding for playback still runs the exact same CPU code path described in Section 2 either way.

`docs/sound-mind-architecture.md`'s *Decisions Needed* #1 (GPU/audio-thread handoff mechanism) and `tech-stack-decisions.md`'s own framing ("spectrogram/DSP transform acceleration (NSGT-style processing)... used for") both describe this as a real, intended future direction - just not one implemented yet.

## 9. On-Disk File Format

Covered in detail by `stream_file.h` and summarized in Section 5's bit-depth table above: a lightweight custom binary format (`SMST` magic, a version number, then the config fields, then three raw `float[bin][frame]` planes) - no compression, no quantization, no generic-viewer compatibility, because Stream's entire reason to exist is speed. Native byte order only (no endian-swapping) - acceptable since this project targets Arm64 and x64 exclusively, both little-endian.

## 10. Known Fidelity Reference Point

`sound-mind-codec-eval`'s own smoke-testing during its development (a synthetic 440/660 Hz stereo tone, default hop length/bin count) measured **0.9695** correlation and ~12.5 dB overall SNR for Stream. Correlation punishes the shared-phase approximation on genuinely stereo material harder than listening does; Section 11 is what real audio showed.

## 11. Real-World Evaluation Findings (2026-10-09)

The first real `sound-mind-codec-eval` runs against actual audio (six files: `mix1.mp3`, `sissel.mp3`, `bass.mp3`, `Flickan och kråkan-...mp3`, and two synthetic `.wav` tones - one pure 400 Hz, one six simultaneous tones at 100/200/400/800/1600/3200 Hz), each swept across `hopLength ∈ {220, 441, 882}` × `binCount ∈ {256, 512, 1024}`, followed by the user directly listening to the decoded output. The combination of measured metrics and listening is what surfaced everything below - neither alone would have been conclusive (see the testing-lesson note in `docs/sound-mind-architecture.md`'s Decision #205 about correlation's own blind spots).

### Stream: a real, audible, but expected tradeoff - not a bug

Listening confirmed the measured correlation drop at larger hop lengths and smaller bin counts (Section 10's Known Fidelity Reference Point already showed this numerically) has a specific, recognizable character: **sibilant "s" sounds disappear almost entirely at larger hop lengths, and the same thing happens at smaller bin counts - across different material, the overall effect reads as a low-pass filter.**

This is exactly what Stream's own fixed-window design (Section 2) predicts, not a defect:

- **Hop length sets `fftSize = hopLength × 4`** uniformly for the whole signal - every frame's analysis/synthesis window is this one fixed size, regardless of what's in it. A sibilant is a short (tens of milliseconds), broadband, high-frequency burst; a larger window averages over a longer span of time, smearing a short transient across it the same way a longer shutter speed blurs fast motion. There is no per-content or per-frequency adaptation anywhere in `encode()`/`decode()` - the window is one size, always.
- **Bin count sets log-frequency resolution uniformly across the whole range** (Section 4) - fewer bins means coarser buckets *everywhere*, including the high frequencies sibilants occupy. Unlike hop length (a time-domain effect), this is a frequency-domain one, but the audible result - loss of fine high-frequency detail - reads the same way.

Both parameters' effects stack in the same direction (less temporal AND less spectral precision for transients), which is why the perceptual result generalizes across different material as "sounds like a low-pass filter," even though neither parameter is a filter. Worth remembering as real, user-confirmed behavior the next time Stream's own default `hopLength`/`binCount` are reconsidered for sibilant-heavy (vocal) content specifically - not something to silently work around, since the tradeoff is inherent to a fixed-window STFT with no adaptive resolution.

## 12. The Pool Codec, Removed (2026-10-22)

The same evaluation found the Pool codec - a from-scratch, near-lossless constant-Q transform (an NSGT via the DFT filter-bank identity) with a 16-bit TIFF file format (`.smpool`, via libtiff) - adding an audible "electric noise" over real music at every bin count, and collapsing at `binCount=1024`. It was never fixed. Meanwhile the Studio never used it for importing, painting, recording, playback or export: its only role was a per-layer "Pool" action that decoded the layer's Stream content, round-tripped it through Pool, and encoded it back to Stream - which could only add error. It was removed in `v0.1.46.1` (architecture Decision #271), taking libtiff and its libjpeg-turbo/liblzma/zlib dependencies with it.
