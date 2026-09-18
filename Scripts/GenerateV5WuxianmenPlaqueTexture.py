"""Generate the Wuxianmen gate plaque base-colour texture.

The reference sheet shows the plaque as a light lime-washed board with a thin
dark border and three dark characters read right to left (五仙門), mounted on the
tower front. The imported asset had no plaque texture at all - the material was a
flat gold tint - so the sign was unreadable.

This renders a deterministic, weathering-varied base colour. Layout matches the
plaque mesh's 4.9 x 1.45 m face; the texture is authored upright with the
characters reading 門 仙 五 left to right, which is how the reference draws it.

Run with the PIL interpreter (Python 3.10).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

PROJECT = Path(__file__).resolve().parents[1]
OUT_DIR = PROJECT / "Saved/RawModelImport/V5/RefTune2"
OUTPUT = OUT_DIR / "Wuxianmen_Plaque_BaseColor_2K.png"
REPORT = OUT_DIR / "Wuxianmen_Plaque_BaseColor_2K.json"

FONT_CANDIDATES = [
    Path("C:/Windows/Fonts/STKAITI.TTF"),
    Path("C:/Windows/Fonts/simkai.ttf"),
    Path("C:/Windows/Fonts/simsunb.ttf"),
]
GLYPHS = "門仙五"  # left to right, as drawn on the reference sheet
WIDTH, HEIGHT = 2048, 608


def _font(size: int) -> ImageFont.FreeTypeFont:
    for candidate in FONT_CANDIDATES:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    raise FileNotFoundError(FONT_CANDIDATES)


def _noise(width: int, height: int, scale: float, seed: int) -> np.ndarray:
    """Smooth low-frequency noise in 0..1 via upsampled random field."""
    rng = np.random.default_rng(seed)
    small = rng.random((max(2, int(height / scale)), max(2, int(width / scale)))).astype(np.float32)
    image = Image.fromarray((small * 255).astype(np.uint8), mode="L").resize((width, height), Image.BICUBIC)
    return np.asarray(image, dtype=np.float32) / 255.0


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # Lime-washed board base: warm off-white, matching the reference plaque.
    base = np.zeros((HEIGHT, WIDTH, 3), dtype=np.float32)
    base[..., 0] = 0.735
    base[..., 1] = 0.700
    base[..., 2] = 0.632

    # Broad staining: uneven wash plus downward streaks, as on the reference board.
    wash = _noise(WIDTH, HEIGHT, 90.0, 11)
    streaks = _noise(WIDTH, HEIGHT, 6.0, 23)
    streaks = np.asarray(
        Image.fromarray((streaks * 255).astype(np.uint8), mode="L")
        .filter(ImageFilter.GaussianBlur(1.5))
        .resize((WIDTH, HEIGHT), Image.BICUBIC),
        dtype=np.float32,
    ) / 255.0
    stain = np.clip(0.55 * wash + 0.45 * streaks, 0.0, 1.0)
    darkening = (1.0 - 0.20 * stain)[..., None]
    base *= darkening
    base += (stain[..., None] - 0.5) * np.array([0.03, 0.02, 0.0], dtype=np.float32)

    # Fine grain so the board is not perfectly flat under direct light.
    grain = _noise(WIDTH, HEIGHT, 1.6, 37)[..., None]
    base += (grain - 0.5) * 0.045

    base = np.clip(base, 0.0, 1.0)
    image = Image.fromarray((base * 255.0).astype(np.uint8), mode="RGB")
    draw = ImageDraw.Draw(image)

    # Thin dark border inset, as drawn in the reference detail panel.
    inset = int(HEIGHT * 0.085)
    draw.rectangle(
        [inset, inset, WIDTH - inset - 1, HEIGHT - inset - 1],
        outline=(38, 30, 24),
        width=max(3, int(HEIGHT * 0.022)),
    )

    # Characters: three evenly spaced glyphs, upright, dark ink.
    font = _font(int(HEIGHT * 0.60))
    cell = (WIDTH - 2 * inset) / len(GLYPHS)
    for index, glyph in enumerate(GLYPHS):
        left, top, right, bottom = draw.textbbox((0, 0), glyph, font=font)
        text_w = right - left
        text_h = bottom - top
        centre_x = inset + cell * (index + 0.5)
        centre_y = HEIGHT * 0.5
        draw.text(
            (centre_x - text_w / 2 - left, centre_y - text_h / 2 - top),
            glyph,
            font=font,
            fill=(30, 24, 19),
        )

    # Ink wear: let the wash show through the glyphs slightly.
    ink = np.asarray(image, dtype=np.float32) / 255.0
    wear = np.clip(_noise(WIDTH, HEIGHT, 24.0, 53), 0.0, 1.0)[..., None]
    blend = np.clip(0.18 + 0.30 * wear, 0.0, 1.0)
    is_ink = (ink.mean(axis=2) < 0.35)[..., None]
    merged = np.where(is_ink, np.clip(ink + (base - ink) * blend * 0.45, 0.0, 1.0), ink)
    image = Image.fromarray((np.clip(merged, 0.0, 1.0) * 255.0).astype(np.uint8), mode="RGB")

    image.save(OUTPUT)
    REPORT.write_text(
        json.dumps(
            {
                "created": "2026-09-17",
                "output": str(OUTPUT),
                "size": [WIDTH, HEIGHT],
                "glyphs_left_to_right": GLYPHS,
                "reads_right_to_left_as": GLYPHS[::-1],
                "font": str(FONT_CANDIDATES[0]),
                "board_mean_rgb": np.asarray(image, dtype=np.float32).reshape(-1, 3).mean(axis=0).round(4).tolist(),
                "intent": "Light lime-washed board with a thin dark border and dark characters, matching the reference gate plaque.",
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print("REFTUNE2_PLAQUE_TEXTURE", OUTPUT, image.size)


if __name__ == "__main__":
    main()
