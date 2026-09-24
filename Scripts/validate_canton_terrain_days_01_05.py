"""Validate the Days 01–05 evidence inventory without promoting provisional data.

Exit 0 means the files, provenance and failed gates agree; historical acceptance
is reported separately and remains false until measured evidence replaces them.
"""

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_csv(relative):
    with (ROOT / relative).open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        return reader.fieldnames, list(reader)


def main():
    errors = []
    blockers = []
    contract = json.loads((ROOT / "Data/Coordinate_Contract.json").read_text(encoding="utf-8"))
    if contract.get("horizontal_crs") != "EPSG:32649":
        errors.append("Unexpected horizontal CRS")
    if contract.get("status") != "provisional_working_horizontal_origin_not_approved":
        errors.append("Working horizontal origin must remain explicitly provisional")
    if [contract.get("origin_easting_m"), contract.get("origin_northing_m")] != [729400, 2557300]:
        errors.append("Working origin differs from provisional extent calculation")
    if contract.get("proposed_landscape", {}).get("extent_m") != 4032 or contract.get("proposed_landscape", {}).get("approved") is not False:
        errors.append("Provisional Landscape extent was changed or promoted")
    if contract.get("terrain_zero_elevation_m") is not None or contract.get("terrain_zero_vertical_datum") is not None:
        errors.append("Historical terrain zero was populated without datum-backed evidence")
    blockers.append("Day 01: working XY origin and numeric budgets need review; terrain Z unresolved (ordinary-foliage fallback selected)")

    fields, sources = read_csv("Data/Source_Register.csv")
    mandatory = {"source_id", "archive_id", "retrieval_url", "historical_confidence", "measurement_reliability", "temporal_relevance", "modern_change_risk", "sha256"}
    if not mandatory.issubset(fields or []):
        errors.append("Source register schema incomplete")
    ids = [row["source_id"] for row in sources]
    if len(set(ids)) != len(ids):
        errors.append("Duplicate source IDs")
    for row in sources:
        if not row["archive_id"] or not row["retrieval_url"] or not row["measurement_reliability"]:
            errors.append("Incomplete provenance: " + row["source_id"])
        if row["historical_confidence"] not in "ABCD":
            errors.append("Invalid confidence: " + row["source_id"])
        if row["local_path"]:
            path = ROOT / row["local_path"]
            if not path.is_file():
                errors.append("Missing source file: " + row["local_path"])
            elif hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
                errors.append("Source checksum mismatch: " + row["source_id"])
    if not {"MAP-001", "OSM-001", "ELEV-002", "ELEV-003", "ARCH-003", "ARCH-004"}.issubset(ids):
        errors.append("Required map, control, DTM, uncertainty or archaeology source not registered")
    map_row = next((row for row in sources if row["source_id"] == "MAP-001"), {})
    if not map_row.get("observation_date", "").startswith("1880 publication"):
        errors.append("British Library publication date not recorded")

    _, artifacts = read_csv("Data/Artifact_Manifest.csv")
    artifact_paths = [row["path"] for row in artifacts]
    if len(artifact_paths) != len(set(artifact_paths)):
        errors.append("Duplicate artifact paths")
    for row in artifacts:
        path = ROOT / row["path"]
        if not path.is_file():
            errors.append("Missing artifact: " + row["path"])
        elif hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            errors.append("Artifact checksum mismatch: " + row["path"])

    _, landmarks = read_csv("Data/Landmark_Candidates.csv")
    if any(row["source_id"] not in ids for row in landmarks):
        errors.append("Landmark with unregistered source")
    if not (ROOT / "Sources/HistoricMaps/Canton_Vrooman_BritishLibrary_001954731.jpg").is_file():
        errors.append("Historic map candidate missing")
    blockers.append("Day 02: publication is 1880, but original survey date and map scale are unverified")

    _, gcps = read_csv("QA/Map_GCP_Residuals.csv")
    transform = json.loads((ROOT / "Data/Map_Transform_Provisional.json").read_text(encoding="utf-8"))
    checks = [float(row["residual_m"]) for row in gcps if row["role_control_or_holdout"] == "holdout"]
    controls = [row for row in gcps if row["role_control_or_holdout"] == "control"]
    if len(controls) != 5 or len(checks) != 3:
        errors.append("Expected five fit controls and three independent checks")
    if any(row.get("status") != "candidate_not_surveyed" for row in gcps):
        errors.append("Unsurveyed map candidates were mislabeled as surveyed")
    if checks:
        rmse = math.sqrt(sum(value * value for value in checks) / len(checks))
        if abs(rmse - transform["independent_check_rmse_m"]) > 0.01 or abs(max(checks) - transform["independent_check_max_m"]) > 0.01:
            errors.append("Holdout summary does not match residual CSV")
    if transform.get("acceptance", {}).get("passed") is not False or transform.get("status") != "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE":
        errors.append("Failed transform was promoted to accepted")
    if not (ROOT / "GIS/Provisional/Canton_Historic_Georef_PROVISIONAL.tif").is_file():
        errors.append("Provisional georeferenced raster missing")
    blockers.append("Day 03: 3 unsurveyed holdouts; 93.24 m RMSE and 132.155 m worst exceed budget")
    if any((ROOT / name).exists() for name in ("GIS/Wall.geojson", "GIS/Gates.geojson", "GIS/Roads_Main.geojson", "GIS/Waterways.geojson")):
        errors.append("Accepted GIS geometry exists before the accepted georeference gate")
    expected_counts = {"Wall": 2, "Gates": 4, "Roads_Main": 2, "Waterways": 2, "Landforms": 1, "Landmarks": 8}
    wall_bounds = None
    for layer, expected in expected_counts.items():
        path = ROOT / f"GIS/Provisional/{layer}.geojson"
        if not path.is_file():
            errors.append("Missing provisional layer: " + layer)
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        features = data.get("features", [])
        if len(features) != expected or data.get("metadata", {}).get("status") != "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE":
            errors.append("Incorrect provisional layer count/status: " + layer)
        for feature in features:
            props = feature.get("properties", {})
            if (props.get("source_id") != "MAP-001" or props.get("historical_confidence") != "D"
                    or props.get("georef_status") != "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE"):
                errors.append("Unattributed provisional feature: " + layer)
        if layer == "Wall" and features:
            wall_bounds = features[0].get("properties", {}).get("metric_bounds_m")
    extent = json.loads((ROOT / "Data/Extent_Provisional.json").read_text(encoding="utf-8"))
    if extent.get("historically_accepted") is not False or not extent.get("fits_with_200m_buffer"):
        errors.append("Provisional extent fit/status inconsistent")
    if extent.get("proposed_origin_epsg32649_m") != [contract["origin_easting_m"], contract["origin_northing_m"]]:
        errors.append("Extent and coordinate origin disagree")
    if wall_bounds is None or any(abs(a - b) > 0.11 for a, b in zip(wall_bounds, extent.get("wall_bounds_epsg32649_m", []))):
        errors.append("Wall feature bounds and extent report disagree")
    if min(extent.get("clearance_from_wall_m", [0])) < 200:
        errors.append("Less than 200 m provisional wall clearance")
    blockers.append("Day 04: coarse layers and extent fit exist only under failed transform; no accepted GIS")

    _, heights = read_csv("Data/Elevation_Constraints.csv")
    if any(row["source_id"] not in ids for row in heights):
        errors.append("Elevation constraint with unregistered source")
    for row in heights:
        if row["height_m"] and (not row["vertical_datum"] or not row["observation_date"]):
            errors.append("Undocumented height: " + row["constraint_id"])
    for row in heights:
        if row["status"] != "candidate_stratum_not_terrain" or row["height_m"]:
            errors.append("Unaccepted archaeological stratum was promoted to terrain Z: " + row["constraint_id"])
        if row["source_id"] == "ARCH-004":
            if row["min_height_m"] != "5.92" or row["max_height_m"] != "6.22" or "unspecified" not in row["vertical_datum"]:
                errors.append("Published Qing layer range or datum caveat altered")
            if row["units"] != "m" or row["historical_confidence"] != "D" or not row["observation_date"]:
                errors.append("Qing layer units, date or terrain-confidence missing")
    if len(heights) != 1 or heights[0]["source_id"] != "ARCH-004":
        errors.append("Expected one non-terrain Qing stratigraphy candidate")
    for source_id in ("ELEV-002", "ELEV-003"):
        row = next((row for row in sources if row["source_id"] == source_id), {})
        if row.get("vertical_datum") != "EGM2008 EPSG:3855" or row.get("historical_confidence") != "D":
            errors.append("Modern model misclassified as historical terrain: " + source_id)
    blockers.append("Day 05: modern bare-earth acquired and one Qing stratum range registered; datum-backed 1880 ground Z remains unavailable")

    print(json.dumps({"inventory_consistent": not errors, "source_count": len(sources),
                      "artifact_count": len(artifacts),
                      "landmark_candidate_count": len(landmarks), "gcp_count": len(gcps),
                      "provisional_layer_feature_counts": expected_counts,
                      "elevation_constraint_count": len(heights), "errors": errors,
                      "blocked_gates": blockers}, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
