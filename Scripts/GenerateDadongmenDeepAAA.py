"""Generate the rollback-safe Dadongmen V4 Deep AAA hero-detail source.

The source is additive: it adds real beveled/chamfered masonry blocks, arch
voussoirs, paved tunnel stones, open-door plank/strap/stud geometry, timber
frames and dougong brackets, and roof-tile relief.  It is exported as one
multi-material FBX and mounted by ApplyDadongmenDeepAAA.py as a visible
NoCollision Blueprint component.  Existing Dadongmen meshes, UVs, collision,
materials, open-door details, preview level, and rollback siblings are left
available.

Run from the repository root with the normal project Python interpreter.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

from GenerateGuangzhouLandmarkDeepGeometry import Builder, add_arch, write_fbx


PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / "Saved/RawModelImport/V4/DeepAAA"
ASSET_NAME = "Dadongmen_V4_DeepAAA"
MATERIALS = (
    "M_Dadongmen_Stone",
    "M_Dadongmen_Plaster",
    "M_Dadongmen_Wood",
    "M_Dadongmen_RoofClay",
    "M_Dadongmen_DoorWood",
    "M_Dadongmen_Iron",
    "M_Dadongmen_VegetationLeaf",
)


def _add_dadongmen_detail():
    b = Builder(MATERIALS)
    stone, plaster, wood, roof, doorwood, iron, vegetation = MATERIALS

    # Lower wall: individually beveled masonry courses on both facade planes.
    # The source gate is approximately 3.8 m wide in the authored Blueprint
    # frame; the -100 cm x offset follows the imported source bounds.
    facade_y = (-1000.0, 1000.0)
    for y in facade_y:
        for row, z in enumerate(range(58, 900, 76)):
            x = -1810.0 + (118.0 if row % 2 else 0.0)
            column = 0
            while x < 1735.0:
                width = 238.0 + (22.0 if column % 3 == 1 else 0.0)
                # Preserve the arched passage as a real opening in the
                # additive course rather than covering it with a decal-like
                # slab.
                if not (abs(x) < 425.0 and z < 760.0):
                    b.add_box(stone, (x, y, z), (width, 46.0, 58.0), bevel=9.0,
                              primitive="beveled_stone_course")
                x += width + 18.0
                column += 1

        # Dressed arch ring and defined jamb blocks at both sides of the gate.
        add_arch(b, stone, y, 505.0, 360.0, 445.0, segments=30)
        for x in (-414.0, 414.0):
            for z in (108.0, 225.0, 342.0, 459.0, 576.0, 693.0):
                b.add_box(stone, (x, y, z), (76.0, 54.0, 96.0), bevel=12.0,
                          primitive="arched_jamb_block")
        b.add_box(stone, (-100.0, y, 922.0), (3650.0, 68.0, 70.0), bevel=11.0,
                  primitive="wall_cap_beam")
        b.add_box(stone, (0.0, y, 930.0), (920.0, 62.0, 82.0), bevel=13.0,
                  primitive="arch_lintel_definition")

        # Chamfered crenellation blocks make the parapet silhouette read as
        # assembled masonry at the hero distance.
        for index, x in enumerate(range(-1780, 1681, 270)):
            if -360 < x < 360:
                continue
            b.add_box(stone, (float(x), y, 1025.0), (188.0, 92.0, 184.0), bevel=20.0,
                      primitive="parapet_merlon")

    # Return walls and their side merlons close the visible lower silhouette.
    for x in (-1855.0, 1755.0):
        for row, z in enumerate(range(58, 900, 76)):
            for column, y in enumerate(range(-850, 851, 225)):
                b.add_box(stone, (x, float(y), z), (48.0, 178.0, 58.0), bevel=8.0,
                          primitive="side_return_course")
        for y in range(-800, 801, 300):
            b.add_box(stone, (x, float(y), 1025.0), (94.0, 188.0, 184.0), bevel=19.0,
                      primitive="side_parapet_merlon")

    # Real stone paving and thresholds in the passage add contact breakup and
    # a controlled value transition under the open leaves.
    for y in range(-850, 851, 170):
        for x in (-225.0, -75.0, 75.0, 225.0):
            b.add_box(stone, (x, float(y), 12.0), (138.0, 154.0, 22.0), bevel=6.0,
                      primitive="tunnel_paving_stone")
    for y in (-900.0, 900.0):
        b.add_box(stone, (0.0, y, 30.0), (720.0, 94.0, 42.0), bevel=8.0,
                  primitive="threshold_stone")

    # Repeated inner arch rings and jamb stones give the tunnel a readable
    # soffit under the existing open-door source, without changing collision.
    for y in (-850.0, -570.0, -290.0, 0.0, 290.0, 570.0, 850.0):
        add_arch(b, stone, y, 505.0, 356.0, 416.0, segments=30)
    for y in range(-820, 821, 205):
        for x in (-430.0, 430.0):
            b.add_box(stone, (x, float(y), 80.0), (56.0, 164.0, 62.0), bevel=8.0,
                      primitive="tunnel_jamb_course")

    # Upper timber hall: sill/header beams, posts, diagonal braces, plaster
    # panel returns, and stepped bracket/dougong stacks on both long facades.
    panel_centers = (-1600, -1200, -800, -400, 0, 400, 800, 1200, 1600)
    for y in (-955.0, 955.0):
        b.add_box(wood, (-100.0, y, 1110.0), (3650.0, 72.0, 66.0), bevel=10.0,
                  primitive="timber_sill")
        b.add_box(wood, (-100.0, y, 1515.0), (3760.0, 78.0, 74.0), bevel=11.0,
                  primitive="timber_header")
        for x in panel_centers:
            b.add_box(wood, (float(x), y, 1312.0), (58.0, 66.0, 400.0), bevel=8.0,
                      primitive="timber_post")
        for index, x in enumerate(panel_centers[:-1]):
            mid = x + 200.0
            b.add_box(plaster, (mid, y - (8.0 if y > 0 else -8.0), 1310.0),
                      (322.0, 18.0, 238.0), bevel=5.0, primitive="plaster_panel_return")
            b.add_cylinder_between(wood, (x + 55.0, y, 1145.0),
                                   (x + 345.0, y, 1480.0), 17.0, sides=10,
                                   primitive="timber_diagonal_brace")
        for x in panel_centers:
            b.add_box(wood, (float(x), y, 1570.0), (190.0, 108.0, 40.0), bevel=7.0,
                      primitive="dougong_lower_step")
            b.add_box(wood, (float(x), y, 1618.0), (142.0, 86.0, 34.0), bevel=6.0,
                      primitive="dougong_upper_step")
            b.add_cylinder_between(iron, (x - 62.0, y, 1595.0), (x + 62.0, y, 1595.0),
                                   7.0, sides=10, primitive="dougong_forged_pin")

    # Roof: geometric tile ribs on both slopes, eave fascia, ridge cap, and
    # hip returns.  Cylindrical relief catches highlights and breaks the flat
    # imported roof plane without relying on a normal-only illusion.
    for x in range(-1840, 1641, 105):
        b.add_cylinder_between(roof, (float(x), 0.0, 2050.0),
                               (float(x), -1370.0, 1670.0), 24.0, sides=10,
                               primitive="front_roof_tile_rib")
        b.add_cylinder_between(roof, (float(x), 0.0, 2050.0),
                               (float(x), 1370.0, 1670.0), 24.0, sides=10,
                               primitive="rear_roof_tile_rib")
    for y in (-1370.0, 1370.0):
        b.add_box(roof, (-100.0, y, 1652.0), (3760.0, 76.0, 54.0), bevel=12.0,
                  primitive="roof_eave_fascia")
        b.add_cylinder_between(roof, (-1900.0, y, 1660.0), (1700.0, y, 1660.0),
                               34.0, sides=12, primitive="roof_eave_cap")
    b.add_cylinder_between(roof, (-1900.0, 0.0, 2050.0), (1700.0, 0.0, 2050.0),
                           34.0, sides=12, primitive="roof_ridge_cap")
    for x in (-1780.0, 1580.0):
        b.add_cylinder_between(roof, (x, 0.0, 2025.0), (x, 0.0, 2160.0),
                               26.0, sides=10, primitive="roof_finial_base")

    # Open leaves: five dimensional plank boards per leaf, relief braces,
    # forged straps, hinge barrels, and studs.  These follow the already
    # authored hinge positions instead of replacing the existing detail layer.
    for depth_sign in (-1.0, 1.0):
        center_y = depth_sign * 705.0
        for leaf_sign in (-1.0, 1.0):
            surface_x = leaf_sign * 292.0
            for index in range(5):
                plank_y = center_y + depth_sign * (index - 2) * 51.0
                b.add_box(doorwood, (surface_x, plank_y, 210.0), (26.0, 40.0, 392.0),
                          bevel=5.0, primitive="open_door_plank")
            for z in (104.0, 210.0, 316.0):
                b.add_cylinder_between(iron,
                                       (surface_x + leaf_sign * 14.0, center_y, z),
                                       (surface_x + leaf_sign * 38.0, center_y, z),
                                       11.0, sides=10, primitive="open_door_stud")
            for z in (105.0, 315.0):
                b.add_box(doorwood, (surface_x + leaf_sign * 3.0, center_y, z),
                          (28.0, 182.0, 24.0), bevel=5.0, primitive="open_door_cross_brace")
            for strap_y in (center_y - depth_sign * 58.0, center_y + depth_sign * 58.0):
                b.add_box(iron, (surface_x + leaf_sign * 12.0, strap_y, 210.0),
                          (30.0, 14.0, 350.0), bevel=5.0, primitive="open_door_iron_strap")
            hinge_y = center_y + depth_sign * 74.0
            b.add_cylinder_between(iron, (leaf_sign * 365.0, hinge_y, 58.0),
                                   (leaf_sign * 365.0, hinge_y, 362.0), 18.0, sides=12,
                                   primitive="open_door_hinge_barrel")

    # Replace the imported floating vegetation shards with a small number of
    # attached stem/leaf clusters.  The source vegetation slot is culled by
    # the apply script; these clusters have an actual wall anchor and visible
    # low-poly silhouette rather than a detached green crystal.
    vegetation_clusters = (
        (-1510.0, 260.0, 1.00), (-1290.0, 430.0, 0.78), (-910.0, 210.0, 0.86),
        (-620.0, 520.0, 0.72), (710.0, 300.0, 0.90), (1040.0, 580.0, 0.72),
        (1390.0, 230.0, 0.86), (1580.0, 470.0, 0.68),
    )
    for facade_sign in (-1.0, 1.0):
        wall_y = facade_sign * 1000.0
        leaf_y = wall_y + facade_sign * 28.0
        for index, (x, z, scale) in enumerate(vegetation_clusters):
            lean = facade_sign * (18.0 if index % 2 else -12.0)
            b.add_cylinder_between(vegetation, (x, wall_y + facade_sign * 5.0, z - 55.0),
                                   (x + lean, leaf_y, z), 9.0, sides=8,
                                   primitive="attached_vine_stem")
            b.add_box(vegetation, (x + lean, leaf_y, z + 18.0 * scale),
                      (76.0 * scale, 38.0, 118.0 * scale), bevel=12.0,
                      primitive="attached_vine_leaf")
            if index % 2 == 0:
                b.add_box(vegetation, (x + lean + 30.0, leaf_y + facade_sign * 5.0, z - 12.0),
                          (52.0 * scale, 34.0, 78.0 * scale), bevel=9.0,
                          primitive="attached_vine_leaf")

    return b


def _basis_copy(builder):
    """Convert authored (X, depth-Y, height-Z) to the imported roll basis.

    Dadongmen's source component preserves the project's established -90 roll.
    The native FBX import expects the authored depth and height axes exchanged;
    the Blueprint component rotation restores them at runtime.
    """
    exported = Builder(MATERIALS)
    for name, group in builder.groups.items():
        target = exported.groups[name]
        target.vertices = [(x, z, y) for x, y, z in group.vertices]
        target.faces = list(group.faces)
        target.uvs = list(group.uvs)
        target.primitive_counts = dict(group.primitive_counts)
    return exported


def manifest_for(builder, fbx):
    vertices = sum(len(group.vertices) for group in builder.groups.values())
    triangles = sum(len(group.faces) for group in builder.groups.values())
    primitives = {}
    for group in builder.groups.values():
        for key, value in group.primitive_counts.items():
            primitives[key] = primitives.get(key, 0) + value
    return {
        "asset": ASSET_NAME,
        "source": str(fbx),
        "units": "centimetres / authored Dadongmen Blueprint frame; FBX export swaps depth-height for roll -90",
        "materials": sorted(builder.groups),
        "vertex_count": vertices,
        "triangle_count": triangles,
        "primitive_counts": primitives,
        "geometry_changed": True,
        "uv0_preserved_on_source_assemblies": True,
        "collision_intent": "existing source Blueprint collision preserved; additive hero detail is NoCollision",
        "nanite_intent": True,
    }


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    authored = _add_dadongmen_detail()
    fbx = OUT / (ASSET_NAME + ".fbx")
    write_fbx(fbx, _basis_copy(authored))
    manifest = OUT / (ASSET_NAME + ".json")
    manifest.write_text(json.dumps(manifest_for(authored, fbx), indent=2), encoding="utf-8")
    print(ASSET_NAME, json.dumps({
        "vertices": sum(len(group.vertices) for group in authored.groups.values()),
        "triangles": sum(len(group.faces) for group in authored.groups.values()),
        "primitives": manifest_for(authored, fbx)["primitive_counts"],
        "fbx": str(fbx),
    }))


if __name__ == "__main__":
    main()
