#!/usr/bin/env python3
"""Generates THIRD_PARTY_NOTICES.md and licenses/ for Sound Mind Studio.

Run it whenever a shipped dependency (Qt, a vcpkg package, JUCE) changes
version, then review and commit the result. It reads:

* vcpkg's installed `share/<port>/copyright` files (full licence texts with
  each project's own copyright lines),
* JUCE's source tree for the licences of the code JUCE embeds (FLAC, Ogg
  Vorbis, zlib),
* Qt's SBOMs (`<Qt>/sbom/*.spdx.json`) for the third-party code Qt bundles,
* standard licence texts from the SPDX licence list (downloaded once into
  licenses/spdx/ and committed, so later runs work offline).

Usage (from the repository root):

    python tools/licenses/generate_notices.py \\
        --vcpkg-share build/release/vcpkg_installed/arm64-windows/share \\
        --juce-modules <vcpkg>/buildtrees/juce/src/<version>/modules \\
        --qt-sbom C:/Qt/6.8.3/msvc2022_arm64/sbom
"""

from __future__ import annotations

import argparse
import json
import shutil
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT_NOTICES = REPO / "THIRD_PARTY_NOTICES.md"
OUT_LICENSES = REPO / "licenses"
SPDX_DIR = OUT_LICENSES / "spdx"
SPDX_URL = "https://raw.githubusercontent.com/spdx/license-list-data/main/text/{}.txt"
SOURCES_URL = "https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1"

QT_VERSION = "6.8.3"
QT_MODULES = ["qtbase", "qtsvg", "qtimageformats"]
# Qt SBOM components that never reach the shipped program: tests, the SQL
# plugin, the Network module (not deployed), and Linux/build-only files.
QT_EXCLUDED_PREFIXES = ("Test", "TestInternals", "QSQLite", "Network_", "Platform_Attribution")


@dataclass
class Component:
    name: str
    version: str
    license: str
    use: str
    homepage: str
    licence_files: list[str] = field(default_factory=list)  # paths under licenses/
    note: str = ""


def spdx_text(identifier: str) -> str:
    """The SPDX standard text for `identifier`, cached in licenses/spdx/."""
    SPDX_DIR.mkdir(parents=True, exist_ok=True)
    path = SPDX_DIR / f"{identifier}.txt"
    if not path.exists():
        with urllib.request.urlopen(SPDX_URL.format(identifier)) as response:
            path.write_bytes(response.read())
    return f"spdx/{identifier}.txt"


def copy_licence(source: Path, name: str) -> str:
    OUT_LICENSES.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, OUT_LICENSES / name)
    return name


def vcpkg_components(share: Path) -> list[Component]:
    def port(port_name: str, name: str, version: str, licence: str, use: str, homepage: str, note: str = ""):
        return Component(name, version, licence, use, homepage,
                         [copy_licence(share / port_name / "copyright", f"{port_name}.txt")], note)

    def version_of(port_name: str) -> str:
        data = json.loads((share / port_name / "vcpkg.spdx.json").read_text(encoding="utf-8"))
        for package in data["packages"]:
            info = package.get("versionInfo", "")
            if info and len(info) < 40:  # skip the source-hash entries
                return info.split("#")[0]
        return "unknown"

    return [
        port("ffmpeg", "FFmpeg (libavcodec, libavformat, libavutil, libswresample, libswscale)", version_of("ffmpeg"),
             "LGPL-2.1-or-later", "Video export and MP3 encoding; separate DLLs you may replace",
             "https://ffmpeg.org",
             f"Built without GPL or non-free components. Source: {SOURCES_URL}"),
        port("mp3lame", "LAME", version_of("mp3lame"), "LGPL-2.0-only",
             "MP3 encoding, built into the FFmpeg libavcodec DLL", "https://lame.sourceforge.io",
             f"Source: {SOURCES_URL}"),
        port("pocketfft", "PocketFFT", version_of("pocketfft"), "BSD-3-Clause", "Fast Fourier transforms",
             "https://gitlab.mpcdf.mpg.de/mtr/pocketfft"),
        port("nlohmann-json", "JSON for Modern C++ (nlohmann/json)", version_of("nlohmann-json"), "MIT",
             "Project and settings files", "https://github.com/nlohmann/json"),
        port("eigen3", "Eigen", version_of("eigen3"), "MPL-2.0", "Linear algebra (header-only, unmodified)",
             "https://eigen.tuxfamily.org",
             f"Source code of the Eigen version used: {SOURCES_URL}"),
    ]


