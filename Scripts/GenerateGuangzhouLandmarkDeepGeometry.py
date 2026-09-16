"""Generate rollback-safe hero detail FBX sources for the two V4 gate assets.

The source meshes are deliberately additive.  They contain real bevel-like
edge breaks, dressed stone blocks, arched voussoirs, door planks/ironwork,
timber framing, dougong/bracket stacks, and roof-tile ribs.  The Unreal apply
script imports each source as a Nanite static mesh and mounts it as a visible
Blueprint-owned component, so the existing V4 meshes, UVs, collision intent,
and rollback material siblings remain intact.

Run from the repository root with the normal project Python interpreter.
No third-party packages are required.
"""
from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass, field
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / "Saved/RawModelImport/V4/DeepAAA"


@dataclass
class Group:
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    faces: list[tuple[int, int, int]] = field(default_factory=list)
    uvs: list[tuple[float, float]] = field(default_factory=list)
    primitive_counts: dict[str, int] = field(default_factory=dict)

    def add(self, vertices, faces, uvs, primitive):
        offset = len(self.vertices)
        self.vertices.extend(vertices)
        self.faces.extend(tuple(i + offset for i in face) for face in faces)
        self.uvs.extend(uvs)
        self.primitive_counts[primitive] = self.primitive_counts.get(primitive, 0) + 1


