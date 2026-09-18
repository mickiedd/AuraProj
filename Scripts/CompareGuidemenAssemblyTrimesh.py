"""Verify the Guidemen assembly against the source using trimesh's world matrices.

The previous attempt composed the glTF node hierarchy by hand and got the parent
scale/rotation composition wrong for nested nodes, so 18 components reported large
residuals while 27 matched at exactly 0.000 cm. trimesh composes the hierarchy correctly —
and its transforms already agreed with the asset to the centimetre in the earlier ridge and
shell measurements — so this redoes the comparison against trimesh's own world matrices.

Reports, per component: instance count agreement, and the sorted-position multiset residual
in Unreal centimetres after the source's Y-up metres are converted with UE = (x, -z, y) * 100.

Read-only.
"""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path

import numpy as np
import trimesh

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild"
SOURCE = ROOT / "source/Model/Guidemen_GuideGate_UE5_100M_Instanced.glb"
DUMP = ROOT / "assembly-dump-20260918.json"
OUTPUT = ROOT / "assembly-vs-source-trimesh-20260918.json"

POSITION_TOLERANCE_CM = 1.0
# source (x, y, z) -> UE (x, -z, y)
M = np.array([[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]])


def main():
    scene = trimesh.load(SOURCE, force="scene", process=False)
    dump = json.loads(DUMP.read_text(encoding="utf-8"))

    by_geometry = defaultdict(list)
    for node in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node]
        matrix = np.asarray(matrix, dtype=float)
        translation = matrix[:3, 3]
        rotation = matrix[:3, :3]
        scale = np.linalg.norm(rotation, axis=0)
        ue_t = (M @ translation) * 100.0
        by_geometry[geometry_name].append({"node": node, "t": ue_t})

    # the importer named each mesh after one of the nodes that references its geometry
    name_to_geometry = {}
    for geometry, entries in by_geometry.items():
        for entry in entries:
            name_to_geometry.setdefault(entry["node"], geometry)

    report = {"source": str(SOURCE), "geometries": len(by_geometry), "components": [], "map_saved": False}
    total_matched = 0
    total_instances = 0
    for component in dump["components"]:
        mesh_name = component["mesh_name"]
        geometry = name_to_geometry.get(mesh_name)
        entry = {"component": component["name"], "mesh": mesh_name, "geometry": geometry}
        if geometry is None:
            entry["reason"] = "no source node with this mesh name"
            report["components"].append(entry)
            continue
        expected = by_geometry[geometry]
        actual = component["instances"]
        total_instances += len(actual)
        entry["source_instances"] = len(expected)
        entry["asset_instances"] = len(actual)
        if len(expected) != len(actual):
            entry["count_match"] = False
            report["components"].append(entry)
            print(f"TRIMESH_CMP {mesh_name[:32]:34s} COUNT {len(expected)} vs {len(actual)}")
            continue
        expected_position = np.atleast_2d(np.array([item["t"] for item in expected], dtype=float))
        actual_position = np.atleast_2d(np.array([item["l"] for item in actual], dtype=float))
        order_e = np.lexsort((expected_position[:, 2], expected_position[:, 1], expected_position[:, 0]))
        order_a = np.lexsort((actual_position[:, 2], actual_position[:, 1], actual_position[:, 0]))
        residual = np.abs(expected_position[order_e] - actual_position[order_a]).max(axis=1)
        matched = int((residual <= POSITION_TOLERANCE_CM).sum())
        total_matched += matched
        entry.update({
            "count_match": True,
            "position_matched": matched,
            "position_unmatched": len(actual) - matched,
            "worst_position_residual_cm": round(float(residual.max()), 4),
        })
        report["components"].append(entry)
        status = "OK" if matched == len(actual) else "MISMATCH"
        print(f"TRIMESH_CMP {mesh_name[:32]:34s} n={len(actual):6d} matched={matched:6d} "
              f"worst={float(residual.max()):8.3f}cm  {status}")

    report["totals"] = {
        "components": len(dump["components"]),
        "count_mismatches": sum(1 for item in report["components"] if item.get("count_match") is False),
        "asset_instances": total_instances,
        "position_matched": total_matched,
        "position_unmatched": total_instances - total_matched,
    }
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("TRIMESH_TOTALS", json.dumps(report["totals"]))
    print("TRIMESH_CMP_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
