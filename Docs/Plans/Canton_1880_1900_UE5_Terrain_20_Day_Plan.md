# HISTORICAL CANTON | UE5 URBAN TERRAIN
## 20-working-day implementation plan | 1880–1900 | Inside the historic city walls

**Outcome.** A source-traceable heightmap and World Partition Landscape for the historic walled-city envelope, plus one integrated, traversable 200 × 200 m gate district used to prove roads, materials and vegetation. This is **not** a promise to complete all streets, buildings or the whole playable city in 20 working days.

**Day numbering.** Day 01–20 means working days, not calendar dates. Assumes one primary UE/GIS technical artist or engineer, access to a UE5 workstation and project repo, and timely availability of map/elevation sources. Source acquisition and reliable georeferencing are critical-path risks.

**Historical integrity rule.** Do not silently convert a plausible reconstruction into a measured 1890 elevation. For every input, keep the original source, observation date, horizontal/vertical accuracy, vertical datum, measurement reliability, temporal relevance to 1880–1900 and modern-change risk. Assign historical reconstruction confidence separately: A (period measurement or independently corroborated period constraint), B (historically constrained), C (interpolated), or D (temporary placeholder). A precise modern DEM is not automatically A-grade historical evidence. If suitable data is unavailable, export a *provisional* playable terrain with the unresolved area marked; do not label M01 historically verified.

**Initial UE envelope (provisional).** 2017 × 2017 16-bit grayscale heightmap, 63 quads/section, 2 × 2 sections/component, 16 × 16 components, XY scale 200 cm (~4032 × 4032 m, 2 m vertex spacing); Z scale 50 **only if** exported vertical encoding matches ±128 m around the agreed Landscape origin. Check the actual georeferenced wall polygon fits before committing. Use centimeters in UE; store GIS in projected metres. Do not invent elevation points to fit gate meshes.

**Suggested handoff cadence.** Day 05 evidence audit, Day 10 M01 gate, Day 14 road/geometry gate, Day 17 material gate, Day 20 integrated district demonstration. Every review should include screenshots, reproducible source/output identifiers and explicit historical uncertainty.

**Acceptance contract.** Day 01 must record numeric georeference tolerances, target hardware and performance thresholds before production work begins. A gate passes only when its declared automated checks and manual/visual checks both pass. Screenshots are evidence, not a substitute for metadata, map-load, cook, collision, navigation or performance checks.

**Data and source-control rule.** Do not commit a source merely because it was downloaded. Record licence and redistribution status, SHA-256, retrieval location and tool/version metadata. Store redistributable large rasters using the repository-approved LFS/artifact policy; store restricted originals outside Git and commit only their manifest and reproducible derivation instructions. Derived files must be either versioned with checksums or reproducible from a versioned script and manifest.

## Milestone schedule

| Milestone | Days | Exit result |
|---|---:|---|
| M01 | Days 01–10 | Georeference, historic elevation constraints, heightmap, UE5 import and validation |
| M02 | Days 11–14 | Road corridors, block grading, gate and drainage integration |
| M03 | Days 15–17 | Ground material instances and modular stone-paving prototype |
| M04 | Days 18–19 | Historically constrained vegetation, surface debris and localized wetness |
| M05 | Day 20 | Gate-district integration, performance sampling, source/geometry QA and handoff |

## Daily execution plan

Each day is also available as an independent execution document with a paired resource/evidence sheet. The detailed sections below remain the canonical cross-day summary.

