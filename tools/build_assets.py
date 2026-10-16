"""Convert ws4kp assets into Wii-friendly PNGs and generate C metadata.

Usage: uv run --with pillow --with freetype-py tools/build_assets.py <path-to-ws4kp-checkout>

Writes PNGs to data/ and data/maps/ (embedded by the Makefile) and tables to source/gen/.
GX textures must be at most 1024x1024 with dimensions divisible by 4, so
every output image is padded to satisfy that.
"""

import json
import subprocess
import sys
from pathlib import Path

import freetype
from PIL import Image, ImageDraw, ImageFont, ImageSequence

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
GEN = ROOT / "source" / "gen"

# (enum suffix, ws4kp font file, pixel size). Add entries as screens need them.
FONTS = [
    ("STAR4000_32", "Star4000.woff", 32),
    ("STAR4000_EXTENDED_32", "Star4000 Extended.woff", 32),
    ("STAR4000_LARGE_32", "Star4000 Large.woff", 32),
    ("STAR4000_LARGE_20", "Star4000 Large.woff", 20),
    ("STAR4000_SMALL_32", "Star4000 Small.woff", 32),
    ("STAR4000_SMALL_26", "Star4000 Small.woff", 26),
    ("STAR4000_SMALL_28", "Star4000 Small.woff", 28),
    ("STAR4000_20", "Star4000.woff", 20),
    ("STAR4000_LARGE_28", "Star4000 Large.woff", 28),
]

# (enum suffix, path under server/images)
IMAGES = [
    ("BACKGROUND_1", "backgrounds/1.png"),
    ("BACKGROUND_2", "backgrounds/2.png"),
    ("LOGO_CORNER", "logos/logo-corner.png"),
    ("NOAA", "logos/noaa.gif"),
    ("BACKGROUND_3", "backgrounds/3.png"),
    ("MOON_NEW", "icons/moon-phases/New-Moon.gif"),
    ("MOON_FIRST", "icons/moon-phases/First-Quarter.gif"),
    ("MOON_FULL", "icons/moon-phases/Full-Moon.gif"),
    ("MOON_FULL_DEGRADED", "icons/moon-phases/Full-Moon-Degraded.gif"),
    ("MOON_LAST", "icons/moon-phases/Last-Quarter.gif"),
    ("BACKGROUND_1_CHART", "backgrounds/1-chart.png"),
]

ICON_DIR = "icons/current-conditions"
# ws4kp's small icons (icons-small.mjs, icons-hourly.mjs). Their names overlap
# the large ones, so their enums are prefixed.
SMALL_ICON_DIR = "icons/regional-maps"
SMALL_ICON_PREFIX = "SMALL_"

# (enum suffix, path under server/images) for the Regional Forecast's maps.
MAPS = [
    ("CONUS", "maps/basemap.webp"),
    ("ALASKA", "maps/radar-alaska.png"),
    ("HAWAII", "maps/radar-hawaii.png"),
]
# The maps are far larger than a texture can be, and decoding a whole one
# would take megabytes, so they are cut into tiles loaded as needed. Each
# tile repeats its neighbors' edge pixels so bilinear filtering is seamless.
MAP_TILE = 256
MAP_STRIDE = MAP_TILE - 2
MAPS_DATA = DATA / "maps"

# Latin-1 printable range; strings on the Wii side are Latin-1 bytes.
CHARS = [c for c in range(32, 256) if c < 127 or c >= 160]
SOFT_HYPHEN = 0xAD  # invisible mid-line in browsers
ATLAS_WIDTH = 512
MAX_TEXTURE = 1024


def round4(n: int) -> int:
    return (n + 3) & ~3


def symbol(stem: str) -> str:
    return stem.replace("-", "_").replace(" ", "_").replace(".", "_").lower()


def save_padded(img: Image.Image, name: str) -> str:
    """Pad to multiples of 4 and save; returns the bin2o symbol base."""
    w, h = round4(img.width), round4(img.height)
    if (w, h) != img.size:
        padded = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        padded.paste(img, (0, 0))
        img = padded
    assert w <= MAX_TEXTURE and h <= MAX_TEXTURE, name
    img.save(DATA / f"{name}.png", optimize=True)
    return f"{name}_png"


