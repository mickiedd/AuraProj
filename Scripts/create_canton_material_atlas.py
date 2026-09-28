"""Assemble the four fixed-camera provisional surface QA captures."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


root = Path(__file__).resolve().parents[1]
source = root / "QA/Canton_District"
destination = root / "Review/M03_Material_Atlas.png"
destination.parent.mkdir(parents=True, exist_ok=True)
items = [
    ("main_street.png", "Principal slab street"),
    ("mixed_lane.png", "Mixed lane"),
    ("gate_approach.png", "Gate approach — scale test"),
    ("courtyard_edge.png", "Courtyard edge — provisional"),
]
tile_w, tile_h, strip, banner = 720, 500, 40, 52
atlas = Image.new("RGB", (tile_w * 2, banner + (tile_h + strip) * 2), "#e5e0d3")
draw = ImageDraw.Draw(atlas)
font_path = Path("/System/Library/Fonts/Supplemental/Arial.ttf")
font = ImageFont.truetype(str(font_path), 19) if font_path.is_file() else ImageFont.load_default()
draw.text((18, 15), "PROVISIONAL ENGINEERING DISTRICT — historical XY / Z not accepted",
          fill="#27231e", font=font)
for index, (filename, label) in enumerate(items):
    x = (index % 2) * tile_w
    y = banner + (index // 2) * (tile_h + strip)
    image = Image.open(source / filename).convert("RGB").resize((tile_w, tile_h))
    atlas.paste(image, (x, y + strip))
    draw.text((x + 12, y + 10), label, fill="#27231e", font=font)
atlas.save(destination)
print(destination)
