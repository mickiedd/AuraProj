"""Author the Zhengdongmen gate plaque (门额) texture set at its true proportions.

The shipped Compact artwork letterboxes three simplified characters (正东门) into a
square canvas, which the plaque quad then stretches 4.34:1 — so the characters
render roughly twice as wide as they are tall. This rebuilds the set at the board's
own aspect ratio, in the traditional forms the reference board uses (正東門), in a
traditional serif rather than a modern geometric sans.

Every map is generated together: a BaseColor swapped without its Height/Normal/
Roughness/Metallic/AO partners would smear the old square relief across the new
board. Written into the staged texture folder that the importer consumes, and
called from PrepareZhengdongmenPackage.py so the pipeline stays reproducible.
"""
from __future__ import annotations

import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

PROJECT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = PROJECT / 'Raw3DPacket/Zhengdongmen/prepared/Textures'

# The plaque quad is 3.3 m x 0.76 m (build_zhengdongmen.py, Signboard_Zhengdongmen).
BOARD_W_M, BOARD_H_M = 3.3, 0.76
WIDTH, HEIGHT = 2048, 512
PPM_X, PPM_Y = WIDTH / BOARD_W_M, HEIGHT / BOARD_H_M

GLYPH = '正東門'
FONT_PATH = '/System/Library/Fonts/Supplemental/Songti.ttc'
FONT_INDEX = 2                      # Songti TC Bold
GLYPH_H_M = 0.50                    # character height on the board
GLYPH_CELL_M = 0.56                 # centre-to-centre spacing

FRAME_MARGIN_M = 0.072
FRAME_THICK_M = 0.042

BOARD_RGB = (57, 32, 25)
GOLD_RGB = (223, 185, 102)
FRAME_RGB = (198, 158, 84)


def mx(metres: float) -> float:
    return metres * PPM_X


def my(metres: float) -> float:
    return metres * PPM_Y


def glyph_layer() -> Image.Image:
    """The three characters, drawn at their true on-board proportions."""
    px_h, px_w = round(my(GLYPH_H_M)), round(mx(GLYPH_H_M))
    font = ImageFont.truetype(FONT_PATH, px_h, index=FONT_INDEX)
    box = font.getbbox(GLYPH)
    canvas_h = box[3] - box[1]
    layer = Image.new('L', (px_w * len(GLYPH), canvas_h), 0)
    draw = ImageDraw.Draw(layer)
    for index, character in enumerate(GLYPH):
        draw.text((index * px_w, -box[1]), character, font=font, fill=255)
    # The board is 4.34:1 but the canvas is 4:1, so squeeze the glyphs by the
    # residual ratio and they land square on the quad.
    target = round(layer.width * (BOARD_W_M / BOARD_H_M) / (WIDTH / HEIGHT))
    return layer.resize((target, canvas_h), Image.LANCZOS)


def paste_glyphs(board: Image.Image, mask: Image.Image, colour) -> None:
    centre_x = (WIDTH - mask.width) // 2
    centre_y = (HEIGHT - mask.height) // 2
    board.paste(Image.new('RGB', mask.size, colour), (centre_x, centre_y), mask)