class Builder:
    def __init__(self, materials):
        self.groups = {name: Group() for name in materials}

    def _group(self, material):
        if material not in self.groups:
            raise KeyError(material)
        return self.groups[material]

    def add_box(self, material, center, size, primitive="beveled_box", bevel=0.0):
        """Add a closed box with chamfered corners when bevel is nonzero.

        The chamfer is represented by an inset top/bottom shell and four
        sloped edge strips.  It is intentionally light-weight but produces a
        genuine silhouette break on close cameras instead of a normal-only
        illusion.
        """
        cx, cy, cz = center
        sx, sy, sz = size
        b = max(0.0, min(float(bevel), min(sx, sy, sz) * 0.24))
        if b <= 0.001:
            x0, x1 = cx - sx / 2, cx + sx / 2
            y0, y1 = cy - sy / 2, cy + sy / 2
            z0, z1 = cz - sz / 2, cz + sz / 2
            quads = [
                [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
                [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],
                [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)],
                [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
                [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
                [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)],
            ]
            vertices, faces, uvs = [], [], []
            for quad in quads:
                start = len(vertices)
                vertices.extend(quad)
                faces.extend(((start, start + 1, start + 2), (start, start + 2, start + 3)))
                uvs.extend(((0, 0), (1, 0), (1, 1), (0, 1)))
            self._group(material).add(vertices, faces, uvs, primitive)
            return

        # 24-vertex chamfered cuboid: each face is inset, each edge is a
        # sloped strip, and the eight corners are triangular caps.
        x0, x1 = cx - sx / 2, cx + sx / 2
        y0, y1 = cy - sy / 2, cy + sy / 2
        z0, z1 = cz - sz / 2, cz + sz / 2
        # face rings in clockwise order when viewed from outside
        rings = [
            [(x0 + b, y0, z0 + b), (x1 - b, y0, z0 + b), (x1 - b, y0, z1 - b), (x0 + b, y0, z1 - b)],
            [(x1 - b, y1, z0 + b), (x0 + b, y1, z0 + b), (x0 + b, y1, z1 - b), (x1 - b, y1, z1 - b)],
            [(x0, y1 - b, z0 + b), (x0, y0 + b, z0 + b), (x0, y0 + b, z1 - b), (x0, y1 - b, z1 - b)],
            [(x1, y0 + b, z0 + b), (x1, y1 - b, z0 + b), (x1, y1 - b, z1 - b), (x1, y0 + b, z1 - b)],
            [(x0 + b, y0 + b, z1), (x1 - b, y0 + b, z1), (x1 - b, y1 - b, z1), (x0 + b, y1 - b, z1)],
            [(x0 + b, y1 - b, z0), (x1 - b, y1 - b, z0), (x1 - b, y0 + b, z0), (x0 + b, y0 + b, z0)],
        ]
        vertices = [p for ring in rings for p in ring]
        faces, uvs = [], []
        for face_index, ring in enumerate(rings):
            s = face_index * 4
            faces.extend(((s, s + 1, s + 2), (s, s + 2, s + 3)))
            uvs.extend(((0, 0), (1, 0), (1, 1), (0, 1)))

        # Connect adjacent face rings.  The strips are the visible chamfers.
        adjacent = ((0, 2), (0, 3), (0, 4), (0, 5), (1, 2), (1, 3), (1, 4), (1, 5),
                    (2, 4), (3, 4), (2, 5), (3, 5))
        for a, c in adjacent:
            ra, rc = a * 4, c * 4
            for i in range(4):
                j = (i + 1) % 4
                faces.extend(((ra + i, rc + i, rc + j), (ra + i, rc + j, ra + j)))
        # Corner triangles close the eight chamfer intersections.
        corners = (
            ((0, 0), (2, 0), (5, 3)), ((0, 1), (3, 0), (5, 2)),
            ((0, 2), (2, 3), (4, 0)), ((0, 3), (3, 3), (4, 1)),
            ((1, 0), (2, 1), (5, 0)), ((1, 1), (3, 1), (5, 1)),
            ((1, 2), (2, 2), (4, 3)), ((1, 3), (3, 2), (4, 2)),
        )
        for corner in corners:
            tri = tuple(face * 4 + vertex for face, vertex in corner)
            faces.append(tri)
        self._group(material).add(vertices, faces, uvs, primitive)

    def add_cylinder_between(self, material, p0, p1, radius, sides=10, primitive="cylindrical_relief"):
        p0 = [float(v) for v in p0]
        p1 = [float(v) for v in p1]
        axis = [p1[i] - p0[i] for i in range(3)]
        length = math.sqrt(sum(v * v for v in axis))
        if length < 1e-5:
            return
        tangent = [v / length for v in axis]
        reference = [0.0, 0.0, 1.0] if abs(tangent[2]) < 0.9 else [1.0, 0.0, 0.0]
        n1 = [tangent[1] * reference[2] - tangent[2] * reference[1],
              tangent[2] * reference[0] - tangent[0] * reference[2],
              tangent[0] * reference[1] - tangent[1] * reference[0]]
        n1_len = math.sqrt(sum(v * v for v in n1))
        n1 = [v / n1_len for v in n1]
        n2 = [tangent[1] * n1[2] - tangent[2] * n1[1],
              tangent[2] * n1[0] - tangent[0] * n1[2],
              tangent[0] * n1[1] - tangent[1] * n1[0]]
        vertices, uvs = [], []
        for end, point in enumerate((p0, p1)):
            for index in range(sides):
                angle = math.tau * index / sides
                vertices.append(tuple(point[i] + radius * (math.cos(angle) * n1[i] + math.sin(angle) * n2[i]) for i in range(3)))
                uvs.append((end, index / sides))
        faces = []
        for index in range(sides):
            nxt = (index + 1) % sides
            faces.extend(((index, nxt, sides + nxt), (index, sides + nxt, sides + index)))
        c0, c1 = len(vertices), len(vertices) + 1
        vertices.extend((tuple(p0), tuple(p1)))
        uvs.extend(((0.5, 0.5), (0.5, 0.5)))
        for index in range(sides):
            nxt = (index + 1) % sides
            faces.extend(((c0, nxt, index), (c1, sides + index, sides + nxt)))
        self._group(material).add(vertices, faces, uvs, primitive)

    def add_arch_wedge(self, material, inner, outer, a0, a1, y0, y1, center_z, primitive="dressed_arch_voussoir"):
        points = []
        for y in (y0, y1):
            points.extend(((inner * math.cos(a0), y, center_z + inner * math.sin(a0)),
                           (outer * math.cos(a0), y, center_z + outer * math.sin(a0)),
                           (outer * math.cos(a1), y, center_z + outer * math.sin(a1)),
                           (inner * math.cos(a1), y, center_z + inner * math.sin(a1))))
        faces = ((0, 1, 2), (0, 2, 3), (4, 7, 6), (4, 6, 5),
                 (0, 4, 5), (0, 5, 1), (1, 5, 6), (1, 6, 2),
                 (2, 6, 7), (2, 7, 3), (3, 7, 4), (3, 4, 0))
        self._group(material).add(points, faces, ((0, 0), (1, 0), (1, 1), (0, 1)) * 2, primitive)


def add_arch(builder, material, y, center_z, inner, outer, segments=20):
    for index in range(segments):
        a0 = math.pi * index / segments
        a1 = math.pi * (index + 1) / segments
        builder.add_arch_wedge(material, inner, outer, a0, a1, y - 2.0, y + 2.0, center_z)


def add_door_detail(builder, materials, front_y, x_half, center_z, height, width, rows=5):
    wood, iron = materials
    plank_count = max(4, int((x_half * 2) / width))
    for index in range(plank_count):
        x = -x_half + width * 0.5 + index * width
        builder.add_box(wood, (x, front_y, center_z), (width - 7, 34, height), bevel=5.0)
    for x in (-x_half + 20, 0.0, x_half - 20):
        builder.add_box(iron, (x, front_y + (18 if front_y > 0 else -18), center_z), (18, 24, height - 26), bevel=4.0)
    for row in range(rows):
        z = center_z - height * 0.35 + row * height * 0.18
        for x in (-x_half + 42, -x_half * 0.35, x_half * 0.35, x_half - 42):
            builder.add_cylinder_between(iron, (x, front_y + (25 if front_y > 0 else -25), z),
                                         (x, front_y + (39 if front_y > 0 else -39), z), 13, sides=10,
                                         primitive="forged_door_stud")


def add_timber_frame(builder, wood, iron, width, y_values, base_z, top_z, post_count):
    for y in y_values:
        builder.add_box(wood, (0, y, base_z), (width, 62, 58), bevel=8.0)
        builder.add_box(wood, (0, y, top_z), (width + 50, 68, 70), bevel=8.0)
        for index in range(post_count):
            x = -width / 2 + 80 + index * (width - 160) / max(1, post_count - 1)
            builder.add_box(wood, (x, y, (base_z + top_z) / 2), (58, 62, top_z - base_z), bevel=7.0)
            # Two stepped dougong blocks at every post.
            builder.add_box(wood, (x, y, top_z + 55), (170, 100, 42), bevel=6.0)
            builder.add_box(wood, (x, y, top_z + 103), (122, 78, 38), bevel=6.0)
            builder.add_cylinder_between(iron, (x - 72, y, top_z + 78), (x + 72, y, top_z + 78), 7, sides=8,
                                         primitive="bracket_pin")


def build_wuxianmen():
    mats = ("M_WeatheredStone", "M_AgedWood", "M_GateWood", "M_DarkMetal", "M_ClayRoof", "M_LimePlaster")
    stone, wood, gatewood, iron, roof, plaster = mats
    b = Builder(mats)
    # Wuxianmen's source components use a -90 degree roll to restore the
    # FBX-imported local basis.  These coordinates are the intended world
    # centimetre layout; write_fbx converts them to that component basis and
    # the apply script restores the same roll on the additive component.
    for front_y in (-360.0, 360.0):
        for row in range(12):
            z = 35.0 + row * 76.0
            for col in range(21):
                x = -3650.0 + col * 365.0 + (182.5 if row % 2 else 0.0)
                if abs(x) < 410.0 and z < 520.0:
                    continue
                b.add_box(stone, (x, front_y, z), (342.0, 28.0, 62.0), bevel=9.0)
        add_arch(b, stone, front_y, 480.0, 330.0, 440.0, segments=24)
        add_door_detail(b, (gatewood, iron), front_y, 292.0, 215.0, 430.0, 92.0, rows=6)
        b.add_box(stone, (0, front_y, 930.0), (940.0, 34.0, 55.0), bevel=11.0)

    add_timber_frame(b, wood, iron, 2360.0, (-360.0, 360.0), 520.0, 1120.0, 11)
    for y in (-430.0, 430.0):
        b.add_box(wood, (0, y, 1250.0), (2540.0, 86.0, 76.0), bevel=10.0)
        for x in (-1170.0, -780.0, -390.0, 0.0, 390.0, 780.0, 1170.0):
            b.add_cylinder_between(wood, (x, y, 1150.0), (x, y, 1390.0), 28.0, sides=10,
                                   primitive="eave_post_roundover")
    # Roof tile ribs follow the roof pitch and create a repeated but geometric
    # relief pattern visible from both hero angles.
    for x in range(-1180, 1181, 86):
        b.add_cylinder_between(roof, (x, -520.0, 1360.0), (x, 520.0, 1450.0), 25.0, sides=10,
                               primitive="roof_tile_rib")
    for y in (-525.0, 525.0):
        b.add_cylinder_between(roof, (-1300.0, y, 1350.0), (1300.0, y, 1420.0), 31.0, sides=10,
                               primitive="roof_eave_cap")
        b.add_box(roof, (0, y, 1570.0), (2600.0, 54.0, 56.0), bevel=12.0)
    # Plaster infill edge trims make the frame read as assembled layers.
    for y in (-395.0, 395.0):
        for z in (650.0, 900.0, 1100.0):
            b.add_box(plaster, (0, y, z), (2060.0, 16.0, 22.0), bevel=4.0)
    return b


def build_zhengximen():
    mats = ("M_GrayBrick", "M_StoneFoundation", "M_AgedWood", "M_DarkTimber", "M_ClayRoofTile", "M_BlackIron", "M_GatePlaque")
    brick, stone, wood, timber, roof, iron, plaque = mats
    b = Builder(mats)
    for front_y in (-558.0, 558.0):
        for row in range(14):
            z = 70.0 + row * 54.0
            for col in range(31):
                x = -2460.0 + col * 164.0 + (82.0 if row % 2 else 0.0)
                if abs(x) < 280.0 and z < 610.0:
                    continue
                b.add_box(brick, (x, front_y, z), (150.0, 25.0, 42.0), bevel=8.0)
        add_arch(b, stone, front_y, 350.0, 230.0, 320.0, segments=26)
        add_door_detail(b, (wood, iron), front_y, 218.0, 158.0, 316.0, 58.0, rows=4)
        for x in (-2500.0, 2500.0):
            b.add_box(stone, (x, front_y, 470.0), (210.0, 36.0, 820.0), bevel=18.0)
        b.add_box(stone, (0, front_y, 690.0), (540.0, 36.0, 72.0), bevel=16.0)

    add_timber_frame(b, timber, iron, 2180.0, (-360.0, 360.0), 770.0, 1080.0, 10)
    for y in (-420.0, 420.0):
        b.add_box(timber, (0, y, 1180.0), (2350.0, 72.0, 62.0), bevel=9.0)
        for x in (-1050.0, -700.0, -350.0, 0.0, 350.0, 700.0, 1050.0):
            b.add_cylinder_between(timber, (x, y, 1080.0), (x, y, 1280.0), 22.0, sides=10,
                                   primitive="eave_post_roundover")
    # Lower and upper roof tile courses; the sloped cylinders make the relief
    # read at oblique camera angles and still remain inexpensive Nanite geo.
    for x in range(-1110, 1111, 76):
        b.add_cylinder_between(roof, (x, -455.0, 1130.0), (x, 455.0, 1190.0), 22.0, sides=10,
                               primitive="lower_roof_tile_rib")
        b.add_cylinder_between(roof, (x, -365.0, 1480.0), (x, 365.0, 1522.0), 19.0, sides=10,
                               primitive="upper_roof_tile_rib")
    for y, z, width in ((-470.0, 1115.0, 2450.0), (470.0, 1175.0, 2450.0),
                        (-380.0, 1465.0, 2300.0), (380.0, 1508.0, 2300.0)):
        b.add_cylinder_between(roof, (-width / 2, y, z), (width / 2, y, z + 28.0), 26.0, sides=10,
                               primitive="roof_eave_cap")
    # Plaque backing and shallow raised border preserve the authored sign while
    # giving its frame a real shadow line.
    b.add_box(plaque, (0, -430.0, 930.0), (560.0, 30.0, 180.0), bevel=18.0)
    b.add_box(plaque, (0, 430.0, 930.0), (560.0, 30.0, 180.0), bevel=18.0)
    return b


def _fbx_array(values, per_line=24):
    values = list(values)
    return ",\n                ".join(",".join(str(v) for v in values[i:i + per_line]) for i in range(0, len(values), per_line))


def write_fbx(path: Path, builder: Builder):
    next_id = 100000
    objects, connections = [], []
    # Keep the object ordering used by the project's proven native FBX
    # imports: materials, root model, then geometry/model pairs.  The native
    # UE 5.5 FbxFactory is stricter than the Interchange reader about this
    # otherwise valid FBX object graph.
    group_specs = []
    for material, group in builder.groups.items():
        if not group.faces:
            continue
        material_id, model_id, geometry_id = next_id, next_id + 1, next_id + 2
        next_id += 3
        group_specs.append((material, group, material_id, model_id, geometry_id))
        objects.append(f'''    Material: {material_id}, "Material::{material}", "" {{
        Version: 102
        ShadingModel: "phong"
        MultiLayer: 0
        Properties70:  {{
            P: "DiffuseColor", "Color", "", "A",0.5,0.5,0.5
            P: "SpecularColor", "Color", "", "A",0.1,0.1,0.1
            P: "Shininess", "double", "Number", "",12
        }}
    }}''')
    root_id = next_id
    next_id += 1
    objects.append(f'''    Model: {root_id}, "Model::{path.stem}", "Null" {{
        Version: 232
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
        }}
    }}''')
    connections.append(f'    C: "OO",{root_id},0')
    # Rewrite the root connection target now that the root is allocated after
    # the material block, and emit the mesh objects in the same stable order.
    for material, group, material_id, model_id, geometry_id in group_specs:
        if path.stem.startswith("Wuxianmen"):
            # Native FBX import maps this source basis into the same local
            # orientation used by the original Wuxianmen components.  The
            # Blueprint roll (-90 around X) then restores world (X,Y,Z):
            # Native FBX import swaps the second and third axes for this
            # project, then the Blueprint roll (-90 around X) maps imported
            # local=(world_x, world_z, world_y) back to world (X,Y,Z).
            basis_vertices = [(x, z, y) for x, y, z in group.vertices]
        else:
            basis_vertices = group.vertices
        vertices = [value for vertex in basis_vertices for value in vertex]
        polygon_indices = []
        uv_direct = [value for uv in group.uvs for value in uv]
        # Per-polygon-vertex normals are intentionally flat at the small part
        # level so bevels and tile ribs catch clean highlights.
        normals = []
        for face in group.faces:
            a, b, c = (basis_vertices[index] for index in face)
            ab = [b[i] - a[i] for i in range(3)]
            ac = [c[i] - a[i] for i in range(3)]
            n = [ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0]]
            length = math.sqrt(sum(v * v for v in n)) or 1.0
            n = [v / length for v in n]
            normals.extend(n * 3)
            polygon_indices.extend((face[0], face[1], -face[2] - 1))
        objects.append(f'''    Geometry: {geometry_id}, "Geometry::{path.stem}_{material}", "Mesh" {{
        Vertices: *{len(vertices)} {{
            a: {_fbx_array(vertices, 18)}
        }}
        PolygonVertexIndex: *{len(polygon_indices)} {{
            a: {_fbx_array(polygon_indices, 30)}
        }}
        LayerElementNormal: 0 {{
            Version: 101
            Name: ""
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "Direct"
            Normals: *{len(normals)} {{
                a: {_fbx_array(normals, 18)}
            }}
        }}
        LayerElementUV: 0 {{
            Version: 101
            Name: "UVChannel_1"
            MappingInformationType: "ByVertice"
            ReferenceInformationType: "Direct"
            UV: *{len(uv_direct)} {{
                a: {_fbx_array(uv_direct, 20)}
            }}
        }}
        LayerElementUV: 1 {{
            Version: 101
            Name: "UVChannel_1"
            MappingInformationType: "ByVertice"
            ReferenceInformationType: "Direct"
            UV: *{len(uv_direct)} {{
                a: {_fbx_array(uv_direct, 20)}
            }}
        }}
        LayerElementMaterial: 0 {{
            Version: 101
            Name: ""
            MappingInformationType: "AllSame"
            ReferenceInformationType: "IndexToDirect"
            Materials: *1 {{ a: 0 }}
        }}
        Layer: 0 {{
            Version: 100
            LayerElement: {{ Type: "LayerElementNormal" TypedIndex: 0 }}
            LayerElement: {{ Type: "LayerElementMaterial" TypedIndex: 0 }}
            LayerElement: {{ Type: "LayerElementUV" TypedIndex: 0 }}
            LayerElement: {{ Type: "LayerElementUV" TypedIndex: 1 }}
        }}
    }}''')
        objects.append(f'''    Model: {model_id}, "Model::{path.stem}_{material}", "Mesh" {{
        Version: 232
        Properties70:  {{
            P: "Lcl Translation", "Lcl Translation", "", "A",0,0,0
            P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
            P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
        }}
        Shading: T
        Culling: "CullingOff"
    }}''')
        connections.extend((f'    C: "OO",{geometry_id},{model_id}',
                            f'    C: "OO",{model_id},{root_id}',
                            f'    C: "OO",{material_id},{model_id}'))
    group_count = sum(1 for group in builder.groups.values() if group.faces)
    header = '''; FBX 7.4.0 project file
; Deterministic Guangzhou V4 Deep AAA additive geometry
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7400
}
GlobalSettings:  {
    Version: 1000
    Properties70:  {
        P: "UpAxis", "int", "Integer", "",2
        P: "UpAxisSign", "int", "Integer", "",1
        P: "FrontAxis", "int", "Integer", "",1
        P: "FrontAxisSign", "int", "Integer", "",-1
        P: "CoordAxis", "int", "Integer", "",0
        P: "CoordAxisSign", "int", "Integer", "",1
        P: "UnitScaleFactor", "double", "Number", "",1
        P: "OriginalUnitScaleFactor", "double", "Number", "",1
    }
}
Definitions:  {
    Version: 100
    Count: __TOTAL_COUNT__
    ObjectType: "Model" { Count: __MODEL_COUNT__ }
    ObjectType: "Geometry" { Count: __GEOMETRY_COUNT__ }
    ObjectType: "Material" { Count: __MATERIAL_COUNT__ }
}
Objects:  {
'''
    header = header.replace("__TOTAL_COUNT__", str(1 + group_count * 3))
    header = header.replace("__MODEL_COUNT__", str(1 + group_count))
    header = header.replace("__GEOMETRY_COUNT__", str(group_count))
    header = header.replace("__MATERIAL_COUNT__", str(group_count))
    footer = "\n}\nConnections:  {\n" + "\n".join(connections) + "\n}\n"
    # The UE 5.5 native FbxFactory requires the token-separated ASCII form
    # used by the project's prepared FBX sources.  This preserves every
    # numeric array while placing FBX tags/braces on distinct lines.
    text = header + "\n".join(objects) + footer
    text = re.sub(
        r'"[^"\n]*"|[A-Za-z_][A-Za-z_0-9]*:|[{}]',
        lambda match: match.group(0) if match.group(0).startswith('"')
        else ('\n' + match.group(0) + '\n' if match.group(0) in '{}'
              else '\n' + match.group(0)),
        text,
    )
    path.write_text(text, encoding="utf-8")


