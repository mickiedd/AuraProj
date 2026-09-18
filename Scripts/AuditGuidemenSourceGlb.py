"""Audit the Guidemen V5 source GLB — structure and, above all, the roof geometry.

This is the measurement that was impossible inside Unreal: the source GLB carries the
real triangles, so the roof's slope can be read directly instead of inferred from
instance AABBs, protected HitResults or blocked vertex accessors.

The build report says the source is Y-up (dimensions 56.0 W x 18.0 D x 19.3 H), so the
vertical axis here is Y and the horizontal plane is X-Z. Every profile below is
reported in source coordinates and then restated in the Y-up sense.

Reports:
  1. geometry inventory with vertex/triangle counts, materials and instance counts;
  2. assembled bounds and which axis is vertical;
  3. for every roof-role geometry, the assembled surface profile along each horizontal
     axis, per tier — a ridge reads highest at the centre, a valley lowest.

Read-only.
"""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path

import numpy as np
import trimesh

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/source/Model/Guidemen_GuideGate_UE5_100M_Instanced.glb"
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/source-audit-20260918.json"

ROOF_TOKENS = ("roof", "ridge", "tile", "t0", "shell", "beam", "parapet", "wing", "wall")


def _is_roof(name: str) -> bool:
    lowered = name.lower()
    return any(token in lowered for token in ("roof", "ridge", "tile", "t0", "shell"))


def main():
    scene = trimesh.load(SOURCE, force="scene", process=False)
    report = {"source": str(SOURCE), "geometries": [], "map_saved": False}

    instance_count = defaultdict(int)
    for node in scene.graph.nodes_geometry:
        _, geometry_name = scene.graph[node]
        instance_count[geometry_name] += 1

    for name, mesh in scene.geometry.items():
        material = None
        try:
            material = mesh.visual.material.name
        except Exception:
            material = None
        report["geometries"].append({
            "geometry": name,
            "vertices": int(len(mesh.vertices)),
            "triangles": int(len(mesh.faces)),
            "instances": instance_count.get(name, 0),
            "material": material,
            "extents": np.asarray(mesh.extents, dtype=float).round(4).tolist(),
            "has_uv": bool(getattr(mesh.visual, "uv", None) is not None),
        })
    report["geometries"].sort(key=lambda item: -item["instances"])
    report["scene_nodes"] = len(scene.graph.nodes_geometry)

    # assembled bounds
    lows, highs = [], []
    for node in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node]
        vertices = np.asarray(scene.geometry[geometry_name].vertices, dtype=float)
        points = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]
        lows.append(points.min(axis=0))
        highs.append(points.max(axis=0))
    low = np.vstack(lows).min(axis=0)
    high = np.vstack(highs).max(axis=0)
    report["assembled_low"] = low.round(4).tolist()
    report["assembled_high"] = high.round(4).tolist()
    report["assembled_extent"] = (high - low).round(4).tolist()
    print("SOURCE assembled low", report["assembled_low"], "high", report["assembled_high"])
    print("SOURCE extent (X, Y, Z) =", report["assembled_extent"])

    print("SOURCE geometry inventory:")
    for item in report["geometries"][:20]:
        print(f"    {item['geometry'][:40]:42s} v={item['vertices']:6d} t={item['triangles']:6d} "
              f"inst={item['instances']:6d} uv={item['has_uv']} mat={item['material']}")

    # ---- roof profile ----
    roof_points = []
    for node in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node]
        if not _is_roof(geometry_name):
            continue
        vertices = np.asarray(scene.geometry[geometry_name].vertices, dtype=float)
        points = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]
        roof_points.append(np.hstack([points, np.full((len(points), 1), hash(geometry_name) % 1000)]))
    if roof_points:
        roof = np.vstack(roof_points)
        report["roof_point_count"] = int(len(roof))
        # vertical axis is Y in source coordinates
        tiers = {}
        y_mid = (roof[:, 1].min() + roof[:, 1].max()) / 2
        for label, subset in (("lower", roof[roof[:, 1] < y_mid]), ("upper", roof[roof[:, 1] >= y_mid])):
            if len(subset) < 100:
                continue
            entry = {"points": int(len(subset)), "y_range": [round(float(subset[:, 1].min()), 3),
                                                             round(float(subset[:, 1].max()), 3)]}
            for axis, key in ((0, "X"), (2, "Z")):
                centre = (subset[:, axis].min() + subset[:, axis].max()) / 2
                bands = {}
                for value, height in zip(subset[:, axis], subset[:, 1]):
                    band = int(round(abs(value - centre) / 150.0)) * 150
                    bands.setdefault(band, []).append(height)
                entry[f"topY_by_abs_offset_{key}"] = {
                    str(band): round(float(max(values)), 2) for band, values in sorted(bands.items())
                }
            tiers[label] = entry
        report["roof_tiers"] = tiers
        for label, entry in tiers.items():
            print(f"SOURCE roof {label}: points={entry['points']} Y={entry['y_range']}")
            for key in ("X", "Z"):
                print(f"    topY by |{key} offset|:", json.dumps(entry[f"topY_by_abs_offset_{key}"]))

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("SOURCE_AUDIT_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
