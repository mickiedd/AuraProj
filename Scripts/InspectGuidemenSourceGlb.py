"""Inventory the Guidemen source GLB from its JSON chunk alone.

Reads the GLB header and JSON chunk without decoding geometry, so it is fast and
memory-light even at 193 MB / 5.4M triangles. Reports the node and mesh hierarchy,
material assignments, each mesh's local bounds (glTF POSITION accessors carry min/max),
each node's world transform, and the implied assembly bounds.

The point is to establish the source's own coordinate contract and structure before
anything is imported: the manifest says "standard glTF Y-up encoding", and the V5
import has already been through one "upright correction", so the basis needs to be
read from the file rather than assumed.

Read-only.
"""
from __future__ import annotations

import json
import struct
from pathlib import Path

SOURCE = Path("C:/Works/Raw3DModels/Guidemen_HighPoly_UE5_4K.glb")
OUTPUT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/Guidemen-source-glb-inventory-20260918.json"


def _read_glb(path: Path):
    with path.open("rb") as handle:
        magic, version, length = struct.unpack("<III", handle.read(12))
        assert magic == 0x46546C67, magic
        chunks = []
        while handle.tell() < length:
            chunk_length, chunk_type = struct.unpack("<II", handle.read(8))
            chunks.append((chunk_type, handle.read(chunk_length)))
    doc = json.loads(next(data for kind, data in chunks if kind == 0x4E4F534A).decode("utf-8"))
    return version, length, doc, chunks


def _mat_mul(a, b):
    """Column-major 4x4 multiply, as glTF stores matrices."""
    out = [0.0] * 16
    for col in range(4):
        for row in range(4):
            out[col * 4 + row] = sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
    return out


def _identity():
    return [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]


def _local_matrix(node):
    if "matrix" in node:
        return list(node["matrix"])
    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    x, y, z, w = r
    rot = [
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0.0,
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0.0,
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]
    for col in range(3):
        for row in range(3):
            rot[col * 4 + row] *= s[col]
    rot[12], rot[13], rot[14] = t
    return rot


def _apply(matrix, point):
    x, y, z = point
    return [
        matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
        matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13],
        matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14],
    ]


def main():
    version, length, doc, chunks = _read_glb(SOURCE)
    report = {
        "source": str(SOURCE),
        "bytes": SOURCE.stat().st_size,
        "glb_version": version,
        "declared_length": length,
        "chunks": [{"type": hex(kind), "bytes": len(data)} for kind, data in chunks],
        "generator": doc.get("asset", {}).get("generator"),
        "asset": doc.get("asset"),
        "scene_count": len(doc.get("scenes", [])),
        "node_count": len(doc.get("nodes", [])),
        "mesh_count": len(doc.get("meshes", [])),
        "material_count": len(doc.get("materials", [])),
        "materials": [m.get("name") for m in doc.get("materials", [])],
    }

    # mesh local bounds from POSITION accessor min/max
    mesh_bounds = {}
    for index, mesh in enumerate(doc.get("meshes", [])):
        lows, highs = [], []
        for primitive in mesh.get("primitives", []):
            accessor = doc["accessors"][primitive["attributes"]["POSITION"]]
            lows.append(accessor.get("min"))
            highs.append(accessor.get("max"))
        if not lows:
            continue
        low = [min(v[a] for v in lows) for a in range(3)]
        high = [max(v[a] for v in highs) for a in range(3)]
        mesh_bounds[index] = (low, high, mesh.get("name"))

    # world transforms by walking the scene
    world = {}
    def walk(node_index, parent):
        node = doc["nodes"][node_index]
        matrix = _mat_mul(parent, _local_matrix(node))
        world[node_index] = matrix
        for child in node.get("children", []):
            walk(child, matrix)

    for scene in doc.get("scenes", []):
        for root in scene.get("nodes", []):
            walk(root, _identity())

    nodes = []
    lows, highs = [], []
    for index, node in enumerate(doc.get("nodes", [])):
        entry = {"index": index, "name": node.get("name")}
        if "mesh" in node:
            low, high, mesh_name = mesh_bounds[node["mesh"]]
            matrix = world.get(index, _identity())
            corners = [_apply(matrix, (x, y, z))
                       for x in (low[0], high[0]) for y in (low[1], high[1]) for z in (low[2], high[2])]
            wlow = [min(c[a] for c in corners) for a in range(3)]
            whigh = [max(c[a] for c in corners) for a in range(3)]
            lows.append(wlow)
            highs.append(whigh)
            entry.update({
                "mesh": mesh_name,
                "mesh_index": node["mesh"],
                "local_low": [round(v, 4) for v in low],
                "local_high": [round(v, 4) for v in high],
                "local_extent": [round(high[a] - low[a], 4) for a in range(3)],
                "world_low": [round(v, 4) for v in wlow],
                "world_high": [round(v, 4) for v in whigh],
                "world_extent": [round(whigh[a] - wlow[a], 4) for a in range(3)],
                "materials": [doc["materials"][p["material"]].get("name")
                              for p in doc["meshes"][node["mesh"]]["primitives"] if "material" in p],
            })
        else:
            entry["group"] = True
        nodes.append(entry)

    if lows:
        report["assembly_low"] = [round(min(l[a] for l in lows), 4) for a in range(3)]
        report["assembly_high"] = [round(max(h[a] for h in highs), 4) for a in range(3)]
        report["assembly_extent"] = [round(report["assembly_high"][a] - report["assembly_low"][a], 4)
                                     for a in range(3)]
    report["nodes"] = nodes
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("GLB", report["generator"], "| nodes", report["node_count"], "meshes", report["mesh_count"],
          "materials", report["material_count"])
    print("GLB_ASSET", json.dumps(report["asset"]))
    print("GLB_MATERIALS", json.dumps(report["materials"]))
    print("GLB_ASSEMBLY extent", report.get("assembly_extent"), "low", report.get("assembly_low"))
    mesh_nodes = [n for n in nodes if "mesh" in n]
    print("GLB_MESH_NODES", len(mesh_nodes))
    for node in sorted(mesh_nodes, key=lambda n: -max(n["world_extent"]))[:20]:
        print(f"    {node['name'][:46]:48s} world_extent={node['world_extent']} "
              f"z=[{node['world_low'][2]}, {node['world_high'][2]}] mats={node['materials']}")
    print("GLB_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