| Day | Milestone | Independent plan | Resource sheet |
|---:|---|---|---|
| 01 | M01 | [Project datum, scope and source register](Canton-Terrain-Implementation/Day-01-project-datum-scope-and-source-register.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-01-Resources.md) |
| 02 | M01 | [Historical wall and landmark source acquisition](Canton-Terrain-Implementation/Day-02-historical-wall-and-landmark-source-acquisition.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-02-Resources.md) |
| 03 | M01 | [Georeference historic city map](Canton-Terrain-Implementation/Day-03-georeference-historic-city-map.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-03-Resources.md) |
| 04 | M01 | [Digitize the walled-city plan](Canton-Terrain-Implementation/Day-04-digitize-the-walled-city-plan.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-04-Resources.md) |
| 05 | M01 | [Acquire elevation and archaeological constraints](Canton-Terrain-Implementation/Day-05-acquire-elevation-and-archaeological-constraints.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-05-Resources.md) |
| 06 | M01 | [Build preliminary historical elevation model](Canton-Terrain-Implementation/Day-06-build-preliminary-historical-elevation-model.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-06-Resources.md) |
| 07 | M01 | [Interpolate historical terrain and confidence surface](Canton-Terrain-Implementation/Day-07-interpolate-historical-terrain-and-confidence-surface.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-07-Resources.md) |
| 08 | M01 | [Resample, encode and audit heightmap](Canton-Terrain-Implementation/Day-08-resample-encode-and-audit-heightmap.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-08-Resources.md) |
| 09 | M01 | [Import and inspect UE5 Landscape](Canton-Terrain-Implementation/Day-09-import-and-inspect-ue5-landscape.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-09-Resources.md) |
| 10 | M01 | [M01 source and terrain acceptance review](Canton-Terrain-Implementation/Day-10-m01-source-and-terrain-acceptance-review.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-10-Resources.md) |
| 11 | M02 | [Urban road and parcel-layout plan](Canton-Terrain-Implementation/Day-11-urban-road-and-parcel-layout-plan.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-11-Resources.md) |
| 12 | M02 | [Street gradients and built-area grading](Canton-Terrain-Implementation/Day-12-street-gradients-and-built-area-grading.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-12-Resources.md) |
| 13 | M02 | [Drainage and low-area blockout](Canton-Terrain-Implementation/Day-13-drainage-and-low-area-blockout.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-13-Resources.md) |
| 14 | M02 | [M02 geometry review and blocker cleanup](Canton-Terrain-Implementation/Day-14-m02-geometry-review-and-blocker-cleanup.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-14-Resources.md) |
| 15 | M03 | [Author master ground materials](Canton-Terrain-Implementation/Day-15-author-master-ground-materials.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-15-Resources.md) |
| 16 | M03 | [Build modular historical stone paving](Canton-Terrain-Implementation/Day-16-build-modular-historical-stone-paving.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-16-Resources.md) |
| 17 | M03 | [Terrain/road blending and material QA](Canton-Terrain-Implementation/Day-17-terrain-road-blending-and-material-qa.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-17-Resources.md) |
| 18 | M04 | [Add urban vegetation with controls](Canton-Terrain-Implementation/Day-18-add-urban-vegetation-with-controls.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-18-Resources.md) |
| 19 | M04 | [Add weathering, wetness and small ground details](Canton-Terrain-Implementation/Day-19-add-weathering-wetness-and-small-ground-details.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-19-Resources.md) |
| 20 | M05 | [M05 gate-district integration and production handoff](Canton-Terrain-Implementation/Day-20-m05-gate-district-integration-and-production-handoff.md) | [Resources](Canton-Terrain-Implementation/Resources/Day-20-Resources.md) |

### Day 01 — Project datum, scope and source register (M01)
**Work**
- Lock study period (1880–1900), walled-city polygon, local XY orientation, world origin convention, centimetre-to-metre conversion, target Unreal version and asset naming.
- Resolve the project's GUID-based Unreal association to an installed engine version. Audit required plugins and prove a minimal PCG smoke asset can load and cook; if PCG is unavailable, record ordinary foliage instances as the approved fallback.
- Create a source register: archive ID, observation date, locality, scan resolution, map scale if known, copyright/licence, redistribution status, retrieval location, SHA-256, location accuracy, vertical datum, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D.
- Define storage policy for original scans, large GeoTIFFs, derived rasters and UE assets, including Git LFS/external-artifact rules and reproducible tool versions.
- Declare georeference acceptance thresholds: independent check-point count and distribution, RMSE, maximum residual and a stricter gate-district tolerance tied to the narrowest geometry being aligned. Declare target hardware, build configuration, resolution/scalability, fixed performance route and frame-time/memory/streaming thresholds.
- Collect existing city-gate mesh dimensions and pivot conventions as *asset data*, not as surveyed historical ground elevations.
**Deliverables:** `Docs/Terrain_Baseline.md`; `Data/Source_Register.csv`; `Data/Coordinate_Contract.json`; `Data/Artifact_Manifest.csv`; `Docs/Terrain_Acceptance_Budget.md`; `QA/PCG_Preflight.md`.
**Done when:** Scope, coordinate and acceptance contracts are approved; the exact UE/plugin path is proven; storage/licence rules are recorded; all candidate sources carry provenance and separate evidence-quality fields.

