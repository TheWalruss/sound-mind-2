# Third-Party Notices

Sound Mind Studio is copyright © 2026 Andreas Lundberg and is licensed under the terms in [LICENSE.md](LICENSE.md). It includes the third-party software listed below, each under its own licence. The full licence texts are in the [`licenses/`](licenses/) folder that ships with the program.

Nothing in Sound Mind Studio's own licence restricts your rights under these licences.

## Replacing the LGPL libraries

Qt and FFmpeg (with LAME inside it) are shipped as separate DLLs next to `sound-mind-studio.exe` (`Qt6*.dll` and the Qt plugin folders; `avcodec-*.dll`, `avformat-*.dll`, `avutil-*.dll`, `swresample-*.dll`, `swscale-*.dll`). You may replace them with your own builds of the same versions (or compatible ones) by overwriting those files. You may modify those libraries and reverse-engineer the program as far as needed to debug such modifications.

The complete source code of the exact Qt, FFmpeg, LAME and Eigen versions used, with the build scripts and patches applied, is available from https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1.

## Components

| Component | Version | Licence | Used for |
|---|---|---|---|
| Qt 6.8.3 (Qt Core, Gui, Widgets, Svg; Windows platform, style, icon-engine and image-format plugins) | 6.8.3 | LGPL-3.0-only | The user interface; separate DLLs you may replace |
| FFmpeg (libavcodec, libavformat, libavutil, libswresample, libswscale) | 9.0.1 | LGPL-2.1-or-later | Video export and MP3 encoding; separate DLLs you may replace |
| LAME | 3.100 | LGPL-2.0-only | MP3 encoding, built into the FFmpeg libavcodec DLL |
| PocketFFT | 2024-11-30 | BSD-3-Clause | Fast Fourier transforms |
| JSON for Modern C++ (nlohmann/json) | 3.12.0 | MIT | Project and settings files |
| Eigen | 5.0.1 | MPL-2.0 | Linear algebra (header-only, unmodified) |
| JUCE (juce_core, juce_events, juce_audio_basics, juce_audio_devices, juce_audio_formats) | 8.0.7 | JUCE 8 End User Licence Agreement (Starter tier) | Audio devices, audio file reading and FLAC/Ogg export; linked into the program |
| FLAC (as embedded in JUCE) | 1.4.3 | BSD-3-Clause | FLAC import and export |
| Ogg and Vorbis (as embedded in JUCE) | libvorbis 1.3.7 | BSD-3-Clause | Ogg Vorbis import and export |
| zlib (as embedded in JUCE) | 1.3.1 | Zlib | Compression inside JUCE |

### Qt 6.8.3 (Qt Core, Gui, Widgets, Svg; Windows platform, style, icon-engine and image-format plugins)

- Version: 6.8.3
- Licence: LGPL-3.0-only
- Website: https://www.qt.io
- Used under the GNU LGPL version 3. Source: https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1
- Licence text: [`licenses/spdx/LGPL-3.0-only.txt`](licenses/spdx/LGPL-3.0-only.txt), [`licenses/spdx/GPL-3.0-only.txt`](licenses/spdx/GPL-3.0-only.txt)

### FFmpeg (libavcodec, libavformat, libavutil, libswresample, libswscale)

- Version: 9.0.1
- Licence: LGPL-2.1-or-later
- Website: https://ffmpeg.org
- Built without GPL or non-free components. Source: https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1
- Licence text: [`licenses/ffmpeg.txt`](licenses/ffmpeg.txt)

### LAME

- Version: 3.100
- Licence: LGPL-2.0-only
- Website: https://lame.sourceforge.io
- Source: https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1
- Licence text: [`licenses/mp3lame.txt`](licenses/mp3lame.txt)

### PocketFFT

- Version: 2024-11-30
- Licence: BSD-3-Clause
- Website: https://gitlab.mpcdf.mpg.de/mtr/pocketfft
- Licence text: [`licenses/pocketfft.txt`](licenses/pocketfft.txt)

### JSON for Modern C++ (nlohmann/json)

