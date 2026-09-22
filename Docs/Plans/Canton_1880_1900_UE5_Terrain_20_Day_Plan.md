# HISTORICAL CANTON | UE5 URBAN TERRAIN
## 20-working-day implementation plan | 1880–1900 | Inside the historic city walls

**Outcome.** A source-traceable heightmap and World Partition Landscape for the historic walled-city envelope, plus one integrated, traversable 200 × 200 m gate district used to prove roads, materials and vegetation. This is **not** a promise to complete all streets, buildings or the whole playable city in 20 working days.

**Day numbering.** Day 01–20 means working days, not calendar dates. Assumes one primary UE/GIS technical artist or engineer, access to a UE5 workstation and project repo, and timely availability of map/elevation sources. Source acquisition and reliable georeferencing are critical-path risks.

**Historical integrity rule.** Do not silently convert a plausible reconstruction into a measured 1890 elevation. Keep original source, date, horizontal accuracy, vertical datum and confidence A (measured), B (historically constrained), C (interpolated) or D (temporary placeholder). If suitable data is unavailable, export a *provisional* playable terrain with the unresolved area marked; do not label M01 historically verified.

**Initial UE envelope (provisional).** 2017 × 2017 16-bit grayscale heightmap, 63 quads/section, 2 × 2 sections/component, 16 × 16 components, XY scale 200 cm (~4032 × 4032 m, 2 m vertex spacing); Z scale 50 **only if** exported vertical encoding matches ±128 m around the agreed Landscape origin. Check the actual georeferenced wall polygon fits before committing. Use centimeters in UE; store GIS in projected metres. Do not invent elevation points to fit gate meshes.

**Suggested handoff cadence.** Day 05 evidence audit, Day 10 M01 gate, Day 14 road/geometry gate, Day 17 material gate, Day 20 integrated district demonstration. Every review should include screenshots, reproducible source/output identifiers and explicit historical uncertainty.

## Milestone schedule

| Milestone | Days | Exit result |
|---|---:|---|
| M01 | Days 01–10 | Georeference, historic elevation constraints, heightmap, UE5 import and validation |
| M02 | Days 11–14 | Road corridors, block grading, gate and drainage integration |
| M03 | Days 15–17 | Ground material instances and modular stone-paving prototype |
| M04 | Days 18–19 | Historically constrained vegetation, surface debris and localized wetness |
| M05 | Day 20 | Gate-district integration, performance sampling, source/geometry QA and handoff |

## Daily execution plan

### Day 01 — Project datum, scope and source register (M01)
**Work**
- Lock study period (1880–1900), walled-city polygon, local XY orientation, world origin convention, centimetre-to-metre conversion, target Unreal version and asset naming.
- Create a source register: archive ID, date, locality, scan resolution, map scale if known, copyright/licence, location accuracy, vertical datum (if applicable), and confidence A–D.
- Collect existing city-gate mesh dimensions and pivot conventions as *asset data*, not as surveyed historical ground elevations.
**Deliverables:** `Docs/Terrain_Baseline.md`; `Data/Source_Register.csv`; `Data/Coordinate_Contract.json`.
**Done when:** Scope and coordinate contract approved; all candidate sources carry provenance and dates.

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
- Add distributed ground control points from demonstrably persistent landmarks; avoid forcing the archive map onto modern realigned streets.
- Test affine/polynomial transformations as appropriate; record per-point residuals, distortion zones and unused checks.
**Deliverables:** `GIS/Canton_Historic_Georef.tif`; `QA/Map_GCP_Residuals.csv`; `Docs/Map_Distortion_Notes.md`.
**Done when:** Georeferenced raster opens at the documented coordinate origin; residuals and unreliable zones are visible.

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
- Create a point/line elevation schema with measured value, range, vertical datum, reference, date and confidence A–D; do not fill missing values.
**Deliverables:** `Sources/Elevation/`; `Data/Elevation_Constraints.csv`; `Docs/Vertical_Datum_Register.md`.
**Done when:** Every height value has units, origin and source; the coverage and uncertainty of the DEM are documented.

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
**Deliverables:** `Export/Canton_1890_Height_2017_R16.png`; `Export/Heightmap_Metadata.json`; `Tools/Validate_Heightmap.py`; `QA/Heightmap_Encode_Report.md`.
**Done when:** PNG is exactly 2017 × 2017, uint16, single-channel and is reversible within expected quantization error; no clipped elevations.