### Day 02 — Historical wall and landmark source acquisition (M01)
**Work**
- Locate high-resolution maps dated within 1880–1900; separately register nearby-date maps as secondary references.
- Record city-wall perimeter, gate locations, Zhenhai Tower and surviving street junctions with source pointers.
- Mark unverified gate names/positions as provisional and record disagreements between maps.
**Deliverables:** `Sources/HistoricMaps/`; `Data/Historic_Map_Selection.md`; `Data/Landmark_Candidates.csv`.
**Done when:** At least one traceable historical map is selected; gaps and date mismatches are explicitly logged.

### Day 03 — Georeference historic city map (M01)
**Work**
- Choose a projected metric CRS suitable for Guangzhou; archive its EPSG code and coordinate transform to a local UE origin.
- Add distributed ground control points from demonstrably persistent landmarks; reserve spatially distributed points as independent checks and avoid forcing the archive map onto modern realigned streets.
- Test affine/polynomial transformations as appropriate; record per-point residuals, holdout-check residuals, distortion zones and rejected points with reasons. Do not select a higher-order transform merely because it reduces fitted-point error.
**Deliverables:** `GIS/Canton_Historic_Georef.tif`; `QA/Map_GCP_Residuals.csv`; `Docs/Map_Distortion_Notes.md`.
**Done when:** Georeferenced raster opens at the documented coordinate origin and independent checks meet the Day 01 RMSE/maximum-error/spatial-coverage budget. Otherwise classify affected areas Provisional or Blocked rather than passing the transform.

### Day 04 — Digitize the walled-city plan (M01)
**Work**
- Trace wall centerline and gate points with source/date/confidence attributes.
- Digitize principal roads, historical water routes, important landform extents and major fixed landmarks into separate GIS layers.
- Calculate required bounding box and buffer; confirm that the proposed 4032 m square Landscape covers the actual wall polygon and needed working margin.
**Deliverables:** `GIS/Wall.geojson`; `GIS/Gates.geojson`; `GIS/Roads_Main.geojson`; `GIS/Waterways.geojson`; `QA/Extent_Check.md`.
**Done when:** Wall/gate layers align to the map and fit within the tested heightmap envelope; re-scope size if not.

### Day 05 — Acquire elevation and archaeological constraints (M01)
**Work**
- Locate the best available bare-earth DEM or surveyed contours and inspect age, spatial resolution, vertical datum, accuracy and evidence of modern earthworks.
- Collect source-backed archaeological site levels, historic contours, hill/terrace constraints, historic water references and gate-approach slope observations.
- Create a point/line elevation schema with measured value, range, vertical datum, reference, observation date, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D; do not fill missing values.
- Select the 200 × 200 m gate district now, based on source coverage, gate-asset readiness and achievable georeference accuracy. Record a ranked fallback district in case the preferred district fails the evidence gate.
**Deliverables:** `Sources/Elevation/`; `Data/Elevation_Constraints.csv`; `Docs/Vertical_Datum_Register.md`.
**Done when:** Every height value has units, origin and source; modern accuracy is not conflated with historical confidence; DEM coverage/uncertainty is documented; primary and fallback prototype districts are selected.

### Day 06 — Build preliminary historical elevation model (M01)
**Work**
- Reconcile vertical datums before combining data; select a documented local terrain-zero reference.
- Use DEM for broad morphology only; tag modern cut/fill, roads, basements and reclamation as potential contamination.
- Construct north-hill / south-lowland conceptual grade and source-informed breaklines; do not invent specific nineteenth-century spot heights.
**Deliverables:** `GIS/Base_Elevation_float32.tif`; `GIS/Historic_Breaklines.geojson`; `QA/Modern_Change_Mask.tif`.
**Done when:** Prototype elevation model is spatially continuous, and every historical correction is traceable to evidence or flagged provisional.

### Day 07 — Interpolate historical terrain and confidence surface (M01)
**Work**
- Interpolate between control points while constraining hills, road-grade logic and documented water-flow directions.
- Separate inferred reconstruction from measured reference with a confidence/uncertainty raster and a feature-level log.
- Inspect for steps, sink artifacts, unrealistically steep urban blocks and residual modern building shapes.
**Deliverables:** `GIS/Historic_Elevation_float32.tif`; `GIS/Terrain_Confidence.tif`; `QA/Elevation_Checkpoints.csv`.
**Done when:** Terrain has no unexplained spikes or missing values; measured checkpoints fall within their source-specific uncertainty.

