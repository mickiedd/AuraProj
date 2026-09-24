# Canton terrain days 01–05 execution — 2026-09-24

Baseline: `c840ce2a22993c33491a5d0fa1c74add125cf3ce`; unrelated working-tree edits were present and left intact. Operator: Codex. Python 3.9.6; UE 5.5.4, changelist 40574608, resolved from the project GUID through `Scripts/macos/unreal-common.sh`. No QGIS/GDAL executable was found on PATH. The first-party validator uses only Python's standard library.

| Check | Command / action | Exit | Evidence |
|---|---|---:|---|
| Repository identity | `git rev-parse HEAD`; `git status --short` | 0 | Baseline revision above; existing Wenmingmen and editor edits preserved |
| Engine identity | Read `Aura.uproject` and `/Volumes/M2/Engine/UE_5.5/Engine/Build/Build.version` | 0 | GUID association; UE 5.5.4 CL 40574608 |
| PCG inventory | Inspect `Aura.uproject` and installed `Engine/Plugins/PCG/PCG.uplugin` | 0 | Installed but not enabled; no load/cook claim |
| Target hardware | `system_profiler SPHardwareDataType SPDisplaysDataType` | 0 | Mac mini, Apple M4, 10 CPU cores, 16 GB, Metal 4; target proposal only |
| Map acquisition | Download original Commons file to `Sources/HistoricMaps/` with Python `urllib.request`; SHA-256 | 0 | 6,815,142 bytes; `ebab90db8e25cd87fea7ac4e5e6d3d2e45e4659fb35bff6b509aba6f62406bd7` |
| Map visual inspection | Open the 7017 × 4384 scan; inspect wall, gate labels and Five Story Pagoda | done | Readable period map; no image-to-ground positions claimed |
| Evidence validation | `python3 Scripts/validate_canton_terrain_days_01_05.py` | 0 | Six registered candidates, four image-only landmark candidates, zero GCP and zero elevation measurements; five outstanding gates reported |
| Script syntax | `python3 -m py_compile Scripts/validate_canton_terrain_days_01_05.py` | 0 | Compiles |
| Patch whitespace | `git diff --check` | 0 | No whitespace errors in tracked edits |

The table above records the **initial evidence pass**, before the continuation below. No accepted historical geometry or height was produced then.

## Continuation: source resolution and isolated working artifacts

Continuation starting revision: `921d2ca00acd54b54b9061ac72c90f0d44bbb4dd` on `main`. Existing unrelated edits were preserved. Terrain tooling used the ignored `Saved/TerrainTools/venv` (Python 3.9.6; numpy 2.0.2, rasterio 1.4.3, pyproj 3.6.1, shapely 2.0.7, Pillow 11.3.0); pinned versions are in `Scripts/requirements-canton-terrain.txt`.

The British Library image record identifies `MAP-001` as page 41 of the **1880** second edition of *The Canton Guide* (BL `001954731`); actual map survey date and scale remain unknown. Eight map icon/modern OSM centroid matches were logged in `Data/Map_Control_Candidates.csv`. The affine builder uses five for fitting and reserves three as independent checks. The checks measure **93.24 m RMSE** and **132.155 m worst**, exceeding the proposed ≤15 m/≤30 m limits and undershooting the eight-check count. The GeoTIFF and derived layers are confined to `GIS/Provisional/`, tagged D confidence and unsuitable for production import.

The map overlay was visually inspected at 1800 × 1125 pixels and archived at `QA/Canton_Provisional_Trace_Overlay.jpg`. It shows the coarse outer/inner wall, four gate candidates, principal road indications, waterways and Kun Yam Hill extent against the original scan. The generator retains preview-pixel vertices in `Data/Map_Pixel_Traces_Provisional.json`. `Data/Extent_Provisional.json` shows the proposed 4032 m square around this trace clears all wall sides by ≥472.7 m. Working lower-left origin E729400/N2557300 is **not approved**.

OpenGeoHub GEDTM30 v1.1 modern modeled bare-earth and uncertainty COG crops were acquired (CC BY 4.0). Rasterio inspection confirmed both are 720 × 520, EPSG:4326 with EGM2008/EPSG:3855 tags and stored-value scales 0.1 and 0.01 respectively. The 177-page official Guangta/Chashu excavation report yielded only relative depths. A 2021 peer reviewed stratigraphy paper (`ARCH-004`) reports a Qing L3 layer at 5.92–6.22 m sea-level elevation at a mapped Jiefang Middle Road site. The datum realization and 1880 ground-surface relation are unresolved; `QING-STRATUM-001` is a blocked candidate with blank `height_m`. Both research PDFs are ignored under `Saved/TerrainTools/`, with original links and SHA in the source register.

| Check | Command / action | Exit | Evidence |
|---|---|---:|---|
| Affine rebuild | `Saved/TerrainTools/venv/bin/python Scripts/build_canton_provisional_georef.py` | 0 | 7017 × 4384 EPSG:32649 review TIFF, SHA `76b6f08edb581147a170dea8bb01fd890c658554600dbc62bdc1f55e35eceec2`; `accepted: false` |
| Vector/extent rebuild | `Saved/TerrainTools/venv/bin/python Scripts/build_canton_provisional_layers.py` | 0 | 2 wall, 4 gate, 2 road, 2 waterway, 1 landform, 8 landmark features; 200 m buffer fits |
| Raster metadata | `Saved/TerrainTools/venv/bin/python` + rasterio open on the three TIFFs | 0 | Modern vertical datum and scale tags; provisional historical-status tag |
| Manifest / integrity | `python3 Scripts/refresh_canton_terrain_manifest.py`; `python3 Scripts/validate_canton_terrain_days_01_05.py` | 0 / 0 | 41 artifact hashes; 11 registered sources; 8 residuals; 6 provisional layers; 1 non-terrain stratigraphic candidate; 0 usable historical terrain Z; 0 integrity errors; five open acceptance gates |
| Syntax / whitespace | `python3 -m py_compile` on terrain scripts; `git diff --check` | 0 / 0 | Final validation |
| Visual archive | `rsvg-convert -o Saved/TerrainTools/canton-terrain-change-archive-preview.png Docs/Reports/Change-Archive/2026-09-24-canton-terrain-provisional-georef-and-elevation.svg`; visual inspection | 0 | Rendered SVG readable; before/after outputs, both gates and validation visible |

The only warnings from the affine rebuild were rasterio's expected `NotGeoreferencedWarning` for the original JPEG. Historical acceptance remains blocked by surveyed map controls and datum-backed late-Qing ground levels; the modern DTM is context only. The five resource sheets contain the superseding status records.
