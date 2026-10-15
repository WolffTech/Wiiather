"""Derive branding PNGs and the 128x48 Homebrew icon from Icon Composer's export.

Usage: uv run --with pillow tools/build_hbc_icon.py

Run zsh branding/export.zsh first when the Icon Composer artwork changes.
"""

from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image, ImageColor, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parent.parent
BRANDING = ROOT / "branding"
SIZE = (128, 48)


def forecast_background(size: tuple[int, int]) -> Image.Image:
    source = BRANDING / "Wiiather.icon" / "Assets" / "forecast.svg"
    stops = [
        (float(stop.attrib["offset"]), ImageColor.getrgb(stop.attrib["stop-color"]))
        for stop in ET.parse(source).iter("{http://www.w3.org/2000/svg}stop")
    ]
    image = Image.new("RGBA", size)
    draw = ImageDraw.Draw(image)
    for y in range(size[1]):
        position = y / (size[1] - 1)
        for (start, first), (end, last) in zip(stops, stops[1:]):
            if start <= position <= end:
                fraction = (position - start) / (end - start)
                color = tuple(round(a + (b - a) * fraction) for a, b in zip(first, last))
                draw.line((0, y, size[0] - 1, y), fill=(*color, 255))
                break
    return image


def main():
    with Image.open(BRANDING / "wiiather-icon.png") as exported:
        if exported.size != (1024, 1024):
            raise ValueError("Icon Composer export must be 1024x1024")
        # Crop and mask the badge to remove the surrounding platform tile.
        badge = exported.convert("RGBA").crop((0, 224, 1024, 800))
    mask = Image.new("L", badge.size)
    ImageDraw.Draw(mask).rounded_rectangle((28, 28, 995, 547), radius=102, fill=255)
    badge.putalpha(mask)
    badge.save(BRANDING / "wiiather-badge.png", optimize=True)

    icon = forecast_background(SIZE)
    small_badge = ImageOps.contain(badge, SIZE, Image.Resampling.LANCZOS)
    position = tuple((outer - inner) // 2 for outer, inner in zip(SIZE, small_badge.size))
    icon.alpha_composite(small_badge, position)
    icon.save(ROOT / "meta" / "icon.png", optimize=True)
    print("Wrote branding/wiiather-badge.png and meta/icon.png (128x48 RGBA)")


if __name__ == "__main__":
    main()
