"""Snapshot provisional M01 outputs without changing the original source manifest."""
import csv
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    'Scripts/build_canton_provisional_heightmap.py', 'Tools/Validate_Heightmap.py',
    'Scripts/test_canton_source_manifest.py', 'Scripts/test_canton_heightmap_contract.py',
    'Scripts/refresh_canton_m01_provisional_manifest.py',
    'GIS/Provisional/Modern_Context_Elevation_float32.tif',
    'GIS/Provisional/Terrain_Confidence_PROVISIONAL.tif',
    'QA/Provisional/Modern_Change_Mask.tif',
    'Export/Provisional/Canton_Modern_Context_2017_R16.png',
    'Export/Provisional/Heightmap_Metadata.json', 'QA/Heightmap_Encode_Report.md',
    'Review/M01_Terrain_Review.md', 'Data/M01_Uncertainty_Log.csv',
    'Docs/Plans/Canton-Terrain-Implementation/README.md',
    'Docs/Reports/Change-Archive/2026-09-26-canton-days-06-10-provisional-pipeline.svg',
    'Docs/Reports/Change-Archive/2026-09-26-canton-days-06-10-provisional-pipeline.md',
] + [f'Docs/Plans/Canton-Terrain-Implementation/Resources/Day-{day:02d}-Resources.md' for day in range(6, 11)]


def main():
    with (ROOT / 'Data/M01_Provisional_Artifacts.csv').open('w', newline='') as stream:
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(['path', 'sha256', 'status', 'owner', 'source_inputs', 'license', 'tool_versions'])
        for relative in FILES:
            writer.writerow([relative, hashlib.sha256((ROOT / relative).read_bytes()).hexdigest(),
                             'Provisional; M01 Blocked', 'AuraProj terrain',
                             'ELEV-002; Coordinate_Contract; existing source manifest; repository plans',
                             'project-authored; raster derivatives CC BY 4.0 OpenGeoHub GEDTM30 v1.1',
                             'Python 3.9.6; Scripts/requirements-canton-terrain.txt'])
    print(f'Snapshotted {len(FILES)} provisional artifacts')


if __name__ == '__main__':
    main()