def compose_masks():
    """Return (frame_mask, glyph_mask, glyph_mask_placed) as numpy floats."""
    x0, x1 = round(mx(FRAME_MARGIN_M)), round(WIDTH - mx(FRAME_MARGIN_M))
    y0, y1 = round(my(FRAME_MARGIN_M)), round(HEIGHT - my(FRAME_MARGIN_M))
    ti_x, ti_y = round(mx(FRAME_THICK_M)), round(my(FRAME_THICK_M))
    outer = Image.new('L', (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(outer).rectangle((x0, y0, x1, y1), fill=255)
    inner = Image.new('L', (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(inner).rectangle((x0 + ti_x, y0 + ti_y, x1 - ti_x, y1 - ti_y), fill=255)
    frame = np.asarray(Image.composite(Image.new('L', (WIDTH, HEIGHT), 0), outer, inner),
                       dtype=np.float32) / 255.0
    glyph = glyph_layer()
    placed = np.zeros((HEIGHT, WIDTH), dtype=np.float32)
    gx, gy = (WIDTH - glyph.width) // 2, (HEIGHT - glyph.height) // 2
    placed[gy:gy + glyph.height, gx:gx + glyph.width] = np.asarray(glyph, dtype=np.float32) / 255.0
    return frame, placed, (gx, gy, glyph)


def sobel_normal(height: np.ndarray, strength: float) -> Image.Image:
    dy, dx = np.gradient(height)
    nx, ny, nz = -dx * strength, dy * strength, np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / length, ny / length, nz / length], axis=-1)
    rgb = ((normal * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    return Image.fromarray(rgb, 'RGB')


def main(out: Path = DEFAULT_OUT) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    frame, glyph, (gx, gy, glyph_img) = compose_masks()

    # --- BaseColor ---------------------------------------------------------
    base = Image.new('RGB', (WIDTH, HEIGHT), BOARD_RGB)
    # A faint horizontal grain, so the board does not read as flat colour.
    grain = np.random.default_rng(1965).normal(0.0, 3.4, (HEIGHT, WIDTH, 1))
    grain[:, :, 0] *= np.linspace(1.25, 0.8, WIDTH)[None, :]
    pixels = np.asarray(base, dtype=np.float32) + grain
    base = Image.fromarray(pixels.clip(0, 255).astype(np.uint8), 'RGB')
    paste_glyphs(base, Image.fromarray((frame * 255).astype(np.uint8), 'L'), FRAME_RGB)
    # A one-pixel darker seat under the characters reads as carved relief.
    seat = glyph_img.filter(ImageFilter.MaxFilter(3))
    seat_mask = Image.new('L', seat.size, 0)
    seat_mask.paste(seat, (gx, gy))
    shadow = Image.new('RGB', (WIDTH, HEIGHT), (34, 18, 14))
    shifted = Image.new('L', (WIDTH, HEIGHT), 0)
    shifted.paste(seat_mask, (2, 2))
    base.paste(shadow, (0, 0), shifted.point(lambda v: int(v * 0.55)))
    paste_glyphs(base, Image.fromarray((glyph * 255).astype(np.uint8), 'L'), GOLD_RGB)
    base.save(out / 'T_ZDM_Sign_BaseColor.png')

    # --- Height / Normal ---------------------------------------------------
    height = np.full((HEIGHT, WIDTH), 0.42, dtype=np.float32)
    height = np.maximum(height, frame * 0.76)
    height = np.maximum(height, glyph * 0.88)
    Image.fromarray((height * 255).astype(np.uint8), 'L').save(out / 'T_ZDM_Sign_Height.png')
    sobel_normal(height, 26.0).save(out / 'T_ZDM_Sign_Normal.png')

    # --- Roughness / Metallic / AO ----------------------------------------
    rough = np.full((HEIGHT, WIDTH), 0.78, dtype=np.float32)
    rough = np.where(frame > 0.5, 0.42, rough)
    rough = np.where(glyph > 0.5, 0.36, rough)
    Image.fromarray((rough * 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'T_ZDM_Sign_Roughness.png')

    metal = np.zeros((HEIGHT, WIDTH), dtype=np.float32)
    metal = np.where(frame > 0.5, 0.70, metal)
    metal = np.where(glyph > 0.5, 0.78, metal)
    Image.fromarray((metal * 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'T_ZDM_Sign_Metallic.png')

    ao = np.ones((HEIGHT, WIDTH), dtype=np.float32)
    groove = np.clip(Image.fromarray((height * 255).astype(np.uint8), 'L')
                     .filter(ImageFilter.GaussianBlur(3)), None, None)
    ao = 0.72 + 0.28 * (np.asarray(groove, dtype=np.float32) / 255.0)
    Image.fromarray((ao * 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'T_ZDM_Sign_AO.png')

    return {'glyph': GLYPH, 'font': f'{Path(FONT_PATH).name}[{FONT_INDEX}]',
            'size': [WIDTH, HEIGHT], 'board_m': [BOARD_W_M, BOARD_H_M],
            'glyph_height_m': GLYPH_H_M, 'maps': 6, 'out': str(out)}


if __name__ == '__main__':
    import json
    print(json.dumps(main(), indent=2))
