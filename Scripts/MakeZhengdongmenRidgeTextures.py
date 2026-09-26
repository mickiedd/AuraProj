"""Author the Zhengdongmen roof-ridge (屋脊) texture set.

The ridges were sharing the roof-tile material, so a hip cap carried a corrugated tile
field along its length and read as a continuation of the roof rather than as a ridge.
This authors a dedicated set for them.

Registration with the geometry matters. `ridge_beam` maps u along the ridge at 1.5 m per
repeat and v once around the cap section, so:

- the section's flat base occupies v 0..0.33, the two flanks 0.33..0.49 and 0.84..1.0, and
  the half-round cap 0.49..0.84 — that band is where the visible surface is, and it is
  shaded as weathering (dirt gathering low, washed bright at the crest) rather than as
  baked lighting;
- the mesh already models the cap's crown, so the height map carries ONLY the ridge-tile
  joints along the ridge — four per repeat, a joint every 0.375 m. Painting a second crown
  would double a profile the geometry already has.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

PROJECT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = PROJECT / 'Raw3DPacket/Zhengdongmen/prepared/Textures'

WIDTH, HEIGHT = 1024, 512          # u along the ridge, v around the cap section
BASE_RGB = (86, 86, 82)
JOINTS_PER_REPEAT = 4
JOINT_DEPTH = 0.30                 # height units removed at a joint
ROUGH_BASE, ROUGH_JOINT = 0.60, 0.80

# Section layout, matching Builder.ridge_beam for a cap of radius r.
R = 0.5
TOTAL = 3.0 * R + math.pi * R
V_BASE_END = 2.0 * R / TOTAL                    # end of the flat base
V_FLANK_END = 3.0 * R / TOTAL                   # end of the right flank
V_ARC_END = (2.0 * R + math.pi * R) / TOTAL     # end of the cap arc
V_CREST = (V_FLANK_END + V_ARC_END) / 2.0


def joint_mask() -> np.ndarray:
    """Ridge-tile joints along u: four grooves per repeat, with a soft edge."""
    u = np.arange(WIDTH) / WIDTH
    mask = np.zeros(WIDTH, dtype=np.float32)
    for k in range(JOINTS_PER_REPEAT):
        centre = (k + 0.5) / JOINTS_PER_REPEAT
        offset = np.abs(((u - centre + 0.5) % 1.0) - 0.5)
        mask = np.maximum(mask, np.clip(1.0 - offset / 0.022, 0.0, 1.0))
    return np.tile(mask[None, :], (HEIGHT, 1))


def vertical_shading() -> np.ndarray:
    """Weathering across the section: dirt low, washed bright over the crest."""
    v = np.arange(HEIGHT) / HEIGHT
    washed = np.clip(1.0 - np.abs(v - V_CREST) / 0.30, 0.0, 1.0)
    dirt = np.clip((V_BASE_END - v) / max(V_BASE_END, 1e-6), 0.0, 1.0)
    dirt = np.maximum(dirt, np.clip((v - (V_ARC_END + 0.04)) / 0.12, 0.0, 1.0))
    return (washed * 10.0 - dirt * 16.0).astype(np.float32)[:, None]


def main(out: Path = DEFAULT_OUT) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(1965)
    joint = joint_mask()
    shade = vertical_shading()

    # --- BaseColor ---------------------------------------------------------
    # Streaks run along the ridge, so the noise is stretched in u.
    noise = rng.normal(0.0, 5.0, (HEIGHT, WIDTH))
    noise = np.asarray(Image.fromarray(((noise + 40) * 3).astype(np.uint8), 'L')
                       .filter(ImageFilter.GaussianBlur((1.6, 7.0))), dtype=np.float32)
    noise = (noise - noise.mean()) * 0.9
    stain = np.asarray(Image.fromarray(
        (rng.normal(0.0, 1.0, (HEIGHT, WIDTH)) * 40 + 128).astype(np.uint8), 'L')
        .filter(ImageFilter.GaussianBlur(22)), dtype=np.float32)
    stain = (stain - stain.mean()) * 0.25

    base = np.zeros((HEIGHT, WIDTH, 3), dtype=np.float32)
    for axis, channel in enumerate(BASE_RGB):
        base[:, :, axis] = channel
    # shade is (H,1) and the noises are (H,W), so this broadcasts across the section.
    base += (shade + noise + stain)[:, :, None]
    base -= (joint * 34.0)[:, :, None]
    Image.fromarray(base.clip(0, 255).astype(np.uint8), 'RGB').save(
        out / 'T_ZDM_Ridge_BaseColor.png')

    # --- Height / Normal ---------------------------------------------------
    height = np.full((HEIGHT, WIDTH), 0.55, dtype=np.float32)
    height -= joint * JOINT_DEPTH
    height += (noise * 0.0022).astype(np.float32)
    Image.fromarray((height * 255).astype(np.uint8), 'L').save(
        out / 'T_ZDM_Ridge_Height.png')

    dy, dx = np.gradient(height)
    nx, ny, nz = -dx * 22.0, dy * 22.0, np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / length, ny / length, nz / length], axis=-1)
    Image.fromarray(((normal * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8),
                    'RGB').save(out / 'T_ZDM_Ridge_Normal.png')

    # --- Roughness / Metallic / AO ----------------------------------------
    rough = ROUGH_BASE + joint * (ROUGH_JOINT - ROUGH_BASE)
    rough += (noise * 0.0016).astype(np.float32)
    Image.fromarray((rough * 255).clip(0, 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'T_ZDM_Ridge_Roughness.png')

    Image.new('RGB', (WIDTH, HEIGHT), (0, 0, 0)).save(out / 'T_ZDM_Ridge_Metallic.png')

    ao = 1.0 - joint * 0.22
    Image.fromarray((ao * 255).clip(0, 255).astype(np.uint8), 'L').convert('RGB').save(
        out / 'T_ZDM_Ridge_AO.png')

    return {'size': [WIDTH, HEIGHT], 'maps': 6,
            'joints_per_repeat': JOINTS_PER_REPEAT,
            'joint_spacing_m': round(1.5 / JOINTS_PER_REPEAT, 4),
            'section_v': {'base': [0.0, round(V_BASE_END, 4)],
                          'right_flank': [round(V_BASE_END, 4), round(V_FLANK_END, 4)],
                          'cap_arc': [round(V_FLANK_END, 4), round(V_ARC_END, 4)],
                          'crest': round(V_CREST, 4)},
            'out': str(out)}


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
