# HISTORICAL CANTON | UE5 URBAN TERRAIN
## 20-working-day implementation and acceptance plan | 1880–1900 | Inside the historic city walls

**Revised 2026-09-29 against repository `1b2cc5d0` and the 2026-09-28 M05 audit disposition.**

**Outcome.** A source-traceable heightmap and World Partition Landscape for the historic walled-city envelope, plus one integrated, traversable 200 × 200 m gate district used to prove roads, materials and vegetation. This is **not** a promise to complete all streets, buildings or the whole playable city in 20 working days.

**Day numbering and remaining effort.** Keep Day 01–20 as the original work-package IDs so existing reviews and evidence remain traceable. They are not twenty new days starting at this revision, and elapsed days do not imply acceptance. Reuse the implemented pipeline; execute only the remaining work below. One primary UE/GIS engineer or technical artist is assumed, with source/survey, archaeology, environment-art and owner support as dependencies. Historical completion cannot be promised within twenty days while external evidence is missing. Re-estimate remaining effort after the first continuation review; record source lead times separately from implementation effort.

**Current execution record.** [Days 01–10 audit](../../QA/Canton_Days_01_10_Execution.md), [Days 11–20 provisional ledger](../../QA/Canton_Days_11_20_Execution.md) and [M05 handoff](../../Review/M05_Final_Handoff.md) distinguish implemented technical work from blocked historical acceptance. The 20-day historical target remains open until H1, H2, historical Z and owner approval are resolved.

**Historical integrity rule.** Do not silently convert a plausible reconstruction into a measured 1890 elevation. For every input, keep the original source, observation date, horizontal/vertical accuracy, vertical datum, measurement reliability, temporal relevance to 1880–1900 and modern-change risk. Assign historical reconstruction confidence separately: A (period measurement or independently corroborated period constraint), B (historically constrained), C (interpolated), or D (temporary placeholder). A precise modern DEM is not automatically A-grade historical evidence. If suitable data is unavailable, export a *provisional* playable terrain with the unresolved area marked; do not label M01 historically verified.

**Implemented UE envelope (provisional; retain unless accepted source coverage requires migration).** 2017 × 2017 16-bit grayscale heightmap, 63 quads/section, 2 × 2 sections/component, 16 × 16 components, XY scale 200 cm (~4032 × 4032 m, 2 m vertex spacing); Z scale 50 **only if** exported vertical encoding matches ±128 m around the agreed Landscape origin. Check the actual georeferenced wall polygon fits before committing. Use centimeters in UE; store GIS in projected metres. Do not invent elevation points to fit gate meshes.

**Suggested handoff cadence.** Day 05 evidence audit, Day 10 M01 gate, Day 14 road/geometry gate, Day 17 material gate, Day 20 integrated district demonstration. Every review should include screenshots, reproducible source/output identifiers and explicit historical uncertainty.

**Acceptance contract.** Day 01 already has a versioned working budget; owner approval remains pending. Obtain approval and resolve the missing pawn/nav-clearance criteria before closing district acceptance. Do not backdate approval or change numeric targets to match observed results. A gate passes only when its declared automated checks and manual/visual checks both pass. Screenshots are evidence, not a substitute for metadata, map-load, cook, collision, navigation or performance checks.

**Data and source-control rule.** Do not commit a source merely because it was downloaded. Record licence and redistribution status, SHA-256, retrieval location and tool/version metadata. Store redistributable large rasters using the repository-approved LFS/artifact policy; store restricted originals outside Git and commit only their manifest and reproducible derivation instructions. Derived files must be either versioned with checksums or reproducible from a versioned script and manifest.

## Current implementation and acceptance baseline

Use the [M01 review](../../Review/M01_Terrain_Review.md), [Days 11–20 ledger](../../QA/Canton_Days_11_20_Execution.md), [M05 handoff](../../Review/M05_Final_Handoff.md) and [audit disposition](../../Review/M05_Independent_Audit_Disposition.md) together. These are retained execution evidence, not results of this documentation revision.

| Area | Implemented and reusable | Actual remaining requirement |
|---|---|---|
| M01 technical pipeline | Modern-context float32 terrain, all-D confidence, 2017² PNG and south-first R16; UE 5.5.4 CL 40574608 WP map; 256 Landscape/collision components; locked base; encoding, reload and automation evidence | Preserve provisional provenance; historical terrain and accepted source geometry are still absent. |
| Historical H1 / H2 / Z | Failed MAP-001 transform preserved; named candidate Wenmingmen, fallback Zhengdongmen; control and datum requests documented | H1 has only three unsurveyed holdouts, RMSE 93.24 m / worst 132.155 m; H2 network and dated datum-backed walking surface missing; owner approval pending. |
| M02 district | Separate 200 × 200 m engineering district; 100 principal slabs, 76 mixed-lane pieces, 48 covered-gutter pieces; three diagnostic flow catchments | Roads are D-confidence; correction layers exist but are unmodified; 16 unsupported parcel pads were removed. Courtyard view is an open-ground reference. |
| Day 14 navigation | Seven intended open/staging route screens pass; 100 principal floor samples; separate 198-joint probe, maximum step 1.33 cm | No pawn walk. Staging is ~6 m north of the closed gate face; near-threshold attempt detours 2.905×. Nav-floor gaps reach 15.63 cm; gate collision warnings at 288,064 / 595,104 triangles are unwaived. |
| M03 surfaces | Six paint targets and material swatches; modular cube paving; fixed-view atlas | Source-backed paving, physical materials and final transitions needed. Road underside gap is 7 cm and shoulder lip 11 cm. |
| M04 vegetation/weather | 38 collision-disabled growth cubes; 14 damp cards and dry/wet views | Final vegetation, debris, wetness and runtime counts missing. M04 is incomplete; no scope reduction approved. |
| M05 delivery/runtime | Provisional handoff and targeted two-map Mac Development cook, 994/994 packages | Cook temporarily excluded four always-cook directories; normal project packaging unvalidated. Development pawn/performance, Lumen and runtime WP streaming absent. Editor tick proxy is diagnostic only. |

