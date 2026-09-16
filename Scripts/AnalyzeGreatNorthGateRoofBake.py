"""Work out what Interchange baked into the roof-tile mesh.

bake_meshes=True was required for the orientation fix, but that also made
Interchange bake a node transform into the roof tile (it came out 1943 x 1006 x
716 cm instead of the 100 x 100 x 9.8 cm unit tile).

This script computes, for every candidate explanation, the UE-space axis-aligned
bounds and compares them to the measured import:
  * each of the 9 roof nodes applied to the unit tile, and
  * the union of all 9 placements.
"""
import json
import struct
from pathlib import Path

SOURCE = Path(r"C:/Works/Raw3DModels/V2/GreatNorthGate_UE5_HighDetail.glb")
REPORT = Path(r"C:/Git/AuraProj/Saved/RawModelImport/GreatNorthGate_HighDetail.json")
OUT = Path(r"C:/Git/AuraProj/Saved/RawModelImport/great-north-gate-roof-bake-analysis.json")

# Confirmed from the five detail meshes: UE = (x_src, -y_src, z_src) * 100.
AXIS_MAP = [(0, 1.0), (1, -1.0), (2, 1.0)]
UNIT_SCALE = 100.0


def read_gltf(path):
    with path.open("rb") as handle:
        struct.unpack("<4sII", handle.read(12))
        chunk_len, _ = struct.unpack("<II", handle.read(8))
        return json.loads(handle.read(chunk_len).decode("utf-8"))


def to_ue(vector):
    return [AXIS_MAP[axis][1] * vector[AXIS_MAP[axis][0]] * UNIT_SCALE for axis in range(3)]


def transform_aabb(matrix, lo, hi):
    """Axis-aligned bounds of the transformed box."""
    columns = [matrix[0:4], matrix[4:8], matrix[8:12], matrix[12:16]]
    out_min = [float("inf")] * 3
    out_max = [float("-inf")] * 3
    for mask in range(8):
        point = [lo[axis] if (mask >> axis) & 1 == 0 else hi[axis] for axis in range(3)]
        world = [columns[3][row] for row in range(3)]
        for axis in range(3):
            for row in range(3):
                world[row] += columns[axis][row] * point[axis]
        ue = to_ue(world)
        for axis in range(3):
            out_min[axis] = min(out_min[axis], ue[axis])
            out_max[axis] = max(out_max[axis], ue[axis])
    return out_min, out_max


doc = read_gltf(SOURCE)
imported = json.loads(REPORT.read_text(encoding="utf-8"))
roof_entry = [item for item in imported["meshes"] if item["name"] == "Roof_Upper_Front"][0]
measured = (roof_entry["bounds_min"], roof_entry["bounds_max"])
measured_size = [round(measured[1][axis] - measured[0][axis], 2) for axis in range(3)]

roof_mesh_index = next(index for index, mesh in enumerate(doc["meshes"])
                       if mesh.get("name", "").startswith("SM_RoofTile"))
prim = doc["meshes"][roof_mesh_index]["primitives"][0]
accessor = doc["accessors"][prim["attributes"]["POSITION"]]
unit_lo, unit_hi = list(accessor["min"]), list(accessor["max"])

roof_nodes = [(index, node) for index, node in enumerate(doc["nodes"])
              if node.get("mesh") == roof_mesh_index]

print("measured roof bounds", [round(v, 2) for v in measured[0]], [round(v, 2) for v in measured[1]])
print("measured roof size  ", measured_size)
print("unit tile glTF      ", unit_lo, unit_hi)
print("unit tile UE size   ", [round(v, 2) for v in
                               [to_ue(unit_hi)[a] - to_ue(unit_lo)[a] for a in range(3)]])
print()

results = []
union_min = [float("inf")] * 3
union_max = [float("-inf")] * 3
for index, node in roof_nodes:
    lo, hi = transform_aabb(node["matrix"], unit_lo, unit_hi)
    size = [round(hi[axis] - lo[axis], 2) for axis in range(3)]
    match = all(abs(size[axis] - measured_size[axis]) < 1.0 for axis in range(3))
    results.append({"node": index, "name": node.get("name"), "size": size, "matches": match,
                    "bounds_min": [round(v, 2) for v in lo], "bounds_max": [round(v, 2) for v in hi]})
    print("node[{:>2}] {:<22} size={} matches={}".format(index, node.get("name"), size, match))
    for axis in range(3):
        union_min[axis] = min(union_min[axis], lo[axis])
        union_max[axis] = max(union_max[axis], hi[axis])

union_size = [round(union_max[axis] - union_min[axis], 2) for axis in range(3)]
print()
print("union of all 9       size={} matches={}".format(
    union_size, all(abs(union_size[axis] - measured_size[axis]) < 1.0 for axis in range(3))))
print("union bounds min={} max={}".format([round(v, 2) for v in union_min],
                                          [round(v, 2) for v in union_max]))

OUT.write_text(json.dumps({
    "axis_map": AXIS_MAP, "unit_scale": UNIT_SCALE,
    "measured_bounds_min": measured[0], "measured_bounds_max": measured[1],
    "measured_size": measured_size,
    "unit_tile_gltf": {"min": unit_lo, "max": unit_hi},
    "per_node": results,
    "union_size": union_size,
    "union_bounds_min": [round(v, 2) for v in union_min],
    "union_bounds_max": [round(v, 2) for v in union_max],
}, indent=2), encoding="utf-8")