### Day 08 — Resample, encode and audit heightmap (M01)
**Work**
- Set world envelope, origin, orientation and grid-spacing metadata; resample elevation to a 2017 × 2017 grid for the proposed 4032 m coverage.
- Encode to single-channel 16-bit PNG with a fixed height-to-code mapping and matching UE Z scale; preserve float32 source raster.
- Test decoder round-trip, axis direction, extreme elevations, clipping and image bit depth using a small reproducible script.
- Add a coordinate round-trip test from projected GIS metres to local UE centimetres and back, using independent landmarks at the corners and within the gate district.
**Deliverables:** `Export/Canton_1890_Height_2017_R16.png`; `Export/Heightmap_Metadata.json`; `Tools/Validate_Heightmap.py`; `QA/Heightmap_Encode_Report.md`.
**Done when:** PNG is exactly 2017 × 2017, uint16, single-channel and reversible within expected quantization error; no elevations clip; the GIS↔UE coordinate round trip meets the Day 01 tolerance.

### Day 09 — Import and inspect UE5 Landscape (M01)
**Work**
- Create a dedicated World Partition map; enable Landscape Edit Layers during creation and import the heightmap into a named `Base_Imported` layer at 200/200/50 XY/Z scale only if the metadata confirms that encoding. Lock the base layer immediately.
- Use 63 quads per section, 2 × 2 sections/component and 16 × 16 components; center/position the Landscape using the documented local-origin transform.
- Record World Partition grid/region sizes, `Flip Y Axis`, Landscape actor transform and all import settings. Apply a neutral diagnostic material; place wall and gate markers and inspect orientation, scaling, collision and northern slope direction.
**Deliverables:** `Content/Canton/Maps/L_Canton_WalledCity.umap`; `Content/Canton/Terrain/MI_Diagnostic`; `QA/UE_Import_Screenshots/`.
**Done when:** UE Landscape imports without distortion; `Base_Imported` exists and is locked; import/WP settings are captured; an automated map-load/configuration check passes; landmark overlay faces the intended direction and sits in the correct XY frame.

### Day 10 — M01 source and terrain acceptance review (M01)
**Work**
- Overlay traced walls and gates against imported terrain and independently check major control-point elevations.
- Mark historical-evidence gaps directly on the GIS confidence map and UE map; classify the milestone Verified / Provisional / Blocked by area.
- Freeze versioned base raster, source register and coordinate contract; produce a short review packet and decision log.
- Run `Scripts/test_canton_source_manifest.py`, `Scripts/test_canton_heightmap_contract.py` and the UE map-load/configuration automation test; record exact commands and outputs.
**Deliverables:** `Review/M01_Terrain_Review.md`; `Review/M01_Screenshots/`; `Data/M01_Uncertainty_Log.csv`.
**Done when:** M01 passes only if import/scales/coords are valid and uncertainty is explicit; missing history means Provisional, not historically verified.

### Day 11 — Urban road and parcel-layout plan (M02)
**Work**
- Import road centre lines and city-wall geometry as non-rendered reference overlays.
- Classify roads: principal stone-slab streets, smaller mixed-earth/pebble lanes, gate approaches and special courtyard areas; link classifications to photo/source evidence.
- Lay out a 200 × 200 m representative district with one gate interface and adjoining street/plots.
**Deliverables:** `GIS/Road_Classifications.geojson`; `Data/Road_Surface_Register.csv`; `Maps/District_Test_Bounds.json`.
**Done when:** All prototype roads have source/date and surface-class labels; uncertain assignments are tagged.

### Day 12 — Street gradients and built-area grading (M02)
**Work**
- Confirm the locked `Base_Imported` layer created on Day 09; create independent Landscape Edit Layers for urban grading, road corridors and drainage.
- Draft road elevations/intersections and representative block platforms from constraints; avoid flattening all of the city.
- At the gate interface, preserve original architectural dimensions and separately record any local ground correction.
- Treat the 2 m Landscape vertex grid as broad grading only. Build narrow lanes, thresholds, kerbs, gutters and small drains with splines, meshes or patches at the resolution required by their evidence and collision needs.
**Deliverables:** `UE/EditLayers_Base_Urban_Roads`; `QA/Street_Grade_Profiles.csv`; `Docs/Gate_Ground_Datum.md`.
**Done when:** Representative roads and plots meet without visible terrain discontinuities; grade decisions are traceable or provisional.