def line_metrics(path: Path, size: int) -> tuple[int, int, int]:
    """Chrome's ascent, descent and line gap: unhinted, each rounded separately.
    Their sum is the height of a line-height: normal line box."""
    face = freetype.Face(str(path))
    scale = size / face.units_per_EM
    gap = face.height - face.ascender + face.descender
    return round(face.ascender * scale), round(-face.descender * scale), round(gap * scale)


def hinted_advances(path: Path, size: int) -> dict[int, int]:
    """Chrome on Linux lays text out with fully hinted TrueType advances, which
    differ per glyph even in these monospaced fonts. Pillow only reports
    unhinted widths, so ask FreeType directly."""
    face = freetype.Face(str(path))
    face.set_pixel_sizes(0, size)
    advances = {}
    for code in CHARS:
        face.load_char(chr(code), freetype.FT_LOAD_DEFAULT)
        advances[code] = face.glyph.advance.x // 64
    advances[SOFT_HYPHEN] = 0
    return advances


def build_font(enum: str, path: Path, size: int):
    font = ImageFont.truetype(str(path), size)
    advances = hinted_advances(path, size)
    ascent, descent, line_gap = line_metrics(path, size)
    glyphs = {}
    boxes = []
    for code in CHARS:
        ch = chr(code)
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        if code == SOFT_HYPHEN:
            x1, y1 = x0, y0
        boxes.append((code, x0, y0, max(0, x1 - x0), max(0, y1 - y0)))

    # Simple shelf packing; a 1px gap stops bilinear filtering bleeding.
    x = y = shelf = 0
    placed = []
    for code, x0, y0, w, h in boxes:
        if x + w + 1 > ATLAS_WIDTH:
            x, y, shelf = 0, y + shelf + 1, 0
        placed.append((code, x, y, x0, y0, w, h))
        x += w + 1
        shelf = max(shelf, h)
    height = round4(y + shelf + 1)

    mask = Image.new("L", (ATLAS_WIDTH, height), 0)
    draw = ImageDraw.Draw(mask)
    for code, ax, ay, x0, y0, w, h in placed:
        if w and h:
            draw.text((ax - x0, ay - y0), chr(code), font=font, fill=255, anchor="ls")
        glyphs[code] = (ax, ay, w, h, x0, y0, advances[code])

    atlas = Image.new("RGBA", mask.size, (255, 255, 255, 0))
    atlas.putalpha(mask)
    sym = save_padded(atlas, f"font_{enum.lower()}")
    return {"enum": enum, "sym": sym, "ascent": ascent, "descent": descent, "line_gap": line_gap,
            "glyphs": glyphs}


