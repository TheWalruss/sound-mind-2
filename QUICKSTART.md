# Quickstart

Five minutes from launching Sound Mind Studio to a saved project with your
first imported sound on screen. For everything this only touches on, see
[`USER_GUIDE.md`](USER_GUIDE.md).

## 1. Create a project

Launch the Studio - you'll land on the **Sound Mind Studio** start screen.
Click **New Project...** and fill in:

- **Name** and **Save Folder** - where the project is saved.
- **Duration** - roughly how much time the canvas should hold. You can
  import audio longer than this; it's split into duration-length pieces
  automatically (see the User Guide's Importing Audio section).

Everything else is under **Advanced** and fine to leave at its defaults for
now. Click **OK** - the canvas opens, showing an empty dark-gray rectangle
(your project's Background layer, with nothing imported into it yet).

## 2. Import something

**File → Import Audio...** or **File → Import Image...** - or just drag a
file (WAV/MP3/FLAC/Ogg/AIFF/M4A/Opus audio; PNG/JPG/BMP/TGA/WebP images)
straight onto the window.
For an image, you'll be asked how to fit it to the canvas; **Rescale to fit
project** is a safe default. The imported file appears in the canvas as a
spectrogram, and as a new row in the **Layers** panel on the right.

## 3. Play it back

Open the **Playback** panel (one of the toolbar buttons, if it isn't
already visible) and hit **Play**. You're hearing the topmost
layer that has content - toggle a layer's visibility (the ● button in the
Layers panel) to change which one that is.

## 4. Save

**Ctrl+S**, or **File → Save Project**. Your project is a `.smproj` file
plus a same-named folder next to it holding the actual imported/rendered
data - keep them together.

## What next?

- Add more layers (more imports, or a recording via the **Record** panel).
- Click a layer's name in the Layers panel to select it and reveal its
  full controls, including translation/rescale for nudging its timing
  once you have more than one layer.
- Try a different **Blend Mode** on a selected layer's own row once you
  have more than one layer - Multiply, Screen, and the rest combine
  layers differently than the default Normal mix.
- Read [`USER_GUIDE.md`](USER_GUIDE.md) for Loop Mode, Pooling, exporting
  audio/video, and every panel's full set of controls.
