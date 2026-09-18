"""Probe the world-space orientation of the Wuxianmen roof assemblies.

Answers one question with numbers: is the roof a pitched surface or a stack of
flat decks? For the tile, ridge, wood (roof decks) and stone (walkway) roles it
transforms the unit geometry by every instance matrix and reports the world
bounding box, the local axis directions, and a least-squares plane fit, so the
roof pitch can be read off directly instead of inferred from renders.

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
OUTPUT = PROJECT / "Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-roof-probe-20260917.json"


def _plane_fit(points: np.ndarray) -> dict:
    centroid = points.mean(axis=0)
    centered = points - centroid
    _, singular, vh = np.linalg.svd(centered, full_matrices=False)
    normal = vh[-1]
    if normal[2] < 0:
        normal = -normal
    # tilt of the fitted plane from horizontal, in degrees
    tilt = float(np.degrees(np.arccos(np.clip(abs(normal[2]), 0.0, 1.0))))
    return {
        "centroid": centroid.round(6).tolist(),
        "normal": normal.round(6).tolist(),
        "tilt_from_horizontal_deg": round(tilt, 4),
        "rms_residual": float(np.sqrt((centered @ normal) ** 2).mean()),
    }


def main() -> None:
    scene = trimesh.load(SOURCE, force="scene", process=False)
    by_geometry = defaultdict(list)
    for node_name in scene.graph.nodes_geometry:
        transform, geometry_name = scene.graph[node_name]
        by_geometry[geometry_name].append((node_name, np.asarray(transform, dtype=float)))

    report = {"created": "2026-09-17", "source": str(SOURCE), "roles": {}}
    for geometry_name, entries in sorted(by_geometry.items()):
        mesh = scene.geometry[geometry_name]
        vertices = np.asarray(mesh.vertices, dtype=float)
        homogeneous = np.hstack([vertices, np.ones((len(vertices), 1))])
        world_points = []
        per_instance = []
        for node_name, matrix in entries:
            transformed = (homogeneous @ matrix.T)[:, :3]
            world_points.append(transformed)
            per_instance.append({
                "node": node_name,
                "z_min": float(transformed[:, 2].min()),
                "z_max": float(transformed[:, 2].max()),
                "x_span": [float(transformed[:, 0].min()), float(transformed[:, 0].max())],
                "y_span": [float(transformed[:, 1].min()), float(transformed[:, 1].max())],
                "local_x": matrix[:3, 0].round(6).tolist(),
                "local_y": matrix[:3, 1].round(6).tolist(),
                "local_z": matrix[:3, 2].round(6).tolist(),
            })
        all_points = np.vstack(world_points)
        per_instance.sort(key=lambda item: item["z_min"])
        report["roles"][geometry_name] = {
            "instances": len(entries),
            "world_bounds": np.asarray(
                [all_points.min(axis=0), all_points.max(axis=0)], dtype=float
            ).round(6).tolist(),
            "plane_fit": _plane_fit(all_points),
            "lowest_instance": per_instance[0],
            "highest_instance": per_instance[-1],
        }
        record = report["roles"][geometry_name]
        print(
            f"{geometry_name:9s} n={record['instances']:5d} "
            f"z=[{record['world_bounds'][0][2]:.3f},{record['world_bounds'][1][2]:.3f}] "
            f"plane_tilt={record['plane_fit']['tilt_from_horizontal_deg']:.2f}deg "
            f"local_y_lowest={record['lowest_instance']['local_y']}"
        )

    # Roof-surface samples: the top face of every tile instance.
    tile_entries = by_geometry["tile"]
    tile_mesh = scene.geometry["tile"]
    tile_vertices = np.asarray(tile_mesh.vertices, dtype=float)
    samples = []
    for node_name, matrix in tile_entries:
        transformed = (np.hstack([tile_vertices, np.ones((len(tile_vertices), 1))]) @ matrix.T)[:, :3]
        top = transformed[transformed[:, 2] >= transformed[:, 2].max() - 1e-6]
        samples.append(top.mean(axis=0))
    samples = np.asarray(samples)
    report["tile_top_surface"] = {
        "samples": len(samples),
        "z_min": float(samples[:, 2].min()),
        "z_max": float(samples[:, 2].max()),
        "z_range": float(samples[:, 2].max() - samples[:, 2].min()),
        "plane_fit": _plane_fit(samples),
    }
    print("tile top surface z range", report["tile_top_surface"]["z_range"],
          "tilt", report["tile_top_surface"]["plane_fit"]["tilt_from_horizontal_deg"])

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE2_ROOF_PROBE", OUTPUT)


if __name__ == "__main__":
    main()