def build_icon(path: Path, prefix: str = ""):
    src = Image.open(path)
    frames, delays = [], []
    for frame in ImageSequence.Iterator(src):
        frames.append(frame.convert("RGBA"))
        delays.append(frame.info.get("duration") or 100)
    w, h = src.size
    cw, ch = round4(w), round4(h)
    cols = min(len(frames), MAX_TEXTURE // cw)
    rows = -(-len(frames) // cols)
    sheet = Image.new("RGBA", (cols * cw, rows * ch), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        sheet.paste(frame, ((i % cols) * cw, (i // cols) * ch))
    enum = prefix + symbol(path.stem).upper()
    sym = save_padded(sheet, f"icon_{enum.lower()}")
    # All ws4kp animations use one constant delay.
    return {"enum": enum, "sym": sym, "w": w, "h": h, "cw": cw, "ch": ch,
            "cols": cols, "frames": len(frames), "delay": delays[0] if len(frames) > 1 else 0}


def build_image(enum: str, path: Path):
    img = Image.open(path).convert("RGBA")
    sym = save_padded(img, f"img_{enum.lower()}")
    return {"enum": enum, "sym": sym, "w": img.width, "h": img.height}


def build_map(enum: str, path: Path):
    img = Image.open(path).convert("RGB")
    w, h = img.size
    cols, rows = -(-w // MAP_STRIDE), -(-h // MAP_STRIDE)
    # The map at (1, 1) with a border filling out the last tiles, where each
    # pixel repeats the nearest edge pixel: stretch the edge columns, then rows.
    padded = Image.new("RGB", (cols * MAP_STRIDE + 2, rows * MAP_STRIDE + 2))
    pw, ph = padded.size
    padded.paste(img, (1, 1))
    stretch = lambda box, size: padded.crop(box).resize(size, Image.NEAREST)
    padded.paste(stretch((1, 0, 2, ph), (1, ph)), (0, 0))
    padded.paste(stretch((w, 0, w + 1, ph), (pw - w - 1, ph)), (w + 1, 0))
    padded.paste(stretch((0, 1, pw, 2), (pw, 1)), (0, 0))
    padded.paste(stretch((0, h, pw, h + 1), (pw, ph - h - 1)), (0, h + 1))
    syms = []
    for r in range(rows):
        for c in range(cols):
            x, y = c * MAP_STRIDE, r * MAP_STRIDE
            name = f"map_{enum.lower()}_{r}_{c}"
            padded.crop((x, y, x + MAP_TILE, y + MAP_TILE)).save(MAPS_DATA / f"{name}.png", optimize=True)
            syms.append(f"{name}_png")
    return {"enum": enum, "w": w, "h": h, "cols": cols, "rows": rows, "tiles": syms}


def write_c(fonts, icons, images, maps, source_rev: str):
    h = [
        "// Generated by tools/build_assets.py. Do not edit.",
        f"// Source: ws4kp {source_rev}",
        "#pragma once",
        "#include <gctypes.h>",
        "",
        "typedef struct {",
        "\tu16 x, y;     // atlas position",
        "\tu8 w, h;      // bitmap size, 0 for blank glyphs",
        "\ts8 xoff, yoff; // bitmap offset from pen position on the baseline",
        "\tu8 advance;",
        "} GlyphInfo;",
        "",
        "typedef struct {",
        "\tconst u8 *png;",
        "\tu8 ascent, descent, line_gap;",
        "\tGlyphInfo glyphs[256]; // indexed by Latin-1 code",
        "} FontAsset;",
        "",
        "typedef struct {",
        "\tconst u8 *png;",
        "\tu16 w, h;          // visible frame size",
        "\tu16 cell_w, cell_h; // padded frame size in the sheet",
        "\tu8 cols, frames;",
        "\tu16 delay_ms;       // 0 for static icons",
        "} IconAsset;",
        "",
        "typedef struct {",
        "\tconst u8 *png;",
        "\tu16 w, h;",
        "} ImageAsset;",
        "",
        f"#define MAP_TILE_SIZE {MAP_TILE}",
        "#define MAP_TILE_STRIDE (MAP_TILE_SIZE - 2) // map pixels per tile, without the 1px apron on each side",
        "",
        "typedef struct {",
        "\tu16 w, h;",
        "\tu8 cols, rows;",
        "\tconst u8 *const *tiles; // row-major; tile (c, r) starts at map pixel (c, r) * stride - 1",
        "} MapAsset;",
        "",
        "typedef enum {",
        *[f"\tFONT_{f['enum']}," for f in fonts],
        "\tFONT_COUNT",
        "} FontId;",
        "",
        "typedef enum {",
        *[f"\tICON_{i['enum']}," for i in icons],
        "\tICON_COUNT",
        "} IconId;",
        "",
        "typedef enum {",
        *[f"\tIMG_{i['enum']}," for i in images],
        "\tIMG_COUNT",
        "} ImageId;",
        "",
        "typedef enum {",
        *[f"\tMAP_{m['enum']}," for m in maps],
        "\tMAP_COUNT",
        "} MapId;",
        "",
        "extern const FontAsset font_assets[FONT_COUNT];",
        "extern const IconAsset icon_assets[ICON_COUNT];",
        "extern const ImageAsset image_assets[IMG_COUNT];",
        "extern const MapAsset map_assets[MAP_COUNT];",
        "",
    ]
    (GEN / "assets_gen.h").write_text("\n".join(h))

    c = ["// Generated by tools/build_assets.py. Do not edit.", '#include "assets_gen.h"']
    tiles = [t for m in maps for t in m["tiles"]]
    for sym in [f["sym"] for f in fonts] + [i["sym"] for i in icons] + [i["sym"] for i in images] + tiles:
        c.append(f'#include "{sym}.h"')
    c += ["", "const FontAsset font_assets[FONT_COUNT] = {"]
    for f in fonts:
        c.append(f"\t[FONT_{f['enum']}] = {{ {f['sym']}, {f['ascent']}, {f['descent']}, {f['line_gap']}, {{")
        for code, (x, y, w, gh, xo, yo, adv) in sorted(f["glyphs"].items()):
            c.append(f"\t\t[{code}] = {{ {x}, {y}, {w}, {gh}, {xo}, {yo}, {adv} }},")
        c.append("\t} },")
    c += ["};", "", "const IconAsset icon_assets[ICON_COUNT] = {"]
    for i in icons:
        c.append(f"\t[ICON_{i['enum']}] = {{ {i['sym']}, {i['w']}, {i['h']}, {i['cw']}, {i['ch']}, "
                 f"{i['cols']}, {i['frames']}, {i['delay']} }},")
    c += ["};", "", "const ImageAsset image_assets[IMG_COUNT] = {"]
    for i in images:
        c.append(f"\t[IMG_{i['enum']}] = {{ {i['sym']}, {i['w']}, {i['h']} }},")
    c += ["};", ""]
    for m in maps:
        c.append(f"static const u8 *const map_{m['enum'].lower()}_tiles[] = {{")
        c += [f"\t{t}," for t in m["tiles"]]
        c += ["};", ""]
    c.append("const MapAsset map_assets[MAP_COUNT] = {")
    for m in maps:
        c.append(f"\t[MAP_{m['enum']}] = {{ {m['w']}, {m['h']}, {m['cols']}, {m['rows']}, map_{m['enum'].lower()}_tiles }},")
    c += ["};", ""]
    (GEN / "assets_gen.c").write_text("\n".join(c))


def c_string(text: str) -> str:
    """Latin-1 C string literal; characters outside Latin-1 become '?'."""
    out = []
    for b in text.encode("latin-1", errors="replace"):
        ch = chr(b)
        if ch in '\\"':
            out.append("\\" + ch)
        elif 32 <= b < 127:
            out.append(ch)
        else:
            out.append(f'\\x{b:02x}""')  # split literal so hex escapes end cleanly
    return '"' + "".join(out) + '"'


def write_stations(ws4kp: Path):
    """ws4kp's curated station list: display names used instead of NWS station names."""
    stations = json.loads((ws4kp / "datagenerators" / "output" / "stations.json").read_text())
    rows = sorted(stations.values(), key=lambda s: s["id"])
    h = [
        "// Generated by tools/build_assets.py. Do not edit.",
        "#pragma once",
        "",
        "// Plain C types so host tests can link the table.",
        "typedef struct {",
        "\tchar id[5];",
        "\tchar state[3];",
        "\tconst char *city;",
        "\tfloat lat, lon;",
        "} StationInfo;",
        "",
        f"#define STATION_COUNT {len(rows)}",
        "extern const StationInfo station_info[STATION_COUNT]; // sorted by id",
        "",
    ]
    (GEN / "stations_gen.h").write_text("\n".join(h))
    c = ["// Generated by tools/build_assets.py. Do not edit.", '#include "stations_gen.h"', "",
         "const StationInfo station_info[STATION_COUNT] = {"]
    for s in rows:
        c.append(f'\t{{ "{s["id"]}", "{s["state"]}", {c_string(s["city"])}, {float(s["lat"])!r}f, {float(s["lon"])!r}f }},')
    c += ["};", ""]
    (GEN / "stations_gen.c").write_text("\n".join(c))
    return len(rows)


def write_travel_cities(ws4kp: Path):
    """ws4kp's Travel Forecast cities, in its order, with their NWS gridpoints.
    ws4kp drops cities whose point lookup failed, so they are left out."""
    cities = json.loads((ws4kp / "datagenerators" / "output" / "travelcities.json").read_text())
    cities = [c for c in cities if "point" in c]
    h = [
        "// Generated by tools/build_assets.py. Do not edit.",
        "#pragma once",
        "",
        "typedef struct {",
        "\tconst char *name;",
        "\tchar wfo[4];",
        "\tunsigned short x, y;",
        "} TravelCity;",
        "",
        f"#define TRAVEL_CITY_COUNT {len(cities)}",
        "extern const TravelCity travel_cities[TRAVEL_CITY_COUNT];",
        "",
    ]
    (GEN / "travel_cities_gen.h").write_text("\n".join(h))
    c = ["// Generated by tools/build_assets.py. Do not edit.", '#include "travel_cities_gen.h"', "",
         "const TravelCity travel_cities[TRAVEL_CITY_COUNT] = {"]
    for city in cities:
        p = city["point"]
        c.append(f'\t{{ {c_string(city["Name"])}, "{p["wfo"]}", {p["x"]}, {p["y"]} }},')
    c += ["};", ""]
    (GEN / "travel_cities_gen.c").write_text("\n".join(c))
    return len(cities)


def write_regional_cities(ws4kp: Path):
    """ws4kp's Regional Forecast cities, in its order, with their NWS gridpoints."""
    cities = json.loads((ws4kp / "datagenerators" / "output" / "regionalcities.json").read_text())
    cities = [c for c in cities if "point" in c]
    h = [
        "// Generated by tools/build_assets.py. Do not edit.",
        "#pragma once",
        "",
        "// Plain C types so host tests can link the table.",
        "typedef struct {",
        "\tconst char *name;",
        "\tdouble lat, lon;",
        "\tchar wfo[4];",
        "\tunsigned short x, y;",
        "} RegionalCity;",
        "",
        f"#define REGIONAL_CITY_COUNT {len(cities)}",
        "extern const RegionalCity regional_cities[REGIONAL_CITY_COUNT];",
        "",
    ]
    (GEN / "regional_cities_gen.h").write_text("\n".join(h))
    c = ["// Generated by tools/build_assets.py. Do not edit.", '#include "regional_cities_gen.h"', "",
         "const RegionalCity regional_cities[REGIONAL_CITY_COUNT] = {"]
    for city in cities:
        p = city["point"]
        c.append(f'\t{{ {c_string(city["city"])}, {city["lat"]}, {city["lon"]}, "{p["wfo"]}", {p["x"]}, {p["y"]} }},')
    c += ["};", ""]
    (GEN / "regional_cities_gen.c").write_text("\n".join(c))
    return len(cities)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ws4kp = Path(sys.argv[1]).resolve()
    server = ws4kp / "server"
    DATA.mkdir(exist_ok=True)
    GEN.mkdir(parents=True, exist_ok=True)
    MAPS_DATA.mkdir(exist_ok=True)
    for old in [*DATA.glob("*.png"), *MAPS_DATA.glob("*.png")]:
        old.unlink()

    rev = subprocess.run(["git", "-C", str(ws4kp), "rev-parse", "HEAD"],
                         capture_output=True, text=True).stdout.strip() or "unknown"
    fonts = [build_font(e, server / "fonts" / f, s) for e, f, s in FONTS]
    icons = [build_icon(p) for p in sorted((server / "images" / ICON_DIR).glob("*.gif"))]
    icons += [build_icon(p, SMALL_ICON_PREFIX) for p in sorted((server / "images" / SMALL_ICON_DIR).glob("*.gif"))]
    images = [build_image(e, server / "images" / p) for e, p in IMAGES]
    maps = [build_map(e, server / "images" / p) for e, p in MAPS]
    write_c(fonts, icons, images, maps, rev)
    station_count = write_stations(ws4kp)
    travel_count = write_travel_cities(ws4kp)
    regional_count = write_regional_cities(ws4kp)
    print(f"{len(fonts)} fonts, {len(icons)} icons, {len(images)} images, {len(maps)} maps, "
          f"{station_count} stations, {travel_count} travel cities, {regional_count} regional cities")


if __name__ == "__main__":
    main()
