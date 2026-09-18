"""Generate the Wuxianmen FullPBR plaque board texture (BaseColor, 2048x2048).

Mirrors the 4K_Core convention (texture named Wuxianmen_Plaque_BaseColor_2K, 2K
resolution). The FullPBR pass needs a source PNG so the apply script can import
it as a .uasset; no reusable source exists in ContentSource for this gate.

Design (degrades gracefully under UV stretch to the 4.9 m x 1.45 m plaque mesh):
- 2048x2048 canvas, dark warm wood background with subtle horizontal grain.
- Centered gold double-border rectangle (~1348x1348) -> maps to a horizontal
  plaque shape when UV-stretched to the 3.38:1 mesh.
- "五仙门" calligraphy in SimKai (regular script), centered, cream-gold fill.
- Scattered faint weathering speckles.

Run (uses system Python 3.10 which has Pillow 9.4; managed Python lacks PIL):
    /c/Users/mickie/AppData/Local/Programs/Python/Python310/python.exe \\
        Scripts/GenerateWuxianmenFullPBRPlaqueTexture.py
"""
import os
import random

from PIL import Image, ImageDraw, ImageFont


SIZE = 2048
OUT = (
    r"C:/Git/AuraProj/ContentSource/GuangzhouLandmarks"
    r"\V5ReferenceTuning20260917\Wuxianmen_V5_FullPBR"
    r"\Textures\Wuxianmen_Plaque_BaseColor_2K.png"
)
FONT_PATH = r"C:/Windows/Fonts/simkai.ttf"

# Palette (light-theme, gate is weathered dark wood with gold trim).
WOOD = (42, 29, 18)        # #2a1d12 base wood
WOOD_DARK = (30, 20, 12)
WOOD_LIGHT = (58, 42, 28)
GOLD = (184, 134, 11)      # #b8860b brass
GOLD_LIGHT = (218, 165, 32)
CREAM = (244, 228, 188)     # #f4e4bc calligraphy


def main():
    random.seed(20260917)
    img = Image.new("RGB", (SIZE, SIZE), WOOD)
    draw = ImageDraw.Draw(img)

    # Wood grain: thin horizontal bands of slightly varying brown.
    for y in range(0, SIZE, 2):
        c = random.choices(
            [WOOD, WOOD, WOOD, WOOD_DARK, WOOD_LIGHT],
            weights=[6, 6, 6, 2, 2],
            k=1,
        )[0]
        draw.line([(0, y), (SIZE, y)], fill=c, width=1)

    # Centered gold double border. Near-square -> UV-stretch to 3.38:1 mesh
    # turns it into a horizontal plaque outline.
    B = 350
    draw.rectangle([(B, B), (SIZE - B, SIZE - B)], outline=GOLD, width=10)
    B2 = B + 38
    draw.rectangle(
        [(B2, B2), (SIZE - B2, SIZE - B2)], outline=GOLD_LIGHT, width=3,
    )

    # 五仙門 (Wuxianmen), SimKai (楷体, traditional regular script).
    font = ImageFont.truetype(FONT_PATH, 560)
    text = "\u4e94\u4ed9\u95e8"  # 五仙門
    cx = (B2 + SIZE - B2) // 2
    cy = (B2 + SIZE - B2) // 2
    draw.text((cx, cy), text, font=font, fill=CREAM, anchor="mm")

    # Subtle weathering: sparse lighter speckles and a few darker cracks.
    for _ in range(240):
        x = random.randint(0, SIZE - 1)
        y = random.randint(0, SIZE - 1)
        draw.point((x, y), fill=(72, 56, 38))
    for _ in range(40):
        x0 = random.randint(0, SIZE - 80)
        y0 = random.randint(0, SIZE - 1)
        draw.line(
            [(x0, y0), (x0 + random.randint(20, 80), y0 + random.randint(-2, 2))],
            fill=(22, 14, 8),
            width=1,
        )

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT, "PNG")
    print(f"PLAQUE_TEXTURE_WRITTEN {OUT} ({os.path.getsize(OUT)} bytes, {img.size})")


if __name__ == "__main__":
    main()