### Implemented paths and safe continuation

- Terrain map: `/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL`.
- District map: `/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL`.
- Raster/encoding: `GIS/Provisional/Modern_Context_Elevation_float32.tif`, `Export/Provisional/Canton_Modern_Context_2017_R16.png`, `Export/Provisional/Canton_Modern_Context_SouthFirst.r16`, and `Export/Provisional/Heightmap_Metadata.json`.
- [Prototype contract](../../Data/Canton_Prototype_Contract.json): EPSG:32649, origin 729400 / 2557300 m; UE X east, Y north; scale 200/200/50; PNG row zero north, prepared R16 row zero south, no second native Y flip. Modern EPSG:3855 EGM2008 zero is tooling-only; historical zero remains null.
- Keep `Base_Imported` locked. `Urban_Grading`, `Road_Corridors` and `Drainage` are separability scaffolding until actual edits are justified. The 2 m Landscape grid cannot represent narrow kerbs or thresholds.
- Import/build scripts refuse existing maps. Use review/refinement tools on existing packages; do not delete and rebuild maps to replay completed days. A new accepted historical frame requires a separately versioned candidate, migration review and regression checks.
- PCG remains unproven. Current growth proxies are ordinary actors; replace them with final foliage/instances under the existing placement/exclusion contract. PCG is optional only after a separate load/cook smoke.
- Generic historical paths in the daily deliverables are **future accepted outputs**, not claims that those files exist. Keep current outputs under `Provisional` / `DistrictPrototype` until their gates pass; never rename modern-context data to `Canton_1890` as a shortcut.

## Acceptance gates retained and clarified

| Gate | Required result |
|---|---|
| H1 city frame | ≥5 fit controls; ≥8 independent holdouts, ≥2 per quadrant with perimeter/interior coverage; RMSE ≤15 m, worst ≤30 m. Use a survey-capable backbone; preserve MAP-001 as period layout evidence and its failed affine as failed evidence. |
| H2 local district | ≥4 independent local survey/conservation/archaeological checks; RMSE ≤2 m, worst ≤4 m, tightened to half the narrowest source-measured alignment feature where known. Final metric bounds follow H2 acceptance. |
| Historical Z | Dated late-Qing walking surface at an exact location with named benchmark and explicit reconciled vertical datum. Modern DTM is morphology context only; `QING-STRATUM-001` is not a usable terrain height. |
| Owner approval | Approve the [working budget](../Terrain_Acceptance_Budget.md), target hardware, gate intent and remaining capsule/nav-floor criteria with reviewer/date. No inferred or backdated approval. |
| Day 14 technical geometry | Real pawn traverses all intended open routes and reaches an explicit accessible threshold endpoint; nav-floor/clearance checks meet approved criteria; relevant collision warnings fixed or explicitly waived. Preserve the closed-door obstruction; no through-gate passage requirement is implied. |
| M03 / M04 art | Final paving, transitions, sparse foliage, debris and localized wetness pass visual and collision/exclusion review with source/licence evidence. Neutral swatches, cubes and dark cards alone do not pass. |
| Day 20 runtime | Declared Development/packaged target at 1920 × 1080 High, fixed 200 m district route, 60 s warm-up + 180 s sample; p95 ≤33.3 ms, p99 ≤50 ms, RSS ≤12 GiB, zero failed WP streaming cells. Capture frame/GPU data, draw calls, instance counts and cell diagnostics. |

Report `Technical_UE`, `Historical_XY_City (H1)`, `Historical_XY_GateLocal (H2)` and `Historical_Z` independently. M01 has a technical pass **for the provisional terrain scope**; the integrated district does not yet have a complete technical pass. Historical M01 is Verified only when H1/H2/Z pass and owner approval is recorded. Near-period geometry (including 1907) retains its actual date and needs in-period persistence evidence. Diagnostic UTM boxes do not locate historical Wenmingmen.

## Milestone schedule and continuation order

| Milestone / original days | Current state | Remaining exit result |
|---|---|---|
| M01 / 01–10 | Provisional terrain pipeline implemented; historical acceptance blocked | Acquire and validate H1/H2/Z; approve budget; version historical candidate only when supported. |
| M02 / 11–14 | District blockout implemented; Day 14 incomplete | Resolve threshold, collision complexity, nav-floor clearance and pawn routes; apply historical grading only after its evidence gates. |
| M03 / 15–17 | Paint and paving infrastructure implemented | Replace blockouts, ground road edges, review four final surface scenes. |
| M04 / 18–19 | Placement/toggle prototype only; incomplete | Final sparse vegetation, debris, drainage-consistent wetness and measured runtime counts. |
| M05 / 20 | Provisional evidence handoff exists; runtime blocked | Repeat candidate-specific checks/cook; complete Development runtime and WP acceptance, then freeze the new handoff. |

