# Wiiather

A WeatherStar 4000 for the Nintendo Wii. Wiiather is a homebrew port of
[ws4kp](https://github.com/netbymatt/ws4kp) that fetches live National Weather
Service data over HTTPS and shows it in the style of The Weather Channel's
1990s local forecast.

US locations only, since all data comes from the NWS.

## Install

1. Build `dist/` (below) or download a release.
2. Copy `dist/apps/` to the root of the SD card.
3. Optionally, set the location in `apps/wiiather/settings.ini`:

   ```ini
   lat=38.8951
   lon=-77.0364
   ```

   Without the file, Wiiather shows Washington, DC.
4. Optionally, add background music: copy `.mp3` files to
   `apps/wiiather/music/`. They play in random order, and the Wii Remote's
   A button turns music on and off. ws4kp's royalty-free tracks in
   [`server/music/default`](https://github.com/netbymatt/ws4kp/tree/main/server/music/default)
   work well.
5. Launch Wiiather from the Homebrew Channel. The Wii needs a working
   internet connection set up in the system settings.

## Build

Requirements:

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `wii-dev` and
  the PowerPC portlibs for jansson, FreeType, libpng, libjpeg-turbo, bzip2,
  brotli and zlib.
- GRRLIB and a Wii-configured mbedTLS, built by `scripts/build-deps.sh`.

```zsh
scripts/build-deps.sh
make        # wiiather.dol
make dist   # dist/apps/wiiather/ for the Homebrew Channel
make test   # host-side unit tests (needs a native C compiler)
```

`make run` sends the build to a Wii running the Homebrew Channel with
`wiiload`. Dolphin can also run `wiiather.dol` directly.

### Regenerating assets

Fonts, icons and backgrounds come from a ws4kp checkout and are already
converted in `data/` and `source/gen/`. To redo the conversion:

```zsh
uv run --with pillow --with freetype-py tools/build_assets.py /path/to/ws4kp
uv run --with pillow tools/build_hbc_icon.py /path/to/ws4kp
```

## Disclaimer

Do not rely on Wiiather in life-threatening weather situations. Its data
can be incomplete or out of date. Use NOAA Weather Radio or official NWS
channels for alerts.

The WeatherSTAR 4000 name and design belong to The Weather Channel. Wiiather
is a free, non-commercial fan project and is not affiliated with The Weather
Channel, the NWS or Nintendo.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for credits and licenses.
