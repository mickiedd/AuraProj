"""Structural probe of the Wuxianmen V5 Core source assembly.

Resolves, with numbers rather than renders:

1. Are the two roof tiers pitched or flat? (world Z span of each roof deck)
2. Which world axis does each ridge bar actually run along, and how far does it
   protrude past the roof surface it is supposed to cap?
3. Where is the gate wall body, where is the front apron, and which way does the
   asset face?
4. Is the plaque a readable upright panel on the front face?

Read-only. Run with the trimesh interpreter.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import trimesh

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = (
    PROJECT
    / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
    / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb"
)
OUTPUT = PROJECT / "Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-structure-probe-20260917.json"

INTERESTING = (
    "main_ridge", "lower_ridge",
    "main_roofdeck_-1", "main_roofdeck_1",
    "lower_roofdeck_-1", "lower_roofdeck_1",
    "tower_floor", "tower_stone_plinth", "stone_walkway_slab",
    "gate_door_-1", "gate_door_1", "Wuxianmen_Plaque",
)


def _world_bounds(mesh, matrix):
    vertices = np.asarray(mesh.vertices, dtype=float)
    transformed = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ matrix.T)[:, :3]
    return transformed


def main() -> None:
    scene = trimesh.load(SOURCE, force="scene", process=False)
    report = {"created": "2026-09-17", "source": str(SOURCE), "parts": {}, "role_summary": {}}

    role_points = {}
    for node_name in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node_name]
        matrix = np.asarray(matrix, dtype=float)
        mesh = scene.geometry[geometry_name]
        points = _world_bounds(mesh, matrix)
        role_points.setdefault(geometry_name, []).append(points)
        if node_name in INTERESTING:
            low = points.min(axis=0)
            high = points.max(axis=0)
            report["parts"][node_name] = {
                "geometry": geometry_name,
                "world_min": low.round(4).tolist(),
                "world_max": high.round(4).tolist(),
                "world_extent": (high - low).round(4).tolist(),
                "longest_axis": ["x", "y", "z"][int(np.argmax(high - low))],
                "z_span": round(float(high[2] - low[2]), 4),
            }

    for geometry_name, chunks in sorted(role_points.items()):
        all_points = np.vstack(chunks)
        report["role_summary"][geometry_name] = {
            "instances": len(chunks),
            "world_min": all_points.min(axis=0).round(4).tolist(),
            "world_max": all_points.max(axis=0).round(4).tolist(),
        }

    # Roof surface envelope: top face of every tile instance, binned by |y|.
    tile_mesh = scene.geometry["tile"]
    tile_vertices = np.asarray(tile_mesh.vertices, dtype=float)
    rows = []
    for node_name in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node_name]
        if geometry_name != "tile":
            continue
        points = _world_bounds(tile_mesh, np.asarray(matrix, dtype=float))
        top = points[points[:, 2] >= points[:, 2].max() - 1e-6]
        centroid = top.mean(axis=0)
        rows.append((centroid[1], centroid[2], centroid[0], node_name))
    rows.sort()
    report["tile_profile_by_y"] = [
        {"y": round(row[0], 4), "z_top": round(row[1], 4), "x": round(row[2], 4), "node": row[3]}
        for row in rows[::max(1, len(rows) // 24)]
    ]

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("=== named parts ===")
    for name, item in sorted(report["parts"].items()):
        print(
            f"{name:24s} ext={item['world_extent']} long={item['longest_axis']} "
            f"min={item['world_min']} max={item['world_max']}"
        )
    print("=== role summary ===")
    for name, item in sorted(report["role_summary"].items()):
        print(f"{name:9s} n={item['instances']:5d} min={item['world_min']} max={item['world_max']}")
    print("=== tile top profile (y, z_top, x) ===")
    for row in report["tile_profile_by_y"]:
        print(f"  y={row['y']:8.3f} z={row['z_top']:7.3f} x={row['x']:8.3f} {row['node']}")
    print("REFTUNE2_STRUCTURE_PROBE", OUTPUT)


if __name__ == "__main__":
    main()
