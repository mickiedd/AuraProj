"""Audit what the Wuxianmen V5 reference-tuning passes still have not addressed.

Answers, with numbers:

1. Wood. The wood role (451 instances, 0.1 m columns up to a 22.5 m roof deck)
   shares one material slot with UV0 of 0..1 per unit cube, so the large pieces
   stretch the sheet across tens of metres. Measure the sheet's pattern period and
   the instance size distribution so a per-instance tiling can be derived.
2. Stone. Confirm the sheet's feature size in pixels so the magnification needed
   to make it visible on a 0.56 m block can be computed.
3. Ridge ornaments. The eight small ridge instances were left in place when the
   roof was un-inverted; measure the corrected roof surface height at their (x, y)
   so they can be re-seated on it instead of being buried.
4. Ridge cap. Report the ridge mesh's UV layout so a dedicated cap material can be
   built instead of reusing the roof-strip contract.

Read-only. Run with the trimesh/PIL interpreter (Python 3.10).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import trimesh
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BASE = PROJECT / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
OUTPUT = ROOT / "Wuxianmen_V5-reftune3-audit-20260917.json"

# Tier pivots used by the reftune2 correction, so the audit reads the corrected
# assembly rather than the authored one.
TIER_PIVOT = {"main": 14.235, "lower": 12.515}
DECK_PIVOT = {"main": 14.035, "lower": 12.365}
ORNAMENT_NAMES = ("ridge_000002", "ridge_000003", "ridge_000004", "ridge_000005",
                  "ridge_000006", "ridge_000007", "ridge_000008", "ridge_000009")


def _period(signal: np.ndarray, minimum_lag: int = 24) -> float:
    centred = signal - signal.mean()
    if not np.any(centred):
        return float("nan")
    corr = np.correlate(centred, centred, mode="full")[len(centred) - 1:]
    corr /= corr[0]
    best, best_lag = 0.0, 0
    for lag in range(minimum_lag, len(corr) // 2):
        if corr[lag] > corr[lag - 1] and corr[lag] >= corr[lag + 1] and corr[lag] > best:
            best, best_lag = float(corr[lag]), lag
    return float(best_lag) if best_lag else float("nan")


def _sheet_report(path: Path) -> dict:
    with Image.open(path) as image:
        rgb = np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0
    grey = rgb.mean(axis=2)
    row_mean = rgb.mean(axis=(1, 2))
    lit = np.where(row_mean > 0.02)[0]
    top, bottom = (int(lit.min()), int(lit.max()) + 1) if len(lit) else (0, grey.shape[0])
    sheet = grey[top:bottom]
    gradient = np.concatenate([
        np.abs(np.diff(sheet, axis=1)).ravel(),
        np.abs(np.diff(sheet, axis=0)).ravel(),
    ])
    return {
        "container": list(grey.shape),
        "authored_rows": [top, bottom],
        "mean_rgb": rgb.mean(axis=(0, 1)).round(4).tolist(),
        "period_u_px": _period(sheet.mean(axis=0)),
        "period_v_px": _period(sheet.mean(axis=1)),
        "p99_gradient": round(float(np.percentile(gradient, 99)), 4),
        "gradient_over_0p15": round(float((gradient > 0.15).mean()), 6),
    }


def _corrected(pts: np.ndarray) -> np.ndarray:
    """Apply the reftune2 tier correction to world points (metres)."""
    out = pts.copy()
    tier = "main" if pts[:, 2].max() >= 13.5 else "lower"
    out[:, 1] = -out[:, 1]
    out[:, 2] = 2 * TIER_PIVOT[tier] - out[:, 2]
    return out


def main():
    report = {"created": "2026-09-18", "revision": "reftune3"}

    # ---- sheets ----
    report["sheets"] = {}
    for variant, folder, names in (
        ("Core", "Wuxianmen_V5_4K_Core", {"Wood": "Wood_BaseColor_4K_Repaired.png",
                                          "Stone": "Stone_BaseColor_4K_Repaired.png"}),
        ("FullPBR", "Wuxianmen_V5_FullPBR", {"Wood": "Wood_BaseColor_4K_Repaired.png",
                                             "Stone": "Stone_BaseColor_4K_Repaired.png"}),
    ):
        for role, name in names.items():
            entry = _sheet_report(ROOT / "RepairedTextures" / folder / name)
            report["sheets"][f"{variant}:{role}"] = entry
            print(f"{variant}:{role:6s} authored={entry['authored_rows']} "
                  f"period_u={entry['period_u_px']} period_v={entry['period_v_px']} "
                  f"p99grad={entry['p99_gradient']} mean={entry['mean_rgb']}")

    # ---- instance size distribution per variant ----
    report["instance_sizes"] = {}
    for variant, glb in (("Core", BASE / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb"),
                         ("FullPBR", BASE / "Wuxianmen_V5_FullPBR/Wuxianmen_V5_50M_Instanced_WindingOutward.glb")):
        scene = trimesh.load(glb, force="scene", process=False)
        rows = []
        for node_name in scene.graph.nodes_geometry:
            matrix, geometry_name = scene.graph[node_name]
            if geometry_name != "wood":
                continue
            vertices = np.asarray(scene.geometry["wood"].vertices, dtype=float)
            points = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]
            size = float(np.max(points.max(axis=0) - points.min(axis=0)))
            rows.append({"node": node_name, "max_dimension_m": round(size, 4)})
        sizes = np.array([row["max_dimension_m"] for row in rows])
        report["instance_sizes"][variant] = {
            "instances": len(rows),
            "min_m": round(float(sizes.min()), 4),
            "median_m": round(float(np.median(sizes)), 4),
            "p90_m": round(float(np.percentile(sizes, 90)), 4),
            "max_m": round(float(sizes.max()), 4),
            "over_1m": int((sizes > 1.0).sum()),
            "over_3m": int((sizes > 3.0).sum()),
            "over_10m": int((sizes > 10.0).sum()),
            "largest": sorted(rows, key=lambda row: -row["max_dimension_m"])[:6],
        }
        entry = report["instance_sizes"][variant]
        print(f"{variant} wood sizes: n={entry['instances']} min={entry['min_m']} "
              f"median={entry['median_m']} p90={entry['p90_m']} max={entry['max_m']} "
              f">1m={entry['over_1m']} >3m={entry['over_3m']} >10m={entry['over_10m']}")

    # ---- corrected roof surface at the ridge ornaments ----
    scene = trimesh.load(BASE / "Wuxianmen_V5_4K_Core/Wuxianmen_V5_50M_Instanced_WindingOutward.glb",
                         force="scene", process=False)
    lower_tiles = []
    for node_name in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node_name]
        if geometry_name != "tile" or not node_name.startswith("lower_tile"):
            continue
        vertices = np.asarray(scene.geometry["tile"].vertices, dtype=float)
        points = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]
        lower_tiles.append(_corrected(points))
    lower_tiles = np.vstack(lower_tiles)

    ornaments = []
    for node_name in scene.graph.nodes_geometry:
        matrix, geometry_name = scene.graph[node_name]
        if node_name not in ORNAMENT_NAMES:
            continue
        vertices = np.asarray(scene.geometry[geometry_name].vertices, dtype=float)
        points = (np.hstack([vertices, np.ones((len(vertices), 1))]) @ np.asarray(matrix, float).T)[:, :3]
        centre = points.mean(axis=0)
        height = float(points[:, 2].max() - points[:, 2].min())
        # roof surface in a small window around the ornament
        window = lower_tiles[
            (np.abs(lower_tiles[:, 0] - centre[0]) < 0.4) & (np.abs(lower_tiles[:, 1] - centre[1]) < 0.4)
        ]
        surface = float(window[:, 2].max()) if len(window) else None
        ornaments.append({
            "node": node_name,
            "x": round(float(centre[0]), 4),
            "y": round(float(centre[1]), 4),
            "z": round(float(centre[2]), 4),
            "height_m": round(height, 4),
            "roof_surface_z_m": round(surface, 4) if surface is not None else None,
            "buried_by_m": round(surface - (float(points[:, 2].max())), 4) if surface is not None else None,
            "reseat_centre_z_m": round(surface + height * 0.30, 4) if surface is not None else None,
        })
    report["ridge_ornaments"] = ornaments
    for item in ornaments:
        print(f"ornament {item['node']} at ({item['x']}, {item['y']}, {item['z']}) "
              f"roof={item['roof_surface_z_m']} buried_by={item['buried_by_m']} "
              f"reseat_z={item['reseat_centre_z_m']}")

    # ---- ridge mesh UV layout ----
    ridge = scene.geometry["ridge"]
    uv = np.asarray(ridge.visual.uv, dtype=float)
    vertices = np.asarray(ridge.vertices, dtype=float)
    report["ridge_mesh"] = {
        "vertices": int(len(vertices)),
        "triangles": int(len(ridge.faces)),
        "uv_range": [uv.min(axis=0).round(4).tolist(), uv.max(axis=0).round(4).tolist()],
        "corr_u_axes": [round(float(np.corrcoef(uv[:, 0], vertices[:, axis])[0, 1]), 3) for axis in range(3)],
        "corr_v_axes": [round(float(np.corrcoef(uv[:, 1], vertices[:, axis])[0, 1]), 3) for axis in range(3)],
        "local_extents": np.asarray(ridge.extents, dtype=float).round(4).tolist(),
    }
    print("ridge mesh", json.dumps(report["ridge_mesh"]))

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE3_AUDIT", OUTPUT)


if __name__ == "__main__":
    main()
