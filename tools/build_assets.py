"""Convert ws4kp assets into Wii-friendly PNGs and generate C metadata.

Usage: uv run --with pillow --with freetype-py tools/build_assets.py <path-to-ws4kp-checkout>

Writes PNGs to data/ (embedded by the Makefile) and tables to source/gen/.
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


def write_c(fonts, icons, images, source_rev: str):
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
        "extern const FontAsset font_assets[FONT_COUNT];",
        "extern const IconAsset icon_assets[ICON_COUNT];",
        "extern const ImageAsset image_assets[IMG_COUNT];",
        "",
    ]
    (GEN / "assets_gen.h").write_text("\n".join(h))

    c = ["// Generated by tools/build_assets.py. Do not edit.", '#include "assets_gen.h"']
    for sym in [f["sym"] for f in fonts] + [i["sym"] for i in icons] + [i["sym"] for i in images]:
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


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ws4kp = Path(sys.argv[1]).resolve()
    server = ws4kp / "server"
    DATA.mkdir(exist_ok=True)
    GEN.mkdir(parents=True, exist_ok=True)
    for old in DATA.glob("*.png"):
        old.unlink()

    rev = subprocess.run(["git", "-C", str(ws4kp), "rev-parse", "HEAD"],
                         capture_output=True, text=True).stdout.strip() or "unknown"
    fonts = [build_font(e, server / "fonts" / f, s) for e, f, s in FONTS]
    icons = [build_icon(p) for p in sorted((server / "images" / ICON_DIR).glob("*.gif"))]
    icons += [build_icon(p, SMALL_ICON_PREFIX) for p in sorted((server / "images" / SMALL_ICON_DIR).glob("*.gif"))]
    images = [build_image(e, server / "images" / p) for e, p in IMAGES]
    write_c(fonts, icons, images, rev)
    station_count = write_stations(ws4kp)
    print(f"{len(fonts)} fonts, {len(icons)} icons, {len(images)} images, {station_count} stations")


if __name__ == "__main__":
    main()
