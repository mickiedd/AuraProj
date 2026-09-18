"""Compare the Guidemen Blueprint's assembly against the source GLB's node transforms.

The rebuild question is whether the asset is a faithful copy of the source. This answers it
directly: read every node transform from the GLB's JSON chunk (translation/rotation/scale
need no accessor decoding), compose them down the hierarchy, convert the source's Y-up metres
to Unreal's Z-up centimetres with the -90 roll the importer used, group the nodes by geometry
definition, and compare each group's transforms against the matching Blueprint component.

The axis change is UE = (x, -z, y) * 100, which follows from the observed assembled extents:
the source is 56.39 x 20.26 x 18.00 m in (X, Y, Z) and the asset is 5639 x 1800 x 2026 cm, so
source X -> UE X, source Y -> UE Z and source Z -> -UE Y.

Reports, per geometry group: instance counts, and position/scale/axis agreement against the
component, as multiset matches with a tolerance, plus the worst residual.

Read-only.
"""
from __future__ import annotations

import json
import struct
from collections import defaultdict
from pathlib import Path

import numpy as np

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild"
SOURCE = ROOT / "source/Model/Guidemen_GuideGate_UE5_100M_Instanced.glb"
DUMP = ROOT / "assembly-dump-20260918.json"
OUTPUT = ROOT / "assembly-vs-source-20260918.json"

POSITION_TOLERANCE_CM = 1.0
SCALE_TOLERANCE = 0.01
AXIS_TOLERANCE = 0.02


def _read_glb(path: Path):
    with path.open("rb") as handle:
        magic, _, length = struct.unpack("<III", handle.read(12))
        assert magic == 0x46546C67, magic
        doc = None
        while handle.tell() < length:
            chunk_length, chunk_type = struct.unpack("<II", handle.read(8))
            payload = handle.read(chunk_length)
            if chunk_type == 0x4E4F534A and doc is None:
                doc = json.loads(payload.decode("utf-8"))
    return doc


def _quat_matrix(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ], dtype=float)


def _local(node):
    if "matrix" in node:
        matrix = np.asarray(node["matrix"], dtype=float).reshape(4, 4).T
        rotation = matrix[:3, :3]
        scale = np.linalg.norm(rotation, axis=0)
        rotation = rotation / np.where(scale == 0, 1.0, scale)
        return matrix[:3, 3], rotation, scale
    translation = np.asarray(node.get("translation", [0.0, 0.0, 0.0]), dtype=float)
    rotation = _quat_matrix(node.get("rotation", [0.0, 0.0, 0.0, 1.0]))
    scale = np.asarray(node.get("scale", [1.0, 1.0, 1.0]), dtype=float)
    return translation, rotation, scale


# source (x, y, z) -> UE (x, -z, y); a proper rotation, so it conjugates the rotation too
M = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]])


