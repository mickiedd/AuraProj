"""Measure the Wuxianmen V5 FullPBR texture sheets so material tiling is derived, not guessed.

The FullPBR package uses a different texture set from the Core package: names are
`<Surface>_BaseColor_4K_Repaired` and roughness arrives packed in
`<Surface>_MetallicRoughness_4K_Repaired` (glTF convention: G = roughness,
B = metallic), and it ships plaster, iron and plaque maps that Core does not.

Reports, per surface: the authored region of the container, the pattern period
along U and V, and the derived repeats per unit UV. Also reports the mean colour
of each base-colour sheet and the reference sheet's wall band, so tints can be
set against a measured target.

Read-only. Run with the PIL interpreter (Python 3.10).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
REPAIRED = ROOT / "RepairedTextures/Wuxianmen_V5_FullPBR"
REFERENCE = PROJECT / "Docs/Reference/GateSheets/Wuxianmen-reference.png"
OUTPUT = ROOT / "Wuxianmen_V5_FullPBR-reftune2-texture-analysis-20260917.json"


def _load(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0


def _authored_rows(image: np.ndarray, threshold: float = 0.02) -> tuple[int, int]:
    row_mean = image.mean(axis=(1, 2))
    lit = np.where(row_mean > threshold)[0]
    return int(lit.min()), int(lit.max()) + 1


def _period(signal: np.ndarray, minimum_lag: int = 40) -> float:
    """Dominant period in samples via autocorrelation of the mean-removed signal."""
    centred = signal - signal.mean()
    if not np.any(centred):
        return float("nan")
    corr = np.correlate(centred, centred, mode="full")[len(centred) - 1:]
    corr /= corr[0]
    for lag in range(minimum_lag, len(corr) // 2):
        if corr[lag] > corr[lag - 1] and corr[lag] >= corr[lag + 1] and corr[lag] > 0.15:
            return float(lag)
    return float("nan")


def main():
    report = {"created": "2026-09-17", "package": "Wuxianmen_V5_FullPBR", "sheets": {}}

    for surface in ("RoofTile", "Stone", "Wood", "Plaster", "Iron"):
        base = _load(REPAIRED / f"{surface}_BaseColor_4K_Repaired.png")
        top, bottom = _authored_rows(base)
        sheet = base[top:bottom]
        column_px = _period(sheet.mean(axis=0).mean(axis=1))
        course_px = _period(sheet.mean(axis=1).mean(axis=1))
        entry = {
            "container": list(base.shape[:2]),
            "authored_rows": [top, bottom],
            "authored_fraction_of_v": round((bottom - top) / base.shape[0], 4),
            "authored_mean_rgb": sheet.mean(axis=(0, 1)).round(4).tolist(),
            "period_u_px": column_px,
            "period_v_px": course_px,
        }
        if column_px == column_px:
            entry["units_per_u"] = round(base.shape[1] / column_px, 3)
        if course_px == course_px:
            entry["units_per_authored_v"] = round((bottom - top) / course_px, 3)
            entry["units_per_v"] = round(base.shape[0] / course_px, 3)
        report["sheets"][surface] = entry
        print(
            f"{surface:8s} authored_rows={entry['authored_rows']} "
            f"period_u={column_px} period_v={course_px} "
            f"units_per_u={entry.get('units_per_u')} units_per_authored_v={entry.get('units_per_authored_v')} "
            f"mean={entry['authored_mean_rgb']}"
        )

    # Metallic-roughness packing check: which channel carries roughness.
    mr = _load(REPAIRED / "RoofTile_MetallicRoughness_4K_Repaired.png")
    report["metallic_roughness"] = {
        "container": list(mr.shape[:2]),
        "mean_r": round(float(mr[:, :, 0].mean()), 4),
        "mean_g": round(float(mr[:, :, 1].mean()), 4),
        "mean_b": round(float(mr[:, :, 2].mean()), 4),
        "note": "glTF packing: G = roughness, B = metallic. The Core package used a separate Roughness map.",
    }
    print("metallic_roughness mean RGB",
          report["metallic_roughness"]["mean_r"], report["metallic_roughness"]["mean_g"],
          report["metallic_roughness"]["mean_b"])

    reference = _load(REFERENCE)
    height, width = reference.shape[:2]
    bands = {
        "wall_upper": (0.50, 0.56, 0.24, 0.46),
        "wall_mid": (0.56, 0.62, 0.26, 0.44),
        "wall_lower": (0.62, 0.68, 0.28, 0.42),
    }
    report["reference_sheet"] = {"container": [height, width], "bands": {}}
    for name, (y0, y1, x0, x1) in bands.items():
        patch = reference[int(height * y0):int(height * y1), int(width * x0):int(width * x1)]
        report["reference_sheet"]["bands"][name] = {
            "mean_rgb": patch.mean(axis=(0, 1)).round(4).tolist(),
            "luminance": round(float(patch.mean()), 4),
        }
        print("reference", name, report["reference_sheet"]["bands"][name])

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("FULLPBR_TEXTURE_ANALYSIS", OUTPUT)


if __name__ == "__main__":
    main()