- Version: 3.12.0
- Licence: MIT
- Website: https://github.com/nlohmann/json
- Licence text: [`licenses/nlohmann-json.txt`](licenses/nlohmann-json.txt)

### Eigen

- Version: 5.0.1
- Licence: MPL-2.0
- Website: https://eigen.tuxfamily.org
- Source code of the Eigen version used: https://github.com/TheWalruss/sound-mind-2/releases/tag/third-party-sources-1
- Licence text: [`licenses/eigen3.txt`](licenses/eigen3.txt)

### JUCE (juce_core, juce_events, juce_audio_basics, juce_audio_devices, juce_audio_formats)

- Version: 8.0.7
- Licence: JUCE 8 End User Licence Agreement (Starter tier)
- Website: https://juce.com
- Used under the JUCE 8 licence, not the AGPLv3: https://juce.com/legal/juce-8-licence/
- Licence text: [`licenses/juce.txt`](licenses/juce.txt)

### FLAC (as embedded in JUCE)

- Version: 1.4.3
- Licence: BSD-3-Clause
- Website: https://xiph.org/flac/
- Licence text: [`licenses/flac.txt`](licenses/flac.txt), [`licenses/spdx/BSD-3-Clause.txt`](licenses/spdx/BSD-3-Clause.txt)

### Ogg and Vorbis (as embedded in JUCE)

- Version: libvorbis 1.3.7
- Licence: BSD-3-Clause
- Website: https://xiph.org/vorbis/
- Licence text: [`licenses/ogg-vorbis.txt`](licenses/ogg-vorbis.txt)

### zlib (as embedded in JUCE)

- Version: 1.3.1
- Licence: Zlib
- Website: https://zlib.net
- Licence text: [`licenses/zlib-juce.txt`](licenses/zlib-juce.txt)

## Third-party code inside Qt