### Day 13 — Drainage and low-area blockout (M02)
**Work**
- Place historically mapped drains, canals or low areas as reference splines; do not fabricate open channels where maps/photos offer no support.
- Model test ditch and puddle depressions only where the ground context calls for them.
- Check that road grading does not introduce dead-end depressions or water running uphill.
**Deliverables:** `GIS/Drainage_Locations.geojson`; `UE/Drainage_Blockout`; `QA/Drainage_Flow_Notes.md`.
**Done when:** Prototype drainage has plausible downhill flow; documented and invented demo features are clearly separated.

### Day 14 — M02 geometry review and blocker cleanup (M02)
**Work**
- Walk all prototype roads from gate to district interior; fix mesh/landscape seams and foundation gaps.
- Check collision and navigation on road grades; compare the layout with the georeferenced map overlay.
- Record corrections that alter evidence-backed geometry separately from implementation fixes.
- Run an automated district traversal/collision/navigation smoke and a commandlet map check. Fix or explicitly waive every warning relevant to the prototype map.
**Deliverables:** `Review/M02_Roads_Review.md`; `QA/Road_Nav_Collision.md`; `Review/M02_Screenshots/`.
**Done when:** 200 × 200 m prototype supports continuous traversal and its principal road/gate alignment has no unresolved geometry blockers.

### Day 15 — Author master ground materials (M03)
**Work**
- Build a Landscape material with restrained layer count: natural soil, compacted earth, mixed pebble/earth, grass/weed soil, damp earth and optional exposed stone.
- Prepare physically based material instances with adjustable tint/roughness; treat monochrome photos as shape/placement evidence, not reliable colour sampling.
- Document scale, texel density, blend masks and texture licence/provenance.
**Deliverables:** `Terrain/M_Canton_Landscape`; `Terrain/MI_*`; `Data/Material_Evidence_Register.csv`.
**Done when:** The Landscape material compiles and the six planned surface families can be painted without obvious repetition at walking height.

### Day 16 — Build modular historical stone paving (M03)
**Work**
- Create reusable stone-slab sets for primary roads, plus selected kerbs, steps, foundation strips and drainage edges.
- Use source-backed slab arrangement where documented; define pivots, snap grid, collision and Nanite usage by mesh.
- Test road-to-earth edge transitions, individual stone heights and slope changes at street intersections.
**Deliverables:** `Roads/SM_StoneSlab_*`; `Roads/BP_RoadSegment_Test`; `QA/StoneRoad_Seams.md`.
**Done when:** The test street is traversable with no floating stones, Z-fighting, visible grid seams or oversized European-style cobble pattern.

### Day 17 — Terrain/road blending and material QA (M03)
**Work**
- Blend Landscape ground with modular paving using appropriate skirts, decals or masked transition meshes; avoid painting a fake sharp stone edge on soil.
- Compare dry and wet material variants under consistent lighting; audit roughness/normal intensity at eye level.
- Capture four representative scenes: main stone street, mixed lane, gate approach and courtyard edge.
**Deliverables:** `Roads/MI_Transition_*`; `Review/M03_Material_Atlas.png`; `Review/M03_Material_QA.md`.
**Done when:** Four scenes show distinct readable surfaces and convincing transitions at human scale, without unverified claims of exact historic soil colour.

### Day 18 — Add urban vegetation with controls (M04)
**Work**
- Create grass tufts, weeds, wall-edge moss and sparse courtyard plants using foliage/PCG masks tied to surface class and evidence.
- Use PCG only if the Day 01 load/cook smoke passed; otherwise use the documented foliage-instance fallback without changing the placement contract.
- Keep dense managed garden vegetation out of ordinary street areas unless specifically documented.
- Set exclusion masks around paved roads, gate access, drain covers and building foundations.
**Deliverables:** `Foliage/PCG_Canton_UrbanGrowth` or `Foliage/Foliage_Canton_UrbanGrowth` according to the Day 01 decision; `Data/Foliage_SpawnRules.json`; `QA/Vegetation_Exclusions.md`.
**Done when:** Vegetation is sparse and place-specific; paths and gate openings remain unobstructed.

