# Sound Mind Codec — Implementation Reference

This document traces every design decision and algorithmic step in the codec, in both the encoding and decoding directions, with exact references to the implementation files.

---

## Contents

1. [The Transform: Why NSGT](#1-the-transform-why-nsgt)
2. [The Storage Format](#2-the-storage-format)
3. [Encoding Pipeline](#3-encoding-pipeline)
   - 3.1 Input normalisation
   - 3.2 Reflection padding
   - 3.3 Context substitution at snippet boundaries
   - 3.4 The forward NSGT
   - 3.5 Time-axis interpolation to a common grid
   - 3.6 Frequency-axis flip
   - 3.7 A-weighting
   - 3.8 dB quantisation to pixels
   - 3.9 Phase resampling and quantisation
   - 3.10 Assembling the four-page image
   - 3.11 TIFF file writing
4. [Snippet Encoding](#4-snippet-encoding)
5. [Decoding Pipeline](#5-decoding-pipeline)
   - 5.1 TIFF reading and validation
   - 5.2 Page extraction
   - 5.3 Pixel-to-dB and A-weighting removal
   - 5.4 Phase pixel decoding
   - 5.5 Complex coefficient reconstruction
   - 5.6 Frequency-axis unflip
   - 5.7 Backend priming
   - 5.8 Carrier-driven phase de-interpolation
   - 5.9 NSGT inverse and stereo assembly
6. [Metadata](#6-metadata)
7. [GPU Backends](#7-gpu-backends)
8. [Frequency Scale Options](#8-frequency-scale-options)

---

## 1. The Transform: Why NSGT

The codec uses the **Non-Stationary Gabor Transform** (NSGT), implemented by the `nsgt` library (`transforms.py`, `NSGTBackend`). The key property is *perfect reconstruction*: the inverse of the forward transform returns exactly the original signal (up to floating-point rounding). This makes the image a lossless representation of the audio — not an approximation.

The NSGT is also *constant-Q*: window length is inversely proportional to frequency. At 20 Hz the analysis window spans roughly one full period of the signal (≈50 ms at 44100 Hz). At 16 kHz the window is ~62 µs. This gives deep frequency resolution at bass frequencies and fine time resolution at high frequencies, matching human hearing. An STFT with a fixed window cannot do both simultaneously.

The NSGT does not produce a uniform time-frequency grid. Each frequency bin has its own native frame count determined by how many hop-length windows fit its analysis window. The lowest-frequency bin produces the fewest frames; the highest produces the most. This is not a limitation for storage — the implementation interpolates every bin to a common pixel grid (Section 3.5) — but it is the central fact that drives most of the design.

The `NSGTBackend` class (`transforms.py:70`) wraps `nsgt.NSGT` with:

- `_build_scale()` (line 114): constructs the frequency scale object. For the default `'log'` scale this is `nsgt.LogScale(min_freq, max_freq, n_bins)`, which spaces bins logarithmically between 20 Hz and 16 kHz.
- `_get_or_build_nsgt(audio_len)` (line 134): constructs the `nsgt.NSGT` object. Crucially, the NSGT object is *tied to a specific signal length* — it pre-computes the analysis/synthesis window lengths for each bin from the signal length and sample rate. The object is cached and rebuilt only when the audio length changes.
- `n_bins` (line 148): derived by calling the scale object and counting the returned frequencies (`frqs, _ = self._scale_obj()`), not from the `ceil(bpo × octaves)` approximation. The approximation is used only to pass `n_bins` into `LogScale`, which then refines it internally.

---

## 2. The Storage Format

Each Sound Mind TIFF file (`format.py`) contains **four 16-bit grayscale pages**, each with shape `(height, width)`:

| Page | Content | Encoding |
|------|---------|----------|
| 0 | Left channel amplitude | −96 to 0 dB → 0–65535 |
| 1 | Right channel amplitude | −96 to 0 dB → 0–65535 |
| 2 | Left channel phase | −π to +π → 0–65535 |
| 3 | Right channel phase | −π to +π → 0–65535 |

**Height** = the number of NSGT frequency bins (e.g. 724 at 75 bpo over 20 Hz–16 kHz). Computed by `nsgt_bin_count(bpo)` (`format.py:72`):

```python
math.ceil(bpo * math.log2(MAX_FREQUENCY_HZ / MIN_FREQUENCY_HZ))
```

**Width** = number of time-axis pixels = `round(n_samples / hop_length)`.

**Why uint16?** Sixteen bits give 65 536 discrete levels. Over the −96 dB to 0 dB amplitude range this is a step of ≈1.5 µdB per level, far below any audible threshold. For phase it provides angular resolution of ≈0.010° (2π / 65535). The storage is compact — a one-minute file at 75 bpo and 10 ms/pixel is approximately 724 × 6000 × 4 pages × 2 bytes ≈ 35 MB before LZW compression, which typically reduces it to 20–25 MB for musical content.

**Why amplitude in dB?** NSGT magnitudes span many orders of magnitude (silence to full scale). Storing raw linear magnitudes in 16 bits would require careful range selection and the low-level detail would be crushed. The dB scale compresses the dynamic range uniformly, so every decibel of signal is equally represented by the same number of pixel levels.

**Why store phase at all?** The phase determines the waveform shape within each analysis window. Without phase, reconstruction uses zero-phase synthesis, which introduces tonal artefacts and time-smearing. Storing amplitude and phase together makes the TIFF a complete, invertible representation of the NSGT coefficients.

---

## 3. Encoding Pipeline

The entry point is `Encoder.encode()` (`encoder.py:272`), which calls `_encode_channel()` once for each stereo channel and then assembles the four-page image.

### 3.1 Input normalisation

`_encode_channel()` (`encoder.py:167`) begins by normalising the audio to a peak of 0.95:

```python
current_peak = np.abs(audio).max()
scale = (0.95 / current_peak) if current_peak > 0 else 1.0
audio = audio * scale
```

0.95 rather than 1.0 leaves a small headroom margin. If the caller supplies a `normalisation_scale` (used by `encode_snippets` for global consistency — see Section 4), that scale is used verbatim instead.

### 3.2 Reflection padding

The NSGT applies very long analysis windows at low frequencies. If the signal starts or ends with silence (or abruptly), the transform will see a discontinuity at the boundary and produce ringing artefacts in the lowest frequency bins. The fix is to pad the signal symmetrically before analysis and then crop the corresponding frames afterward.

The pad length is one full period of the lowest-frequency bin:

```python
pad_samples = min(int(self.sample_rate / self.backend.min_freq), n_samples - 1)
padded = np.pad(audio, pad_width=pad_samples, mode='reflect')
```

`reflect` mode mirrors the signal at both ends without repeating the boundary sample (`[a, b, c, d]` → `[c, b, a, b, c, d, c, b]`). This gives the analysis window a smooth, continuous signal to look at during the ramp-in and ramp-out.

After the transform the corresponding frames are trimmed. The frame count scales linearly with signal length, so the fraction of frames to remove equals the fraction of samples that were padding:

```python
pad_frames = round(pad_samples * n_padded_frames / n_padded)
end_frame = n_padded_frames - pad_frames
transform = transform[:, pad_frames:end_frame]
```

### 3.3 Context substitution at snippet boundaries

When encoding a long file as sequential snippets (`encode_snippets`, Section 4), the reflection at the end of snippet N does not match the beginning of snippet N+1. The actual neighbouring audio is passed in as `pre_audio` and `post_audio`, and the reflected padding is overwritten with it:

```python
if pre_audio is not None:
    ctx = pre_audio[-pad_samples:].astype(np.float32) * scale
    padded[pad_samples - len(ctx):pad_samples] = ctx
if post_audio is not None:
    ctx = post_audio[:pad_samples].astype(np.float32) * scale
    padded[pad_samples + n_samples:pad_samples + n_samples + len(ctx)] = ctx
```

This ensures the low-frequency bins at each snippet boundary see real audio continuity rather than an artificial reflection, which would otherwise introduce a visible seam in the spectrogram.

### 3.4 The forward NSGT

`backend.compute(padded, target_frames=padded_target_frames)` (`transforms.py:185`) calls `nsgt.forward()`, which returns a list of complex arrays — one per frequency bin plus a DC term and a Nyquist term. The inner bins (excluding DC and Nyquist) are the useful content:

```python
all_coeffs = list(nsgt.forward(audio_data))
if len(all_coeffs) == self._n_bins + 2:
    coeffs = all_coeffs[1:-1]   # strip DC and Nyquist
```

DC (0 Hz) and Nyquist are set to zero on the inverse pass (Section 5.8). They are outside the 20 Hz–16 kHz range and contribute nothing to the reconstructed audio within that band.

Each element `coeffs[k]` is a 1D complex array of length `bin_lengths[k]`, which varies per bin. The longest array belongs to the highest-frequency bin.

**Normalisation factors.** The NSGT library does not normalise its output to unit gain. Each bin's coefficients have an implicit scale factor of `2 × bin_length / audio_len`. The codec removes this by dividing by the normalisation factor during the forward pass and reapplying it during the inverse:

```python
norm_factors[k] = audio_len / (2.0 * max(bin_length, 1))
```

The factor is stored on the backend object so both directions use the same values.

### 3.5 Time-axis interpolation to a common grid

Every bin's coefficient array is interpolated to a uniform time grid of `out_frames` columns. This is the step that converts the jagged NSGT output into a rectangular pixel image.

`out_frames` defaults to `max_frames` (the length of the highest-frequency bin's array). When `target_frames` is provided and is smaller, interpolation goes directly to that smaller size:

```python
out_frames = (target_frames
              if target_frames is not None and 0 < target_frames < max_frames
              else max_frames)
```

This memory optimisation (`target_frames` ≈ pixel width of the output image) avoids allocating a large `(n_bins × max_frames)` intermediate array when the output is a small pixel grid. The `target_frames` passed in is `round(n_samples / hop_length)`, i.e. the pixel width of the output image.

The interpolation uses magnitude and **unwrapped phase** separately, not real and imaginary parts:

```python
mag = np.abs(c)
pha = np.unwrap(np.angle(c))
mag_i = np.interp(tgt_x, src_x, mag)
pha_i = np.interp(tgt_x, src_x, pha)
matrix[k, :] = (mag_i * np.exp(1j * pha_i)) / norm
```

Interpolating real and imaginary parts directly causes *phasor cancellation*: when the phase rotates by close to π between two samples, the real and imaginary components trace the long arc around the origin, and their linear interpolation crosses near zero, introducing a phantom silence. Magnitude plus unwrapped phase avoids this — magnitude interpolates smoothly and phase advances monotonically after unwrapping.

### 3.6 Frequency-axis flip

After the transform, `transform[::-1]` flips the frequency axis. The NSGT returns rows in ascending order (row 0 = lowest frequency bin). The TIFF convention is row 0 = top = highest frequency, matching how spectrograms are displayed visually (bass at the bottom, treble at the top).

```python
transform = transform[::-1]
bin_freqs_desc = self._bin_frequencies[::-1]
```

The frequency array is also flipped so downstream steps can index into it with the same row indices.

### 3.7 A-weighting

The human ear is not equally sensitive at all frequencies. At 1 kHz human hearing peaks; sensitivity drops sharply below 200 Hz and above 8 kHz. Without correction, bass and treble content would appear faint in the image relative to its perceptual loudness, making visual editing unintuitive.

A-weighting adds a per-row dB offset derived from the standard A-weighting curve (`utils.py:509`):

```python
a_weight_db = 20 * np.log10(a_weight + 1e-10) + 2.0
```

Applied after the dB conversion:

```python
amplitude_db = np.clip(
    apply_equal_loudness_weighting(amplitude_db, bin_freqs_desc, weighting='a'),
    -96.0, 0.0,
)
```

The offset is added to each frequency row. Frequencies around 1–4 kHz where the ear is most sensitive are boosted (appear brighter); frequencies below 100 Hz and above 12 kHz are attenuated (appear darker). The same offset is subtracted back on decode (`decoder.py:267`), so A-weighting is cosmetic — it does not affect the reconstructed audio.

### 3.8 dB quantisation to pixels

The dB range [−96, 0] is mapped linearly to [0, 65535] (`utils.py:221`):

```python
clamped = np.clip(amplitude_db_array, -96.0, 0.0)
normalized = (clamped - (-96.0)) / (0.0 - (-96.0))   # → [0, 1]
pixels = (normalized * 65535).round().astype(np.uint16)
```

Pixel 0 represents −96 dBFS (silence floor). Pixel 65535 represents 0 dBFS (full scale). Values outside [−96, 0] are clamped before quantisation; this never happens in normal operation because the NSGT output is bounded and the audio is normalised to ≤0.95 before encoding.

**Why −96 dB as the floor?** −96 dBFS is the theoretical noise floor of 16-bit audio (6.02 dB × 16 bits ≈ 96 dB). Any signal below this level is below the audible noise floor and carries no useful information. Clamping there means silent regions are stored as pixel 0 exactly, which compresses well under LZW.

### 3.9 Phase resampling and quantisation

When `target_frames` was set and the amplitude array was resampled (Section 3.5), the phase also needs resampling. Wrapping is the problem: phase lives on a circle. Interpolating raw `arctan2` values across the ±π discontinuity would produce an interpolated phase of 0 at the worst point, rotating in the wrong direction.

The fix is to decompose into cosine and sine, interpolate each component (which are smooth), and reconstruct phase:

```python
cos_r = resample_time_axis(np.cos(phase), target_frames)
sin_r = resample_time_axis(np.sin(phase), target_frames)
phase = np.arctan2(sin_r, cos_r)
```

This traces the short arc on the unit circle rather than wrapping around the long way.

Phase pixels are encoded by mapping [−π, +π] → [0, 65535] (`utils.py:263`):

```python
wrapped = (phase + np.pi) % (2 * np.pi)          # → [0, 2π)
pixels = (wrapped / (2 * np.pi) * 65535).round().astype(np.uint16)
```

The modulo is a safety measure — it handles the rare case where a floating-point phase value falls slightly outside [−π, +π] due to `arctan2` precision. Pixel 0 = −π, pixel 65535 ≈ +π (one step below the 2π wrap).

### 3.10 Assembling the four-page image

Back in `encode()` (`encoder.py:309`):

```python
image = np.zeros((n_bins, n_frames, 4), dtype=np.uint16)
image[:, :, 0] = left_amp
image[:, :, 1] = right_amp
image[:, :, 2] = left_phase
image[:, :, 3] = right_phase
```

The in-memory layout is `(height, width, pages)` with `dtype=uint16`. This is the canonical in-memory representation used throughout the rest of the codec and the Studio.

### 3.11 TIFF file writing

`SoundMindFormat.write_tiff()` (`format.py:263`) transposes to `(pages, height, width)` before writing, which is the conventional layout for multi-page TIFFs:

```python
out = np.moveaxis(self.image_data, 2, 0)   # (H, W, 4) → (4, H, W)
```

Two TIFF variants are supported:

**Stripped** (`TiffVariant.STRIPPED`): the default. Pages are stored as horizontal strips. Sequential read and write is fast. Best for clips, patches, and files under ≈30 seconds.

**Tiled** (`TiffVariant.TILED`): pages are stored as 256 × 256-pixel tiles (configurable). Any tile can be read independently without loading the whole file, enabling random-access seeking within the image. Used for long compositions where the Studio needs to decode a small playback segment without loading the full file into memory.

Both variants use **LZW compression** (`compression='lzw'`). LZW is lossless and exploits the structured repetition in spectrograms well — silent regions are entirely zeros (pixel 0), and harmonic content repeats patterns across time. Empirically, LZW compresses a typical spectrogram to 50–70% of uncompressed size.

The `photometric='minisblack'` tag tells TIFF readers that pixel 0 = black and higher values = brighter, which renders correctly in standard image viewers.

The `description` field (`SoundMind:…` key=value lines) is written into the TIFF `ImageDescription` tag (page 0). This makes the file self-describing without any sidecar: all parameters needed to decode it (sample rate, duration, hop length, bpo, scale, varq zones) are embedded in the file itself.

---

## 4. Snippet Encoding

`Encoder.encode_snippets()` (`encoder.py:378`) handles files longer than a chosen snippet duration (default 60 s). It exists because very long files would require enormous intermediate arrays; splitting also produces files that random-access playback can seek into.

Two design choices make the output gapless:

**Global normalisation.** The normalisation scale is derived from the peak of the entire audio array before splitting:

```python
global_peak = np.abs(audio).max()
global_scale = (0.95 / global_peak) if global_peak > 0 else 1.0
```

This scale is passed to every snippet's `encode()` call via `normalisation_scale=global_scale`. Without this, each snippet would independently normalise to its own peak, making loud passages and quiet passages appear at the same brightness — destroying relative dynamics.

**Context passing.** Each snippet's `encode()` call receives the immediately adjacent audio from neighbouring snippets as `pre_audio` and `post_audio`:

```python
pre  = audio[max(0, start - ctx_samples):start]
post = audio[end:end + ctx_samples]
```

The context length `ctx_samples = sample_rate / backend.min_freq` matches the pad length used inside `_encode_channel`, so the padding region at each boundary is replaced with real audio, eliminating the reflection artefacts described in Section 3.3.

---

## 5. Decoding Pipeline

The entry point is `Decoder.decode()` (`decoder.py:114`), which calls `decode_sound_mind()` (`decoder.py:139`) after reading the file.

### 5.1 TIFF reading and validation

`validate_format()` (`format.py:371`) checks that the file is a valid TIFF (magic bytes `II` or `MM` followed by magic number 42) and that the `ImageDescription` tag contains `SoundMind:` lines. This runs before any decode attempt.

`SoundMindFormat.read_tiff()` (`format.py:303`) reads the image with `tifffile`, finds the `ImageDescription` tag, and parses it with `ImageMetadata.from_tiff_description()` (`format.py:186`).

The on-disk layout is `(4, height, width)`. It is transposed back to `(height, width, 4)` in memory:

```python
if image_data.ndim == 3 and image_data.shape[0] in (2, 4):
    image_data = np.moveaxis(image_data, 0, 2)
```

The shape check `image_data.shape[0] in (2, 4)` guards against accidentally transposing a file that was already written in the other orientation.

A compatibility case handles files that somehow loaded as uint8 — they are upscaled to uint16 by multiplying by 257 (≈65535/255).

### 5.2 Page extraction

```python
left_pixels  = image[:, :, 0]
right_pixels = image[:, :, 1]
left_phase_pixels  = image[:, :, 2]
right_phase_pixels = image[:, :, 3]
```

Each page is a `(height, width)` uint16 array. Row 0 = highest frequency bin (flipped from NSGT order).

### 5.3 Pixel-to-dB and A-weighting removal

`pixels_to_amplitude_db_array()` (`utils.py:238`) inverts the encoding:

```python
return -96.0 + (pixels.astype(np.float64) / 65535) * (0.0 - (-96.0))
```

A-weighting is removed by subtracting the same per-row offset that was added during encoding. The frequencies are in *descending* order at this point (row 0 = highest frequency), matching the stored row order:

```python
a_weight = compute_a_weighting(frequencies).reshape(-1, 1)
amplitude_db = amplitude_db - a_weight
```

`magnitude = db_to_linear(amplitude_db)` (`utils.py:445`) recovers the linear NSGT magnitudes:

```python
return 10 ** (db / 20.0)
```

### 5.4 Phase pixel decoding

`pixels_to_phase_array()` (`utils.py:274`) inverts the encoding:

```python
return (pixels.astype(np.float32) / 65535 * 2 * np.pi) - np.pi
```

Result is float32 in [−π, +π).

### 5.5 Complex coefficient reconstruction

```python
transform_flipped = magnitude * np.exp(1j * phase)
```

This reconstructs the complex NSGT coefficient matrix in the same flipped (descending-frequency) orientation as stored.

### 5.6 Frequency-axis unflip

```python
transform = transform_flipped[::-1]
```

The NSGT inverse expects rows in ascending order (row 0 = lowest frequency). This restores that order.

### 5.7 Backend priming

Before calling `inverse()`, the decoder calls `backend.prime_for_decode(audio_len)` (`transforms.py:159`). This is necessary because the NSGT object is tied to a signal length, and the inverse transform needs the per-bin `_bin_lengths` and `_norm_factors` that would normally be populated during `compute()`. Since the decoder never calls `compute()` on real audio, `prime_for_decode` runs a dummy forward pass on a zero-valued signal of the correct length:

```python
dummy = list(nsgt.forward(np.zeros(audio_len)))
inner = dummy[1:-1]
self._bin_lengths = [c.shape[0] for c in inner]
self._norm_factors = np.array([
    audio_len / (2.0 * max(l, 1)) for l in self._bin_lengths
])
```

The dummy pass is cheap (zeros are all the same, so the FFT operations are trivial), but it fully initialises the internal data structures needed by `inverse()`.

### 5.8 Carrier-driven phase de-interpolation

The stored matrix has each bin at a uniform time grid of `n_stored` columns. The NSGT inverse expects each bin at its *native* frame count. The decoding direction must reverse the interpolation performed during encoding.

Simple linear interpolation of the stored phase back to the native grid fails for the same reason it fails during encoding: phase wraps around. At high frequencies the carrier wave can advance by multiple radians per stored pixel column, so the "between" phase values are not obtainable by naive linear interpolation.

The solution (`transforms.py:247`) is **carrier-driven phase synthesis**. For each bin at centre frequency `f_k`:

1. Compute the expected carrier phase advance per sample: `dphi = 2π × f_k / sr`
2. At each stored time point `t[i]` (in samples), the carrier phase is `dphi × t[i]`
3. The *residual* = stored phase − carrier phase. This residual is the deviation from a pure sinusoid at `f_k` — it changes slowly and stays within a small range
4. Unwrap and interpolate the residual to the native frame count
5. Add the native carrier back: `phase_i = carrier_native + residual_i`

```python
carrier_stored = dphi * stored_t
residual       = np.unwrap(np.angle(row) - carrier_stored)
residual_i     = np.interp(native_norm, stored_norm, residual)
carrier_native = dphi * native_t
phase_i        = carrier_native + residual_i
```

Magnitude is interpolated independently with linear `np.interp`. The reconstructed coefficient is:

```python
c = mag_i * np.exp(1j * phase_i)
inner_coeffs.append(c * norm)
```

Multiplying by `norm` reapplies the normalisation factor removed during encoding.

### 5.9 NSGT inverse and stereo assembly

DC and Nyquist terms (set to zero during the forward pass) are prepended and appended:

```python
full_coeffs = (
    [np.zeros(dc_len, dtype=np.complex128)]
    + inner_coeffs
    + [np.zeros(nyq_len, dtype=np.complex128)]
)
audio = nsgt.backward(full_coeffs)
```

`nsgt.backward()` performs the actual NSGT synthesis. The result is float32 audio.

Back in `decode_sound_mind()`:

```python
n = min(len(left_audio), len(right_audio))
stereo = np.stack([left_audio[:n], right_audio[:n]], axis=1)
return AudioBuffer(stereo, sample_rate=metadata.sample_rate, channels=2)
```

The `min()` trim guards against the rare case where left and right channels emerge at slightly different lengths due to floating-point rounding in the per-bin frame count calculations.

---

## 6. Metadata

All parameters needed for decoding are stored as `SoundMind:Key=Value` lines in the TIFF `ImageDescription` tag (`format.py:157`). The format is plain text, readable by any TIFF viewer that shows metadata (e.g. GIMP, ExifTool).

| Tag | Example | Purpose |
|-----|---------|---------|
| `SoundMind:Version` | `0200` | Format version (hex, V2.0 = 0x0200) |
| `SoundMind:SampleRate` | `44100` | Sample rate in Hz |
| `SoundMind:Duration` | `180.000000` | Audio duration in seconds (6 decimal places) |
| `SoundMind:ImageWidth` | `18000` | Width in pixels |
| `SoundMind:ImageHeight` | `724` | Height in pixels = NSGT bin count |
| `SoundMind:EncodingScheme` | `spectrogram_stereo64` | Identifies the codec type |
| `SoundMind:BitsPerChannel` | `16` | Bit depth per pixel |
| `SoundMind:TiffVariant` | `stripped` | `stripped` or `tiled` |
| `SoundMind:TileSize` | `256` | Tile size (tiled variant only) |
| `SoundMind:TransformBackend` | `NSGT (log, 75 bpo)` | Backend name string |
| `SoundMind:Channels` | `2` | Always 2 (stereo) |
| `SoundMind:FrequencyBins` | `724` | Synonym for ImageHeight |
| `SoundMind:MinFrequency` | `20.0` | Lower bound of frequency range |
| `SoundMind:MaxFrequency` | `16000.0` | Upper bound of frequency range |
| `SoundMind:HopLength` | `441` | Samples per time-axis pixel |
| `SoundMind:BinsPerOctave` | `75` | NSGT BPO |
| `SoundMind:TransformScale` | `varq` | Only present for non-log scales |
| `SoundMind:VarQZones` | `80:4,320:8,…` | VarQScale zone breakpoints (varq only) |

`HopLength` is the ground truth for the time axis: `timestep_ms = hop_length / sample_rate × 1000`. `ImageMetadata.__post_init__` (`format.py:115`) warns if `image_width` differs from `round(duration × sample_rate / hop_length)` by more than 3 pixels — this can happen legitimately because the NSGT determines its own output frame count and it does not always equal exactly `n_samples / hop_length`.

For VarQScale (`TransformScale=varq`), the zone breakpoints are serialised as `"upper_hz:bpo,upper_hz:bpo,…"` and stored in `VarQZones`. The decoder parses this string (`decoder.py:34`) to reconstruct the identical `VarQScale` object, so the frequency spacing matches the encoder exactly.

---

## 7. GPU Backends

### TorchNSGTBackend (`transforms.py:336`)

Runs the NSGT forward and inverse on a PyTorch device. The `nsgt.NSGT` object is still used to compute the scale and window lengths; `TorchNSGT` (`gpu_nsgt.py`) wraps it so that the actual FFT operations run on the GPU via PyTorch's FFT kernels.

On CUDA, `dtype=torch.float32` is used for throughput. On CPU (PyTorch device `cpu`), `dtype=torch.float64` is used to match the reference `NSGTBackend` output exactly.

### VulkanSTFTBackend (`transforms.py:688`)

Falls back to a fixed-hop STFT (not the NSGT) for non-CUDA GPUs (AMD, Intel, Qualcomm Adreno) via `pyopencl` and `pyvkfft`. This sacrifices constant-Q frequency resolution in favour of GPU availability on devices that lack CUDA support.

The STFT uses a Hann window (`_window = np.hanning(n_fft)`) with `stft_hop = n_fft // 4` (75% overlap). Reconstruction uses **Weighted Overlap-Add** (WOLA) — the output of each IFFT frame is multiplied by the analysis window again and summed, then divided by the summed squared window to cancel the overlap gain:

```python
output[start:start+n_fft] += frames[i] * self._window
norm[start:start+n_fft]   += self._w2
...
return (output / norm)[pad:pad + audio_len]
```

The VkFFT operations (`_fft_frames`, `_ifft_frames`) fall back to `numpy.fft` silently on any OpenCL error, so the backend degrades gracefully.

### Backend selection logic (`encoder.py:115`, `decoder.py:228`)

```
use_gpu=True
  → CUDA device available?   → TorchNSGTBackend (CUDA)
  → OpenCL device available? → VulkanSTFTBackend (OpenCL)
  → else                     → NSGTBackend (CPU, with warning)
use_gpu=False (default)      → NSGTBackend (CPU)
```

`get_torch_device()` (`gpu_nsgt.py`) queries PyTorch for a CUDA device. `get_opencl_device()` queries PyOpenCL for a GPU device. If neither is available and `use_gpu=True`, the CPU NSGT backend is used with a printed warning rather than an error.

---

## 8. Frequency Scale Options

The `scale` parameter controls the frequency spacing of NSGT bins.

**`'log'` (default)** — Logarithmic spacing (`nsgt.LogScale`). Bins are equally spaced in log-frequency, producing a constant number of bins per octave across the entire range. This gives uniform visual resolution: one octave looks the same width whether it spans 40–80 Hz or 4000–8000 Hz.

**`'mel'`** — Mel-scale spacing (`nsgt.MelScale`). Bins are spaced according to the mel scale, which approximates the non-linear frequency sensitivity of the cochlea. Mel spacing allocates more bins to low frequencies than `'log'`. This improves perceptual quality for speech analysis but produces a slightly non-uniform image appearance.

**`'oct'`** — Octave-band spacing (`nsgt.OctScale`). Similar to log but aligned to musical octave boundaries.

**`'varq'`** — Variable-Q spacing (`nsgt.VarQScale`). Different bins-per-octave values are used in different frequency zones. The zone breakpoints are defined by `varq_zones`, a sequence of `(upper_freq_hz, bpo)` pairs. Lower zones use fewer bins per octave (shorter windows, better time resolution at bass frequencies); upper zones use more bins per octave (longer windows, better frequency resolution at treble). The zone breakpoints are serialised into the TIFF metadata so the decoder can reconstruct the identical spacing without guessing (Section 6).

For all scales other than `'varq'`, the `bins_per_octave` constructor parameter determines the total bin count via `_estimate_n_bins()`:

```python
octaves = np.log2(self.max_freq / self.min_freq)
return int(np.ceil(octaves * self.bins_per_octave))
```

This estimate is passed to the scale constructor, which then reports the actual bin count back via `self._scale_obj()`. The actual count is what the encoder stores in the metadata and uses for all subsequent calculations — the estimate is never used after construction.
