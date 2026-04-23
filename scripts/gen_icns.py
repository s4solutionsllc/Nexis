#!/usr/bin/env python3
"""
Generate macOS squircle icon set from the circular 512x512 source artwork.

Produces macos/nexis.icns with all required sizes including icon_512x512@2x.png
(1024x1024 Retina dock size) which was previously missing.

The squircle uses a superellipse with n=5 (matches macOS Big Sur icon shape).
The circular artwork is centered on a white squircle background.
"""

import math
import os
import shutil
import subprocess
import sys

from PIL import Image
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, "shared/nexis/static/nexis_source.png")
OUT_ICONSET = os.path.join(REPO, "macos/nexis.iconset")
OUT_ICNS = os.path.join(REPO, "macos/nexis.icns")

# Background color: white to match existing icon_1024.png design direction
BG_COLOR = (0, 0, 0, 255)

# Superellipse exponent — n=5 matches macOS Big Sur squircle
SQUIRCLE_N = 5

# Artwork padding: the circular art fills this fraction of the squircle canvas.
# 1.0 = fills edge to edge (the circle sits tangent to the squircle sides),
# leaving visible background only in the squircle corners.
ARTWORK_FILL = 1.0


def make_squircle_mask(size: int, n: float = 5.0, supersample: int = 4) -> Image.Image:
    """Antialiased superellipse mask at `size` pixels."""
    big = size * supersample
    half = big / 2.0
    y_idx, x_idx = np.ogrid[:big, :big]
    nx = (x_idx - half + 0.5) / half
    ny = (y_idx - half + 0.5) / half
    inside = (np.abs(nx) ** n + np.abs(ny) ** n) <= 1.0
    big_mask = (inside * 255).astype(np.uint8)
    return Image.fromarray(big_mask, "L").resize((size, size), Image.LANCZOS)


def make_squircle_icon(src: Image.Image, size: int) -> Image.Image:
    """
    Composite `src` (RGBA) on a white squircle background at `size`×`size`.

    The artwork is scaled to ARTWORK_FILL × size and centred. The squircle
    mask is applied last so the result has a transparent exterior.
    """
    artwork_px = round(size * ARTWORK_FILL)
    artwork = src.resize((artwork_px, artwork_px), Image.LANCZOS)

    # White squircle background
    bg = Image.new("RGBA", (size, size), BG_COLOR)
    mask = make_squircle_mask(size, n=SQUIRCLE_N)
    squircle_bg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    squircle_bg.paste(bg, mask=mask)

    # Centre the artwork
    offset = ((size - artwork_px) // 2, (size - artwork_px) // 2)
    canvas = squircle_bg.copy()
    canvas.paste(artwork, offset, artwork)

    # Clip everything outside the squircle
    result = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    result.paste(canvas, mask=mask)
    return result


# iconset name → pixel size
SIZES = {
    "icon_16x16.png":      16,
    "icon_16x16@2x.png":   32,
    "icon_32x32.png":      32,
    "icon_32x32@2x.png":   64,
    "icon_128x128.png":    128,
    "icon_128x128@2x.png": 256,
    "icon_256x256.png":    256,
    "icon_256x256@2x.png": 512,
    "icon_512x512.png":    512,
    "icon_512x512@2x.png": 1024,
}


def main():
    src = Image.open(SOURCE).convert("RGBA")
    print(f"Source: {SOURCE} ({src.width}×{src.height})")

    os.makedirs(OUT_ICONSET, exist_ok=True)

    for filename, px in SIZES.items():
        icon = make_squircle_icon(src, px)
        out_path = os.path.join(OUT_ICONSET, filename)
        icon.save(out_path, "PNG")
        print(f"  {filename:30s} {px}×{px}")

    # Build .icns
    result = subprocess.run(
        ["iconutil", "-c", "icns", OUT_ICONSET, "-o", OUT_ICNS],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"iconutil error: {result.stderr}", file=sys.stderr)
        sys.exit(1)

    print(f"\nWrote {OUT_ICNS}")

    # Also update the shared iconset (used by Linux builds and static.qrc)
    shared_iconset = os.path.join(REPO, "shared/nexis/static/nexis.iconset")
    shared_sizes = [f for f in SIZES if "@2x" not in f]  # standard sizes only
    for filename in shared_sizes:
        px = SIZES[filename]
        src_path = os.path.join(OUT_ICONSET, filename)
        dst_path = os.path.join(shared_iconset, filename)
        shutil.copy2(src_path, dst_path)
        print(f"  Updated shared: {filename}")

    # Copy 1024px version as icon_1024.png
    icon_1024_path = os.path.join(REPO, "shared/nexis/static/icon_1024.png")
    shutil.copy2(os.path.join(OUT_ICONSET, "icon_512x512@2x.png"), icon_1024_path)
    print(f"  Updated icon_1024.png (1024×1024)")

    # Rebuild shared .icns
    shared_icns = os.path.join(REPO, "shared/nexis/static/nexis.icns")
    result2 = subprocess.run(
        ["iconutil", "-c", "icns", OUT_ICONSET, "-o", shared_icns],
        capture_output=True, text=True
    )
    if result2.returncode == 0:
        print(f"  Updated {shared_icns}")
    else:
        print(f"  shared icns warning: {result2.stderr}", file=sys.stderr)


if __name__ == "__main__":
    main()
