"""Fit and publish the 1880 Canton scan as an explicitly provisional GeoTIFF.

Requires numpy, pyproj, rasterio. Control coordinates are modern OSM candidates,
not survey marks. A failed holdout budget remains failed in every output.
"""

import csv
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import rasterio
from pyproj import Transformer
from rasterio.transform import Affine

ROOT = Path(__file__).resolve().parents[1]
GCP_PATH = ROOT / "Data/Map_Control_Candidates.csv"
SOURCE_PATH = ROOT / "Sources/HistoricMaps/Canton_Vrooman_BritishLibrary_001954731.jpg"
OUTPUT_DIR = ROOT / "GIS/Provisional"
OUTPUT_PATH = OUTPUT_DIR / "Canton_Historic_Georef_PROVISIONAL.tif"


def main():
    with GCP_PATH.open(newline="", encoding="utf-8") as stream:
        points = list(csv.DictReader(stream))
    controls = [p for p in points if p["role"] == "control"]
    holdouts = [p for p in points if p["role"] == "holdout"]
    if len(controls) < 3 or not holdouts:
        raise ValueError("Need at least three controls and an independent holdout")

    projection = Transformer.from_crs("EPSG:4326", "EPSG:32649", always_xy=True)
    for point in points:
        point["easting_m"], point["northing_m"] = projection.transform(
            float(point["longitude_deg"]), float(point["latitude_deg"])
        )

    design = np.array([[float(p["pixel_x"]), float(p["pixel_y"]), 1] for p in controls])
    target = np.array([[p["easting_m"], p["northing_m"]] for p in controls])
    coefficients, _, rank, _ = np.linalg.lstsq(design, target, rcond=None)
    if rank != 3:
        raise ValueError("Control points do not define a full affine fit")

    residual_rows = []
    for point in points:
        predicted = np.array([float(point["pixel_x"]), float(point["pixel_y"]), 1]) @ coefficients
        delta = predicted - np.array([point["easting_m"], point["northing_m"]])
        residual_rows.append({
            "point_id": point["point_id"], "role_control_or_holdout": point["role"],
            "source_id": point["map_source_id"], "location_source_id": point["location_source_id"],
            "pixel_x": point["pixel_x"], "pixel_y": point["pixel_y"],
            "easting_m": round(point["easting_m"], 3), "northing_m": round(point["northing_m"], 3),
            "predicted_easting_m": round(float(predicted[0]), 3),
            "predicted_northing_m": round(float(predicted[1]), 3),
            "residual_e_m": round(float(delta[0]), 3),
            "residual_n_m": round(float(delta[1]), 3),
            "residual_m": round(float(np.linalg.norm(delta)), 3),
            "landmark_persistence_evidence": point["persistence_note"],
            "status": "candidate_not_surveyed", "rejection_reason": point["notes"],
        })

    checks = [float(p["residual_m"]) for p in residual_rows if p["role_control_or_holdout"] == "holdout"]
    check_rmse = math.sqrt(sum(value * value for value in checks) / len(checks))
    check_max = max(checks)
    budget_met = len(checks) >= 8 and check_rmse <= 15 and check_max <= 30
    if budget_met:
        raise AssertionError("This provisional-only builder cannot promote a fit to verified")

    result = {
        "status": "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE", "map_source_id": "MAP-001",
        "map_sha256": hashlib.sha256(SOURCE_PATH.read_bytes()).hexdigest(),
        "coordinate_source_id": "OSM-001", "target_crs": "EPSG:32649",
        "pixel_to_projected": {
            "easting_m": {"pixel_x": float(coefficients[0, 0]), "pixel_y": float(coefficients[1, 0]), "offset": float(coefficients[2, 0])},
            "northing_m": {"pixel_x": float(coefficients[0, 1]), "pixel_y": float(coefficients[1, 1]), "offset": float(coefficients[2, 1])},
        },
        "control_count": len(controls), "independent_check_count": len(checks),
        "independent_check_rmse_m": round(check_rmse, 3), "independent_check_max_m": round(check_max, 3),
        "acceptance": {"required_independent_checks": 8, "max_rmse_m": 15, "max_residual_m": 30,
                       "passed": False, "reason": "Too few independent checks and residual budget exceeded; OSM centroids are not surveyed points"},
        "method": "affine least squares; no polynomial adjustment; no forced modern street fit",
    }

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    transform = Affine(float(coefficients[0, 0]), float(coefficients[1, 0]), float(coefficients[2, 0]),
                       float(coefficients[0, 1]), float(coefficients[1, 1]), float(coefficients[2, 1]))
    with rasterio.open(SOURCE_PATH) as source:
        profile = source.profile.copy()
        profile.update(driver="GTiff", crs="EPSG:32649", transform=transform, compress="jpeg",
                       photometric="YCBCR", interleave="pixel", jpeg_quality=90,
                       tiled=True, blockxsize=256, blockysize=256)
        temporary_path = OUTPUT_PATH.with_suffix(".tmp.tif")
        with rasterio.open(temporary_path, "w", **profile) as output:
            for band in range(1, source.count + 1):
                output.write(source.read(band), band)
            output.update_tags(HISTORICAL_STATUS=result["status"], MAP_SOURCE_ID="MAP-001",
                               HOLDOUT_RMSE_M=str(result["independent_check_rmse_m"]),
                               HOLDOUT_MAX_M=str(result["independent_check_max_m"]))
        temporary_path.replace(OUTPUT_PATH)
    with (ROOT / "QA/Map_GCP_Residuals.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(residual_rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(residual_rows)
    (ROOT / "Data/Map_Transform_Provisional.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"geotiff": str(OUTPUT_PATH.relative_to(ROOT)), "checks": len(checks),
                      "check_rmse_m": result["independent_check_rmse_m"], "check_max_m": result["independent_check_max_m"],
                      "accepted": False, "geotiff_sha256": hashlib.sha256(OUTPUT_PATH.read_bytes()).hexdigest()}))


if __name__ == "__main__":
    main()
