"""Per-instance transform audit of the Wuxianmen V5 Core source GLB.

The building is assembled from a handful of primitive geometries instanced
thousands of times. Visual defects such as needle-like blades sticking out of
the roof and wall are usually a small number of outlier instances rather than a
geometry problem, so this pass decomposes every node transform and reports
per-geometry scale/rotation distributions plus the extreme outliers.

Read-only. Run with the trimesh interpreter.
"""
from __future__ import annotations

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
OUTPUT = PROJECT / "Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-instance-audit-20260917.json"


def main() -> None:
    scene = trimesh.load(SOURCE, force="scene", process=False)
    per_geometry = defaultdict(list)
    for node_name in scene.graph.nodes_geometry:
        transform, geometry_name = scene.graph[node_name]
        matrix = np.asarray(transform, dtype=float)
        linear = matrix[:3, :3]
        scale = np.linalg.norm(linear, axis=0)
        rotation = linear / np.where(scale == 0, 1.0, scale)
        # approximate Euler-ish orientation probe: direction of the local Z axis
        axis_z = rotation[:, 2]
        per_geometry[geometry_name].append({
            "node": node_name,
            "translation": matrix[:3, 3].round(6).tolist(),
            "scale": scale.round(6).tolist(),
            "axis_z": axis_z.round(6).tolist(),
            "det": float(np.linalg.det(linear)),
        })

    report = {"created": "2026-09-17", "source": str(SOURCE), "geometries": {}}
    for geometry_name, items in sorted(per_geometry.items()):
        scale = np.array([item["scale"] for item in items], dtype=float)
        translation = np.array([item["translation"] for item in items], dtype=float)
        volume = scale.prod(axis=1)
        order = np.argsort(-volume)
        extents = scene.geometry[geometry_name].extents
        # world-space size of each instance
        world = scale * np.asarray(extents, dtype=float)
        world_volume = world.prod(axis=1)
        order_world = np.argsort(-world_volume)
        report["geometries"][geometry_name] = {
            "instances": len(items),
            "unit_extents": np.asarray(extents, dtype=float).round(6).tolist(),
            "scale_min": scale.min(axis=0).round(6).tolist(),
            "scale_max": scale.max(axis=0).round(6).tolist(),
            "scale_median": np.median(scale, axis=0).round(6).tolist(),
            "world_extent_max": world.max(axis=0).round(6).tolist(),
            "world_extent_median": np.median(world, axis=0).round(6).tolist(),
            "translation_min": translation.min(axis=0).round(6).tolist(),
            "translation_max": translation.max(axis=0).round(6).tolist(),
            "negative_determinant_count": int(sum(1 for item in items if item["det"] < 0)),
            "largest_world_instances": [
                {
                    "node": items[int(index)]["node"],
                    "world_extents": world[int(index)].round(6).tolist(),
                    "scale": items[int(index)]["scale"],
                    "translation": items[int(index)]["translation"],
                }
                for index in order_world[:8]
            ],
        }
        record = report["geometries"][geometry_name]
        print(
            f"{geometry_name:9s} n={record['instances']:5d} "
            f"world_ext_med={record['world_extent_median']} "
            f"world_ext_max={record['world_extent_max']} "
            f"negdet={record['negative_determinant_count']}"
        )

    report["scene_bounds"] = np.asarray(scene.bounds, dtype=float).round(6).tolist()
    report["scene_extents"] = np.asarray(scene.extents, dtype=float).round(6).tolist()
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE2_INSTANCE_AUDIT", OUTPUT)
    print("scene_extents", report["scene_extents"])


if __name__ == "__main__":
    main()