**Next execution order:** (1) revisit Day 01 owner/target/clearance decisions and source requests; (2) prioritize Days 12–14 collision, threshold and pawn work on the existing district; (3) complete Days 15–19 art with documented provisional placement; (4) perform Day 20 runtime acceptance on the resulting candidate. H1/H2/Z research proceeds as an external dependency throughout. If new accepted sources arrive, return to Days 03–10, migrate dependent district geometry, then repeat affected checks. Do not postpone all technical work until historical research completes, or spend the remaining time rebuilding proven imports.

**Capacity rule:** the original one-day slots are sequencing budgets, not current estimates or guaranteed completion dates. At the next review, owners must estimate the open issue groups and source delivery dates, then publish a remaining-work calendar. If the work exceeds the original twenty-day envelope, extend the schedule or obtain an explicit scope decision; retain incomplete gates. Missing evidence can result in a provisional handoff, never automatic historical approval or a silent M04 cut.

## Daily execution plan

Each day is also available as an independent execution document with a paired resource/evidence sheet. The detailed sections below remain the canonical cross-day summary. Their **Current baseline / Remaining focus** entries govern continuation; the Work and Done-when clauses retain the full requirements. Blank resource-sheet checklists and “Not started” template fields are not current execution status; use the linked QA ledgers and fill a new dated completion record when work resumes.

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
**Current baseline (2026-09-29):** Working coordinate, source and acceptance contracts exist; owner approval and PCG smoke remain pending.

**Remaining focus:** Reuse UE 5.5.4 and ordinary-instance fallback. Obtain budget sign-off, specify pawn capsule/step/slope and nav-floor limits, and resolve Metal SM5 Nanite applicability or choose supported hardware. Owners: project owner / technical lead; CANTON-APP-001, CANTON-M05-005.

**Work**
- Lock study period (1880–1900), walled-city polygon, local XY orientation, world origin convention, centimetre-to-metre conversion, target Unreal version and asset naming.
- Resolve the project's GUID-based Unreal association to an installed engine version. Audit required plugins and prove a minimal PCG smoke asset can load and cook; if PCG is unavailable, record ordinary foliage instances as the approved fallback.
- Create a source register: archive ID, observation date, locality, scan resolution, map scale if known, copyright/licence, redistribution status, retrieval location, SHA-256, location accuracy, vertical datum, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D.
- Define storage policy for original scans, large GeoTIFFs, derived rasters and UE assets, including Git LFS/external-artifact rules and reproducible tool versions.
- Declare georeference acceptance thresholds: independent check-point count and distribution, RMSE, maximum residual and a stricter gate-district tolerance tied to the narrowest geometry being aligned. Declare target hardware, build configuration, resolution/scalability, fixed performance route and frame-time/memory/streaming thresholds.
- Collect existing city-gate mesh dimensions and pivot conventions as *asset data*, not as surveyed historical ground elevations.
**Deliverables:** `Docs/Terrain_Baseline.md`; `Data/Source_Register.csv`; `Data/Coordinate_Contract.json`; `Data/Artifact_Manifest.csv`; `Docs/Terrain_Acceptance_Budget.md`; `QA/PCG_Preflight.md`.
**Done when:** Scope and coordinate conventions are versioned; the working acceptance budget is versioned with an explicit `owner_approval` state; the exact UE/plugin path is proven; storage/licence rules are recorded; all candidate sources carry provenance and separate evidence-quality fields. Historical gates cannot be `Verified` while `owner_approval` is pending.

### Day 02 — Historical wall and landmark source acquisition (M01)
**Current baseline (2026-09-29):** MAP-001 source acquisition is verified; metric precision is not.

**Remaining focus:** Reuse registered scans and hashes. Close source-date/scale gaps where possible and obtain survey-capable H1, local H2 and period comparison evidence; do not repeat download work or equate source acquisition with georeference acceptance. Owner: source/GIS owner; CANTON-H1-001.

**Work**
- Locate high-resolution maps dated within 1880–1900; separately register nearby-date maps as secondary references.
- Record city-wall perimeter, gate locations, Zhenhai Tower and surviving street junctions with source pointers.
- Mark unverified gate names/positions as provisional and record disagreements between maps.
**Deliverables:** `Sources/HistoricMaps/`; `Data/Historic_Map_Selection.md`; `Data/Landmark_Candidates.csv`.
**Done when:** At least one traceable historical map is selected; gaps and date mismatches are explicitly logged.

### Day 03 — Georeference historic city map (M01)
**Current baseline (2026-09-29):** The retained affine fails H1: three unsurveyed holdouts, 93.24 m RMSE / 132.155 m worst.

**Remaining focus:** Acquire measured controls and ≥8 distributed independent holdouts, build a new transform and compare period geometry. Preserve failed outputs; no production XY promotion until H1 passes. Owner: survey/GIS owner; CANTON-H1-001.

