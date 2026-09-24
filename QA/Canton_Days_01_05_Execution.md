# Canton terrain days 01–05 execution — 2026-09-24

Baseline: `c840ce2a22993c33491a5d0fa1c74add125cf3ce`; unrelated working-tree edits were present and left intact. Operator: Codex. Python 3.9.6; UE 5.5.4, changelist 40574608, resolved from the project GUID through `Scripts/macos/unreal-common.sh`. No QGIS/GDAL executable was found on PATH. The first-party validator uses only Python's standard library.

| Check | Command / action | Exit | Evidence |
|---|---|---:|---|
| Repository identity | `git rev-parse HEAD`; `git status --short` | 0 | Baseline revision above; existing Wenmingmen and editor edits preserved |
| Engine identity | Read `Aura.uproject` and `/Volumes/M2/Engine/UE_5.5/Engine/Build/Build.version` | 0 | GUID association; UE 5.5.4 CL 40574608 |
| PCG inventory | Inspect `Aura.uproject` and installed `Engine/Plugins/PCG/PCG.uplugin` | 0 | Installed but not enabled; no load/cook claim |
| Map acquisition | Download original Commons file to `Sources/HistoricMaps/` with Python `urllib.request`; SHA-256 | 0 | 6,815,142 bytes; `ebab90db8e25cd87fea7ac4e5e6d3d2e45e4659fb35bff6b509aba6f62406bd7` |
| Map visual inspection | Open the 7017 × 4384 scan; inspect wall, gate labels and Five Story Pagoda | done | Readable period map; no image-to-ground positions claimed |
| Evidence validation | `python3 Scripts/validate_canton_terrain_days_01_05.py` | 0 | Six registered candidates, four image-only landmark candidates, zero GCP and zero elevation measurements; five outstanding gates reported |
| Script syntax | `python3 -m py_compile Scripts/validate_canton_terrain_days_01_05.py` | 0 | Compiles |
| Patch whitespace | `git diff --check` | 0 | No whitespace errors in tracked edits |

No georeferenced TIFF, wall polygon, road/waterway GIS layer, bare-earth DEM, absolute height or district coordinate was produced. These outputs require measured controls and source-backed vertical evidence. Day 02 source research proceeded despite Day 01's open approval/origin gate because it has no projected outputs. Day 04/05 schemas proceeded despite Day 03's blocked transform for the same reason. Review/approval remains pending for coordinate origin and numeric acceptance budget.
