"""Generate additive, silhouette-visible UltraAAA hero meshes for five gates.

The meshes are deliberately separate from every previous base/AAA/AAADeep asset.
Masonry is authored as individually irregular, tessellated blocks; roofs are
made from separate staggered tile ribs and caps; brackets, voussoirs, doors,
bolts, quoins and vegetation are physical geometry.  The extra tessellation is
surface relief on visible pieces, not hidden triangle padding.
"""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

from GenerateGuangzhouLandmarkDeepGeometry import Builder, add_arch, write_fbx


PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / "Saved/RawModelImport/V4/UltraAAA"


def add_relief_block(builder, material, center, size, seed, grid=5, primitive="irregular_stone_block"):
    """Add a closed, visibly uneven high-resolution block with chamfer-like relief."""
    cx, cy, cz = (float(v) for v in center)
    sx, sy, sz = (float(v) for v in size)
    rng = random.Random(seed)
    verts, faces, uvs = [], [], []

    # Faces are independent so the block keeps hard dressed-stone breaks.
    face_specs = (
        ("y", -1.0), ("y", 1.0), ("x", -1.0),
        ("x", 1.0), ("z", -1.0), ("z", 1.0),
    )
    for axis, sign in face_specs:
        start = len(verts)
        for j in range(grid + 1):
            v = j / grid
            for i in range(grid + 1):
                u = i / grid
                relief = (rng.random() - 0.5) * min(sx, sy, sz) * 0.10
                if axis == "y":
                    p = (cx - sx / 2 + sx * u, cy + sign * sy / 2 + relief, cz - sz / 2 + sz * v)
                elif axis == "x":
                    p = (cx + sign * sx / 2 + relief, cy - sy / 2 + sy * u, cz - sz / 2 + sz * v)
                else:
                    p = (cx - sx / 2 + sx * u, cy - sy / 2 + sy * v, cz + sign * sz / 2 + relief)
                verts.append(p)
                uvs.append((u, v))
        row = grid + 1
        for j in range(grid):
            for i in range(grid):
                a = start + j * row + i
                b = a + 1
                c = a + row + 1
                d = a + row
                if (i + j) % 2:
                    faces.extend(((a, b, d), (b, c, d)))
                else:
                    faces.extend(((a, b, c), (a, c, d)))
    builder._group(material).add(verts, faces, uvs, primitive)


def add_sphere(builder, material, center, radius, seed, primitive="domed_bolt"):
    cx, cy, cz = center
    rings, sides = 6, 12
    verts, faces, uvs = [], [], []
    for r in range(rings + 1):
        phi = math.pi * r / rings
        for s in range(sides):
            theta = math.tau * s / sides
            wobble = 1.0 + (random.Random(seed + r * 31 + s).random() - 0.5) * 0.05
            verts.append((cx + radius * wobble * math.sin(phi) * math.cos(theta),
                          cy + radius * wobble * math.sin(phi) * math.sin(theta),
                          cz + radius * wobble * math.cos(phi)))
            uvs.append((s / sides, r / rings))
    for r in range(rings):
        for s in range(sides):
            n = (s + 1) % sides
            a = r * sides + s
            b = r * sides + n
            c = (r + 1) * sides + n
            d = (r + 1) * sides + s
            faces.extend(((a, b, c), (a, c, d)))
    builder._group(material).add(verts, faces, uvs, primitive)


def basis_copy(builder):
    """Exchange depth/height for blueprints whose established component roll is -90."""
    out = Builder(tuple(builder.groups))
    for name, group in builder.groups.items():
        target = out.groups[name]
        target.vertices = [(x, z, y) for x, y, z in group.vertices]
        target.faces = list(group.faces)
        target.uvs = list(group.uvs)
        target.primitive_counts = dict(group.primitive_counts)
    return out