**Work**
- Choose a projected metric CRS suitable for Guangzhou; archive its EPSG code and coordinate transform to a local UE origin.
- Build **H1 (city historical frame)** from demonstrably persistent, precisely defined control points. Reserve spatially distributed points as independent checks and avoid forcing an archive map onto modern realigned streets. Use MAP-001 as a historical comparison layer unless its own metric quality is independently demonstrated.
- Prefer a survey-capable geometry backbone (including a clearly tagged near-period source if necessary) and compare it against in-period maps to detect temporal change. Test affine/polynomial/local transformations as appropriate; record per-point residuals, holdout residuals, distortion zones and rejected points with reasons. Do not select a higher-order transform merely because it reduces fitted-point error.
- H1 keeps the city acceptance budget. The strict gate-district budget belongs to H2 and is not waived merely because H1 passes.
**Deliverables:** `GIS/Canton_Historic_Georef_H1.tif`; `QA/Map_GCP_Residuals_H1.csv`; `Docs/Map_Distortion_Notes.md`; `Data/H1_Source_Comparison.csv`.
**Done when:** H1 opens at the documented coordinate origin and independent city checks meet the Day 01 city RMSE/maximum-error/spatial-coverage budget. Otherwise classify affected areas Provisional or Blocked rather than passing the transform.

### Day 04 — Digitize the walled-city plan (M01)
**Current baseline (2026-09-29):** Coarse wall/gate/road overlays exist under GIS/Provisional and convert correctly to UE.

**Remaining focus:** Retrace or migrate against the accepted H1 frame, retain source/date/confidence, and recheck wall plus ≥200 m margin on every side. Until H1 passes, retain diagnostic overlays only. Owner: GIS owner; CANTON-H1-001.

**Work**
- Trace wall centerline and gate points with source/date/confidence attributes.
- Digitize principal roads, historical water routes, important landform extents and major fixed landmarks into separate GIS layers.
- Calculate required bounding box and buffer; confirm that the proposed 4032 m square Landscape covers the actual wall polygon and needed working margin.
**Deliverables:** `GIS/Wall.geojson`; `GIS/Gates.geojson`; `GIS/Roads_Main.geojson`; `GIS/Waterways.geojson`; `QA/Extent_Check.md`.
**Done when:** Wall/gate layers align to the map and fit within the tested heightmap envelope; re-scope size if not.

### Day 05 — Acquire elevation and archaeological constraints (M01)
**Current baseline (2026-09-29):** Modern EGM2008 DTM exists; usable historical walking-surface Z and independent H2 controls do not.

**Remaining focus:** Obtain the Wenmingmen local network and dated benchmark-linked surface; keep Zhengdongmen as source-driven fallback. Do not infer metric bounds from asset readiness or use cultural-layer ranges as terrain points. Owners: survey/conservation and archaeology owners; CANTON-H2-001, CANTON-HZ-001.

**Work**
- Locate the best available bare-earth DEM or surveyed contours and inspect age, spatial resolution, vertical datum, accuracy and evidence of modern earthworks.
- Collect source-backed archaeological site levels, historic contours, hill/terrace constraints, historic water references and gate-approach slope observations.
- Create a point/line elevation schema with measured value, range, vertical datum, reference, observation date, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D; do not fill missing values.
- Select the **named gate/district candidate** now based on source coverage, gate-asset readiness and the availability of local survey/conservation/archaeological geometry. Record a ranked fallback. Do **not** freeze the final 200 × 200 m UTM bounds from a failed city transform.
- Start **H2 (gate-district local frame)**: identify the local source that can provide ≥4 independent checks with the required point definition, horizontal datum and precision. Derive final metric district bounds only after H2 is accepted.
**Deliverables:** `Sources/Elevation/`; `Data/Elevation_Constraints.csv`; `Docs/Vertical_Datum_Register.md`; `Data/Gate_District_Candidates.csv`; `Data/H2_Local_Control_Request.md`.
**Done when:** Every height value has units, origin and source; modern accuracy is not conflated with historical confidence; DEM coverage/uncertainty is documented; primary/fallback named districts are selected; and H2's evidence source is identified or explicitly blocked.

### Day 06 — Build preliminary historical elevation model (M01)
**Current baseline (2026-09-29):** Continuous modern-context float32 raster and contamination mask are implemented.

**Remaining focus:** Retain them as D-confidence context. Reconcile new vertical evidence and author traceable historical corrections/breaklines only after inputs pass; historical terrain zero stays null otherwise. Owner: terrain/GIS owner; CANTON-HZ-001.

**Work**
- Reconcile vertical datums before combining data; select a documented local terrain-zero reference.
- Use DEM for broad morphology only; tag modern cut/fill, roads, basements and reclamation as potential contamination.
- Construct north-hill / south-lowland conceptual grade and source-informed breaklines; do not invent specific nineteenth-century spot heights.
**Deliverables:** `GIS/Base_Elevation_float32.tif`; `GIS/Historic_Breaklines.geojson`; `QA/Modern_Change_Mask.tif`.
**Done when:** Prototype elevation model is spatially continuous, and every historical correction is traceable to evidence or flagged provisional.

### Day 07 — Interpolate historical terrain and confidence surface (M01)
**Current baseline (2026-09-29):** The current raster has full-area D confidence; it is resampled modern DTM, not historical interpolation.

**Remaining focus:** Use validated constraints to build a separately versioned historical surface and uncertainty/checkpoint report. Never improve confidence solely because the raster is continuous. Owner: terrain/GIS owner; CANTON-HZ-001.