def manifest_for(name, builder, fbx):
    vertices = sum(len(group.vertices) for group in builder.groups.values())
    triangles = sum(len(group.faces) for group in builder.groups.values())
    primitive_counts = {}
    for group in builder.groups.values():
        for key, value in group.primitive_counts.items():
            primitive_counts[key] = primitive_counts.get(key, 0) + value
    return {
        "asset": name,
        "source": str(fbx),
        "units": ("centimetres / intended Unreal world axes; Wuxianmen FBX basis is restored by Blueprint roll -90 degrees"
                   if name.startswith("Wuxianmen") else "centimetres / Unreal world axes"),
        "materials": sorted(builder.groups),
        "vertex_count": vertices,
        "triangle_count": triangles,
        "primitive_counts": primitive_counts,
        "geometry_changed": True,
        "uv0_preserved_on_source_assemblies": True,
        "collision_intent": "existing source Blueprint collision preserved; additive hero detail is NoCollision",
        "nanite_intent": True,
    }


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    builds = {
        "Wuxianmen_V4_DeepAAA": build_wuxianmen(),
        "Zhengximen_V4_DeepAAA": build_zhengximen(),
    }
    for name, builder in builds.items():
        fbx = OUT / (name + ".fbx")
        write_fbx(fbx, builder)
        report = OUT / (name + ".json")
        report.write_text(json.dumps(manifest_for(name, builder, fbx), indent=2), encoding="utf-8")
        print(name, json.dumps({"vertices": sum(len(g.vertices) for g in builder.groups.values()),
                                "triangles": sum(len(g.faces) for g in builder.groups.values()),
                                "fbx": str(fbx)}))


if __name__ == "__main__":
    main()
