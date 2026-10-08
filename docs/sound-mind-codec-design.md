# Sound Mind Codec — Design and Implementation Description

Status: accurate as of `v0.1.11.1`, written by reading `sound-mind-codec`'s actual source directly (not from memory of the design docs alone) - every claim below traces to a specific file. Companion to the CSV reports `sound-mind-codec-eval` produces (see that tool's own Doxygen docs under `sound-mind-codec-eval/`) - this document describes *what the codec does*; that tool measures *how well it does it* against real audio.

This is **not** `docs/legacy/CODEC_DETAILS.md`. That document describes the previous Python prototype's NSGT-library-based codec (`grrrr/nsgt`), kept only as a "lessons learned" reference per `CLAUDE.md`. The current C++ codec (`sound-mind-codec`) was implemented from scratch (`docs/sound-mind-architecture.md`'s Decision #8) and differs from the legacy implementation in several concrete, consequential ways this document calls out explicitly wherever they matter.

## Contents

1. [Two Codecs, One Config Struct](#1-two-codecs-one-config-struct)
2. [The Transforms](#2-the-transforms)
3. [Window Functions](#3-window-functions)
4. [Axis Scaling](#4-axis-scaling)
5. [Bit Depth, by Representation](#5-bit-depth-by-representation)
6. [Pre-Processing and Post-Processing](#6-pre-processing-and-post-processing)
7. [What the Codec Does *Not* Do](#7-what-the-codec-does-not-do)
8. [CPU vs. GPU](#8-cpu-vs-gpu)
9. [On-Disk File Formats](#9-on-disk-file-formats)
10. [Known Fidelity Reference Point](#10-known-fidelity-reference-point)

---

## 1. Two Codecs, One Config Struct

Both codecs share one parameter struct, `StreamCodecConfig` (`stream_codec.h`):

| Field | Default | Meaning |
|---|---|---|
| `sampleRateHz` | 44100 | Overwritten by both `encode()`/`poolEncode()` with the actual `AudioBuffer`'s own sample rate - a caller-supplied value here is only ever a hint. |
| `hopLength` | 441 | Samples per time-axis column (~10 ms at 44100 Hz). |
| `binCount` | 512 | Number of frequency bins - the image height. |
| `minFrequencyHz` | 20.0 | Lower edge of the encoded frequency range. |
| `maxFrequencyHz` | 16000.0 | Upper edge - clamped to Nyquist (`sampleRateHz/2`) if it would otherwise exceed it. |

Deliberately independent of `sound_mind::core::ProjectSettings` (`stream_codec.h`'s own docs) - `sound-mind-codec` has no project model and must stay usable standalone; `sound_mind::core::streamCodecConfigFor()` is the one-way bridge from a project's settings to this struct.

- **`sound_mind::codec::StreamImage`** (`stream_codec.h`) - the Stream codec's in-memory result: independent left/right amplitude (dB), plus one *shared* phase channel (not independent left/right phase).
- **`sound_mind::codec::PoolImage`** (`pool_codec.h`) - the Pool codec's in-memory result: independent left/right amplitude **and** independent left/right phase (four planes, not three).

Both store `float` row-major `[bin][frame]` planes in memory - see [Section 5](#5-bit-depth-by-representation) for why that's a different bit depth question than either codec's own on-disk file format.

## 2. The Transforms

### Stream: a fixed-window STFT

`encode()`/`decode()` (`stream_codec.cpp`, sharing per-frame primitives with the live/incremental `StreamIncrementalEncoder` via the private `stream_frame_codec.h`):

1. A mono/mid downmix (`0.5*(left+right)`) is computed once, alongside the independent left/right channels.
2. For each hop-spaced analysis frame (`fftSize = hopLength * 4`, i.e. a fixed 75% overlap): extract `fftSize` samples (zero-filled past either end of the signal - see [Section 6](#6-pre-processing-and-post-processing)), apply a Hann window, forward real FFT (PocketFFT) for all three signals (left, right, mid).
3. For each of `binCount` **log-spaced** target bins, the corresponding fractional linear-FFT-bin index is linearly interpolated out of the frame's own linear spectrum (magnitude directly; phase via cosine/sine decomposition first, to avoid wraparound - see `SpectrumSample`/`sampleSpectrum()`). Left and right amplitude are stored in dB; the mid signal's phase becomes the one shared phase value for that cell.
4. Decode runs the same process in reverse per frame (log-bin-indexed stored data resampled back to the linear FFT grid, inverse FFT, Hann-windowed again, and overlap-added with the standard window-sum-squared normalization) - a conventional analysis/synthesis STFT, nothing non-standard about the reconstruction math itself.

**This is a genuinely lossy approximation**, by design and clearly documented as such (`StreamImage`'s own class docs): reconstructing left and right from one shared phase is only exact when the two channels' true phase already agree (mono-ish content); genuinely stereo material trades some fidelity for this three-plane budget. Stream exists for speed, not for bit-exactness.

### Pool: a from-scratch, frequency-domain-windowed constant-Q transform

`poolEncode()`/`poolDecode()` (`pool_codec.cpp`) implement what the design doc and architecture doc both call "NSGT" (Non-Stationary/Non-Static Gabor Transform), via the standard DFT filter-bank identity, confirmed against `grrrr/nsgt`'s documented behavior rather than ported from it line-for-line (Decision #8):

1. **One global real FFT per channel**, of the *entire* signal at once - not framed the way Stream is. `fftSize` is just the sample count (`numSamples`, or `1` for an empty buffer).
2. For each of `binCount` log-spaced bins: a Hann-windowed slice of that *global spectrum*, centered on the bin's own center frequency, is extracted (`extractWindowedSlice()`) and **inverse**-FFT'd directly. By the DFT filter-bank identity, this one step yields that bin's own "native-rate" complex baseband coefficient sequence - no separate demodulation/downmixing step needed.
3. The frequency-domain window's own length is proportional to the bin's center frequency (`binWindowLength()`): a constant-Q design, giving long analysis windows (deep frequency resolution) at low frequencies and short ones (fine time resolution) at high frequencies - the defining property design-doc-level "NSGT"/constant-Q discussions describe, achieved here without the legacy codec's own `nsgt` library.
4. Each bin's native-rate sequence (a different length per bin - the lowest bin has the fewest native samples, the highest the most) is linearly interpolated (magnitude; phase via cosine/sine decomposition, same wraparound-avoidance as Stream) onto the image's common `frameCount`-wide pixel grid - **this interpolation is the one source of reconstruction error** in an otherwise near-perfectly-invertible transform (`PoolImage`'s own docs: "lossless" is a practical, not mathematically absolute, claim, matching the wider NSGT literature).
5. Decode reverses the process per bin: the stored common-grid magnitude/phase is interpolated back to that bin's own native frame count, forward-FFT'd, and overlap-added (frequency-domain, windowed-slice placement, normalized by summed squared window) back into a global spectrum per channel, which is then inverse-FFT'd once to recover the time-domain signal.

Frequencies outside `[minFrequencyHz, maxFrequencyHz]` are not represented at all and reconstruct as silence - an accepted, by-design loss (`poolEncode()`'s own docs), not a bug.

## 3. Window Functions

**Exactly one window function is used anywhere in this codec: a periodic Hann window**, `0.5 - 0.5*cos(2*pi*i/N)`. It appears in two different places with two different roles:

- **Stream**: a *time-domain* analysis/synthesis window, applied to each `fftSize`-sample frame before/after the FFT - the conventional STFT role.
- **Pool**: a *frequency-domain* window, applied to each bin's own slice of the global spectrum before the inverse FFT that recovers its native-rate sequence - the dual of Stream's role, operating on frequency bins instead of time samples.

No other window (Blackman, Hamming, Kaiser, etc.) is implemented anywhere in the codec. The legacy Python codec's own CPU-fallback STFT backend used the same Hann choice at the same 75% overlap ratio (`docs/legacy/CODEC_DETAILS.md`'s `VulkanSTFTBackend`) - Stream's `fftSizeFor()` explicitly cites that precedent in its own code comment.

## 4. Axis Scaling

### Horizontal (time)

Uniform and linear in both codecs: each column represents exactly `hopLength` samples (`hopLength / sampleRateHz` seconds - 441/44100 = 10 ms by default). `frameCount = ceil(numSamples / hopLength)`. Identical formula for Stream and Pool - a deliberate choice (`PoolImage`'s own docs) so a layer's Pool and Stream renders are directly, pixel-for-pixel comparable on the time axis.

### Vertical (frequency)

**Purely logarithmic, in both codecs - not mel, not octave-aligned, not variable-Q.** `centerFrequencyHz(bin) = minFrequencyHz * exp(t * log(maxFrequencyHz / minFrequencyHz))`, where `t = bin / (binCount - 1)` ranges linearly over `[0, 1]`. This is `docs/sound-mind-design.md`'s "Frequency Scale" concept's *default* ("close, but not identical, to mel") - but as of this writing, **it is also the only scale the implementation offers**. `StreamCodecConfig` has no `scale`/mel/octave/variable-Q field at all; the legacy codec's `'log'`/`'mel'`/`'oct'`/`'varq'` options (`docs/legacy/CODEC_DETAILS.md` Section 8) have no counterpart here yet. Bin 0 (in-memory storage) is always the *lowest* frequency; both codecs' own display/file-writing conventions flip this so row 0 reads as the *highest* frequency (bass at the bottom, treble at the top - `color_mapping.cpp`/`pool_file.cpp`'s shared `height - 1 - y` convention), matching how a spectrogram is conventionally displayed.

## 5. Bit Depth, by Representation

There is no single answer to "what bit depth are the pixels" - it depends on which of three distinct representations is meant:

| Representation | Amplitude | Phase | Notes |
|---|---|---|---|
| **In-memory** (`StreamImage`/`PoolImage`) | 32-bit float, **unclamped**, raw dB | 32-bit float radians | The canonical representation every Studio operation (painting, filters, compositing) actually reads/writes. |
| **Pool on-disk file** (`.smpool`, a TIFF) | 16-bit unsigned int, dB **clamped to [-96, 0]** before quantizing | 16-bit unsigned int, phase wrapped to `[-pi, +pi)` | `pool_file.cpp`'s `dbToUint16()`/`phaseToUint16()` - ~1.5 microdB per level. A real, additional precision loss on top of `poolEncode()`'s own interpolation error (`pool_file.h`'s own docs are explicit about this). |
| **Stream on-disk file** (custom binary) | 32-bit float, **no quantization at all** | 32-bit float | A raw dump of the in-memory planes (`stream_file.cpp`) - no bit-depth reduction, unlike Pool's file format. |
| **RGB display** (`toRgbImage()`, `color_mapping.cpp`) | 8-bit per channel, dB clamped to [-96, 0] | 8-bit (phase, where used as the blue channel) | Purely for on-screen rendering (`QImage`-compatible) - never read back into either codec's own in-memory representation except via the deliberate image-import round trip (`fromRgbImage()`). |

So: **the in-memory data the Studio actually operates on is 32-bit float, unclamped.** 16-bit quantization only happens at the Pool file's own on-disk boundary; 8-bit only at the RGB-display boundary. Painting, filtering, and compositing all happen before either of those lossy boundaries is ever crossed.

## 6. Pre-Processing and Post-Processing

This section answers the specific questions the legacy codec's own documentation (`docs/legacy/CODEC_DETAILS.md`) raises, for the *current* C++ codec. As of `v0.1.11.1`, two of the three (equal-loudness weighting, input-level normalization) are implemented for **both** codecs; the third (reflection padding) is implemented for Stream only - see `docs/sound-mind-architecture.md`'s Decision #205 for why Pool's own attempt was deferred rather than shipped with a real, measured correctness problem.

### Equal-loudness / perceptual weighting (A-weighting, ISO 226, or similar)

**Implemented for both codecs** (`aWeightingDb()`, duplicated per this codebase's small-helper convention in `stream_frame_codec.h` and `pool_codec.cpp`): the standard IEC 61672 A-weighting curve, normalized to `0` dB at 1 kHz, added to every stored dB value per bin during encode and subtracted back out per bin during decode - cosmetic, never affecting the reconstructed audio, exactly the legacy codec's own Pool-only mechanism (`docs/legacy/CODEC_DETAILS.md` Section 3.7), now applied identically to Stream too. Resolved `docs/sound-mind-architecture.md`'s former *Decisions Needed* #4. ISO 226 (full equal-loudness contours) was considered and passed on - it's level-dependent, needing a calibrated absolute-SPL reference this all-relative-dBFS system has no equivalent of, whereas A-weighting is frequency-only and was already the legacy codec's own choice.

### Input-level normalization (so quiet/dark content stays audible, loud/bright content doesn't clip)

**Implemented for both codecs**, but deliberately **not** the legacy codec's own one-way behavior. The legacy codec normalized every encode's input to a peak of 0.95 before transforming (`docs/legacy/CODEC_DETAILS.md` Section 3.1) and never restored the original level on decode - fine for its own calling pattern (one normalize per whole-file import), but this codebase's `encode()`/`poolEncode()` are called per-paint-stroke/per-layer/per-live-buffer, where blindly normalizing every call would make every quiet and every loud layer converge on the same brightness, destroying the relative dynamics multi-layer compositing depends on. The current implementation (`StreamImage`/`PoolImage` both gain `inputNormalizationScale`) is **fully reversible**: `scale = (peak > 0) ? 0.95/peak : 1.0` (one combined left+right peak, not independent per-channel scales, to preserve stereo balance) is applied before transforming and carried on the returned image; `decode()`/`poolDecode()` divide the final reconstructed audio by that same scale before returning, so the decoded level always matches the original input exactly, regardless of what the stored image's own internal dynamic-range usage needed.

### Start/end boundary handling (removing encoding artifacts at clip edges)

**Implemented for Stream; deliberately deferred for Pool, after two failed attempts - see Decision #205 for the full account.** The legacy codec's own technique (`docs/legacy/CODEC_DETAILS.md` Section 3.2): reflection-pad the signal at both ends (mirroring without repeating the boundary sample) by one low-frequency bin's own period before transforming, so the longest analysis windows see a smooth continuation instead of a hard edge, then crop the extra frames back out.

- **Stream**: `extractFrame()` (`stream_frame_codec.h`) now reflects into any out-of-range sample within `computePadSamples()`'s own pad length, instead of the hard zero-fill it used before - entirely local to frame extraction, invisible to `decode()`, zero effect on frame count or timing. `StreamIncrementalEncoder` is deliberately excluded (always `padSamples=0`) - a live, causally-arriving stream has no future samples to reflect around, the same reasoning that already excludes it from normalization.
- **Pool**: **not implemented.** Pool's single whole-signal FFT means padding requires growing that FFT, which means every per-bin window/placement calculation is derived from the padded length - and reconciling that against `poolDecode()`'s own independent reconstruction proved to have a real, structural correctness problem: Pool's single *global* spectrum (unlike Stream's independent per-frame one) means a bin's contribution anywhere reaches *every* reconstructed sample via the one final inverse FFT, with no way found yet to safely represent a "don't-care, padding-only" contribution without it leaking into the real result - measured as a signal-wide gain error up to 3-4x in testing, not merely a softer approximation. `poolEncode()`/`poolDecode()` remain exactly as fixed-length (unpadded) as before this decision; the one real cost of this gap - whatever audible artifact an abrupt clip start/end produces in Pool's longest analysis windows - is unmitigated and not yet measured (a real `sound-mind-codec-eval` run against real clips, per Decision #205's own backlog note, is the way to actually quantify it).

### Snippet/boundary continuity for long files split into pieces

Not applicable yet - the current codec has no snippet-splitting mechanism at all (unlike the legacy codec's `encode_snippets()`, `docs/legacy/CODEC_DETAILS.md` Section 4). Every encode is a single whole-buffer operation today.

### Overlap-add normalization

**Implemented in both codecs**, and this *is* the standard, necessary kind of post-processing a windowed analysis/synthesis transform needs regardless of any perceptual/safety concern: both `decode()` and `poolDecode()` divide their accumulated reconstruction by the summed squared window (floored at a small epsilon to avoid dividing by near-zero at a signal's very start/end) - the conventional WOLA (weighted overlap-add) normalization, not something specific to Sound Mind.

### Amplitude-to-dB floor

Both `amplitudeToDb()` implementations (`stream_frame_codec.h`, `pool_codec.cpp`) floor the linear magnitude at `1e-7` before taking `20*log10(...)`, i.e. a soft floor around -140 dBFS, purely to avoid `-inf`/`NaN` for an exactly-silent bin - not a perceptual or safety choice, just numerical hygiene.

## 7. What the Codec Does *Not* Do

Collected in one place for a quick scan against the legacy codec's own feature list:

- No reflection-padding or other deliberate edge-artifact mitigation **for Pool specifically** - implemented for Stream (Section 6).
- No snippet-splitting for long files.
- No mel, octave-aligned, or variable-Q frequency scale - log only (Section 4).
- No GPU-accelerated encode/decode path (Section 8).

(A-weighting and input-level normalization, both previously listed here, are implemented for both codecs as of `v0.1.11.1` - see Section 6.)

None of these are regressions from a working feature that was removed - the current codec was built from scratch, and each of these either hasn't been reached yet or (Pool's own reflection padding) was attempted and deliberately deferred after a real, measured correctness problem - see `docs/sound-mind-architecture.md`'s Decision #205. They're listed here precisely so that absence is a documented, deliberate fact rather than something a future reader has to rediscover by reading source.

## 8. CPU vs. GPU

**Both codecs are CPU-only, in both directions, as of this writing.** Checked directly: nothing in `sound-mind-codec` references `sound_mind::gpu::ComputeDevice` or any DirectX 12 type. `StreamCodecConfig`'s own docs and `encode()`'s own Doxygen comment say so explicitly ("CPU-only for now... the not-yet-prototyped GPU path").

This is easy to conflate with a different, already-shipped GPU capability: `sound-mind-gpu`'s compute shaders (`mix_amplitude_phase_signal.hlsl` and friends) *do* accelerate GPU work in this codebase, but for `sound-mind-core`'s **compositor** - blending already-encoded layers' amplitude/phase together - not for the codec's own audio-to-spectrogram or spectrogram-to-audio transform. A project with GPU acceleration enabled gets a faster *composite*; encoding a new import or decoding for playback still runs the exact same CPU code path described in Section 2 either way.

`docs/sound-mind-architecture.md`'s *Decisions Needed* #1 (GPU/audio-thread handoff mechanism) and `tech-stack-decisions.md`'s own framing ("spectrogram/DSP transform acceleration (NSGT-style processing)... used for") both describe this as a real, intended future direction - just not one implemented yet.

## 9. On-Disk File Formats

Covered in detail by each format's own header docs (`pool_file.h`/`stream_file.h`) and summarized in Section 5's bit-depth table above:

- **Pool (`.smpool`)**: a standard TIFF 6.0 container, four 16-bit grayscale pages (left amplitude, right amplitude, left phase, right phase, in that order), LZW-compressed, via `libtiff`. Metadata (`SoundMindPool:key=value` lines) lives in the first page's `ImageDescription` tag - sample rate, hop length, bin count, frequency range, frame count, sample count, and (`v0.1.11.1`) `InputNormalizationScale`. Carries forward the legacy format's general shape (TIFF, 16-bit, LZW) without being bound to its exact metadata schema (`pool_file.h`'s own docs) - the stored amplitude is already A-weighted and normalized by the time it reaches this format, via `poolEncode()` itself (Section 6), not via any format-level step of this file's own.
- **Stream**: a lightweight custom binary format (`SMST` magic, a version number, then the config fields, then three raw `float[bin][frame]` planes, then (`v0.1.11.1`) one trailing `inputNormalizationScale` float) - no compression, no quantization, no generic-viewer compatibility, because Stream's entire reason to exist is speed. The trailing scale field is additive, not a format-version bump: a file written before it existed is simply shorter by one float, and the reader treats a missing/short trailing field as "default to `1.0`" rather than an error - deliberately avoiding a breaking file-format change for a field whose absence already has an exact, correct default. Native byte order only (no endian-swapping) - acceptable since this project targets Arm64 and x64 exclusively, both little-endian.

## 10. Known Fidelity Reference Point

`docs/sound-mind-architecture.md`'s Decision #8 records a measured correlation of **0.999984** between original and Pool-decoded audio, for both mono and genuinely-stereo content, on the first working implementation of `poolEncode()`/`poolDecode()`. `sound-mind-codec-eval`'s own smoke-testing during its own development (a synthetic 440/660 Hz stereo tone, default hop length/bin count) measured a consistent **0.999992** correlation and ~52 dB overall SNR for Pool, versus **0.9695** correlation and ~12.5 dB overall SNR for Stream on the same input - a concrete illustration of the fidelity/speed tradeoff Section 2 describes, not a comprehensive evaluation.

**These specific numbers predate `v0.1.11.1`** (Decision #205's normalization/A-weighting/Stream-padding additions) and have not been re-measured against the current implementation - normalization and A-weighting are both designed to be exactly reversible/cosmetic and so shouldn't move a correlation-based number, but that expectation itself hasn't been re-verified end-to-end with real audio. Real, user-provided material across a genuine sweep of hop lengths and bin counts - what `sound-mind-codec-eval` exists to automate - is what the next iteration of this design doc's own numbers should be based on.
