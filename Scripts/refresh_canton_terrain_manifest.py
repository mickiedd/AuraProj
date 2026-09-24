"""Refresh deterministic SHA-256 records for Days 01–05 terrain artifacts."""

import csv
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FILES = [
    "Docs/Terrain_Baseline.md", "Docs/Terrain_Acceptance_Budget.md",
    "Data/Source_Register.csv", "Data/Coordinate_Contract.json",
    "QA/PCG_Preflight.md", "Sources/HistoricMaps/README.md",
    "Sources/HistoricMaps/Canton_Vrooman_BritishLibrary_001954731.jpg",
    "Data/Historic_Map_Selection.md", "Data/Landmark_Candidates.csv",
    "Data/Map_Control_Candidates.csv", "Data/Map_Transform_Provisional.json",
    "QA/Map_GCP_Residuals.csv", "Docs/Map_Distortion_Notes.md",
    "GIS/Provisional/Canton_Historic_Georef_PROVISIONAL.tif",
    "Data/Map_Pixel_Traces_Provisional.json", "Data/Extent_Provisional.json",
    "QA/Canton_Provisional_Trace_Overlay.jpg", "QA/Extent_Check.md",
    "GIS/README.md", "Sources/Elevation/README.md",
    "Sources/Elevation/GEDTM30_Guangzhou_context_20250611.tif",
    "Sources/Elevation/GEDTM30_Guangzhou_uncertainty_20250611.tif",
    "Data/Elevation_Constraints.csv", "Docs/Vertical_Datum_Register.md",
    "Scripts/build_canton_provisional_georef.py",
    "Scripts/build_canton_provisional_layers.py",
    "Scripts/validate_canton_terrain_days_01_05.py",
    "Scripts/refresh_canton_terrain_manifest.py",
    "Scripts/requirements-canton-terrain.txt",
    "QA/Canton_Days_01_05_Execution.md",
]
FILES += [f"GIS/Provisional/{name}.geojson" for name in
          ("Wall", "Gates", "Roads_Main", "Waterways", "Landforms", "Landmarks")]
FILES += [f"Docs/Plans/Canton-Terrain-Implementation/Resources/Day-{day:02d}-Resources.md"
          for day in range(1, 6)]


def source_for(path):
    if path.startswith("Sources/Elevation/GEDTM30_Guangzhou_context"):
        return "ELEV-002", "CC BY 4.0"
    if path.startswith("Sources/Elevation/GEDTM30_Guangzhou_uncertainty"):
        return "ELEV-003", "CC BY 4.0"
    if path.startswith("Sources/HistoricMaps/"):
        return "MAP-001", "no known copyright restrictions / public domain mark"
    if path == "Data/Elevation_Constraints.csv":
        return "ARCH-004", "published paper transcribed as blocked candidate; original PDF link only"
    if path.startswith("GIS/") or "Map_" in path or "Extent_" in path:
        return "MAP-001;OSM-001", "derived from public-domain map and ODbL candidate coordinates"
    return "repository plans and registered sources", "project-authored; sources cited"


def main():
    rows = []
    for index, relative in enumerate(FILES, 1):
        path = ROOT / relative
        if not path.is_file():
            raise FileNotFoundError(relative)
        source, license_status = source_for(relative)
        rows.append({"artifact_id": f"TERRAIN-{index:03d}", "owner": "AuraProj terrain",
                     "path": relative, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                     "source_inputs": source, "generator_tool_version": "Codex / Python 3.9.6; rasterio 1.4.3 for rasters",
                     "license_status": license_status, "status": "Provisional",
                     "notes": "Review-only; historical acceptance remains blocked"})
    output = ROOT / "Data/Artifact_Manifest.csv"
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} artifact hashes to {output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