def add_masonry(builder, stone, width, wall_h, wall_d, opening_r, spring, block_w, block_h, seed_base):
    for side, y in enumerate((-wall_d / 2, wall_d / 2)):
        for row, z in enumerate(range(35, int(wall_h), int(block_h))):
            x = -width / 2 + block_w / 2 + (block_w * 0.47 if row % 2 else 0.0)
            col = 0
            while x < width / 2:
                opening = abs(x) < opening_r * 1.03 and z < spring + block_h
                if not opening:
                    bw = block_w * (0.94 + 0.08 * ((col + row) % 3) / 2)
                    bh = block_h * (0.90 + 0.08 * ((col + row + side) % 3) / 2)
                    add_relief_block(builder, stone, (x, y, z), (bw, wall_d * 0.18, bh),
                                     seed_base + side * 100000 + row * 1000 + col, grid=5)
                x += block_w
                col += 1
        add_arch(builder, stone, y, spring, opening_r, opening_r + 118.0, segments=36)
        for x in (-opening_r - 35.0, opening_r + 35.0):
            for row, z in enumerate(range(80, int(spring + 120), int(block_h * 1.25))):
                add_relief_block(builder, stone, (x, y, z), (120.0, wall_d * 0.22, block_h * 1.15),
                                 seed_base + 700000 + side * 10000 + row, grid=4, primitive="arched_jamb_quoin")
        # Battered cap and irregular merlons are full silhouette pieces.
        builder.add_box(stone, (0.0, y, wall_h + 25.0), (width + 80.0, wall_d * 0.24, 56.0),
                         primitive="worn_wall_cap", bevel=12.0)
        for idx, x in enumerate(range(int(-width / 2 + 120), int(width / 2 - 80), 255)):
            if abs(x) < opening_r * 1.45:
                continue
            builder.add_box(stone, (float(x), y, wall_h + 120.0), (150.0, wall_d * 0.36, 180.0),
                             primitive="crenellation_merlon", bevel=18.0)
    # Deep visible corner quoins.
    for x in (-width / 2 + 40, width / 2 - 40):
        for row, z in enumerate(range(100, int(wall_h), 150)):
            add_relief_block(builder, stone, (x, -wall_d / 2, z), (180.0, wall_d * 0.32, 142.0),
                             seed_base + 900000 + row, grid=4, primitive="corner_quoin")
            add_relief_block(builder, stone, (x, wall_d / 2, z), (180.0, wall_d * 0.32, 142.0),
                             seed_base + 910000 + row, grid=4, primitive="corner_quoin")
    # Worn threshold/sill made of individually dished slabs rather than one razor box.
    for i, x in enumerate((-opening_r * 0.58, 0.0, opening_r * 0.58)):
        builder.add_box(stone, (x, 0.0, 22.0 + (i % 2) * 4), (opening_r * 0.55, wall_d * 0.86, 42.0),
                        primitive="worn_threshold_slab", bevel=10.0)


def add_door(builder, wood, iron, y, width, height, seed):
    leaves = (-width / 4, width / 4)
    for leaf, cx in enumerate(leaves):
        for plank in range(5):
            x = cx - width / 10 * 2 + plank * width / 10
            builder.add_box(wood, (x, y, height / 2), (width / 11, 38.0, height - 24.0),
                            primitive="door_plank_with_gap", bevel=7.0)
        for strap in (-width / 7, 0.0, width / 7):
            builder.add_box(iron, (cx + strap, y - 24.0, height * 0.53), (18.0, 18.0, height * 0.77),
                            primitive="door_iron_strap", bevel=4.0)
        for row in range(5):
            for col in range(2):
                add_sphere(builder, iron, (cx - width / 12 + col * width / 6, y - 42.0,
                                            110.0 + row * (height - 190.0) / 4), 21.0,
                           seed + leaf * 100 + row * 10 + col)
    for yoff in (-width * 0.55, width * 0.55):
        add_sphere(builder, iron, (yoff, y - 47.0, height * 0.50), 42.0, seed + int(yoff), primitive="door_ring_mount")


