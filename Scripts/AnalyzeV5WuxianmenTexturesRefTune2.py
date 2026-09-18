"""Measure the Wuxianmen V5 Core texture sheets so material tiling is derived, not guessed.

Two questions, both answered numerically:

1. Roof base colour: the authored tile sheet occupies only part of the texture
   container. Measure the authored region, then measure the tile column period
   (U) and course period (V) inside it. That gives the exact number of tile
   repeats per unit UV, which is what the material tiling must divide by so the
   strip's 0.3589 m width shows one tile and its 4.24 m slope shows real courses.

2. Stone base colour: report the mean colour of the texture and of the matching
   reference sheet wall region, so the material tint can be set to close a
   measured gap instead of being picked by eye.

Read-only. Run with the trimesh/PIL interpreter (Python 3.10).
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
REPAIRED = ROOT / "RepairedTextures/Wuxianmen_V5_4K_Core"
REFERENCE = PROJECT / "Docs/Reference/GateSheets/Wuxianmen-reference.png"
RENDER = ROOT / "Wuxianmen_V5_4K_Core-reference-tuning-hero.png"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-reftune2-texture-analysis-20260917.json"


def _load(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.float64) / 255.0


def _authored_rows(image: np.ndarray) -> tuple[int, int]:
    """Row range whose luminance is not essentially black padding."""
    row_mean = image.mean(axis=(1, 2))
    lit = np.where(row_mean > 0.02)[0]
    return int(lit.min()), int(lit.max()) + 1


def _period(signal: np.ndarray) -> float:
    """Dominant period in samples via autocorrelation of the mean-removed signal."""
    centred = signal - signal.mean()
    if not np.any(centred):
        return float("nan")
    corr = np.correlate(centred, centred, mode="full")[len(centred) - 1:]
    corr /= corr[0]
    # first strong local maximum after the trivial peak
    for lag in range(8, len(corr) // 2):
        if corr[lag] > corr[lag - 1] and corr[lag] >= corr[lag + 1] and corr[lag] > 0.15:
            return float(lag)
    return float("nan")


def main() -> None:
    report = {"created": "2026-09-17"}

    roof = _load(REPAIRED / "Roof_BaseColor_4K_Repaired.png")
    top, bottom = _authored_rows(roof)
    sheet = roof[top:bottom]
    report["roof_base_color"] = {
        "container": list(roof.shape[:2]),
        "authored_rows": [top, bottom],
        "authored_fraction_of_v": round((bottom - top) / roof.shape[0], 4),
        "sheet_mean_rgb": sheet.mean(axis=(0, 1)).round(4).tolist(),
    }
    # Column structure: average columns then look along U.
    column_signal = sheet.mean(axis=0).mean(axis=1)
    row_signal = sheet.mean(axis=1).mean(axis=1)
    column_px = _period(column_signal)
    course_px = _period(row_signal)
    report["roof_base_color"].update({
        "tile_column_period_px": column_px,
        "tile_course_period_px": course_px,
        "tile_columns_per_unit_u": round(sheet.shape[1] / column_px, 3) if column_px == column_px else None,
        "tile_courses_per_unit_v_of_sheet": round((bottom - top) / course_px, 3) if course_px == course_px else None,
    })

    stone = _load(REPAIRED / "Stone_BaseColor_4K_Repaired.png")
    stone_column_px = _period(stone.mean(axis=0).mean(axis=1))
    stone_course_px = _period(stone.mean(axis=1).mean(axis=1))
    report["stone_base_color"] = {
        "container": list(stone.shape[:2]),
        "mean_rgb": stone.mean(axis=(0, 1)).round(4).tolist(),
        "luminance": round(float(stone.mean()), 4),
        "stone_period_u_px": stone_column_px,
        "stone_period_v_px": stone_course_px,
        "stones_per_unit_u": round(stone.shape[1] / stone_column_px, 3) if stone_column_px == stone_column_px else None,
        "stones_per_unit_v": round(stone.shape[0] / stone_course_px, 3) if stone_course_px == stone_course_px else None,
    }

    wood = _load(REPAIRED / "Wood_BaseColor_4K_Repaired.png")
    report["wood_base_color"] = {
        "container": list(wood.shape[:2]),
        "mean_rgb": wood.mean(axis=(0, 1)).round(4).tolist(),
    }

    # Reference sheet: several candidate bands on the perspective view's gate wall.
    reference = _load(REFERENCE)
    height, width = reference.shape[:2]
    bands = {
        "wall_upper": (0.50, 0.56, 0.24, 0.46),
        "wall_mid": (0.56, 0.62, 0.26, 0.44),
        "wall_lower": (0.62, 0.68, 0.28, 0.42),
        "arch_face": (0.55, 0.62, 0.30, 0.36),
    }
    report["reference_sheet"] = {"container": [height, width], "bands": {}}
    for name, (y0, y1, x0, x1) in bands.items():
        patch = reference[int(height * y0):int(height * y1), int(width * x0):int(width * x1)]
        report["reference_sheet"]["bands"][name] = {
            "mean_rgb": patch.mean(axis=(0, 1)).round(4).tolist(),
            "luminance": round(float(patch.mean()), 4),
        }

    render = _load(RENDER)
    rheight, rwidth = render.shape[:2]
    render_wall = render[int(rheight * 0.52):int(rheight * 0.62), int(rwidth * 0.30):int(rwidth * 0.62)]
    report["render"] = {
        "container": [rheight, rwidth],
        "wall_band_mean_rgb": render_wall.mean(axis=(0, 1)).round(4).tolist(),
        "wall_band_luminance": round(float(render_wall.mean()), 4),
    }

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    print("REFTUNE2_TEXTURE_ANALYSIS", OUTPUT)


if __name__ == "__main__":
    main()
