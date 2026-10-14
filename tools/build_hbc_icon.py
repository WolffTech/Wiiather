"""Draw the Homebrew Channel icon (128x48) from ws4kp artwork.

Usage: uv run --with pillow tools/build_hbc_icon.py <path-to-ws4kp-checkout>

Writes meta/icon.png.
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SIZE = (128, 48)
YELLOW = (255, 255, 0)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    server = Path(sys.argv[1]) / "server"

    # The header band of background 1 above the blue box: orange fading to purple.
    band = Image.open(server / "images/backgrounds/1.png").convert("RGBA").crop((0, 0, 240, 90))
    icon = band.resize(SIZE, Image.LANCZOS)

    logo = Image.open(server / "images/logos/logo-corner.png").convert("RGBA")
    logo = logo.resize((round(logo.width * 40 / logo.height), 40), Image.LANCZOS)
    icon.alpha_composite(logo, (3, 4))

    # Star4000 with ws4kp's hard 3px drop shadow, scaled down with the text.
    font = ImageFont.truetype(str(server / "fonts/Star4000.woff"), 16)
    draw = ImageDraw.Draw(icon)
    x, y = 3 + logo.width + 3, 15
    draw.text((x + 1, y + 1), "Wiiather", font=font, fill=(0, 0, 0))
    draw.text((x, y), "Wiiather", font=font, fill=YELLOW)

    out = ROOT / "meta" / "icon.png"
    icon.save(out)
    print(out)


if __name__ == "__main__":
    main()
