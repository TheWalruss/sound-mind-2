# Sound Mind TIFF — Format Specification

---

## Overview

A Sound Mind TIFF is a standard TIFF file that stores a stereo audio spectrogram as four 16-bit grayscale image pages. The format is self-describing: all parameters needed to decode the image back to audio are stored in a metadata block inside the file itself. No sidecar files or external configuration are required.

The spectrogram representation used is the **Non-Stationary Gabor Transform (NSGT)**. The NSGT is a perfectly-reconstructing transform: encoding and then decoding without modification is mathematically lossless. It stores the raw NSGT output — log-spaced frequency bins, variable image height — with mel-scale display handled in the viewer rather than baked into the codec.

**Design goals:**

- TIFF compatibility — any compliant TIFF reader can open the file (pages appear as four separate grayscale images)
- Self-describing — metadata embedded in a standard TIFF tag
- Lossless round-trip — encoding and decoding the same audio with the same parameters reconstructs the original samples exactly
- Editable — every pixel is meaningful; editing amplitude pixels changes loudness, editing phase pixels changes timbre/phase
- Portable — relative file sizes remain modest (a 4-minute song at default settings is ≈ 60 MB)

---

## 1. TIFF Container Compliance

Sound Mind TIFFs are fully TIFF 6.0-compliant. A compliant writer MUST:

- Write a valid TIFF 6.0 header (byte order mark, magic number `42`, IFD offset)
- Store each of the four spectrogram pages as a separate IFD entry in the same file
- Set `BitsPerSample = 16` and `SamplesPerPixel = 1` for each page
- Set `Compression = 5` (LZW) for all pages
- Set `PhotometricInterpretation = 1` (BlackIsZero / MinIsBlack) for all pages
- Store the Sound Mind metadata block in the `ImageDescription` tag (tag `270`) of the **first page only**

A compliant reader MUST:

- Locate the `ImageDescription` tag in the first IFD and parse the key=value metadata
- Use only the metadata fields defined in Section 5 to reconstruct audio parameters
- Treat the `image_width` and `image_height` metadata fields as authoritative if they differ from the TIFF dimensions (they will match in well-formed files)

---

## 2. Image Geometry

| Axis | Dimension | Meaning |
|------|-----------|---------|
| Height (rows) | Variable — actual NSGT bin count | Frequency axis, row 0 = highest frequency |
| Width (columns) | Variable | Time axis, left = start, right = end |

### Frequency Axis

Rows span **20 Hz to 16 000 Hz** on a **log scale** (V2.0), using the actual NSGT frequency bin count as the image height. With the default `bins_per_octave = 150`, this yields ≈ 1447 rows (150 × log₂(16000/20) ≈ 1447). Row 0 (top) corresponds to ~16 kHz; the last row (bottom) corresponds to ~20 Hz.

> **Display note:** File stores log-spaced bins. Mel-scale display is applied by the viewer as a Y-axis remapping (LUT). Log-scale and linear-scale display modes are also available in the viewer.

The image height is always stored in the `SoundMind:ImageHeight` metadata field and must be used when reading the file.

### Time Axis

The width of the image is determined by the audio duration and the hop length:

```
image_width = ceil(num_samples / hop_length)
hop_length  = round(timestep_ms * sample_rate / 1000)
```

Default `timestep_ms = 23.0` ms at `sample_rate = 44100` Hz gives `hop_length = 1015` samples and ≈ 23 ms per pixel.

---

## 3. Page Layout

Each Sound Mind TIFF contains exactly **four pages** (IFDs), all of identical dimensions:

| Page index | Content | Range | Encoding |
|------------|---------|-------|----------|
| 0 | Left channel amplitude | 0 – 65535 | See §4.1 |
| 1 | Right channel amplitude | 0 – 65535 | See §4.1 |
| 2 | Left channel phase | 0 – 65535 | See §4.2 |
| 3 | Right channel phase | 0 – 65535 | See §4.2 |

All four pages share the same `image_width` and `image_height` (1440). All four pages use `uint16` (unsigned 16-bit integer) pixel values with no floating-point conversion in the file.

---

## 4. Pixel Encoding

### 4.1 Amplitude Pages (Pages 0 and 1)

Amplitude is stored as **A-weighted decibels mapped linearly to uint16**:

```
pixel = round(clamp((amplitude_dB + 96.0) / 96.0, 0.0, 1.0) × 65535)
```

Inverse (decoding pixel → amplitude magnitude):

```
amplitude_dB = (pixel / 65535) × 96.0 − 96.0
```

The A-weighted range is **−96 dB to 0 dB**:

| Pixel value | dB | Perceptual meaning |
|-------------|----|--------------------|
| 0 | −96 dB | Silence / noise floor |
| 32768 | −48 dB | −48 dB (half full scale) |
| 65535 | 0 dB | Full scale (maximum) |

