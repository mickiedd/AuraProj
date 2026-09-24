"""Read-only evidence gates for the first five Canton terrain days.

Run from any directory: python3 Scripts/validate_canton_terrain_days_01_05.py
Exit 0 means the evidence inventory is internally consistent; it does not
mean all five historical production gates have passed.
"""

import csv
import hashlib
import json
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
    if any(contract.get(key) is not None for key in ("origin_easting_m", "origin_northing_m", "terrain_zero_elevation_m")):
        errors.append("Unreviewed project/terrain origin was populated")
    else:
        blockers.append("Day 01: approved projected and vertical origins missing")

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

    _, landmarks = read_csv("Data/Landmark_Candidates.csv")
    if any(row["source_id"] not in ids for row in landmarks):
        errors.append("Landmark with unregistered source")
    if not (ROOT / "Sources/HistoricMaps/Canton_Vrooman_BritishLibrary_001954731.jpg").is_file():
        errors.append("Historic map candidate missing")
    blockers.append("Day 02: map edition/date and scale unresolved")

    _, gcps = read_csv("QA/Map_GCP_Residuals.csv")
    if not gcps:
        blockers.append("Day 03: no surveyed controls or independent holdouts; no georeferenced TIFF")
    if any((ROOT / name).exists() for name in ("GIS/Wall.geojson", "GIS/Gates.geojson", "GIS/Roads_Main.geojson", "GIS/Waterways.geojson")):
        errors.append("GIS geometry exists before the accepted georeference gate")
    else:
        blockers.append("Day 04: wall/gate/road/waterway digitization and extent await georeference")

    _, heights = read_csv("Data/Elevation_Constraints.csv")
    if any(row["source_id"] not in ids for row in heights):
        errors.append("Elevation constraint with unregistered source")
    for row in heights:
        if row["height_m"] and (not row["vertical_datum"] or not row["observation_date"]):
            errors.append("Undocumented height: " + row["constraint_id"])
    if not heights:
        blockers.append("Day 05: no datum-backed historical heights or accepted bare-earth DEM")

    print(json.dumps({"inventory_consistent": not errors, "source_count": len(sources),
                      "landmark_candidate_count": len(landmarks), "gcp_count": len(gcps),
                      "elevation_constraint_count": len(heights), "errors": errors,
                      "blocked_gates": blockers}, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
