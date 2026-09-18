"""Compare the Wuxianmen V5 Core and FullPBR corrected assemblies, and resolve the
plaque mesh's UV convention.

Two questions:

1. Is the FullPBR assembly geometrically the same as the Core assembly, so the
   same roof/ridge defects and the same corrections apply? Compared per geometry:
   vertex and triangle counts, node counts, and every node transform.
2. On the plaque's front face (the one facing -Y, where the sign is read from),
   does texture U increase with world +X or -X? That decides whether a plaque
   texture drawn left to right appears left to right to a viewer standing in
   front of the gate, and so whether the existing FullPBR plaque art or the
   generated Core art reads correctly.

Read-only. Run with the trimesh interpreter (Python 3.10).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import trimesh

PROJECT = Path(__file__).resolve().parents[1]
BASE = PROJECT / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
CORE = BASE / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb"
FULL = BASE / "Wuxianmen_V5_FullPBR/Wuxianmen_V5_50M_Instanced_WindingOutward.glb"
OUTPUT = PROJECT / "Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reftune2-compare-20260917.json"


def _node_transforms(scene):
    rows = {}
    for node_name in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node_name]
        rows[node_name] = (geometry_name, np.asarray(matrix, dtype=float).round(5).tolist())
    return rows


def _plaque_uv(scene):
    """Report the UV direction on each plaque face group."""
    mesh = scene.geometry["plaque"]
    vertices = np.asarray(mesh.vertices, dtype=float)
    faces = np.asarray(mesh.faces)
    uv = np.asarray(mesh.visual.uv, dtype=float)
    normals = np.asarray(mesh.face_normals, dtype=float)
    result = {}
    for axis, label in ((0, "X"), (1, "Y"), (2, "Z")):
        for sign, side in ((1.0, "+"), (-1.0, "-")):
            mask = normals[:, axis] * sign > 0.9
            if mask.sum() == 0:
                continue
            corner_faces = faces[mask]
            flat = np.unique(corner_faces)
            # Per triangle, does U increase with +axis?
            corners = vertices[corner_faces]
            uvs = uv[corner_faces]
            agree = 0
            total = 0
            for index in range(len(corner_faces)):
                coords = corners[index][:, axis]
                if coords.max() - coords.min() < 1e-6:
                    continue
                total += 1
                high = uvs[index][np.argmax(coords)][0]
                low = uvs[index][np.argmin(coords)][0]
                if high > low:
                    agree += 1
            result[f"{side}{label}"] = {
                "triangles": int(mask.sum()),
                "vertices": int(len(flat)),
                "uv_u_increases_with_plus_axis": f"{agree}/{total}",
                "coord_range": [
                    np.asarray(vertices[flat]).min(axis=0).round(4).tolist(),
                    np.asarray(vertices[flat]).max(axis=0).round(4).tolist(),
                ],
                "uv_range": [
                    uv[flat].min(axis=0).round(4).tolist(),
                    uv[flat].max(axis=0).round(4).tolist(),
                ],
            }
    return result


def main():
    core = trimesh.load(CORE, force="scene", process=False)
    full = trimesh.load(FULL, force="scene", process=False)
    report = {"created": "2026-09-17", "core": str(CORE), "fullpbr": str(FULL)}

    core_geometry = {name: (len(mesh.vertices), len(mesh.faces)) for name, mesh in core.geometry.items()}
    full_geometry = {name: (len(mesh.vertices), len(mesh.faces)) for name, mesh in full.geometry.items()}
    report["geometry_identical"] = core_geometry == full_geometry
    report["geometry"] = {"core": core_geometry, "fullpbr": full_geometry}

    core_nodes = _node_transforms(core)
    full_nodes = _node_transforms(full)
    report["node_count"] = {"core": len(core_nodes), "fullpbr": len(full_nodes)}
    shared = sorted(set(core_nodes) & set(full_nodes))
    differing = [
        name for name in shared
        if core_nodes[name][0] != full_nodes[name][0]
        or np.abs(np.asarray(core_nodes[name][1]) - np.asarray(full_nodes[name][1])).max() > 1e-4
    ]
    report["nodes_shared"] = len(shared)
    report["nodes_only_core"] = sorted(set(core_nodes) - set(full_nodes))[:10]
    report["nodes_only_fullpbr"] = sorted(set(full_nodes) - set(core_nodes))[:10]
    report["nodes_differing_transforms"] = differing[:20]
    report["transforms_identical"] = not differing and set(core_nodes) == set(full_nodes)

    report["plaque_uv"] = _plaque_uv(full)

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("FULLPBR_COMPARE", OUTPUT)
    print("geometry_identical", report["geometry_identical"])
    print("node_count", report["node_count"], "shared", len(shared), "differing", len(differing))
    print("transforms_identical", report["transforms_identical"])
    print("plaque_uv")
    for key, item in report["plaque_uv"].items():
        print("  ", key, item["triangles"], "tris | u increases with +axis:", item["uv_u_increases_with_plus_axis"],
              "| coord", item["coord_range"], "| uv", item["uv_range"])


if __name__ == "__main__":
    main()