def add_tower(builder, wood, roof, iron, width, depth, base_z, levels, seed):
    for level in range(levels):
        floor_z = base_z + level * 480.0
        top_z = floor_z + 330.0
        for y in (-depth / 2, depth / 2):
            builder.add_box(wood, (0.0, y, floor_z), (width, 78.0, 62.0), primitive="timber_sill", bevel=10.0)
            builder.add_box(wood, (0.0, y, top_z), (width + 70.0, 82.0, 66.0), primitive="timber_header", bevel=10.0)
            posts = max(7, int(width / 250))
            for idx in range(posts):
                x = -width / 2 + 90 + idx * (width - 180) / max(1, posts - 1)
                builder.add_box(wood, (x, y, (floor_z + top_z) / 2), (48.0, 70.0, top_z - floor_z),
                                primitive="individual_timber_post", bevel=7.0)
                # Lattice members are separate, catching highlights at close range.
                for lx in (-78.0, 0.0, 78.0):
                    builder.add_box(wood, (x + lx, y - (8 if y < 0 else -8), (floor_z + top_z) / 2),
                                    (10.0, 20.0, top_z - floor_z - 55.0), primitive="window_lattice_vertical", bevel=3.0)
                for lz in (floor_z + 95.0, floor_z + 165.0, floor_z + 235.0):
                    builder.add_box(wood, (x, y - (10 if y < 0 else -10), lz), (145.0, 20.0, 10.0),
                                    primitive="window_lattice_horizontal", bevel=3.0)
                # Separate dougong arms/blocks.
                builder.add_box(wood, (x, y, top_z + 56.0), (170.0, 112.0, 42.0), primitive="dougong_arm_lower", bevel=7.0)
                builder.add_box(wood, (x, y, top_z + 102.0), (125.0, 86.0, 34.0), primitive="dougong_block_upper", bevel=6.0)
                builder.add_cylinder_between(iron, (x - 65.0, y, top_z + 80.0), (x + 65.0, y, top_z + 80.0),
                                             7.0, sides=12, primitive="dougong_forged_pin")
        # Add each roof as real ribs, with staggered tile-end caps and ridge caps.
        roof_z = top_z + 120.0
        roof_w = width + 260.0
        roof_d = depth + 260.0
        rise = 180.0
        for row in range(8):
            y0 = -roof_d / 2 + 35.0 + row * (roof_d / 2 - 70.0) / 8
            y1 = 0.0
            z0 = roof_z + row * 2.0
            z1 = roof_z + rise + row * 2.0
            for idx, x in enumerate(range(int(-roof_w / 2), int(roof_w / 2) + 1, 92)):
                xx = float(x + (46 if row % 2 else 0))
                builder.add_cylinder_between(roof, (xx, y0, z0), (xx, y1, z1), 24.0,
                                             sides=14, primitive="staggered_roof_tile_rib_front")
                builder.add_cylinder_between(roof, (xx, -y0, z0), (xx, y1, z1), 24.0,
                                             sides=14, primitive="staggered_roof_tile_rib_rear")
        builder.add_box(roof, (0.0, -roof_d / 2, roof_z), (roof_w + 60.0, 88.0, 60.0),
                        primitive="eave_tile_end_fascia", bevel=13.0)
        builder.add_box(roof, (0.0, roof_d / 2, roof_z), (roof_w + 60.0, 88.0, 60.0),
                        primitive="eave_tile_end_fascia", bevel=13.0)
        builder.add_cylinder_between(roof, (-roof_w / 2, 0.0, roof_z + rise),
                                     (roof_w / 2, 0.0, roof_z + rise), 34.0, sides=16, primitive="ridge_cap_row")
        for finial_x in (-roof_w / 2 + 50.0, roof_w / 2 - 50.0):
            builder.add_cylinder_between(roof, (finial_x, 0.0, roof_z + rise),
                                         (finial_x, 0.0, roof_z + rise + 180.0), 24.0, sides=14, primitive="roof_finial_stem")


