"""Audit V5 source topology and generate private Wuxianmen winding variants.

The Wuxianmen source meshes are authored as consistently inward-facing shells
(negative signed volume). This pass reverses triangle order only; it preserves
vertex positions, UV indices, node transforms, and instance counts. Guidemen
roof sheets are intentionally open visual shells and are audited but not
rewritten here.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
import trimesh


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
OUT = PROJECT / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
PACKAGES = [
    {
        "name": "Wuxianmen_V5_4K_Core",
        "source": ROOT / "Wuxianmen_UE5_HighDetail_Package_With_4K_Materials/Wuxianmen_UE5_50M_Instanced.glb",
        "output": OUT / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb",
    },
    {
        "name": "Wuxianmen_V5_FullPBR",
        "source": ROOT / "Wuxianmen_UE5_HighDetail_Package/Wuxianmen_UE5_50M_Instanced.glb",
        "output": OUT / "Wuxianmen_V5_FullPBR/Wuxianmen_V5_50M_Instanced_WindingOutward.glb",
    },
]
GUIDEMEN = ROOT / "Model/Guidemen_GuideGate_UE5_100M_Instanced.glb"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _mesh_stats(mesh):
    finite = bool(np.isfinite(mesh.vertices).all() and np.isfinite(mesh.faces).all())
    edges = np.sort(mesh.edges.reshape(-1, 2), axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    return {
        "vertices": int(len(mesh.vertices)),
        "triangles": int(len(mesh.faces)),
        "finite": finite,
        "watertight": bool(mesh.is_watertight),
        "winding_consistent": bool(mesh.is_winding_consistent),
        "signed_volume": float(mesh.volume) if finite else None,
        "boundary_edges": int((counts == 1).sum()),
        "nonmanifold_edges": int((counts > 2).sum()),
        "degenerate_triangles": int((mesh.area_faces < 1e-12).sum()),
        "uv0": bool(getattr(mesh.visual, "uv", None) is not None),
        "bounds": np.asarray(mesh.bounds, dtype=float).round(6).tolist(),
    }


def _audit_scene(path: Path):
    scene = trimesh.load(path, force="scene", process=False)
    return {
        "source": str(path),
        "source_sha256": _sha256(path),
        "node_count": int(len(scene.graph.nodes_geometry)),
        "geometry_count": int(len(scene.geometry)),
        "meshes": {name: _mesh_stats(mesh) for name, mesh in scene.geometry.items()},
    }


def _reverse_scene(source: Path, output: Path):
    scene = trimesh.load(source, force="scene", process=False)
    before = {name: _mesh_stats(mesh) for name, mesh in scene.geometry.items()}
    for mesh in scene.geometry.values():
        if mesh.volume < -1e-8:
            mesh.faces = np.ascontiguousarray(mesh.faces[:, ::-1])
            mesh._cache.clear()
    output.parent.mkdir(parents=True, exist_ok=True)
    scene.export(output, file_type="glb")
    after_scene = trimesh.load(output, force="scene", process=False)
    after = {name: _mesh_stats(mesh) for name, mesh in after_scene.geometry.items()}
    assert set(before) == set(after), (sorted(before), sorted(after))
    assert all(item["signed_volume"] is None or item["signed_volume"] >= 0.0 for item in after.values())
    assert int(len(after_scene.graph.nodes_geometry)) == int(len(scene.graph.nodes_geometry))
    return before, after


def main():
    audit = {"created": "2026-09-17", "guidemen_audit": _audit_scene(GUIDEMEN), "packages": []}
    print("V5_SOURCE_AUDIT", GUIDEMEN.name, audit["guidemen_audit"]["geometry_count"])
    for package in PACKAGES:
        before = _audit_scene(package["source"])
        old, new = _reverse_scene(package["source"], package["output"])
        record = {
            "name": package["name"],
            "source": str(package["source"]),
            "source_sha256": before["source_sha256"],
            "corrected": str(package["output"]),
            "corrected_sha256": _sha256(package["output"]),
            "node_count": before["node_count"],
            "geometry_count": before["geometry_count"],
            "meshes_before": old,
            "meshes_after": new,
            "repair": "reverse triangle winding; preserve positions, UVs and node transforms",
        }
        audit["packages"].append(record)
        print("V5_WINDING_VARIANT", package["name"], package["output"], package["output"].stat().st_size)
    output = ROOT / "reference-tuning-source-audit-20260917.json"
    output.write_text(json.dumps(audit, indent=2), encoding="utf-8")
    print("V5_SOURCE_AUDIT_WRITTEN", output)


if __name__ == "__main__":
    main()
