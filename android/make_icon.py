"""Draw the launcher icon: a plane over a winding river between green banks.

Writes app/src/main/res/mipmap-<density>/ic_launcher.png at the five
standard densities. Needs Pillow. Run from the android/ folder:

    python make_icon.py
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

# dp size of a launcher icon is 48; each density is a multiple of that.
DENSITIES: dict[str, int] = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
SUPERSAMPLE: int = 4  # draw big, shrink with a filter: smooth edges at every size

GRASS = (72, 140, 52)
WATER = (52, 120, 200)
WATER_DARK = (36, 92, 168)
PLANE = (245, 245, 235)
PLANE_SHADE = (180, 180, 170)
FLAME = (255, 170, 40)


def river_edges(y: float, size: float) -> tuple[float, float]:
    """Left and right bank x at height y: a gentle S-bend, widest mid-icon."""
    assert 0.0 <= y <= size
    centre = size * 0.5 + size * 0.11 * math.sin(y / size * 2.0 * math.pi)
    half = size * (0.20 + 0.05 * math.cos(y / size * math.pi))
    assert half > 0.0
    return centre - half, centre + half


def draw_icon(size: int) -> Image.Image:
    """The icon at 'size' pixels square, rendered SUPERSAMPLE times larger then shrunk."""
    assert size > 0
    big = size * SUPERSAMPLE
    img = Image.new("RGBA", (big, big), GRASS + (255,))
    draw = ImageDraw.Draw(img)

    # River, one scanline at a time, with a darker ripple every few rows.
    for y in range(big):
        left, right = river_edges(float(y), float(big))
        shade = WATER_DARK if (y // (SUPERSAMPLE * 3)) % 4 == 0 else WATER
        draw.line([(left, y), (right, y)], fill=shade)

    # Plane: fuselage, wings, tail, flame; nose up, sitting low on the river.
    cx, cy = big * 0.5, big * 0.62
    s = big / 48.0
    draw.polygon([(cx, cy - 14 * s), (cx + 3 * s, cy - 6 * s), (cx + 3 * s, cy + 10 * s), (cx - 3 * s, cy + 10 * s), (cx - 3 * s, cy - 6 * s)], fill=PLANE)
    draw.polygon([(cx - 14 * s, cy + 4 * s), (cx + 14 * s, cy + 4 * s), (cx + 3 * s, cy - 3 * s), (cx - 3 * s, cy - 3 * s)], fill=PLANE)
    draw.polygon([(cx - 6 * s, cy + 12 * s), (cx + 6 * s, cy + 12 * s), (cx + 2 * s, cy + 7 * s), (cx - 2 * s, cy + 7 * s)], fill=PLANE_SHADE)
    draw.polygon([(cx - 2 * s, cy + 12 * s), (cx + 2 * s, cy + 12 * s), (cx, cy + 18 * s)], fill=FLAME)

    # Rounded corners so it reads as one tile on launchers that do not mask icons.
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, big - 1, big - 1], radius=big // 6, fill=255)
    img.putalpha(mask)

    small = img.resize((size, size), Image.Resampling.LANCZOS)
    assert small.size == (size, size)
    return small


def main() -> int:
    root = Path(__file__).resolve().parent / "app" / "src" / "main" / "res"
    assert root.is_dir(), f"missing {root}"
    for density, size in DENSITIES.items():
        folder = root / f"mipmap-{density}"
        folder.mkdir(exist_ok=True)
        out = folder / "ic_launcher.png"
        draw_icon(size).save(out)
        assert out.stat().st_size > 0
        print(f"wrote {out.relative_to(root.parent.parent.parent)} ({size}px)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