### Day 09 — Import and inspect UE5 Landscape (M01)
**Work**
- Create a dedicated World Partition map; import the heightmap at 200/200/50 XY/Z scale only if the metadata confirms that encoding.
- Use 63 quads per section, 2 × 2 sections/component and 16 × 16 components; center/position the Landscape using the documented local-origin transform.
- Apply a neutral diagnostic material; place wall and gate markers and inspect orientation, scaling, collision and northern slope direction.
**Deliverables:** `Content/Canton/Maps/L_Canton_WalledCity.umap`; `Content/Canton/Terrain/MI_Diagnostic`; `QA/UE_Import_Screenshots/`.
**Done when:** UE Landscape imports without distortion; landmark overlay faces the intended direction and sits in the correct XY frame.

### Day 10 — M01 source and terrain acceptance review (M01)
**Work**
- Overlay traced walls and gates against imported terrain and independently check major control-point elevations.
- Mark historical-evidence gaps directly on the GIS confidence map and UE map; classify the milestone Verified / Provisional / Blocked by area.
- Freeze versioned base raster, source register and coordinate contract; produce a short review packet and decision log.
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
- Create independent Landscape Edit Layers for the imported base, urban grading, road corridors and drainage; lock the base.
- Draft road elevations/intersections and representative block platforms from constraints; avoid flattening all of the city.
- At the gate interface, preserve original architectural dimensions and separately record any local ground correction.
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
- Keep dense managed garden vegetation out of ordinary street areas unless specifically documented.
- Set exclusion masks around paved roads, gate access, drain covers and building foundations.
**Deliverables:** `Foliage/PCG_Canton_UrbanGrowth`; `Data/Foliage_SpawnRules.json`; `QA/Vegetation_Exclusions.md`.
**Done when:** Vegetation is sparse and place-specific; paths and gate openings remain unobstructed.

### Day 19 — Add weathering, wetness and small ground details (M04)
**Work**
- Distribute local leaf litter, grit, small stone debris, puddles and dampness through decals/instances rather than noisy full-map height edits.
- Test dry and after-rain scenarios; validate the wet ground placement against drainage and material boundaries.
- Capture representative before/after screenshots and a draw-call/instance-count snapshot.
**Deliverables:** `GroundDetails/PCG_Canton_Debris`; `Materials/MI_Wetness_*`; `Review/M04_Weathering_QA.md`.
**Done when:** Localized details enhance readability without clutter or implausible uniform mud/wetness; no material/foliage collision issues.

### Day 20 — M05 gate-district integration and production handoff (M05)
**Work**
- Place one existing historical gate as an integration test; preserve architectural scale, record any nonhistorical asset fixes and align gate threshold to its modeled approach.
- Validate scale, collision, Lumen/Nanite compatibility as used, basic World Partition streaming, visual seams and sample-frame performance on the actual target hardware.
- Publish terrain data, source confidence, screenshots, known deviations, open blocker list and next-stage citywide replication rules.
**Deliverables:** `Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.
**Done when:** A test gate connects cleanly to a traversable 200 × 200 m district; all full-city areas not yet built or validated remain explicitly out of scope.

## Critical-path and stop/go rules

- **Map unavailable or unusably distorted (Days 02–04):** preserve the provenance audit; create only a flagged provisional footprint and do not claim historically correct gate coordinates. Escalate source acquisition rather than improving an arbitrary visual map.
- **DEM / vertical datum unavailable (Days 05–07):** make a clearly named `PROVISIONAL` heightfield; defer source-verified M01 approval. A flat or stylized prototype can prove UE tools, not historic elevation.
- **Coverage exceeds 4032 m:** recalculate grid size / scale before resampling. Do not clip the city wall just to satisfy the provisional import setting.
- **Heightmap orientation or encoding fails (Days 08–09):** block detailed road construction until XY origin, axis convention and vertical scale are fixed.
- **Gate asset disagrees with verified terrain:** document the mismatch and investigate the gate model, survey datum or location independently; do not overwrite elevation evidence to conceal it.
- **Performance slows (Days 16–20):** first profile instance counts, material shader cost, collision and WP streaming on target hardware. Do not assume Nanite alone solves grass/foliage or shader cost.

## Minimum review/evidence packet

Include: original map identifiers and date; spatial reference and local origin; control-point residual table; raster source and heightmap encoding metadata; confidence raster; UE import settings; wall/gate overlay screenshots; one terrain profile from north to south; district walk-through images; source-vs-reconstruction change log; list of areas that remain provisional.

## After Day 20 (outside this plan)

Expand the validated district systems across the remainder of the walled city; add citywide street detail, blocks/buildings, gate-to-gate traversal and progressive historical-source validation. Do not treat the first prototype as proof that every city district is completed or accurate.
