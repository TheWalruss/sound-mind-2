# Publishing to the Microsoft Store

How Sound Mind Studio gets from a tagged release to the Microsoft Store, added in `v0.1.42.2` (architecture
Decision #261). The Store signs every package it distributes. That gives users an installer with no
"unknown publisher" warning, and no certificate to buy.

## What the build produces

- **`cmake --build build/release --target msix`** builds one architecture's package locally:
  `build/release/msix/sound-mind-studio-v<version>-windows-<arch>.msix`, plus the unpacked layout next to it
  (`msix-layout-<arch>/`).
- **The release workflow** (`.github/workflows/release.yml`, on a `v*` tag):
  - builds both architectures' packages;
  - bundles them into `sound-mind-studio-<tag>.msixbundle`;
  - keeps the bundle as the workflow artifact **`sound-mind-studio-msix-store-upload`** for 90 days.

  The bundle isn't attached to the GitHub release. It's unsigned, so it can't be installed outside the Store.

Package versions map `X.Y.Z.W` to `X.Y.(Z*1000+W).0`, for example `0.1.42.2` becomes `0.1.42002.0`. The Store
requires the last part to be 0, and each part must be at most 65535.

## One-time setup

1. **Register in Partner Center** as an individual developer: [partner.microsoft.com](https://partner.microsoft.com/dashboard)
   → *Windows & Xbox*. It asks for identity verification (ID and a selfie). Individual registration was free when
   this was written.
2. **Reserve the app name** "Sound Mind Studio" (*Apps and games → New product → MSIX or PWA app*).
3. **Copy the package identity.** Open *Product management → Product identity* and copy:
   - `Package/Identity/Name`
   - `Package/Identity/Publisher` (`CN=...`)
   - `Package/Properties/PublisherDisplayName`
4. **Set them as repository variables.** On GitHub, go to *Settings → Secrets and variables → Actions →
   Variables* and create these (they aren't secrets):
   - `MSIX_IDENTITY_NAME`
   - `MSIX_PUBLISHER`
   - `MSIX_PUBLISHER_DISPLAY_NAME`

   Until they're set, packages use the CMake defaults (`AndreasLundberg.SoundMindStudio`, `CN=Andreas Lundberg`).
   Those are fine for local testing, but Partner Center rejects them. For a local build with the real identity,
   pass `-DSOUND_MIND_MSIX_IDENTITY_NAME=...` and the other two the same way.
5. **Publish a privacy policy.** The app asks for the microphone (Recording, Loop Mode), and the Store requires
   a privacy policy URL for that. A short page will do, saying the app works offline and collects nothing
   (the same statement as `BETA.md`).

## Each submission

1. Tag a release (`vX.Y.Z.W`). When the workflow finishes, download the `sound-mind-studio-msix-store-upload`
   artifact from the run's page and unzip it to get the `.msixbundle`.
2. In Partner Center, start a submission:
   - **Packages:** upload the `.msixbundle`.
   - **Restricted capabilities:** `runFullTrust` needs a justification. Use: "a desktop audio/graphics
     application (Qt/JUCE) packaged as MSIX; needs full trust for audio device access and the file system".
   - **Properties:** category *Music*, plus the privacy policy URL.
   - **Store listing:** description, at least one screenshot, the Store logo (Partner Center accepts
     `packaging/msix/Assets/StoreLogo.png`, or a larger one), and age ratings (questionnaire).
   - **Pricing and availability:** free. For the beta, set **Visibility** to a *private audience* (a list of
     testers' Microsoft account emails), so only they can find and install it. Switch to public at release.
3. Submit. Certification usually takes 1-3 business days, longer for a first submission.

## Testing a package locally

An unsigned package can't be installed, but its layout can be registered on a PC with **Developer Mode**
switched on (*Settings → System → For developers*):

```powershell
cmake --build build/release --target msix
Add-AppxPackage -Register build\release\msix\msix-layout-arm64\AppxManifest.xml
# launch it from the Start menu; afterwards:
Get-AppxPackage *SoundMindStudio* | Remove-AppxPackage
```

This runs the app exactly as the Store version will, including its file association and the microphone
permission prompt. Use `msix-layout-x64` on an x64 build.

## Updating the package

- **Tile images:** `packaging/msix/Assets/`. Regenerate them with `packaging/msix/make_assets.ps1` when the logo
  changes.
- **Manifest:** `packaging/msix/AppxManifest.xml.in` (capabilities, file association, minimum Windows version).
- **Packaging steps:** `packaging/msix/make_msix.cmake`.
