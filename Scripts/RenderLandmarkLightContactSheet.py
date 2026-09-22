"""Build a contact sheet of the per-building lights-on / lights-off pairs.

Reads the PPM pairs and the report written by
CaptureGuangzhouLandmarkLightsPerBuilding.py, and lays them out as one row per
building with the light OFF on the left and ON on the right, annotated with the
measured difference. One image to review instead of eighteen files.

Run with an interpreter that has PIL (the editor's bundled Python does not):
    <venv>/Scripts/python.exe Scripts/RenderLandmarkLightContactSheet.py
"""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

SRC = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLightsPerBuilding")
REPORT = SRC / "per-building-report.json"
STRENGTH = SRC / "light-strength-analysis.json"
OUT = SRC / "per-building-contact-sheet.png"

IMAGE_WIDTH = 330
ROW_HEIGHT = 200
LABEL_WIDTH = 212
BLOCK_GAP = 44
HEADER = 86
FOOTER = 70
BACKGROUND = (251, 251, 252)
INK = (27, 39, 51)
MUTED = (91, 107, 124)
RULE = (216, 222, 230)
OFF_TINT = (120, 130, 142)
ON_TINT = (179, 84, 30)


def font(size, bold=False):
    for name in ("segoeuib.ttf" if bold else "segoeui.ttf", "arialbd.ttf", "arial.ttf"):
        path = Path("C:/Windows/Fonts") / name
        if path.exists():
            try:
                return ImageFont.truetype(str(path), size)
            except Exception:
                pass
    return ImageFont.load_default()


def main():
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    buildings = report["buildings"]
    # Strength measured on the pixels each light actually brightens. The whole-frame
    # mean delta is NOT used to compare buildings: it is dominated by how much of the
    # frame the building fills, which made the two Wuxianmen lights look weakest when
    # they are in fact among the strongest.
    strengths = {}
    if STRENGTH.exists():
        strengths = {row["key"]: row
                     for row in json.loads(STRENGTH.read_text(encoding="utf-8"))["buildings"]}
    height = IMAGE_WIDTH * 9 // 16

    half = (len(buildings) + 1) // 2
    blocks = [buildings[:half], buildings[half:]]
    rows = max(len(block) for block in blocks)
    width = BLOCK_GAP + len(blocks) * (LABEL_WIDTH + 2 * IMAGE_WIDTH + 12) + BLOCK_GAP
    total_height = HEADER + rows * ROW_HEIGHT + FOOTER

    sheet = Image.new("RGB", (width, total_height), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    title_font = font(21, bold=True)
    sub_font = font(13)
    label_font = font(13, bold=True)
    meta_font = font(12)
    tag_font = font(11, bold=True)

    draw.text((24, 18), "Each building with its own light OFF and ON",
              font=title_font, fill=INK)
    draw.text((24, 48),
              "L_GuangzhouLandmarkShowcase · one shadow-casting spot light per landmark, "
              "intensity zeroed for the OFF frame · EV100 4.0 · level not saved",
              font=sub_font, fill=MUTED)
    draw.line((24, HEADER - 16, width - 24, HEADER - 16), fill=RULE, width=2)

    for block_index, block in enumerate(blocks):
        x = BLOCK_GAP + block_index * (LABEL_WIDTH + 2 * IMAGE_WIDTH + 12)
        y = HEADER
        for entry in block:
            draw.text((x + 2, y + 4), entry["key"], font=label_font, fill=INK)
            draw.text((x + 2, y + 24),
                      "camera {:.1f} m out".format(entry["camera_distance_cm"] / 100.0),
                      font=meta_font, fill=MUTED)
            strength = strengths.get(entry["key"], {})
            draw.text((x + 2, y + 42),
                      "Δ {:.3f} on lit px".format(strength.get("mean_delta_lit", 0.0)),
                      font=meta_font, fill=ON_TINT)
            draw.text((x + 2, y + 60),
                      "lights {:.1f}% of frame".format(strength.get("coverage_pct", 0.0)),
                      font=meta_font, fill=MUTED)

            for column, (state, tint) in enumerate((("off", OFF_TINT), ("on", ON_TINT))):
                image = Image.open(entry["{}_file".format(state)]).convert("RGB")
                resample = getattr(Image, "Resampling", Image).LANCZOS
                image = image.resize((IMAGE_WIDTH, height), resample)
                left = x + LABEL_WIDTH + column * (IMAGE_WIDTH + 6)
                sheet.paste(image, (left, y))
                draw.rectangle((left, y, left + IMAGE_WIDTH - 1, y + height - 1),
                               outline=RULE, width=1)
                tag = "OFF" if state == "off" else "ON"
                draw.text((left + 6, y + 5), tag, font=tag_font, fill=tint)
            y += ROW_HEIGHT

    footer_y = HEADER + rows * ROW_HEIGHT + 8
    draw.text((24, footer_y),
              "A light is A/B'd by setting its intensity to 0 and restoring it — hiding a light "
              "actor does not disable the light, so a \"hide the lights\" test would prove nothing.",
              font=meta_font, fill=MUTED)
    draw.text((24, footer_y + 18),
              "Δ is measured on the pixels each light actually brightens, not over the whole frame: "
              "a whole-frame mean is dominated by how much of the picture the building fills, which "
              "made the two shortest gates look weakest when they are among the strongest.",
              font=meta_font, fill=MUTED)
    draw.text((24, footer_y + 36),
              "Exposure control was verified before these frames were trusted "
              "(EV100 3.0 vs 6.0: mean {:.3f} vs {:.3f}). All nine lights land within "
              "1.17× of each other on that basis.".format(
                  report["self_check"]["means"]["3.0"],
                  report["self_check"]["means"]["6.0"]),
              font=meta_font, fill=MUTED)

    sheet.save(OUT)
    print("wrote", OUT, sheet.size)


if __name__ == "__main__":
    main()
