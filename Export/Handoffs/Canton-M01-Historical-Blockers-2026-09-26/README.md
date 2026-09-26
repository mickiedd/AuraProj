# Canton M01 historical-blocker handoff

Exported: 2026-09-26 from AuraProj. This packet is self-contained for research and GIS work on the two unresolved evidence gates in Days 03 and 05. The Unreal Landscape and engineering pipeline already pass their provisional technical checks; do not spend this handoff rebuilding them.

## Objective

Produce a new, versioned historical evidence set that can legitimately replace the failed provisional map transform and/or establish an 1880–1900 ground-height model. A useful result may resolve either gate independently. Preserve negative findings and report why a candidate cannot be used.

### Horizontal gate

Find surveyed or otherwise authoritative coordinates for exact persistent features that can be identified on `MAP-001`. Build at least five fit controls and eight separate holdouts, with at least two holdouts in each quadrant and both perimeter and interior coverage. Also retain at least four separate local checks within the selected 200 × 200 m gate district.

Acceptance targets:

- city holdout RMSE no more than 15 m;
- city worst residual no more than 30 m;
- gate-district holdout RMSE no more than 2 m;
- gate-district worst residual no more than 4 m, tightened to half the narrowest measured alignment feature when applicable.

Do not treat modern building or temple-compound centroids as surveyed points. Do not reuse fit controls as holdouts. If a global affine fails, preserve the same holdouts while testing a documented local or higher-order model.

### Vertical gate

Find at least one dated late-Qing walking/ground surface tied to an exact section location, named benchmark and explicit vertical datum. Record the archaeological interpretation and uncertainty. A cultural layer, relative depth below modern grade, building pivot, DEM minimum, or generic “sea-level elevation” without a datum realization does not pass.

The official conditional relation currently registered is `H_1985 = H_Guangzhou - 4.256 m`. Apply it only to heights explicitly identified as using the Guangzhou height system. Comparison with the modern DTM requires a documented 1985-datum-to-EGM2008 bridge.

## Start here

1. Read `context/QA/Control_Survey_Handoff.md` for the exact evidence request.
2. Read `context/Review/M01_Terrain_Review.md` for the current implementation state.
3. Inspect `context/Data/Map_Control_Candidates.csv`, `context/QA/Map_GCP_Residuals.csv`, and `context/Data/Map_Transform_Provisional.json` to understand why the current fit fails.
4. Inspect `context/Data/Elevation_Constraints.csv` and `context/Docs/Vertical_Datum_Register.md` before using any height.
5. Verify the export with `python3 tools/validate_packet.py`.
6. Put researched records into the templates under `incoming/`. Add copies only when redistribution is permitted; otherwise record stable publisher/archive links, access dates and checksums.

## Required return package

Return a directory with:

- completed `Measured_Map_Controls.csv` and/or `Historical_Ground_Elevations.csv`;
- copies or stable links for every cited survey/source;
- a new versioned transform and full control/holdout residual table if attempting Day 03;
- a datum-conversion derivation with named source/target datums if attempting Day 05;
- a short decision report classifying each gate `Verified`, `Provisional`, or `Blocked`;
- exact commands, software versions and checksums;
- no modification or deletion of the supplied failed transform.

## Existing result

The provisional transform uses five OSM-derived fit points and three OSM-derived holdouts. Its holdout RMSE is 93.24 m and its worst residual is 132.155 m; the northeast quadrant has no holdout. The one registered Qing stratigraphic range is 5.92–6.22 m, but its datum realization and relation to an 1880 ground surface are unknown. Both gates therefore remain blocked.

The included modern GEDTM30 rasters are context only. Their vertical datum is EGM2008 / EPSG:3855 and their approximate source resolution is 30 m. They do not prove late-Qing elevations.

## Packet layout

- `context/`: repository evidence, plans, scripts and permitted source rasters.
- `incoming/`: schemas for the receiving agent's findings.
- `tools/validate_packet.py`: validates the export manifest and key guardrails.
- `MANIFEST.sha256`: checksum inventory generated after assembly.
- `FILE_INDEX.csv`: file path, size and SHA-256 inventory.