**Work**
- Interpolate between control points while constraining hills, road-grade logic and documented water-flow directions.
- Separate inferred reconstruction from measured reference with a confidence/uncertainty raster and a feature-level log.
- Inspect for steps, sink artifacts, unrealistically steep urban blocks and residual modern building shapes.
**Deliverables:** `GIS/Historic_Elevation_float32.tif`; `GIS/Terrain_Confidence.tif`; `QA/Elevation_Checkpoints.csv`.
**Done when:** Terrain has no unexplained spikes or missing values; measured checkpoints fall within their source-specific uncertainty.

### Day 08 — Resample, encode and audit heightmap (M01)
**Current baseline (2026-09-29):** 2017² uint16 PNG, south-first R16, encoding and analytic coordinate tests already pass.

**Remaining focus:** Reuse the exporter and tests; regenerate only when accepted input/envelope changes. Validate hashes, clipping, quantization and one Y flip. Analytic round trips do not substitute for H1/H2 surveyed accuracy. Owner: terrain/tools owner.

**Work**
- Set world envelope, origin, orientation and grid-spacing metadata; resample elevation to a 2017 × 2017 grid for the proposed 4032 m coverage.
- Encode to single-channel 16-bit PNG with a fixed height-to-code mapping and matching UE Z scale; preserve float32 source raster.
- Test decoder round-trip, axis direction, extreme elevations, clipping and image bit depth using a small reproducible script.
- Add a coordinate round-trip test from projected GIS metres to local UE centimetres and back, using independent H1 landmarks at the envelope corners. Add H2 local gate checks only after an accepted local frame exists; otherwise label any gate-district round-trip as diagnostic/provisional.
**Deliverables:** `Export/Canton_1890_Height_2017_R16.png`; `Export/Heightmap_Metadata.json`; `Tools/Validate_Heightmap.py`; `QA/Heightmap_Encode_Report.md`.
**Done when:** PNG is exactly 2017 × 2017, uint16, single-channel and reversible within expected quantization error; no elevations clip; the GIS↔UE coordinate round trip meets the Day 01 tolerance.

### Day 09 — Import and inspect UE5 Landscape (M01)
**Current baseline (2026-09-29):** The provisional WP terrain map, locked base, 256 components and native reload automation exist.

**Remaining focus:** Review the saved map; do not rerun create-only import. Reimport a separately versioned historical candidate only after datum/frame approval and repeat asymmetric height/collision and overlay checks. Owner: UE terrain owner.

**Work**
- Create a dedicated World Partition map; enable Landscape Edit Layers during creation and import the heightmap into a named `Base_Imported` layer at 200/200/50 XY/Z scale only if the metadata confirms that encoding. Lock the base layer immediately.
- Use 63 quads per section, 2 × 2 sections/component and 16 × 16 components; center/position the Landscape using the documented local-origin transform.
- Record World Partition grid/region sizes, `Flip Y Axis`, Landscape actor transform and all import settings. Apply a neutral diagnostic material; place wall and gate markers and inspect orientation, scaling, collision and northern slope direction.
**Deliverables:** `Content/Canton/Maps/L_Canton_WalledCity.umap`; `Content/Canton/Terrain/MI_Diagnostic`; `QA/UE_Import_Screenshots/`.
**Done when:** UE Landscape imports without distortion; `Base_Imported` exists and is locked; import/WP settings are captured; an automated map-load/configuration check passes; landmark overlay faces the intended direction and sits in the correct XY frame.

### Day 10 — M01 source and terrain acceptance review (M01)
**Current baseline (2026-09-29):** M01 technical provisional review is delivered; H1/H2/Z remain blocked and approval pending.

**Remaining focus:** Update the four-axis decision only with new evidence. Freeze candidate-specific source/raster/native hashes and retain the failed baseline. No complete historical M01 claim from a successful native import. Owners: technical reviewer / historical reviewer.

**Work**
- Overlay traced walls and gates against imported terrain and independently check major control-point elevations. Run H1 city checks and, for the selected named gate district, H2 local checks separately.
- Mark historical-evidence gaps directly on the GIS confidence map and UE map; report `Technical_UE`, `Historical_XY_City`, `Historical_XY_GateLocal`, and `Historical_Z` separately as Verified / Provisional / Blocked.
- Freeze the versioned provisional/verified raster variants, source register and coordinate contracts; produce a short review packet and decision log. Do not freeze failed-transform-derived UTM district bounds as historical truth.
- Run `Scripts/test_canton_source_manifest.py`, `Scripts/test_canton_heightmap_contract.py` and the UE map-load/configuration automation test; record exact commands and outputs.
**Deliverables:** `Review/M01_Terrain_Review.md`; `Review/M01_Screenshots/`; `Data/M01_Uncertainty_Log.csv`.
**Done when:** `Technical_UE` may pass if import/scales/coords are valid and uncertainty is explicit. Historical M01 is `Verified` only when H1, H2 and Historical_Z each pass and Day 01 `owner_approval` is approved. Missing evidence leaves the corresponding axis Provisional/Blocked without invalidating the technical prototype.

### Day 11 — Urban road and parcel-layout plan (M02)
**Current baseline (2026-09-29):** A separate 200 × 200 m engineering district and D-confidence road classifications exist.

**Remaining focus:** Reuse layout scaffolding; replace road/parcel geometry with H2-aligned sources when available. Document the removed 16 parcel pads and open-ground courtyard reference; do not count either as built historical blocks. Owner: GIS/environment owner; CANTON-M02-001.