def juce_components(share: Path, modules: Path) -> list[Component]:
    juce = Component(
        "JUCE (juce_core, juce_events, juce_audio_basics, juce_audio_devices, juce_audio_formats)",
        "8.0.7", "JUCE 8 End User Licence Agreement (Starter tier)",
        "Audio devices, audio file reading and FLAC/Ogg export; linked into the program",
        "https://juce.com",
        [copy_licence(share / "juce" / "copyright", "juce.txt")],
        "Used under the JUCE 8 licence, not the AGPLv3: https://juce.com/legal/juce-8-licence/")
    flac = Component("FLAC (as embedded in JUCE)", "1.4.3", "BSD-3-Clause", "FLAC import and export",
                     "https://xiph.org/flac/",
                     [copy_licence(modules / "juce_audio_formats/codecs/flac/Flac Licence.txt", "flac.txt"),
                      spdx_text("BSD-3-Clause")])
    vorbis = Component("Ogg and Vorbis (as embedded in JUCE)", "libvorbis 1.3.7", "BSD-3-Clause",
                       "Ogg Vorbis import and export", "https://xiph.org/vorbis/",
                       [copy_licence(modules / "juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt",
                                     "ogg-vorbis.txt")])
    zlib = Component("zlib (as embedded in JUCE)", "1.3.1", "Zlib", "Compression inside JUCE", "https://zlib.net",
                     [copy_licence(modules / "juce_core/zip/zlib/README", "zlib-juce.txt")])
    return [juce, flac, vorbis, zlib]


def qt_components(sbom_dir: Path) -> tuple[Component, list[tuple[str, str, str, str]]]:
    qt = Component(
        f"Qt {QT_VERSION} (Qt Core, Gui, Widgets, Svg; Windows platform, style, icon-engine and image-format plugins)",
        QT_VERSION, "LGPL-3.0-only", "The user interface; separate DLLs you may replace", "https://www.qt.io",
        [spdx_text("LGPL-3.0-only"), spdx_text("GPL-3.0-only")],
        f"Used under the GNU LGPL version 3. Source: {SOURCES_URL}")
    bundled: list[tuple[str, str, str, str]] = []
    seen: set[str] = set()
    for module in QT_MODULES:
        path = sbom_dir / f"{module}-{QT_VERSION}.spdx.json"
        if not path.exists():
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        extracted = {info["licenseId"]: info for info in data.get("hasExtractedLicensingInfos", [])}
        for package in data["packages"]:
            spdx_id = package.get("SPDXID", "")
            name = package.get("name", "")
            if "ThirdParty" not in spdx_id and "Attribution" not in name and not name.startswith("Bundled"):
                continue
            if name.startswith(QT_EXCLUDED_PREFIXES):
                continue
            licence = package.get("licenseConcluded") or package.get("licenseDeclared") or "NOASSERTION"
            pretty = name.split("_Attribution_")[-1].replace("Bundled", "")
            key = (pretty.lower(), licence)
            if key in seen:
                continue
            seen.add(key)
            copyright_text = (package.get("copyrightText") or "").replace("\r", "").strip()
            bundled.append((pretty, package.get("versionInfo", ""), licence, copyright_text))
            for part in licence.replace("(", " ").replace(")", " ").split():
                if part in ("AND", "OR", "WITH"):
                    continue
                if part.startswith("LicenseRef-"):
                    info = extracted.get(part)
                    if info:
                        OUT_LICENSES.joinpath("spdx").mkdir(parents=True, exist_ok=True)
                        (SPDX_DIR / f"{part}.txt").write_text(info.get("extractedText", ""), encoding="utf-8")
                else:
                    spdx_text(part)
    return qt, bundled