### Day 19 — Add weathering, wetness and small ground details (M04)
**Work**
- Distribute local leaf litter, grit, small stone debris, puddles and dampness through PCG or ordinary decals/instances according to the Day 01 decision, rather than noisy full-map height edits.
- Test dry and after-rain scenarios; validate the wet ground placement against drainage and material boundaries.
- Capture representative before/after screenshots and a draw-call/instance-count snapshot.
**Deliverables:** `GroundDetails/PCG_Canton_Debris` or `GroundDetails/Canton_Debris_Instances` according to the Day 01 decision; `Materials/MI_Wetness_*`; `Review/M04_Weathering_QA.md`.
**Done when:** Localized details enhance readability without clutter or implausible uniform mud/wetness; no material/foliage collision issues.

### Day 20 — M05 gate-district integration and production handoff (M05)
**Work**
- Place one existing historical gate as an integration test; preserve architectural scale, record any nonhistorical asset fixes and align gate threshold to its modeled approach.
- Validate scale, collision, Lumen/Nanite compatibility as used, World Partition streaming and visual seams. Run the fixed Day 01 route on the declared target hardware/build/resolution/scalability after a defined warm-up; capture frame-time percentiles, memory, draw calls, instance counts and streaming failures against the declared thresholds.
- Run the focused Python contracts, UE automation/map check and a Development cook containing `L_Canton_WalledCity`; record exact commands, engine identity and output logs.
- Publish terrain data, source confidence, screenshots, known deviations, open blocker list and next-stage citywide replication rules.
**Deliverables:** `Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.
**Done when:** A test gate connects cleanly to a traversable 200 × 200 m district; source/heightmap/map-load/collision/navigation/cook checks pass; the fixed performance route meets its predeclared thresholds or is explicitly blocked; all full-city areas not yet built or validated remain explicitly out of scope.

## Critical-path and stop/go rules

- **Map unavailable or unusably distorted (Days 02–04):** preserve the provenance audit; create only a flagged provisional footprint and do not claim historically correct gate coordinates. Escalate source acquisition rather than improving an arbitrary visual map.
- **DEM / vertical datum unavailable (Days 05–07):** make a clearly named `PROVISIONAL` heightfield; defer source-verified M01 approval. A flat or stylized prototype can prove UE tools, not historic elevation.
- **Coverage exceeds 4032 m:** recalculate grid size / scale before resampling. Do not clip the city wall just to satisfy the provisional import setting.
- **Heightmap orientation or encoding fails (Days 08–09):** block detailed road construction until XY origin, axis convention and vertical scale are fixed.
- **Gate asset disagrees with verified terrain:** document the mismatch and investigate the gate model, survey datum or location independently; do not overwrite elevation evidence to conceal it.
- **Performance slows (Days 16–20):** first profile instance counts, material shader cost, collision and WP streaming on target hardware. Do not assume Nanite alone solves grass/foliage or shader cost.
- **Plugin or cook preflight fails (Day 01):** use the documented foliage fallback and do not defer plugin discovery until Day 18.
- **Georeference exceeds its accuracy budget (Days 03–05):** change transform/source or narrow/reselect the prototype district. Do not average away a failed independent check or call the affected geometry verified.
- **Restricted or oversized source data cannot be committed:** keep the original outside Git, commit its manifest/checksum/licence/retrieval instructions and verify that an authorised workstation can reproduce the derived output.

## Minimum review/evidence packet

Include: original map identifiers and observation dates; licence/redistribution status and SHA-256; spatial reference and local origin; fitted-control and independent-check residual tables with declared thresholds; raster source and heightmap encoding metadata; separate measurement-reliability, temporal-relevance, modern-change-risk and historical-confidence fields; confidence raster; UE import/edit-layer/WP settings; wall/gate overlay screenshots; one terrain profile from north to south; district walk-through images; source-vs-reconstruction change log; automated test/map-check/cook commands and logs; fixed-route performance configuration and results; list of areas that remain provisional.

## After Day 20 (outside this plan)

Expand the validated district systems across the remainder of the walled city; add citywide street detail, blocks/buildings, gate-to-gate traversal and progressive historical-source validation. Do not treat the first prototype as proof that every city district is completed or accurate.
