"""Source-side audit of the Wuxianmen V5 Core winding-corrected GLB.

This pass answers the question the Blueprint cannot: which authored node/geometry
actually backs each HISM role, and whether the geometry bound to a role is
topologically plausible for that role. It is read-only.

Run with the interpreter that has trimesh available:
    "C:/Users/mickie/AppData/Local/Programs/Python/Python310/python.exe" \
        Scripts/AuditV5WuxianmenRefTune2.py
"""
from __future__ import annotations

import hashlib
import json
from collections import defaultdict
from pathlib import Path

import numpy as np
import trimesh

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = (
    PROJECT
    / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
    / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb"
)
OUTPUT = PROJECT / "Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-source-audit-20260917.json"

# Role -> the geometry the Blueprint currently binds (from the winding rebind).
BOUND = {
    "HISM_000_Wuxianmen_Plaque_GEN_VARIABLE": ("plaque", "Wuxianmen_Plaque", 1),
    "HISM_001_iron_000088_GEN_VARIABLE": ("iron", "iron_000001", 88),
    "HISM_002_lower_tile_1_077_GEN_VARIABLE": ("tile", "main_tile_-1_000", 324),
    "HISM_003_plaster_000016_GEN_VARIABLE": ("plaster", "plaster_000001", 16),
    "HISM_004_ridge_000010_GEN_VARIABLE": ("tile", "main_tile_-1_000", 10),
    "HISM_005_stone_004213_GEN_VARIABLE": ("stone", "stone_000001", 4213),
    "HISM_006_wood_000451_GEN_VARIABLE": ("wood", "gate_door_-1", 451),
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _stats(mesh) -> dict:
    finite = bool(np.isfinite(mesh.vertices).all() and np.isfinite(mesh.faces).all())
    edges = np.sort(mesh.edges.reshape(-1, 2), axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    volume = float(mesh.volume) if finite else None
    return {
        "vertices": int(len(mesh.vertices)),
        "triangles": int(len(mesh.faces)),
        "finite": finite,
        "watertight": bool(mesh.is_watertight),
        "winding_consistent": bool(mesh.is_winding_consistent),
        "signed_volume": volume,
        "boundary_edges": int((counts == 1).sum()),
        "nonmanifold_edges": int((counts > 2).sum()),
        "degenerate_triangles": int((mesh.area_faces < 1e-12).sum()),
        "uv0": bool(getattr(mesh.visual, "uv", None) is not None),
        "bounds": np.asarray(mesh.bounds, dtype=float).round(6).tolist(),
        "extents": np.asarray(mesh.extents, dtype=float).round(6).tolist(),
    }


def _node_usage(scene) -> dict:
    """Map geometry name -> list of node names that instance it, with transforms."""
    usage: dict[str, list] = defaultdict(list)
    for node_name in scene.graph.nodes_geometry:
        transform, geometry_name = scene.graph[node_name]
        usage[geometry_name].append({
            "node": node_name,
            "translation": np.asarray(transform, dtype=float)[:3, 3].round(6).tolist(),
            "scale": np.asarray(np.linalg.norm(np.asarray(transform, dtype=float)[:3, :3], axis=0), dtype=float).round(6).tolist(),
        })
    return usage


def main() -> None:
    scene = trimesh.load(SOURCE, force="scene", process=False)
    usage = _node_usage(scene)

    meshes = {}
    for name, mesh in scene.geometry.items():
        record = _stats(mesh)
        nodes = usage.get(name, [])
        record["node_instances"] = len(nodes)
        record["node_names_sample"] = sorted({item["node"] for item in nodes})[:6]
        record["node_translation_sample"] = [item["translation"] for item in nodes[:4]]
        record["node_scale_sample"] = [item["scale"] for item in nodes[:4]]
        meshes[name] = record

    report = {
        "created": "2026-09-17",
        "source": str(SOURCE),
        "source_sha256": _sha256(SOURCE),
        "node_count": int(len(scene.graph.nodes_geometry)),
        "geometry_count": int(len(scene.geometry)),
        "geometry_names": sorted(scene.geometry.keys()),
        "meshes": meshes,
        "blueprint_binding": {
            component: {"role": role, "geometry": geometry, "instances": instances,
                        "geometry_present": geometry in scene.geometry,
                        "geometry_node_instances": len(usage.get(geometry, []))}
            for component, (role, geometry, instances) in BOUND.items()
        },
        "unbound_geometries": sorted(set(scene.geometry) - {item[1] for item in BOUND.values()}),
    }
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("REFTUNE2_SOURCE_AUDIT", OUTPUT)
    print("nodes", report["node_count"], "geometries", report["geometry_count"])
    print("--- geometry stats ---")
    for name in sorted(meshes):
        item = meshes[name]
        print(
            f"{name:28s} tri={item['triangles']:7d} v={item['vertices']:7d} "
            f"inst={item['node_instances']:5d} vol={item['signed_volume']:.6g} "
            f"wt={int(item['watertight'])} wc={int(item['winding_consistent'])} "
            f"deg={item['degenerate_triangles']} uv0={int(item['uv0'])} "
            f"ext={item['extents']}"
        )
    print("--- unbound ---")
    print(report["unbound_geometries"])


if __name__ == "__main__":
    main()