**Work**
- Import road centre lines and city-wall geometry as non-rendered reference overlays.
- Classify roads: principal stone-slab streets, smaller mixed-earth/pebble lanes, gate approaches and special courtyard areas; link classifications to photo/source evidence.
- Lay out a 200 × 200 m representative district with one gate interface and adjoining street/plots. If H2 is still blocked, this is an explicitly provisional implementation district identified by name/source context, not accepted historic UTM bounds.
**Deliverables:** `GIS/Road_Classifications.geojson`; `Data/Road_Surface_Register.csv`; `Maps/District_Test_Bounds.json`.
**Done when:** All prototype roads have source/date and surface-class labels; uncertain assignments are tagged.

### Day 12 — Street gradients and built-area grading (M02)
**Current baseline (2026-09-29):** Meshed roads conform to modern R16; Urban_Grading/Road_Corridors/Drainage layers are present and empty.

**Remaining focus:** Prioritize grounded road shoulders and threshold approach without changing gate scale or historic claims. Apply evidence-backed grading only after H1/H2/Z; distinguish mesh conformance from actual Landscape layer edits. Owners: terrain / gate owners; CANTON-M02-002, CANTON-M02-003, CANTON-M05-004.

**Work**
- Confirm the locked `Base_Imported` layer created on Day 09; create independent Landscape Edit Layers for urban grading, road corridors and drainage.
- Draft road elevations/intersections and representative block platforms from constraints; avoid flattening all of the city.
- At the gate interface, preserve original architectural dimensions and separately record any local ground correction.
- Treat the 2 m Landscape vertex grid as broad grading only. Build narrow lanes, thresholds, kerbs, gutters and small drains with splines, meshes or patches at the resolution required by their evidence and collision needs.
**Deliverables:** `UE/EditLayers_Base_Urban_Roads`; `QA/Street_Grade_Profiles.csv`; `Docs/Gate_Ground_Datum.md`.
**Done when:** Representative roads and plots meet without visible terrain discontinuities; grade decisions are traceable or provisional.

### Day 13 — Drainage and low-area blockout (M02)
**Current baseline (2026-09-29):** 48 covered-gutter pieces and three monotonic engineering catchments exist.

**Remaining focus:** Retest drainage after road/edge fixes; replace diagnostic locations only with source-supported drainage. Keep invented outlets and puddles labelled engineering-only. Owner: terrain owner; CANTON-M02-001.

**Work**
- Place historically mapped drains, canals or low areas as reference splines; do not fabricate open channels where maps/photos offer no support.
- Model test ditch and puddle depressions only where the ground context calls for them.
- Check that road grading does not introduce dead-end depressions or water running uphill.
**Deliverables:** `GIS/Drainage_Locations.geojson`; `UE/Drainage_Blockout`; `QA/Drainage_Flow_Notes.md`.
**Done when:** Prototype drainage has plausible downhill flow; documented and invented demo features are clearly separated.

### Day 14 — M02 geometry review and blocker cleanup (M02)
**Current baseline (2026-09-29):** Seven open/staging screens pass, but threshold access, physical pawn, nav-floor acceptance and warning disposition remain open.

**Remaining focus:** Preserve closed Wenmingmen doors. Simplify collision with an obstruction-preserving proxy; resolve the failed ~2 m threshold approach, measure floor at nav corners, and run the actual pawn across all seven routes plus the resolved threshold endpoint. A ~6 m staging stop is insufficient for threshold closure. Owners: gate/navigation owners; CANTON-M05-001, CANTON-M05-004, CANTON-M05-006.

**Work**
- Walk all intended open prototype roads with the actual pawn, including the district-side threshold approach; fix mesh/landscape seams and foundation gaps. Keep the authored gate closed and test its obstruction separately.
- Check collision and navigation on road grades; compare the layout with the georeferenced map overlay.
- Record corrections that alter evidence-backed geometry separately from implementation fixes.
- Run an automated district traversal/collision/navigation smoke and a commandlet map check. Fix or explicitly waive every warning relevant to the prototype map.
**Deliverables:** `Review/M02_Roads_Review.md`; `QA/Road_Nav_Collision.md`; `Review/M02_Screenshots/`.
**Done when:** The 200 × 200 m prototype supports physical pawn traversal of the intended open network and resolved district-side threshold approach, meets approved nav-floor/capsule criteria, and has no unresolved implementation geometry blockers or unwaived relevant collision warnings. Historical road/gate alignment is reported separately and remains Provisional/Blocked wherever H2 or source evidence is unresolved.

### Day 15 — Author master ground materials (M03)
**Current baseline (2026-09-29):** Six compiled paint targets and neutral swatches exist.

**Remaining focus:** Reuse the material graph and layer-info assets; author licensed physical soil/earth/pebble/grass/damp/stone instances with scale, normals and roughness. Review at walking height; compiled swatches alone do not close M03. Owner: environment artist; CANTON-M03-001.

**Work**
- Build a Landscape material with restrained layer count: natural soil, compacted earth, mixed pebble/earth, grass/weed soil, damp earth and optional exposed stone.
- Prepare physically based material instances with adjustable tint/roughness; treat monochrome photos as shape/placement evidence, not reliable colour sampling.
- Document scale, texel density, blend masks and texture licence/provenance.
**Deliverables:** `Terrain/M_Canton_Landscape`; `Terrain/MI_*`; `Data/Material_Evidence_Register.csv`.
**Done when:** The Landscape material compiles and the six planned surface families can be painted without obvious repetition at walking height.

