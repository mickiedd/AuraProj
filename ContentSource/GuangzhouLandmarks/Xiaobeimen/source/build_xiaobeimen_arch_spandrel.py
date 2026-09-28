"""Author a finely-tessellated Xiaobeimen arch spandrel.

`Wall_ArchSpandrel__M_StoneWall` is the pair of panels that give the gate its arch: each is
bounded below by the intrados and above by a flat top, swept through the wall's depth. It is
tessellated at roughly **4 degrees**, an 18 cm chord on a 265 cm radius, and that coarse edge
is the 锯齿 visible where the wall meets the opening — the door's own arc is already at 1 cm
and the voussoir ring at 6 degrees, so neither is the cause.

Measured from the existing mesh and the arch dump:

    intrados    r = 265 cm centred at (x 0, z 370)   -> crown z 635
    panel       x +-260, top z 630.2, depth +-450 (the wall's full depth)
    coverage    {(x, z) : |x| <= 260, z_arc(x) <= z <= 630.2}

The region is empty where the arc rises above the top, which is what makes it two panels
rather than one. Rebuilt here at 0.5 degrees, and with UVs at the asset's own convention of
80 cm per unit so it matches every other stone part.

Axes: mesh-local is `(actor_x, actor_z, -actor_y)` and Interchange maps the file's
`(X, Y, Z)` to `(X, Z, Y)` at roll 0, so the file is authored as `(actor_x, -actor_y, actor_z)`.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
import trimesh
from trimesh.visual.material import PBRMaterial
from trimesh.visual.texture import TextureVisuals

PROJECT = Path(__file__).resolve().parents[4]
OUT = PROJECT / 'Saved/Reports/Xiaobeimen/door-glb'

SPRING_Z = 370.0
INNER_R = 265.0
PANEL_X = 260.0
PANEL_TOP = 630.2
DEPTH = 450.0
UV_SCALE = 80.0
ARC_DEG = 0.5


def arc_z(x: float) -> float:
    return SPRING_Z + math.sqrt(max(0.0, INNER_R ** 2 - x ** 2))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Sample the arc by ANGLE so every chord is the same length. Points whose arc rises
    # above the panel's top are dropped, which is what splits the panel in two.
    profile = []
    for step in range(int(round(180.0 / ARC_DEG)) + 1):
        theta = step * ARC_DEG
        x = INNER_R * math.cos(math.radians(theta))
        z = SPRING_Z + INNER_R * math.sin(math.radians(theta))
        if abs(x) > PANEL_X or z >= PANEL_TOP - 1e-6:
            continue
        profile.append((max(-PANEL_X, min(PANEL_X, x)), z))
    profile.sort()
    # Merge any columns that clipping collapsed onto the same x.
    merged = []
    for x, z in profile:
        if merged and abs(merged[-1][0] - x) < 1e-6:
            continue
        merged.append((x, z))
    profile = merged

    verts, uvs, faces = [], [], []

    def push(px, py, pz, u, v):
        verts.append((px, py, pz))
        uvs.append((u, v))
        return len(verts) - 1

    for i in range(len(profile) - 1):
        (x0, z0), (x1, z1) = profile[i], profile[i + 1]
        for depth_sign in (-1.0, 1.0):                 # front and back faces
            y = depth_sign * DEPTH
            a = push(x0, y, z0, x0 / UV_SCALE, z0 / UV_SCALE)
            b = push(x1, y, z1, x1 / UV_SCALE, z1 / UV_SCALE)
            c = push(x1, y, PANEL_TOP, x1 / UV_SCALE, PANEL_TOP / UV_SCALE)
            d = push(x0, y, PANEL_TOP, x0 / UV_SCALE, PANEL_TOP / UV_SCALE)
            # (a, b, c) faces -mesh-y, which is +actor_y: the facade. Reverse for the back.
            faces += [(a, b, c), (a, c, d)] if depth_sign < 0 else [(a, c, b), (a, d, c)]
        # the arc soffit, swept through the depth, facing into the opening
        e = push(x0, -DEPTH, z0, x0 / UV_SCALE, -DEPTH / UV_SCALE)
        f = push(x1, -DEPTH, z1, x1 / UV_SCALE, -DEPTH / UV_SCALE)
        g = push(x1, DEPTH, z1, x1 / UV_SCALE, DEPTH / UV_SCALE)
        h = push(x0, DEPTH, z0, x0 / UV_SCALE, DEPTH / UV_SCALE)
        faces += [(e, g, f), (e, h, g)]

    mesh = trimesh.Trimesh(vertices=np.array(verts, dtype=np.float32),
                           faces=np.array(faces, dtype=np.int64),
                           process=False, validate=False)
    mesh.visual = TextureVisuals(
        uv=np.array(uvs, dtype=np.float32),
        material=PBRMaterial(name='M_StoneWall', baseColorFactor=(150, 143, 128),
                             metallicFactor=0.0, roughnessFactor=0.8))
    mesh.export(OUT / 'Wall_ArchSpandrel__M_StoneWall.glb')

    report = {'profile_columns': len(profile), 'vertices': len(verts), 'triangles': len(faces),
              'arc_deg': ARC_DEG,
              'chord_on_intrados_cm': round(2 * math.pi * INNER_R * (ARC_DEG / 360.0), 3),
              'sagitta_cm': round(INNER_R * (1 - math.cos(math.radians(ARC_DEG) / 2)), 5),
              'old_chord_cm': 18.0, 'uv_scale_cm': UV_SCALE,
              'out': str(OUT)}
    (OUT / 'spandrel-generation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
