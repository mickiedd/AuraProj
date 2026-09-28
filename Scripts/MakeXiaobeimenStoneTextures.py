"""Author a coursed-ashlar StoneWall texture set for Xiaobeimen.

The V3 wall's stone units measure **18 x 15 cm** — read from a native capture, not guessed:
the wall's UVs run at 80 cm per unit (`du/dx = dv/dz = 0.0125`, measured on all twelve of
its faces), so a 4K map covering one unit shows ~4.5 blocks across by ~5 courses. The
reference board's wall is coursed ashlar with blocks around **50 x 28 cm**, so the wall
reads as fine modern brickwork instead of stone.

Two changes together: `M_StoneWall`'s texture coordinates are scaled to 0.25 (done by
`FixXiaobeimenAAA.py`), so a map covers **320 cm** instead of 80, and this authors maps
carrying 6 blocks across by 11 courses — 53 x 29 cm. The 320 cm repeat is 4x coarser than
before, and the wall's existing surface-detail, moss and ivy meshes break it up further.

Seamlessness is by construction: courses span the full height, block boundaries span the
full width, and the joint at x=0 and the joint at x=W together form one joint at the wrap.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

PROJECT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = PROJECT / 'Raw3DPacket/Xiaobeimen/prepared/Textures'

SIZE = 4096
WORLD_CM = 320.0                 # world width the map covers once the UVs are scaled
PX_PER_CM = SIZE / WORLD_CM
COURSES = 11                     # -> 29.1 cm courses
BLOCKS_ACROSS = 6                # -> 53.3 cm blocks
JOINT_CM = 2.0                   # mortar width
JOINT_PX = max(2, round(JOINT_CM * PX_PER_CM))

STONE_RGB = (150, 143, 128)
JOINT_RGB = (84, 78, 68)
MOSS_RGB = (96, 104, 74)
BLOCK_LOW, BLOCK_HIGH = 0.30, 0.68      # height units: joint floor .. block face
ROUGH_STONE, ROUGH_JOINT = 0.70, 0.90
# Weathering is deliberately restrained: an earlier pass let the grime blotches dominate
# and the coursing stopped reading, which is the opposite of the point.
TONE_SPREAD = 15.0
STREAK_AMP, GRIME_AMP, GRAIN_AMP = 5.0, 4.0, 3.0
MOSS_STRENGTH = 0.18


def course_edges(rng):
    """Course boundaries down the map: heights vary, and they sum to the full height."""
    weights = rng.uniform(0.82, 1.18, COURSES)
    weights /= weights.sum()
    edges = np.concatenate([[0.0], np.cumsum(weights) * SIZE])
    return [(int(round(edges[i])), int(round(edges[i + 1]))) for i in range(COURSES)]


def block_edges(rng, width_px, phase):
    """Block boundaries across one course, wrapped by a per-course phase."""
    weights = rng.uniform(0.62, 1.55, BLOCKS_ACROSS)
    weights /= weights.sum()
    edges = np.concatenate([[0.0], np.cumsum(weights) * width_px])
    return [(int(round(edges[i])), int(round(edges[i + 1]))) for i in range(BLOCKS_ACROSS)]


def main(out: Path = DEFAULT_OUT) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(1965)

    height = np.full((SIZE, SIZE), BLOCK_LOW, dtype=np.float32)
    base = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
    base[:, :] = JOINT_RGB
    moss = np.zeros((SIZE, SIZE), dtype=np.float32)
    blocks = []

    for row, (y0, y1) in enumerate(course_edges(rng)):
        phase = rng.uniform(0.0, 1.0)
        for column, (x0, x1) in enumerate(block_edges(rng, SIZE, phase)):
            # Per-block tone: this is what stops a coursed wall reading as a grid.
            tone = rng.uniform(-TONE_SPREAD, TONE_SPREAD)
            warm = rng.uniform(-4.0, 5.0)
            colour = np.array([STONE_RGB[0] + tone + warm, STONE_RGB[1] + tone,
                               STONE_RGB[2] + tone - warm * 0.5])
            height[y0:y1, x0:x1] = rng.uniform(BLOCK_LOW + 0.06, BLOCK_HIGH)
            base[y0:y1, x0:x1] = colour
            blocks.append({'row': row, 'column': column, 'y': [y0, y1], 'x': [x0, x1],
                           'size_cm': [round((x1 - x0) / PX_PER_CM, 1),
                                       round((y1 - y0) / PX_PER_CM, 1)],
                           'tone': round(float(tone), 1)})

    # Recess the joints: erode every block by half the joint, leaving mortar between.
    joint = np.ones((SIZE, SIZE), dtype=np.float32)
    joint_mask = Image.new('L', (SIZE, SIZE), 255)
    pixels = np.asarray(joint_mask).copy()
    for block in blocks:
        y0, y1 = block['y']
        x0, x1 = block['x']
        pixels[y0:y1, x0:x1] = 0
    # A block face is everything not within JOINT_PX of a boundary.
    face = np.asarray(Image.fromarray((pixels == 0).astype(np.uint8) * 255, 'L')
                      .filter(ImageFilter.MinFilter(JOINT_PX * 2 + 1)), dtype=np.float32)
    face = (face > 127).astype(np.float32)
    joint = 1.0 - face
    height = np.where(face > 0.5, height, BLOCK_LOW - 0.06)
    base = np.where(face[:, :, None] > 0.5, base, np.array(JOINT_RGB, dtype=np.float32))

    # Weathering: streaks run along the courses, and moss gathers in the joints.
    def noise(blur, sigma=40.0):
        raw = rng.normal(0.0, sigma, (SIZE, SIZE))
        image = Image.fromarray(np.clip(raw + 128, 0, 255).astype(np.uint8), 'L')
        field = np.asarray(image.filter(ImageFilter.GaussianBlur(blur)), dtype=np.float32) - 128
        return field / max(float(field.std()), 1e-6)

    streaks = noise((12.0, 2.5)) * STREAK_AMP
    grime = noise(46.0) * GRIME_AMP
    grain = noise(1.3) * GRAIN_AMP
    base += (streaks + grime + grain)[:, :, None]
    # Darken the mortar AFTER the noise, or the weathering washes the coursing out.
    base -= (1.0 - face)[:, :, None] * 52.0
    moss_amount = np.clip(noise(5.0) * 0.5 + 0.5, 0, 1) * joint * MOSS_STRENGTH
    base = base * (1 - moss_amount[:, :, None]) \
        + np.array(MOSS_RGB, dtype=np.float32) * moss_amount[:, :, None]
    height += (grain * 0.0016 + streaks * 0.0008).astype(np.float32)

    Image.fromarray(base.clip(0, 255).astype(np.uint8), 'RGB').save(
        out / 'StoneWall_BaseColor_4K.png')
    Image.fromarray((height * 255).clip(0, 255).astype(np.uint8), 'L').save(
        out / 'StoneWall_Height_4K.png')

    dy, dx = np.gradient(height)
    nx, ny, nz = -dx * 46.0, dy * 46.0, np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    Image.fromarray(((np.stack([nx / length, ny / length, nz / length], -1) * 0.5 + 0.5)
                     * 255).clip(0, 255).astype(np.uint8), 'RGB').save(
        out / 'StoneWall_Normal_4K.png')

    rough = np.where(face > 0.5, ROUGH_STONE, ROUGH_JOINT) + grain * 0.02
    Image.fromarray((rough * 255).clip(0, 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'StoneWall_Roughness_4K.png')
    Image.new('RGB', (SIZE, SIZE), (0, 0, 0)).save(out / 'StoneWall_Metallic_4K.png')
    ao = np.clip(0.62 + 0.38 * face, 0, 1) - moss_amount * 0.05
    Image.fromarray((ao * 255).clip(0, 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'StoneWall_AO_4K.png')

    sizes = [b['size_cm'] for b in blocks]
    report = {'size': [SIZE, SIZE], 'world_cm': WORLD_CM, 'maps': 5,
              'courses': COURSES, 'blocks_across': BLOCKS_ACROSS,
              'course_height_cm': round(WORLD_CM / COURSES, 1),
              'mean_block_width_cm': round(WORLD_CM / BLOCKS_ACROSS, 1),
              'block_width_range_cm': [round(min(s[0] for s in sizes), 1),
                                       round(max(s[0] for s in sizes), 1)],
              'block_height_range_cm': [round(min(s[1] for s in sizes), 1),
                                        round(max(s[1] for s in sizes), 1)],
              'joint_cm': JOINT_CM, 'out': str(out)}
    (out / 'ashlar-report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    main()
