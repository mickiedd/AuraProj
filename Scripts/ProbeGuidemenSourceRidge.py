"""Locate Guidemen's ridge line and roof slope from the source GLB.

The aggregate profile in the previous pass collapsed, so this measures it directly and
simply:

1. The 12 `G_RidgeBeast` ornaments sit ON the ridge. Their world positions give the
   ridge line's location and axis outright — no inference needed. This is the same
   ridge-anchor argument that identified the Wuxianmen defect, but now read from real
   source geometry instead of from Unreal.
2. The 16,248 roof tiles are split into tiers by height, and each tier's top surface is
   binned along both horizontal axes, with the min/max printed so a collapsed axis is
   obvious rather than silent.
3. The single-instance roof shells and ridges are listed with their world bounds.

Source coordinates are Y-up: Y is height, X is width, Z is depth.

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
OUTPUT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild/ridge-and-slope-20260918.json"


def _world_vertices(scene, node):
    matrix, geometry_name = scene.graph[node]
    vertices = np.asarray(scene.geometry[geometry_name].vertices, dtype=float)
    return (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]


def main():
    scene = trimesh.load(SOURCE, force="scene", process=False)
    by_geometry = defaultdict(list)
    for node in scene.graph.nodes_geometry:
        _, geometry_name = scene.graph[node]
        by_geometry[geometry_name].append(node)
    report = {"source": str(SOURCE), "map_saved": False}

    # ---- 1. ridge ornaments ----
    beast_nodes = by_geometry.get("G_RidgeBeast", [])
    beasts = []
    for node in beast_nodes:
        points = _world_vertices(scene, node)
        beasts.append({
            "node": node,
            "centre": np.round(points.mean(axis=0), 3).tolist(),
            "bounds_low": np.round(points.min(axis=0), 3).tolist(),
            "bounds_high": np.round(points.max(axis=0), 3).tolist(),
        })
    report["ridge_ornaments"] = beasts
    if beasts:
        centres = np.array([b["centre"] for b in beasts])
        report["ridge_ornament_spread"] = np.round(centres.max(axis=0) - centres.min(axis=0), 3).tolist()
        report["ridge_ornament_mean"] = np.round(centres.mean(axis=0), 3).tolist()
        print("RIDGE_ORNAMENTS", len(beasts), "spread (X,Y,Z) =", report["ridge_ornament_spread"],
              "mean =", report["ridge_ornament_mean"])
        for b in beasts:
            print(f"    node {b['node'][-28:]:30s} centre={b['centre']} top={b['bounds_high'][1]}")

    # ---- 2. tile surface per tier ----
    tile_nodes = by_geometry.get("G_RoofTile_HiDetail", [])
    step = max(1, len(tile_nodes) // 4000)
    rows = []
    for node in tile_nodes[::step]:
        points = _world_vertices(scene, node)
        rows.append({
            "x": float((points[:, 0].min() + points[:, 0].max()) / 2),
            "z": float((points[:, 2].min() + points[:, 2].max()) / 2),
            "top": float(points[:, 1].max()),
            "bottom": float(points[:, 1].min()),
        })
    report["tile_instances_sampled"] = len(rows)
    print("TILE samples", len(rows), "of", len(tile_nodes))
    xs = [r["x"] for r in rows]
    zs = [r["z"] for r in rows]
    ys = [r["top"] for r in rows]
    print(f"    tile centre X range [{min(xs):.2f}, {max(xs):.2f}]  Z range [{min(zs):.2f}, {max(zs):.2f}]"
          f"  top Y range [{min(ys):.2f}, {max(ys):.2f}]")
    report["tile_centre_ranges"] = {"x": [round(min(xs), 2), round(max(xs), 2)],
                                    "z": [round(min(zs), 2), round(max(zs), 2)],
                                    "top_y": [round(min(ys), 2), round(max(ys), 2)]}
    if rows:
        z_mid = (min(r["bottom"] for r in rows) + max(r["top"] for r in rows)) / 2
        tiers = {}
        for label, subset in (("lower", [r for r in rows if (r["bottom"] + r["top"]) / 2 < z_mid]),
                              ("upper", [r for r in rows if (r["bottom"] + r["top"]) / 2 >= z_mid])):
            if len(subset) < 20:
                continue
            entry = {"instances": len(subset)}
            for key, label2 in (("x", "X"), ("z", "Z")):
                values = [r[key] for r in subset]
                centre = (min(values) + max(values)) / 2
                bands = {}
                for r in subset:
                    band = int(round(abs(r[key] - centre) / 150.0)) * 150
                    bands.setdefault(band, []).append(r["top"])
                entry[f"topY_by_abs_offset_{label2}"] = {
                    str(band): round(float(max(v)), 2) for band, v in sorted(bands.items())
                }
                entry[f"{label2}_centre"] = round(centre, 2)
            tiers[label] = entry
        report["tile_tiers"] = tiers
        for label, entry in tiers.items():
            print(f"TILE_TIER {label} n={entry['instances']}")
            for label2 in ("X", "Z"):
                print(f"    topY by |{label2} offset|:", json.dumps(entry[f"topY_by_abs_offset_{label2}"]))

    # ---- 3. single-instance roof parts ----
    singles = []
    for name in ("R1_MainRidge", "R2_MainRidge", "R1_RoofShellFront", "R1_RoofShellBack",
                 "R1_RoofShellLeft", "R1_RoofShellRight", "R2_RoofShellFront", "R2_RoofShellBack",
                 "R2_RoofShellLeft", "R2_RoofShellRight"):
        for node in by_geometry.get(name, []):
            points = _world_vertices(scene, node)
            singles.append({"geometry": name, "node": node,
                            "low": np.round(points.min(axis=0), 3).tolist(),
                            "high": np.round(points.max(axis=0), 3).tolist(),
                            "extent": np.round(points.max(axis=0) - points.min(axis=0), 3).tolist()})
    report["single_instance_roof_parts"] = singles
    print("SINGLE ROOF PARTS", len(singles))
    for s in singles:
        print(f"    {s['geometry'][:26]:28s} extent(X,Y,Z)={s['extent']} low={s['low']}")

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("RIDGE_SLOPE_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
