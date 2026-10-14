# Third-party notices

Wiiather includes or links the following work.

| Component | Use in Wiiather | License |
| --- | --- | --- |
| [ws4kp](https://github.com/netbymatt/ws4kp) by Matt Walsh, based on Mike Battaglia's original | Display logic and layout ported to C; backgrounds and logos | MIT (below) |
| WeatherStar 4000 fonts by Nick Smith ([TWCClassics](https://twcclassics.com/downloads.html)) | Star4000 fonts, distributed with ws4kp | Credited as in ws4kp |
| Weather icons by Charles Abel, Nick Smith and Malek Masoud | Condition icons, distributed with ws4kp | Credited as in ws4kp |
| [Mozilla CA certificate bundle](https://curl.se/docs/caextract.html) | `data/cacert.pem`, unmodified, for TLS verification | MPL 2.0 |
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