Qt bundles the following components (from Qt's own software bills of materials). Their licence texts are in [`licenses/spdx/`](licenses/spdx/), named by SPDX identifier.

| Component | Version | Licence | Copyright |
|---|---|---|---|
| pcre2-sljit | 10.45 | BSD-2-Clause | Copyright (c) 2009-2024 Zoltan Herczeg |
| Pcre2 | 10.45 | LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception | Copyright (c) 1997-2024 University of Cambridge<br>Copyright (c) 2010-2024 Zoltan Herczeg |
| ZLIB | 1.3.1 | Zlib | (C) 1995-2024 Jean-loup Gailly and Mark Adler |
| unicode-character-database | 34 | Unicode-3.0 | Copyright (C) 1991-2022 Unicode, Inc. |
| unicode-cldr | v46.1 | Unicode-3.0 | Copyright (C) 2004-2023 Unicode, Inc. |
| siphash | unknown | CC0-1.0 | Copyright (C) 2012-2014 Jean-Philippe Aumasson<br>Copyright (C) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to><br>Copyright (C) 2016 Intel Corporation |
| blake2 | ed1974ea83433eba7b2d95c5dcd9ac33cb847913 | CC0-1.0 OR Apache-2.0 | Copyright 2012, Samuel Neves <sneves@dei.uc.pt> |
| md4 | unknown | CC0-1.0 | Written by Alexander Peslyak - better known as Solar Designer <solar@openwall.com> - in 2001, and placed in the public domain. There's absolutely no warranty. |
| md5 | unknown | CC0-1.0 | Written by Colin Plumb in 1993, no copyright is claimed. Ian Jackson <ian@chiark.greenend.org.uk>. |
| sha1 | unknown | LicenseRef-SHA1-Public-Domain | Copyright (C) Dominik Reichl <dominik.reichl@t-online.de><br>Copyright (C) 2016 The Qt Company Ltd |
| sha3_endian | 4b9e13ead2c5b5e41ca27c65de4dd69ae0bac228 | BSD-2-Clause | Copyright (c) 1998-2013, Brian Gladman, Worcester, UK. All rights reserved. |
| sha3_keccak | 3.2 | CC0-1.0 | Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche.<br>To the extent possible under law, the implementers have waived all copyright<br>and related or neighboring rights to the source code in this file. |
| rfc6234 | unknown | BSD-3-Clause | Copyright (c) 2011 IETF Trust and the persons identified as authors of the code. |
| tinycbor | 0.6.1 | MIT | Copyright (C) 2015-2021 Intel Corporation |
| doubleconversion | 3.3.0 | BSD-3-Clause | Copyright 2006-2012, the V8 project authors |
| easing | unknown | BSD-3-Clause | Copyright (c) 2001 Robert Penner |
| tika-mimetypes | 5101bc7fb090ed7deffe56837d7633c9485a1e5d | Apache-2.0 | Copyright 2024 The Apache Software Foundation |
| Libpng | 1.6.47 | Libpng AND libpng-2.0 | Copyright (c) 1995-2025 The PNG Reference Library Authors<br>Copyright (c) 2000-2025 Cosmin Truta<br>Copyright (c) 1998-2018 Glenn Randers-Pehrson<br>Copyright (c) 1996-1997 Andreas Dilger<br>Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.<br>Copyright (c) 2000-2017 Simon-Pierre Cadieux<br>Copyright (c) 2000-2017 Eric S. Raymond<br>Copyright (c) 2000-2017 Mans Rullgard<br>Copyright (c) 2000-2017 Gilles Vollant<br>Copyright (c) 2000-2017 James Yu<br>Copyright (c) 2000-2017 Mandar Sahastrabuddhe<br>Copyright (c) 1998-2000 Tom Lane<br>Copyright (c) 1998-2000 Willem van Schaik<br>Copyright (c) 1996-1997 John Bowler<br>Copyright (c) 1996-1997 Kevin Bracey<br>Copyright (c) 1996-1997 Sam Bushell<br>Copyright (c) 1996-1997 Magnus Holmgren<br>Copyright (c) 1996-1997 Greg Roelofs<br>Copyright (c) 1996-1997 Tom Tanner<br>Copyright (c) 1995-1996 Dave Martindale<br>Copyright (c) 1995-1996 Paul Schmidt<br>Copyright (c) 1995-1996 Tim Wegner |
| Libjpeg16bits | 3.1.0 | IJG AND BSD-3-Clause | Copyright (C) 2009-2024 D. R. Commander<br>Copyright (C) 2015, 2020 Google, Inc.<br>Copyright (C) 2019-2020 Arm Limited<br>Copyright (C) 2015-2016, 2018 Matthieu Darbois<br>Copyright (C) 2011-2016 Siarhei Siamashka<br>Copyright (C) 2015 Intel Corporation<br>Copyright (C) 2013-2014 Linaro Limited<br>Copyright (C) 2013-2014 MIPS Technologies, Inc.<br>Copyright (C) 2009, 2012 Pierre Ossman for Cendio AB<br>Copyright (C) 2009-2011 Nokia Corporation and/or its subsidiary(-ies)<br>Copyright (C) 1999-2006 MIYASAKA Masaru<br>Copyright (C) 1999 Ken Murchison<br>Copyright (C) 1991-2020 Thomas G. Lane, Guido Vollbeding |
| Libjpeg12bits | 3.1.0 | IJG AND BSD-3-Clause | Copyright (C) 2009-2024 D. R. Commander<br>Copyright (C) 2015, 2020 Google, Inc.<br>Copyright (C) 2019-2020 Arm Limited<br>Copyright (C) 2015-2016, 2018 Matthieu Darbois<br>Copyright (C) 2011-2016 Siarhei Siamashka<br>Copyright (C) 2015 Intel Corporation<br>Copyright (C) 2013-2014 Linaro Limited<br>Copyright (C) 2013-2014 MIPS Technologies, Inc.<br>Copyright (C) 2009, 2012 Pierre Ossman for Cendio AB<br>Copyright (C) 2009-2011 Nokia Corporation and/or its subsidiary(-ies)<br>Copyright (C) 1999-2006 MIYASAKA Masaru<br>Copyright (C) 1999 Ken Murchison<br>Copyright (C) 1991-2020 Thomas G. Lane, Guido Vollbeding |
| Libjpeg | 3.1.0 | IJG AND BSD-3-Clause | Copyright (C) 2009-2024 D. R. Commander<br>Copyright (C) 2015, 2020 Google, Inc.<br>Copyright (C) 2019-2020 Arm Limited<br>Copyright (C) 2015-2016, 2018 Matthieu Darbois<br>Copyright (C) 2011-2016 Siarhei Siamashka<br>Copyright (C) 2015 Intel Corporation<br>Copyright (C) 2013-2014 Linaro Limited<br>Copyright (C) 2013-2014 MIPS Technologies, Inc.<br>Copyright (C) 2009, 2012 Pierre Ossman for Cendio AB<br>Copyright (C) 2009-2011 Nokia Corporation and/or its subsidiary(-ies)<br>Copyright (C) 1999-2006 MIYASAKA Masaru<br>Copyright (C) 1999 Ken Murchison<br>Copyright (C) 1991-2020 Thomas G. Lane, Guido Vollbeding |
| freetype-zlib | 2.13.3 | Zlib | Copyright (C) 1995-2023 Jean-loup Gailly and Mark Adler |
| freetype-bdf | 2.13.3 | MIT | Copyright (c) 2000 Computing Research Labs, New Mexico State University<br>Copyright (c) 2001-2014 Francesco Zappa Nardelli |
| freetype-pcf | 2.13.3 | MIT AND MIT-open-group | Copyright (c) 2001, 2012 David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2000-2014 Francesco Zappa Nardelli<br>Copyright (c) 1990, 1994, 1998 The Open Group |
| Freetype | 2.13.3 | FTL OR GPL-2.0-only | Copyright (c) 2007-2014 Adobe Systems Incorporated<br>Copyright (c) 2004-2024 Albert Chin-A-Young<br>Copyright (c) 2018-2024 Armin Hasitzka, David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2000 Computing Research Labs, New Mexico State University<br>Copyright (c) 1996-2024 David Turner, Robert Wilhelm, Dominik Röttsches, and Werner Lemberg<br>Copyright (c) 2004-2024 David Turner, Robert Wilhelm, Werner Lemberg and George Williams<br>Copyright (c) 2022-2024 David Turner, Robert Wilhelm, Werner Lemberg, and Moazin Khatti<br>Copyright (c) 2008-2024 David Turner, Robert Wilhelm, Werner Lemberg, and suzuki toshiya<br>Copyright (c) 2003-2024 David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2005-2024 David Turner<br>Copyright (c) 2007-2024 Derek Clegg and Michael Toftdal<br>Copyright (c) 2007 Dmitry Timoshkov for Codeweavers<br>Copyright (c) 2001-2015 Francesco Zappa Nardelli<br>Copyright (c) 2005, 2007, 2008, 2013 George Williams<br>Copyright (c) 2013-2024 Google, Inc. Google Author(s) Behdad Esfahbod and Stuart Gill<br>Copyright (c) 2013-2022 Google, Inc.<br>Copyright (c) 2003 Huw D M Davies for Codeweavers<br>Copyright (c) 2010-2024 Joel Klinghed<br>Copyright (c) 1996-2024 Just van Rossum, David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2003-2024 Masatake YAMATO and Redhat K.K.<br>Copyright (c) 2004-2024 Masatake YAMATO, Redhat K.K, David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2019-2024 Nikhil Ramakrishnan, David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2009-2024 Oran Agra and Mickey Gabel<br>Copyright (c) 2007-2024 Rahul Bhalerao <rahul.bhalerao@redhat.com><br>Copyright (c) 2002-2024 Roberto Alameda<br>Copyright (c) 2015-2024 Werner Lemberg<br>Copyright (c) 2004-2024 suzuki toshiya, Masatake YAMATO, Red Hat K.K., David Turner, Robert Wilhelm, and Werner Lemberg<br>Copyright (c) 2019 nyorain<br>Copyright (c) 2022-2024 David Turner, Robert Wilhelm, Werner Lemberg, George Williams, and Dominik Röttsches<br>Copyright (C) 2009, 2023  Red Hat, Inc. |
| Harfbuzz | 10.4.0 | MIT | Copyright © 2010-2022  Google, Inc.<br>Copyright © 2015-2020  Ebrahim Byagowi<br>Copyright © 2019,2020  Facebook, Inc.<br>Copyright © 2012,2015  Mozilla Foundation<br>Copyright © 2011  Codethink Limited<br>Copyright © 2008,2010  Nokia Corporation and/or its subsidiary(-ies)<br>Copyright © 2009  Keith Stribley<br>Copyright © 2011  Martin Hosken and SIL International<br>Copyright © 2007  Chris Wilson<br>Copyright © 2005,2006,2020,2021,2022,2023  Behdad Esfahbod<br>Copyright © 2004,2007,2008,2009,2010,2013,2021,2022,2023  Red Hat, Inc.<br>Copyright © 1998-2005  David Turner and Werner Lemberg<br>Copyright © 2016  Igalia S.L.<br>Copyright © 2022  Matthias Clasen<br>Copyright © 2018,2021  Khaled Hosny<br>Copyright © 2018,2019,2020  Adobe, Inc<br>Copyright © 2013-2015  Alexei Podtelezhnikov |
| libdbus-1-headers | dbus-1.13.12 | AFL-2.1 OR GPL-2.0-or-later | Copyright (C) 2002, 2003 CodeFactory AB<br>Copyright (C) 2004, 2005 Red Hat, Inc. |
| rhi-miniengine-d3d12-mipmap | 0aa79bad78992da0b6a8279ddb9002c1753cb849 | MIT | Copyright (c) 2015 Microsoft |
| opengl-headers | Revision 27684 | MIT | Copyright (c) 2013-2014 The Khronos Group Inc. |
| opengl-es2-headers | Revision 27673 | MIT | Copyright (c) 2013-2014 The Khronos Group Inc. |
| grayraster | unknown | FTL OR GPL-2.0-only | Copyright 2000-2016 by David Turner, Robert Wilhelm, and Werner Lemberg. |
| smooth-scaling-algorithm | unknown | BSD-2-Clause AND Imlib2 | Copyright (C) 2004, 2005 Daniel M. Duley.<br>(C) Carsten Haitzler and various contributors.<br>(C) Willem Monsuwe <willem@stack.nl> |
| xserverhelper | unknown | X11 AND HPND | Copyright (c) 1987, 1988 X Consortium<br>Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts. |
| aglfn | 1.7 | BSD-3-Clause | Copyright 2002, 2003, 2005, 2006, 2008, 2010, 2015 Adobe Systems |
| vulkanmemoryallocator | 3.0.1 | MIT | Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All rights reserved. |
| webgradients | unknown | MIT | Copyright (c) 2017 itmeo |
| icc-srgb-color-profile | unknown | LicenseRef-ICC-License | Copyright International Color Consortium, 2015 |
| d3d12memoryallocator | f128d39b7a95b4235bd228d231646278dc6c24b2 | MIT | Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All rights reserved. |
| md4c | 0.5.2 | MIT | Copyright © 2016-2024 Martin Mitáš |
| vulkan-xml-spec | 1.3.223 | Apache-2.0 OR MIT | Copyright (c) 2015-2022 The Khronos Group Inc. |
| wintab | unknown | LicenseRef-Lcs-Telegraphics | Copyright 1991-1998 by LCS/Telegraphics. |
| xsvg | unknown | HPND-sell-variant | Copyright 2002 USC/Information Sciences Institute |
| libtiff | 4.7.0 | libtiff | Copyright (c) 1988-1997 Sam Leffler<br>Copyright (c) 1991-1997 Silicon Graphics, Inc. |
| libwebp | 1.5.0 | BSD-3-Clause | Copyright (c) 2010, Google Inc. All rights reserved. |

Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.

## Other

- **Microsoft Visual C++ Redistributable**: Sound Mind Studio needs the Microsoft Visual C++ runtime; where an installer for it is included, it is distributed under Microsoft's own licence terms.
- **The bundled user guide** is generated with Doxygen; its JavaScript files are © Dimitri van Heesch and are licensed under the MIT licence (see the notice at the top of each file).
