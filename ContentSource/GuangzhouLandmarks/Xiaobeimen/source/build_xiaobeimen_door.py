"""Author the Xiaobeimen gate door so its leaves fit the arch.

The V3 asset has no procedural source: `Wooden_Doors__M_AgedWood` and
`Door_Metal__M_Metal` are binary meshes, and the leaves they carry are plain rectangular
blocks 146.6 cm wide with flat tops at z = 344.5 — below the arch's springing, so the arch
above them stands wide open and nothing about the door follows the arc. Measured from
`Arch_Voussoirs__M_StoneWall`:

    springing  z = 370 cm      intrados radius = 265 cm      crown z = 635 cm
    extrados radius = 315 cm   (ring 50 cm thick)

Each leaf's top edge is therefore the intrados: z(x) = 370 + sqrt(265^2 - x^2), so the
leaves rise from 370 at the jambs to 635 where they meet at the centre.

Axes: the Blueprint's components carry a -90 roll, and the dumped meshes show
`actor = (x_mesh, -z_mesh, y_mesh)`, so the mesh's local space is
`(actor_x, actor_z, -actor_y)`. Measured on this GLB: Interchange maps the file's
`(X, Y, Z)` to mesh-local `(X, Z, Y)` at **roll 0**, so the file is authored as
`(actor_x, -actor_y, actor_z)` — Z-up with the height on Z, and the depth negated.
UVs follow the asset's own convention, `world_cm / 80`.
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

# Measured from the arch mesh.
SPRING_Z = 370.0
INNER_R = 265.0
CROWN_Z = SPRING_Z + INNER_R          # 635

LEAF_THICK = 15.0                     # cm
LEAF_FRONT_Y = 445.0                  # actor y of the outer face (wall face is 450)
LEAF_BACK_Y = LEAF_FRONT_Y - LEAF_THICK
CENTRE_GAP = 0.0                      # the leaves meet: any gap shows sky through the unmodelled tunnel
JAMB_GAP = 1.0                        # cm at each jamb
ARC_STEP = 0.25                       # cm between profile samples; the arc is the door's silhouette
UV_SCALE = 80.0                       # cm per UV unit, the asset's own convention

STUD_RADIUS = 4.2
STUD_HEIGHT = 3.4
STUD_COLUMNS = 7
STUD_ROWS = 10
STUD_RING = 8


def top_z(x: float) -> float:
    """The arch's intrados at |x|, clamped so a leaf never exceeds the opening."""
    inside = INNER_R ** 2 - x ** 2
    return SPRING_Z + math.sqrt(max(0.0, inside))


def leaf_outline(sign: int) -> list[tuple[float, float]]:
    """(x, z) outline of one leaf: up the jamb, across the arc, back down the centre."""
    outer = sign * (INNER_R - JAMB_GAP)
    inner = sign * CENTRE_GAP
    steps = max(2, int(abs(outer - inner) / ARC_STEP))
    xs = np.linspace(inner, outer, steps + 1)
    outline = [(inner, 0.0), (inner, top_z(inner))]
    outline += [(float(x), top_z(float(x))) for x in xs[1:]]
    outline.append((outer, 0.0))
    return outline


def prism(outline, y_back, y_front, uv_scale=UV_SCALE):
    """Extrude an (x, z) outline along mesh y, with planar UVs on the big faces.

    Mesh y carries -actor_y (the depth) and mesh z carries actor_z (the height), which is
    the file layout Interchange maps to the mesh-local space the components expect.
    """
    n = len(outline)
    verts, uvs, faces = [], [], []

    def push(x, y, z, u, v):
        verts.append((x, y, z))
        uvs.append((u, v))
        return len(verts) - 1

    back = [push(x, y_back, z, x / uv_scale, z / uv_scale) for x, z in outline]
    front = [push(x, y_front, z, x / uv_scale, z / uv_scale) for x, z in outline]
    for i in range(1, n - 1):                       # back and front caps (fan)
        faces.append((back[0], back[i], back[i + 1]))
        faces.append((front[0], front[i + 1], front[i]))
    for i in range(n):                              # the side wall
        j = (i + 1) % n
        faces.append((back[i], front[i], front[j]))
        faces.append((back[i], front[j], back[j]))
    return verts, uvs, faces


