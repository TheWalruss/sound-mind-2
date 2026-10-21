# Sound Mind Studio Beta

> **Status: in preparation.** The public beta opens once the remaining pre-beta work is done (saving a
> project's full editing history, macro recording, and a final readiness check). Until then, releases are
> development builds.

Thank you for helping test Sound Mind Studio - an app that treats sound as a picture: import audio and see it
as a spectrogram, paint and filter it like an image, and hear the result.

## Getting it

Download the latest release from the [Releases page](https://github.com/TheWalruss/sound-mind-2/releases):

- `...-windows-x64.msi` or `.zip` for most Windows PCs (Intel or AMD);
- `...-windows-arm64.msi` or `.zip` for Arm PCs (Snapdragon).

The `.msi` installs it with a Start menu shortcut; the `.zip` runs from any folder (run the included
`vc_redist` installer first if it doesn't start). Windows may warn that the app is from an unknown publisher,
because it isn't code-signed yet - choose **More info → Run anyway**.

## What to test

- Start with [`QUICKSTART.md`](QUICKSTART.md), then explore with [`USER_GUIDE.md`](USER_GUIDE.md) (also under
  Help → User Guide).
- Try your own audio and images: import, paint, filter, layer, play back, record, loop, analyze and export.
- Tell us what's confusing, slow, or missing - not only what's broken.

## Reporting

[Open an issue](https://github.com/TheWalruss/sound-mind-2/issues/new/choose) and pick **Bug report** or
**Feedback or idea**. Include the version (Help → About) and, for bugs, the steps that lead to the problem.
Issues are public - only attach files you're happy to share.

## Good to know

- **It's pre-release software.** Keep backups of projects that matter. A later beta may not open project files
  saved by an earlier one.
- **Nothing is collected.** The app works entirely offline and sends no data anywhere.
- **Licence:** free to use, including for commercial work; what you make is yours. You may not pass the
  program on to others - point them to the Releases page instead. See [`LICENSE.md`](LICENSE.md).

## Known issues

- The app isn't code-signed yet, so Windows SmartScreen warns about an unknown publisher.
- Project files from earlier test builds may not open in later ones.
- See "What's Not Here Yet" in the [user guide](USER_GUIDE.md) for features that aren't available yet.