def build_gate(name):
    if name == "Wuxianmen":
        mats = ("M_WeatheredStone_UltraAAA", "M_AgedWood_UltraAAA", "M_GateWood_UltraAAA", "M_DarkMetal_UltraAAA", "M_ClayRoof_UltraAAA", "M_LimePlaster_UltraAAA", "M_Plaque_UltraAAA")
        stone, wood, doorwood, iron, roof, plaster, plaque = mats
        b = Builder(mats)
        add_masonry(b, stone, 8200, 1200, 1120, 520, 620, 164, 78, 100)
        add_tower(b, wood, roof, iron, 2700, 960, 1180, 2, 2000)
        add_door(b, doorwood, iron, -590, 820, 950, 3000)
        for y in (-600, 600):
            b.add_box(plaster, (0, y, 910), (1700, 24, 260), primitive="lime_plaster_survivor", bevel=8.0)
            b.add_box(plaque, (0, y, 1810), (620, 28, 190), primitive="hypothetical_gate_plaque", bevel=14.0)
        return b, True
    if name == "Zhengximen":
        mats = ("M_GrayBrick_UltraAAA", "M_StoneFoundation_UltraAAA", "M_AgedWood_UltraAAA", "M_DarkTimber_UltraAAA", "M_ClayRoofTile_UltraAAA", "M_BlackIron_UltraAAA", "M_GatePlaque_UltraAAA")
        brick, stone, wood, timber, roof, iron, plaque = mats
        b = Builder(mats)
        add_masonry(b, brick, 5400, 1130, 1180, 390, 510, 142, 70, 400)
        add_tower(b, timber, roof, iron, 2600, 930, 1100, 2, 5000)
        add_door(b, wood, iron, -625, 620, 760, 6000)
        for y in (-640, 640):
            b.add_box(plaque, (0, y, 1670), (640, 32, 188), primitive="gate_plaque_frame", bevel=14.0)
        return b, False
    if name == "Dadongmen":
        mats = ("M_Dadongmen_Stone_UltraAAA", "M_Dadongmen_Plaster_UltraAAA", "M_Dadongmen_Wood_UltraAAA", "M_Dadongmen_RoofClay_UltraAAA", "M_Dadongmen_DoorWood_UltraAAA", "M_Dadongmen_Iron_UltraAAA", "M_Dadongmen_Vegetation_UltraAAA")
        stone, plaster, wood, roof, doorwood, iron, vegetation = mats
        b = Builder(mats)
        add_masonry(b, stone, 3900, 1320, 2100, 520, 650, 170, 82, 800)
        add_tower(b, wood, roof, iron, 3200, 1650, 1280, 2, 9000)
        add_door(b, doorwood, iron, -1110, 820, 980, 12000)
        for side in (-1, 1):
            for idx, x in enumerate((-1500, -980, -450, 450, 980, 1500)):
                b.add_cylinder_between(vegetation, (x, side * 1070, 580 + (idx % 3) * 80),
                                       (x + side * 45, side * 1110, 780 + (idx % 2) * 90), 14.0,
                                       sides=10, primitive="attached_vine_stem")
                b.add_box(vegetation, (x + side * 42, side * 1140, 810 + (idx % 2) * 90),
                          (90.0, 32.0, 150.0), primitive="attached_vine_leaf", bevel=18.0)
            b.add_box(plaster, (0, side * 1075, 1090), (1800, 22, 140), primitive="weathered_plaster_survivor", bevel=9.0)
        return b, True
    if name == "Guidemen":
        mats = ("M_Stone_BlueGrey_UltraAAA", "M_Plaster_OffWhite_UltraAAA", "M_Wood_Aged_UltraAAA", "M_Tile_ClayGrey_UltraAAA", "M_Metal_Bronze_UltraAAA", "M_Plaque_Guide_UltraAAA", "M_Inscription_Guide_UltraAAA", "M_Sign_Guidemen_UltraAAA")
        stone, plaster, wood, roof, iron, plaque, inscription, sign = mats
        b = Builder(mats)
        add_masonry(b, stone, 4200, 1450, 1320, 600, 745, 162, 82, 16000)
        add_tower(b, wood, roof, iron, 3000, 1120, 1400, 2, 19000)
        add_door(b, wood, iron, -730, 940, 1180, 22000)
        for y in (-750, 750):
            b.add_box(plaster, (0, y, 1130), (1850, 26, 240), primitive="aged_plaster_panel", bevel=9.0)
            b.add_box(plaque, (0, y, 1260), (720, 34, 160), primitive="surviving_plaque", bevel=12.0)
            b.add_box(inscription, (0, y, 970), (740, 22, 115), primitive="stone_inscription_panel", bevel=8.0)
            b.add_box(sign, (0, y, 2110), (570, 28, 170), primitive="guide_signboard", bevel=10.0)
        return b, True
    if name == "Zhengnanmen":
        mats = ("M_Stone_Aged_UltraAAA", "M_Wood_DarkAged_UltraAAA", "M_Wood_RedLacquer_UltraAAA", "M_GlazedTile_Green_UltraAAA", "M_Gold_RidgeOrnament_UltraAAA", "M_Signboard_Zhengnanmen_UltraAAA", "M_DarkInterior_UltraAAA")
        stone, darkwood, redwood, roof, gold, sign, interior = mats
        b = Builder(mats)
        add_masonry(b, stone, 2780, 910, 1500, 490, 600, 132, 76, 26000)
        add_tower(b, redwood, roof, gold, 2500, 1120, 880, 3, 30000)
        add_door(b, darkwood, gold, -800, 700, 850, 36000)
        for level, z in enumerate((1320, 1800, 2280)):
            b.add_box(sign, (0, -700, z), (590 - level * 18, 34, 170), primitive="south_gate_signboard", bevel=12.0)
            b.add_box(interior, (0, 720, z - 80), (1450, 26, 190), primitive="deep_interior_shadow", bevel=6.0)
        return b, False
    raise KeyError(name)