### Day 16 — Build modular historical stone paving (M03)
**Current baseline (2026-09-29):** 100 principal cube slabs have floor support; the independent joint probe measures 198 pairs and a 1.33 cm maximum step.

**Remaining focus:** Replace cube slabs with period-context modules, preserve pivots/collision, and ground the 7 cm underside gap / 11 cm shoulder lip using proper kerbs or transitions. Recheck joints and routes after replacement. Owner: environment artist; CANTON-M03-001, CANTON-M02-003.

**Work**
- Create reusable stone-slab sets for primary roads, plus selected kerbs, steps, foundation strips and drainage edges.
- Use source-backed slab arrangement where documented; define pivots, snap grid, collision and Nanite usage by mesh.
- Test road-to-earth edge transitions, individual stone heights and slope changes at street intersections.
**Deliverables:** `Roads/SM_StoneSlab_*`; `Roads/BP_RoadSegment_Test`; `QA/StoneRoad_Seams.md`.
**Done when:** The test street is traversable with no floating stones, Z-fighting, visible grid seams or oversized European-style cobble pattern.

### Day 17 — Terrain/road blending and material QA (M03)
**Current baseline (2026-09-29):** Four fixed scenes and atlas document blockout surfaces.

**Remaining focus:** Finish road/earth/gate transitions, audit dry/wet roughness and update all four camera views from the same candidate. Re-run seams/collision after changes; visual modules and material joints remain open despite geometric sample passes. Owner: environment artist; CANTON-M03-001.

**Work**
- Blend Landscape ground with modular paving using appropriate skirts, decals or masked transition meshes; avoid painting a fake sharp stone edge on soil.
- Compare dry and wet material variants under consistent lighting; audit roughness/normal intensity at eye level.
- Capture four representative scenes: main stone street, mixed lane, gate approach and courtyard edge.
**Deliverables:** `Roads/MI_Transition_*`; `Review/M03_Material_Atlas.png`; `Review/M03_Material_QA.md`.
**Done when:** Four scenes show distinct readable surfaces and convincing transitions at human scale, without unverified claims of exact historic soil colour.

### Day 18 — Add urban vegetation with controls (M04)
**Current baseline (2026-09-29):** 38 collision-disabled cube growth proxies implement placement rules; PCG remains unproven.

**Remaining focus:** Replace proxies with sparse period-context vegetation using ordinary foliage/instances; preserve gate, road, gutter and foundation exclusions and document source/licence/species uncertainty. Verify collision and actual instance counts. Owner: environment artist; CANTON-M04-001.

**Work**
- Create grass tufts, weeds, wall-edge moss and sparse courtyard plants using foliage/PCG masks tied to surface class and evidence.
- Use PCG only if the Day 01 load/cook smoke passed; otherwise use the documented foliage-instance fallback without changing the placement contract.
- Keep dense managed garden vegetation out of ordinary street areas unless specifically documented.
- Set exclusion masks around paved roads, gate access, drain covers and building foundations.
**Deliverables:** `Foliage/PCG_Canton_UrbanGrowth` or `Foliage/Foliage_Canton_UrbanGrowth` according to the Day 01 decision; `Data/Foliage_SpawnRules.json`; `QA/Vegetation_Exclusions.md`.
**Done when:** Vegetation is sparse and place-specific; paths and gate openings remain unobstructed.

### Day 19 — Add weathering, wetness and small ground details (M04)
**Current baseline (2026-09-29):** 14 grounded dark cards provide a dry/wet toggle only; debris and final wetness are missing.

**Remaining focus:** Author localized debris and drainage-consistent damp/puddle materials; capture matched dry/rain views and runtime draw calls/instances. Preserve access and exclusions. M04 remains incomplete until final art is reviewed or the owner explicitly changes scope. Owner: environment artist; CANTON-M04-001.

**Work**
- Distribute local leaf litter, grit, small stone debris, puddles and dampness through PCG or ordinary decals/instances according to the Day 01 decision, rather than noisy full-map height edits.
- Test dry and after-rain scenarios; validate the wet ground placement against drainage and material boundaries.
- Capture representative before/after screenshots and a draw-call/instance-count snapshot.
**Deliverables:** `GroundDetails/PCG_Canton_Debris` or `GroundDetails/Canton_Debris_Instances` according to the Day 01 decision; `Materials/MI_Wetness_*`; `Review/M04_Weathering_QA.md`.
**Done when:** Localized details enhance readability without clutter or implausible uniform mud/wetness; no material/foliage collision issues.

### Day 20 — M05 gate-district integration and production handoff (M05)
**Current baseline (2026-09-29):** Targeted two-map cook and provisional handoff exist; Development performance, WP streaming and pawn evidence are absent.

**Remaining focus:** Build a runnable Development candidate with the declared pawn/route; run Lumen and agreed Nanite/fallback compatibility checks, capture runtime metrics, and validate the intended delivery cook configuration. Re-freeze both maps/external actors and all evidence after final changes. Editor proxy timings and the temporary cook exclusion cannot close runtime or normal-project packaging. Owners: build/performance lead / reviewer; CANTON-M05-002, CANTON-M05-003, CANTON-M05-005.