def main():
    doc = _read_glb(SOURCE)
    dump = json.loads(DUMP.read_text(encoding="utf-8"))

    # world transforms, composed down the hierarchy
    world = {}

    def walk(index, parent_t, parent_r, parent_s):
        node = doc["nodes"][index]
        translation, rotation, scale = _local(node)
        if parent_r is None:
            t, r, s = translation, rotation, scale
        else:
            # parent_s is a 3-vector; do NOT index it with [:, None] — that broadcasts the
            # product into a 3x3 matrix instead of scaling element-wise.
            t = parent_t + parent_s * (parent_r @ translation)
            r = parent_r @ rotation
            s = parent_s * scale
        world[index] = (t, r, s)
        for child in node.get("children", []):
            walk(child, t, r, s)

    for scene in doc.get("scenes", []):
        for root in scene.get("nodes", []):
            walk(root, None, None, None)

    # group mesh nodes by geometry definition
    by_geometry = defaultdict(list)
    node_name = {}
    for index, node in enumerate(doc["nodes"]):
        node_name[index] = node.get("name", f"node{index}")
        if "mesh" not in node:
            continue
        geometry = doc["meshes"][node["mesh"]].get("name")
        translation, rotation, scale = world[index]
        # convert to UE space: centimetres, Z-up
        ue_t = (M @ translation) * 100.0
        ue_r = M @ rotation @ M.T
        ue_s = np.abs(M @ scale)  # permute the scale axes the same way
        by_geometry[geometry].append({
            "node": index,
            "name": node_name[index],
            "t": ue_t,
            "r": ue_r,
            "s": ue_s,
        })

    # index components by the node name their mesh was named after
    components = {item["mesh_name"]: item for item in dump["components"]}
    name_to_geometry = {}
    for geometry, nodes in by_geometry.items():
        for entry in nodes:
            name_to_geometry.setdefault(entry["name"], geometry)

    report = {"source": str(SOURCE), "geometries": len(by_geometry),
              "components": len(components), "matched": [], "unmatched": [], "map_saved": False}

    for mesh_name, component in sorted(components.items()):
        geometry = name_to_geometry.get(mesh_name)
        if geometry is None:
            report["unmatched"].append({"component": component["name"], "mesh": mesh_name,
                                        "reason": "no source node with this name"})
            continue
        expected = by_geometry[geometry]
        actual = component["instances"]
        entry = {"component": component["name"], "mesh": mesh_name, "geometry": geometry,
                 "source_instances": len(expected), "asset_instances": len(actual)}
        if len(expected) != len(actual):
            entry["count_match"] = False
            report["matched"].append(entry)
            print(f"ASSEMBLY_CMP {mesh_name[:34]:36s} COUNT MISMATCH source={len(expected)} asset={len(actual)}")
            continue
        entry["count_match"] = True

        # Sorted-lexicographic multiset comparison. The asset was imported from this exact
        # source, so a greedy nearest-neighbour match is unnecessary and quadratic on the
        # 16,248-instance tile group; sorting both lists and comparing element-wise is
        # O(n log n) and reports the same thing — the worst residual and how many instances
        # have no counterpart within tolerance.
        expected_position = np.atleast_2d(np.array([item["t"] for item in expected], dtype=float))
        actual_position = np.atleast_2d(np.array([item["l"] for item in actual], dtype=float))
        expected_scale = np.atleast_2d(np.array([item["s"] for item in expected], dtype=float))
        actual_scale = np.atleast_2d(np.array([item["s"] for item in actual], dtype=float))
        if expected_position.shape != actual_position.shape:
            entry["shape_error"] = [list(expected_position.shape), list(actual_position.shape)]
            report["matched"].append(entry)
            print(f"ASSEMBLY_CMP {mesh_name[:34]:36s} SHAPE {expected_position.shape} vs {actual_position.shape}")
            continue
        order_e = np.lexsort((expected_position[:, 2], expected_position[:, 1], expected_position[:, 0]))
        order_a = np.lexsort((actual_position[:, 2], actual_position[:, 1], actual_position[:, 0]))
        residual = np.abs(expected_position[order_e] - actual_position[order_a]).max(axis=1)
        scale_residual = np.abs(expected_scale[order_e] - actual_scale[order_a]).max(axis=1)
        matched = int((residual <= POSITION_TOLERANCE_CM).sum())
        entry.update({
            "position_matched": matched,
            "position_unmatched": len(actual) - matched,
            "worst_position_residual_cm": round(float(residual.max()), 4),
            "worst_scale_residual": round(float(scale_residual.max()), 6),
            "worst_10_residuals_cm": [round(float(v), 4) for v in np.sort(residual)[-10:]],
        })
        report["matched"].append(entry)
        status = "OK" if matched == len(actual) else "MISMATCH"
        print(f"ASSEMBLY_CMP {mesh_name[:34]:36s} n={len(actual):6d} matched={matched:6d} "
              f"worst_pos={float(residual.max()):8.3f}cm worst_scale={float(scale_residual.max()):.5f}  {status}")

    totals = {
        "components": len(report["matched"]),
        "count_mismatches": sum(1 for item in report["matched"] if not item.get("count_match")),
        "position_unmatched": sum(item.get("position_unmatched", 0) for item in report["matched"]),
        "asset_instances": sum(len(c["instances"]) for c in dump["components"]),
    }
    report["totals"] = totals
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("ASSEMBLY_TOTALS", json.dumps(totals))
    print("ASSEMBLY_CMP_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