def stud_mesh():
    """A grid of studs on each leaf's outer face, following the leaf's arc."""
    verts, uvs, faces = [], [], []
    for sign in (-1, 1):
        outer = sign * (INNER_R - JAMB_GAP)
        inner = sign * CENTRE_GAP
        for column in range(STUD_COLUMNS):
            frac = (column + 0.5) / STUD_COLUMNS
            x = inner + (outer - inner) * frac
            headroom = top_z(x) - 24.0
            for row in range(STUD_ROWS):
                z = 30.0 + (headroom - 30.0) * (row + 0.5) / STUD_ROWS
                base = len(verts)
                # mesh y = -actor_y, so the outer face (actor y = 445) is mesh y = -445
                # and a stud stands proud of it along -y.
                centre = np.array([x, -LEAF_FRONT_Y, z])
                axis = np.array([0.0, -1.0, 0.0])
                helper = np.array([1.0, 0.0, 0.0])
                u = np.cross(axis, helper)
                v = np.cross(axis, u)
                # a shallow dome: a ring at the base plus an apex
                ring = []
                for k in range(STUD_RING):
                    a = 2 * math.pi * k / STUD_RING
                    p = centre + u * (STUD_RADIUS * math.cos(a)) \
                        + v * (STUD_RADIUS * math.sin(a))
                    verts.append((float(p[0]), float(p[1]), float(p[2])))
                    uvs.append((p[0] / UV_SCALE, p[2] / UV_SCALE))
                    ring.append(base + k)
                apex = centre + axis * STUD_HEIGHT
                verts.append((float(apex[0]), float(apex[1]), float(apex[2])))
                uvs.append((apex[0] / UV_SCALE, apex[2] / UV_SCALE))
                tip = len(verts) - 1
                for k in range(STUD_RING):
                    faces.append((ring[k], ring[(k + 1) % STUD_RING], tip))
    return verts, uvs, faces


def build(name, verts, uvs, faces, colour):
    mesh = trimesh.Trimesh(vertices=np.array(verts, dtype=np.float32),
                           faces=np.array(faces, dtype=np.int64),
                           process=False, validate=False)
    mesh.visual = TextureVisuals(
        uv=np.array(uvs, dtype=np.float32),
        material=PBRMaterial(name=name, baseColorFactor=colour,
                             metallicFactor=0.0, roughnessFactor=0.85))
    return mesh


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # mesh y = -actor_y, so the leaf's outer face (actor y = 445) is mesh y = -445.
    y_front, y_back = -LEAF_FRONT_Y, -LEAF_BACK_Y

    door_verts, door_uvs, door_faces = [], [], []
    for sign in (-1, 1):
        outline = leaf_outline(sign)
        v, u, f = prism(outline, y_back, y_front)
        base = len(door_verts)
        door_verts += v
        door_uvs += u
        door_faces += [(a + base, b + base, c + base) for a, b, c in f]
    leaves = build('M_AgedWood', door_verts, door_uvs, door_faces, (92, 62, 44))
    leaves.export(OUT / 'Wooden_Doors__M_AgedWood.glb')

    sv, su, sf = stud_mesh()
    studs = build('M_Metal', sv, su, sf, (58, 58, 60))
    studs.export(OUT / 'Door_Metal__M_Metal.glb')

    report = {
        'arch': {'springing_z': SPRING_Z, 'intrados_r': INNER_R, 'crown_z': CROWN_Z},
        'leaves': {'thickness_cm': LEAF_THICK, 'front_actor_y': LEAF_FRONT_Y,
                   'back_actor_y': LEAF_BACK_Y, 'centre_gap_cm': CENTRE_GAP,
                   'jamb_gap_cm': JAMB_GAP},
        'door_triangles': len(door_faces), 'door_vertices': len(door_verts),
        'stud_triangles': len(sf), 'stud_vertices': len(sv),
        'studs_per_leaf': STUD_COLUMNS * STUD_ROWS,
        'uv_scale_cm': UV_SCALE,
        'files': [p.name for p in sorted(OUT.glob('*.glb'))],
    }
    (OUT / 'door-generation.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