The amplitude stored is the **magnitude of the complex NSGT coefficient** for that time-frequency bin, after dB conversion.

Values above 65535 (i.e. above 0 dBFS) cannot be stored in the file. If the decoded audio peaks above 0 dBFS after pixel painting, normalise or reduce amplitude pixels in pages 0 and 1 before decoding.

### 4.2 Phase Pages (Pages 2 and 3)

Phase is stored as the **phase angle in radians mapped linearly from [−π, +π] to [0, 65535]**:

```
pixel = round(clamp((phase + π) / (2π), 0.0, 1.0) × 65535)
```

Inverse:

```
phase = (pixel / 65535) × 2π − π
```

| Pixel value | Phase (radians) |
|-------------|-----------------|
| 0 | −π |
| 32768 | ≈ 0 (mid-point) |
| 65535 | +π |

Phase pages are used by the NSGT decoder to reconstruct the complex coefficient and therefore the waveform. In version 1.0 files (two pages, amplitude only) the decoder uses zero-phase reconstruction.

**Editing note:** Phase values are periodic. Painting a smooth horizontal gradient on a phase page creates a frequency shift (pitch change). Random noise on a phase page creates a "smeared" timbre. Setting all phase pixels to 32768 (≈ 0 rad) produces cosine-phase reconstruction.

---

## 5. Metadata Block

The Sound Mind metadata block is stored as the UTF-8 value of the TIFF `ImageDescription` tag (tag `270`) in the **first IFD only**. It uses a simple `key=value\n` text format.

### 5.1 Required Fields

| Key | Type | Description |
|-----|------|-------------|
| `SoundMind:Version` | hex string | Format version. `0200` = V2.0, `0101` = V1.1, `0100` = V1.0 |
| `SoundMind:SampleRate` | integer | Audio sample rate in Hz (e.g. `44100`) |
| `SoundMind:HopLength` | integer | NSGT hop length in samples (time-axis step size) |
| `SoundMind:ImageWidth` | integer | Width of the spectrogram in pixels (= number of time frames) |
| `SoundMind:ImageHeight` | integer | Height of the spectrogram in pixels (actual NSGT bin count for V2.0; `1440` for V1.x) |
| `SoundMind:Duration` | float | Audio duration in seconds |
| `SoundMind:TiffVariant` | string | `stripped` or `tiled` (see §6) |
| `SoundMind:EncodingScheme` | string | `spectrogram_stereo64` (V2.0 / V1.1) |
| `SoundMind:BinsPerOctave` | integer | **Required for V2.0.** NSGT bins per octave (default `150`). Used to reconstruct the backend. |

### 5.2 Optional / Informational Fields

| Key | Type | Description |
|-----|------|-------------|
| `SoundMind:TileSize` | integer | Tile width/height in pixels — present for `tiled` variant only |
| `SoundMind:TransformBackend` | string | Backend used during encoding: `NSGTBackend`, `TorchNSGT`, `VulkanSTFT` |
| `SoundMind:MinFrequency` | float | Lowest frequency in Hz (default `20.0`) |
| `SoundMind:MaxFrequency` | float | Highest frequency in Hz (default `16000.0`) |

### 5.3 Example Block (V2.0)

```
SoundMind:Version=0200
SoundMind:SampleRate=44100
SoundMind:Duration=25.170000
SoundMind:ImageWidth=1094
SoundMind:ImageHeight=1447
SoundMind:EncodingScheme=spectrogram_stereo64
SoundMind:BitsPerChannel=16
SoundMind:TiffVariant=stripped
SoundMind:TileSize=256
SoundMind:TransformBackend=NSGTBackend
SoundMind:Channels=2
SoundMind:FrequencyBins=1447
SoundMind:MinFrequency=20.0
SoundMind:MaxFrequency=16000.0
SoundMind:TimePerPixel=21
SoundMind:HopLength=1015
SoundMind:BinsPerOctave=150
```

### 5.4 Parsing Rules

- Lines are separated by `\n` (LF). `\r\n` (CRLF) is also accepted.
- Leading and trailing whitespace on each line is ignored.
- Blank lines and lines that do not contain `=` are ignored.
- Keys and values are case-sensitive.
- Unknown keys MUST be silently ignored (forward compatibility).
- Missing required fields should cause a parse error with a descriptive message.

---

## 6. Storage Variants

### 6.1 Stripped TIFF (`tiff_variant = stripped`)

Pages are divided into horizontal strips. Each strip is LZW-compressed independently. This variant is recommended for:

- Short clips (under ≈ 30 seconds)
- Instrument patches and samples
- Files that will be read sequentially from start to finish

Strip height is not fixed; the writer may use any strip height. Typical writers use the full height as a single strip per page.

### 6.2 Tiled TIFF (`tiff_variant = tiled`)

