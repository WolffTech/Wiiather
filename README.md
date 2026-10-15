# Wiiather

A WeatherStar 4000 for the Nintendo Wii. Wiiather is a homebrew port of
[ws4kp](https://github.com/netbymatt/ws4kp) that fetches live National Weather
Service data over HTTPS and shows it in the style of The Weather Channel's
1990s local forecast.

US locations only, since all data comes from the NWS.

## Install

1. Build `dist/` (below) or download a release.
2. Copy `dist/apps/` to the root of the SD card.
3. Optionally, add background music: copy `.mp3` files to
   `apps/wiiather/music/`. They play in random order, and the Wii Remote's
   A button turns music on and off. ws4kp's royalty-free tracks in
   [`server/music/default`](https://github.com/netbymatt/ws4kp/tree/main/server/music/default)
   work well.
4. Launch Wiiather from the Homebrew Channel. The Wii needs a working
   internet connection set up in the system settings.

## Location

On first launch, Wiiather asks for a ZIP code. It suggests one based on the
Wii's public IP address, looked up with [ipinfo.io](https://ipinfo.io), which
sees that address. Change digits with the D-pad's up and down, move between
them with left and right, and press A to save. Press + at any time to pick a
new location, and B to back out.

The location is saved in `apps/wiiather/settings.ini`. To use exact
coordinates instead, for example for a place without a ZIP code, edit the
file:

```ini
lat=38.8951
lon=-77.0364
```

## Certificate updates

Wiiather checks each server's HTTPS certificate against a bundle of trusted
certificate authorities (CAs) built into the app. Authorities change over
time, so once a day Wiiather also checks this repository's
[`ca-bundle`](https://github.com/WolffTech/Wiiather/tree/ca-bundle) branch on
raw.githubusercontent.com for a newer bundle. It installs one only if the
project's signature verifies and the bundle is newer than the one it has,
and saves it to `apps/wiiather/cacert.pem` for the next launch.

If a Wii sits unused long enough that the weather servers move to an
authority its bundle doesn't know, Wiiather can no longer verify them. It
then fetches the signed bundle without checking GitHub's certificate, since
the signature alone proves where the bundle came from, and resumes once a
newer bundle installs. Until then it shows "Wiiather may need updating."

> [!IMPORTANT]
> Certificate updates depend on this project continuing to publish signed
> bundles from a public GitHub repository. If publishing stops, the repository
> goes private or the signing key is lost, Wiiather will eventually be unable
> to verify the weather servers, and only a new release will fix it.
> Certificate checks also fail if the Wii's clock is wrong, so keep its date
> and time set correctly.

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

The ZIP code table comes from the Census Bureau's
[ZCTA Gazetteer file](https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.html).
To regenerate it from a newer year's extracted `.txt`:

```zsh
python3 tools/build_zips.py /path/to/2025_Gaz_zcta_national.txt
```

## Disclaimer

Do not rely on Wiiather in life-threatening weather situations. Its data
can be incomplete or out of date. Use NOAA Weather Radio or official NWS
channels for alerts.

The WeatherSTAR 4000 name and design belong to The Weather Channel. Wiiather
is a free, non-commercial fan project and is not affiliated with The Weather
Channel, the NWS or Nintendo.

## License

Wiiather is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version. See [LICENSE](LICENSE).

It includes or links third-party work under other licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for credits and licenses.