def manifest_for(name, authored, exported, source_path):
    vertices = sum(len(g.vertices) for g in authored.groups.values())
    triangles = sum(len(g.faces) for g in authored.groups.values())
    primitive_counts = {}
    for group in authored.groups.values():
        for key, value in group.primitive_counts.items():
            primitive_counts[key] = primitive_counts.get(key, 0) + value
    return {
        "asset": name + "_V4_UltraAAA" if name != "Zhengnanmen" else "Zhengnanmen_HighFidelity_UltraAAA",
        "source": str(source_path),
        "materials": sorted(authored.groups),
        "vertex_count": vertices,
        "triangle_count": triangles,
        "visible_surface_relief": True,
        "basis_export_swapped_for_roll_minus_90": exported is not authored,
        "primitive_counts": primitive_counts,
        "geometry_changed": True,
        "collision_intent": "existing collision preserved; additive hero geometry is NoCollision",
        "nanite_intent": True,
    }


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for gate in ("Wuxianmen", "Zhengximen", "Dadongmen", "Guidemen", "Zhengnanmen"):
        authored, swap = build_gate(gate)
        exported = basis_copy(authored) if swap else authored
        # Keep the source stem away from the legacy writer's Wuxianmen-specific
        # basis branch: the authored builder has already been converted for the
        # three blueprints that preserve the established -90 roll.
        source = OUT / ("UltraAAA_" + gate + "_V4_source.fbx")
        write_fbx(source, exported)
        report = OUT / (gate + "_V4_UltraAAA.json")
        report.write_text(json.dumps(manifest_for(gate, authored, exported, source), indent=2), encoding="utf-8")
        print(gate, json.dumps({"vertices": sum(len(g.vertices) for g in authored.groups.values()),
                               "triangles": sum(len(g.faces) for g in authored.groups.values()),
                               "source": str(source)}))


if __name__ == "__main__":
    main()
