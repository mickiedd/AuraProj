# Canton terrain days 01–05 evidence foundation

Date: 2026-09-24. [Visual summary](2026-09-24-canton-terrain-days-01-05-evidence-foundation.svg).

## Intent and changed behavior

Started the first five daily plans with source provenance and explicit historical evidence gates. Added a local copy of the 1880/1890-labelled Vrooman map, six source-register candidates, four image-only landmark candidates, coordinate and acceptance contracts, artifact checksums, map and elevation schemas, per-day resource status, and a read-only validator. The working projection is EPSG:32649; the local origin and vertical zero remain unset. PCG is installed but not enabled or smoke-cooked, so ordinary foliage instances are the recorded prototype path.

Day 02 map selection is provisional because the Commons date and embedded British Library metadata disagree. Day 03 has no surveyed controls or holdouts, so there is no georeferenced TIFF. Day 04 has no accepted transform, so no wall/gate/road/water GeoJSON or extent pass is claimed. Day 05 has no bare-earth source or datum-backed historic height; Copernicus GLO-30 is registered only as a modern DSM candidate. Gate district names remain asset-readiness candidates without claimed locations.

## Validation and limits

`python3 Scripts/validate_canton_terrain_days_01_05.py` exited 0: six sources, four landmark candidates, zero control points, zero elevation constraints, no inventory errors, and all outstanding gates listed. `python3 -m py_compile` and `git diff --check` exited 0. The acquired scan's SHA-256 is `ebab90db8e25cd87fea7ac4e5e6d3d2e45e4659fb35bff6b509aba6f62406bd7`. A human opened the full scan and confirmed the wall and labelled landmarks are readable. No UE PCG cook, GIS map load, field survey, or elevation validation was performed.

The record and diagram distinguish reproducible preparation from historical approval. Unrelated pre-existing Wenmingmen working-tree changes were preserved.
