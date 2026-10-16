# Third-party notices

Wiiather includes or links the following work.

| Component | Use in Wiiather | License |
| --- | --- | --- |
| [ws4kp](https://github.com/netbymatt/ws4kp) by Matt Walsh, based on Mike Battaglia's original | Display logic and layout ported to C; backgrounds, logos, and regional and radar maps | MIT (below) |
| [SunCalc](https://github.com/mourner/suncalc) by Vladimir Agafonkin | Sun and moon calculations ported to C | BSD 2-Clause (below) |
| WeatherStar 4000 fonts by Nick Smith ([TWCClassics](https://twcclassics.com/downloads.html)) | Star4000 fonts, distributed with ws4kp | Credited as in ws4kp |
| Weather icons by Charles Abel, Nick Smith and Malek Masoud | Condition icons, distributed with ws4kp | Credited as in ws4kp |
| [Liberation Sans](https://github.com/liberationfonts/liberation-fonts) by Red Hat | Rendered in place of Arial for the Local Radar's title | SIL Open Font License 1.1 |
| [Census ZCTA Gazetteer](https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.html), U.S. Census Bureau | ZIP code coordinates in `source/gen/zips_gen.c` | Public domain (U.S. government work) |
| [Mozilla CA certificate bundle](https://curl.se/docs/caextract.html) | `data/cacert.pem` and updates downloaded from the `ca-bundle` branch, unmodified, for TLS verification | MPL 2.0 |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) | HTTPS | Apache 2.0 |
| [GRRLIB](https://github.com/GRRLIB/GRRLIB) | Graphics | MIT |
| [Jansson](https://github.com/akheron/jansson) | JSON parsing | MIT |
| [libogc](https://github.com/devkitPro/libogc) | Wii platform library | See its repository |
| [libmad](https://www.underbit.com/products/mad/) | MP3 decoding, through libogc's MP3 player | GPL 2.0 or later |
| [FreeType](https://freetype.org) | Linked through GRRLIB | FreeType License |
| [libpng](http://www.libpng.org/), [libjpeg-turbo](https://libjpeg-turbo.org/), [zlib](https://zlib.net/), [bzip2](https://sourceware.org/bzip2/), [Brotli](https://github.com/google/brotli) | Image decoding, linked through GRRLIB | Their respective licenses |

The Mozilla CA bundle's source is available from the link above.

Portions of this software are copyright © The FreeType Project
(www.freetype.org). All rights reserved.

This software is based in part on the work of the Independent JPEG Group.

## ws4kp

```text
MIT License

Copyright (c) 2020-2026 Matt Walsh

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## SunCalc

```text
Copyright (c) 2011-2015, Vladimir Agafonkin
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