Pages are divided into rectangular tiles (default 256 × 256 px). Each tile is LZW-compressed independently. This variant enables random access — a decoder can seek to any time position and decompress only the relevant tiles, without reading or decompressing the whole file. Recommended for:

- Full-length songs (over ≈ 30 seconds)
- Studio projects where the spectrogram view must scroll smoothly

The `tile_size` metadata field records the tile width and height (always square). Standard value is `256`.

---

## 7. Multi-Snippet Files

Audio longer than a threshold (default 30 seconds) is split into contiguous **snippet files** named with a zero-padded index:

```
basename_0000.tiff
basename_0001.tiff
basename_0002.tiff
…
```

Each snippet is a fully self-describing Sound Mind TIFF. The snippets are independent: any single snippet can be decoded without the others. The last snippet will typically be shorter than the others.

There is no multi-snippet index file in format version 1.1. Applications that need to open a full song from snippets should discover the sibling files by enumerating `basename_NNNN.tiff` in the same directory.

---

## 8. Version History

| Version | `codec_version` hex | Pages | Frequency axis | Default bpo | Default hop |
|---------|---------------------|-------|----------------|-------------|-------------|
| 1.0 | `0100` | 2 (amplitude only) | Mel-spaced, 1440 rows fixed | 36 | 46 ms |
| 1.1 | `0101` | 4 (amplitude + phase) | Mel-spaced, 1440 rows fixed | 36 | 46 ms |
| 2.0 | `0200` | 4 (amplitude + phase) | Log-spaced, actual bin count | 150 | 23 ms |

**Migration notes:**
- A V2.0 decoder MUST accept V1.1 and V1.0 files (identified by `SoundMind:Version`).
- V1.0 files with two pages use zero-phase reconstruction.
- V1.x files use mel interpolation in the decoder; V2.0 files do not.
- V2.0 adds the required `SoundMind:BinsPerOctave` metadata field for NSGT reconstruction.
- V2.0 image height is NOT fixed; always read it from `SoundMind:ImageHeight`.

---

## 9. Interoperability Notes

### Opening in standard image software

Any TIFF-capable application (GIMP, Photoshop, ImageMagick, tifffile/Python, etc.) can open a Sound Mind TIFF. The file will appear as four separate 16-bit grayscale images. The four pages will look like abstract textures or noise to a human observer, but their pixel values are meaningful spectrogram data.

**GIMP:** Open as a multi-layer TIFF. Pages appear as layers. Export back as TIFF with 16-bit depth to preserve pixel precision.

**Python (tifffile):**
```python
import tifffile
pages = tifffile.imread("my_file.tiff")   # shape: (4, 1440, width), dtype: uint16
left_amp   = pages[0]   # page 0
right_amp  = pages[1]   # page 1
left_phase = pages[2]   # page 2
right_phase = pages[3]  # page 3
```

### Amplitude and phase as a complex spectrum

Each `(row, col)` position represents one NSGT time-frequency bin. The full complex coefficient can be reconstructed as:

```python
import numpy as np

amp_norm   = left_amp.astype(np.float64)   / 65535.0  # [0, 1]
phase_norm = left_phase.astype(np.float64) / 65535.0  # [0, 1]

amp_db     = amp_norm * 96.0 - 96.0          # dB in [-96, 0]
amplitude  = 10.0 ** (amp_db / 20.0)         # linear magnitude
phase_rad  = phase_norm * 2.0 * np.pi - np.pi  # radians in [-π, +π]

coefficient = amplitude * np.exp(1j * phase_rad)  # complex NSGT coefficient
```

### Extension — custom channel mappings

When a Sound Mind TIFF is created from an RGB image (rather than audio), the four pages may contain arbitrary image data rather than the canonical amplitude/phase layout. The `encoding_scheme` field in the metadata MUST be set to a non-standard value (e.g. `image_rgb_default`) to indicate this. Sound Mind Studio stores the original channel mapping in the layer source provenance record inside the project file.

Third-party tools MUST NOT assume the canonical amplitude/phase layout unless `encoding_scheme = spectrogram_stereo32`.

---

## 10. Limitations and Known Issues

| Limitation | Detail |
|------------|--------|
| Fixed height | Image height is always 1440 px in version 1.1. Future versions may relax this. |
| No multichannel beyond stereo | The four pages are defined for stereo only. Surround or ambisonic representations are not defined. |
| Phase page precision | Phase wraps at ±π. Very small differences near the wrap point may round-trip with ≤ 1 pixel error (< 0.01% of the phase range). |


---

*Copyright © 2024-2026 TheWalruss / Sound Mind Project. All rights reserved.*

*This specification document is provided for interoperability purposes. Implementors may read Sound Mind TIFF files using this specification. Writing files that claim the `format=sound_mind` identifier requires compliance with all normative requirements in this document.*