def write_notices(components: list[Component], qt: Component, qt_bundled) -> None:
    lines = [
        "# Third-Party Notices",
        "",
        "Sound Mind Studio is copyright © 2026 Andreas Lundberg and is licensed under the terms in "
        "[LICENSE.md](LICENSE.md). It includes the third-party software listed below, each under its own "
        "licence. The full licence texts are in the [`licenses/`](licenses/) folder that ships with the program.",
        "",
        "Nothing in Sound Mind Studio's own licence restricts your rights under these licences.",
        "",
        "## Replacing the LGPL libraries",
        "",
        "Qt and FFmpeg (with LAME inside it) are shipped as separate DLLs next to `sound-mind-studio.exe` "
        "(`Qt6*.dll` and the Qt plugin folders; `avcodec-*.dll`, `avformat-*.dll`, `avutil-*.dll`, "
        "`swresample-*.dll`, `swscale-*.dll`). You may replace them with your own builds of the same versions "
        "(or compatible ones) by overwriting those files. You may modify those libraries and reverse-engineer "
        "the program as far as needed to debug such modifications.",
        "",
        f"The complete source code of the exact Qt, FFmpeg, LAME and Eigen versions used, with the build "
        f"scripts and patches applied, is available from {SOURCES_URL}.",
        "",
        "## Components",
        "",
        "| Component | Version | Licence | Used for |",
        "|---|---|---|---|",
    ]
    for component in [qt] + components:
        lines.append(f"| {component.name} | {component.version} | {component.license} | {component.use} |")
    lines.append("")
    for component in [qt] + components:
        lines += [f"### {component.name}", "", f"- Version: {component.version}",
                  f"- Licence: {component.license}", f"- Website: {component.homepage}"]
        if component.note:
            lines.append(f"- {component.note}")
        files = ", ".join(f"[`licenses/{name}`](licenses/{name})" for name in component.licence_files)
        if files:
            lines.append(f"- Licence text: {files}")
        lines.append("")
    lines += [
        "## Third-party code inside Qt",
        "",
        "Qt bundles the following components (from Qt's own software bills of materials). Their licence texts are "
        "in [`licenses/spdx/`](licenses/spdx/), named by SPDX identifier.",
        "",
        "| Component | Version | Licence | Copyright |",
        "|---|---|---|---|",
    ]
    for name, version, licence, copyright_text in qt_bundled:
        safe = copyright_text.replace("|", "\\|").replace("\n", "<br>")
        lines.append(f"| {name} | {version} | {licence} | {safe} |")
    lines += [
        "",
        "Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.",
        "",
        "## Other",
        "",
        "- **Microsoft Visual C++ Redistributable**: Sound Mind Studio needs the Microsoft Visual C++ runtime; "
        "where an installer for it is included, it is distributed under Microsoft's own licence terms.",
        "- **The bundled user guide** is generated with Doxygen; its JavaScript files are © Dimitri van Heesch "
        "and are licensed under the MIT licence (see the notice at the top of each file).",
        "",
    ]
    OUT_NOTICES.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--vcpkg-share", type=Path, required=True)
    parser.add_argument("--juce-modules", type=Path, required=True)
    parser.add_argument("--qt-sbom", type=Path, required=True)
    args = parser.parse_args()

    components = vcpkg_components(args.vcpkg_share) + juce_components(args.vcpkg_share, args.juce_modules)
    spdx_text("MIT")  # the Doxygen JavaScript
    qt, qt_bundled = qt_components(args.qt_sbom)
    write_notices(components, qt, qt_bundled)
    print(f"Wrote {OUT_NOTICES.relative_to(REPO)} and {OUT_LICENSES.relative_to(REPO)}/")


if __name__ == "__main__":
    main()