**Work**
- Reuse the existing Wenmingmen integration; preserve architectural scale and closed-door intent, record nonhistorical asset fixes, and validate the resolved district-side threshold approach. A through-gate route requires a separately approved design change.
- Validate scale, collision, Lumen/Nanite compatibility as used, World Partition streaming and visual seams. Run the fixed Day 01 route on the declared target hardware/build/resolution/scalability after a defined warm-up; capture frame-time percentiles, memory, draw calls, instance counts and streaming failures against the declared thresholds.
- Run the focused Python contracts, UE automation/map check and a Development cook of both current Canton maps (or their explicitly versioned successors). Record exact arguments and cook configuration. Validate the intended delivery configuration; retain any scoped cook exclusion as a limitation and do not claim normal full-project packaging from it.
- Publish terrain data, source confidence, screenshots, known deviations, open blocker list and next-stage citywide replication rules.
**Deliverables:** `Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.
**Done when:** The closed test gate has a physically accessible district-side threshold connected to the traversable 200 × 200 m open district; source/heightmap/map-load/collision/navigation/cook checks pass; the fixed Development performance route meets its approved thresholds and runtime WP checks pass; the handoff reports `Technical_UE`, H1, H2 and Historical_Z separately. If runtime acceptance fails or is missing, Day 20 remains blocked and only a provisional handoff may be delivered. A technically complete district may remain historically Provisional/Blocked; all full-city areas not yet built or validated remain explicitly out of scope.

## Critical-path and stop/go rules

- **Map unavailable or unusably distorted (Days 02–04):** preserve the provenance audit; create only a flagged provisional footprint and do not claim historically correct gate coordinates. Escalate source acquisition rather than improving an arbitrary visual map.
- **DEM / vertical datum unavailable (Days 05–07):** make a clearly named `PROVISIONAL` heightfield; defer source-verified M01 approval. A flat or stylized prototype can prove UE tools, not historic elevation.
- **Coverage exceeds 4032 m:** recalculate grid size / scale before resampling. Do not clip the city wall just to satisfy the provisional import setting.
- **Heightmap orientation or encoding fails (Days 08–09):** block detailed road construction until XY origin, axis convention and vertical scale are fixed.
- **Gate asset disagrees with verified terrain:** document the mismatch and investigate the gate model, survey datum or location independently; do not overwrite elevation evidence to conceal it.
- **Performance slows (Days 16–20):** first profile instance counts, material shader cost, collision and WP streaming on target hardware. Do not assume Nanite alone solves grass/foliage or shader cost.
- **Plugin or cook preflight fails (Day 01):** use the documented foliage fallback and do not defer plugin discovery until Day 18.
- **H1 city georeference exceeds its accuracy budget (Days 03–05):** change transform/source; preserve the failed result. Do not average away a failed independent check.
- **H2 local gate frame is unavailable or misses its tighter budget (Days 03–10):** keep the named district provisional or select a better-evidenced gate. Do not derive “accepted” local bounds from H1/MAP-001 alone and do not lower the 2 m / 4 m budget.
- **Restricted or oversized source data cannot be committed:** keep the original outside Git, commit its manifest/checksum/licence/retrieval instructions and verify that an authorised workstation can reproduce the derived output.

## Minimum review/evidence packet

Include: original map identifiers and observation dates; licence/redistribution status and SHA-256; spatial reference and local origin; fitted-control and independent-check residual tables with declared thresholds; raster source and heightmap encoding metadata; separate measurement-reliability, temporal-relevance, modern-change-risk and historical-confidence fields; confidence raster; UE import/edit-layer/WP settings; wall/gate overlay screenshots; one terrain profile from north to south; district walk-through images; source-vs-reconstruction change log; automated test/map-check/cook commands and logs; fixed-route performance configuration and results; list of areas that remain provisional.

## After Day 20 (outside this plan)

Expand the validated district systems across the remainder of the walled city; add citywide street detail, blocks/buildings, gate-to-gate traversal and progressive historical-source validation. Do not treat the first prototype as proof that every city district is completed or accurate.

## Validation and evidence refresh for continuation

Run only checks appropriate to the changed candidate; record commands, exit codes and logs. Existing snapshot tests prove their declared engineering contracts, not historical acceptance. Some district assertions intentionally describe current blockout counts and blocked states: when implementation changes, update those contracts to the new explicit requirements and retain prior evidence rather than disabling assertions to obtain a pass.

```sh
python3 Scripts/test_canton_source_manifest.py
Saved/TerrainTools/venv/bin/python Scripts/test_canton_heightmap_contract.py
Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py
python3 Scripts/test_canton_m01_readiness.py
python3 Scripts/test_canton_district_contract.py
python3 Scripts/test_canton_district_check_runner.py
```

After UE/geometry changes, use `zsh Scripts/run_canton_provisional_review.sh automation` and `python3 Scripts/run_canton_district_check.py review`, `seam`, and `traversal` as applicable. The guarded runner requires fresh JSON and completed clean shutdown; raw scripted exit 1 is never sufficient. Its `performance` mode is an editor diagnostic and does not implement the missing Development acceptance capture. Physical pawn and runtime capture remain deliverables to implement, not tests already supplied by the runner.

Refresh affected manifests from inner dependencies outward (source, provisional, native, district) and capture a new repository freeze after the evidence is final. Run `python3 Scripts/test_canton_handoff_freeze.py` against that new snapshot. Preserve previous dated archive records and failed measurements; a checksum refresh is not a new UE execution or approval. Link every closed issue to fresh candidate-specific evidence in `Data/Open_Issues.csv`.
