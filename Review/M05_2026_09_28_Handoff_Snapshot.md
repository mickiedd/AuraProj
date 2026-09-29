# M05 Canton provisional district handoff — 2026-09-28

**Decision:** the isolated 200 × 200 m district is a usable **D-confidence engineering prototype**, not an accepted 1880–1900 Canton reconstruction. M05's provisional packet is complete, but the 20-day implementation is **not closed**: Day 14 near-threshold access, pawn traversal and collision-warning resolution, M04 final art, and Day 20 runtime acceptance remain open. H1 city alignment, H2 gate-local alignment, historical Z and Day-01 owner approval remain blocked or pending. The 1.5× detour and <8% grade values are now explicitly in working budget `2026-09-28-h1-h2-district-v2` as **engineering screens**, not owner-approved acceptance gates.

## Repository freeze and scope

[The machine-readable repository freeze](M05_Repository_Freeze.json) records the exact branch, base HEAD, full `git status --porcelain=v1 --untracked-files=all`, dirty files inside and outside the package manifest, SHA-256 of this handoff and the manifest, timestamp, engine and host. **Final handoff commit: none**; this is an uncommitted, dirty-worktree delivery. Its base HEAD is `14041340a781dc8bbb417c6f36a9dd922b56a023`; unrelated Zhengdongmen/Xiaobeimen edits are preserved. The [package manifest](../Data/M02_M05_District_Artifacts.csv) now hashes **both cooked Canton levels** and their M01/M05 evidence, including the walled-city World Partition actor packages. The append-only visual-change index is excluded from immutable package hashes; the freeze stores its line count and tail-line hash as a point-in-time observation. The freeze JSON is generated after this document and the manifest to avoid a self-hash cycle.

The World Partition map is `/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL`. [Diagnostic bounds](../Maps/District_Test_Bounds.json) sample the M01 modern-context R16 and **do not locate historical Wenmingmen**. The original Wenmingmen Blueprint remains unchanged and is placed at architectural scale with a diagnostic `+333 cm` pivot offset, not a surveyed threshold. The modern source is approximately 30 m resolution, though the R16 grid is sampled at 2 m. Road/parcel/drainage geometry and source-neutral art are engineering-only. `Base_Imported` is locked; `Urban_Grading`, `Road_Corridors` and `Drainage` are separate unmodified edit layers awaiting evidence-backed corrections. No broad floating parcel platforms were retained. Host: Apple M4 Mac mini, 16 GiB unified memory, Mac/Metal; UE 5.5.4 CL 40574608.

The map has 100 principal 2 m road slabs, 76 mixed-lane pieces, 48 covered-gutter pieces, six Landscape paint targets, one gate scale interface, 38 collision-disabled weed cubes and 14 local wet cards. Three diagnostic gutter reaches slope toward engineering outlets, not documented Qing drains. Worst sampled 2 m road grades are 5.66% principal and 7.62% mixed. The six paint targets and visible material/vegetation/wetness treatments are blockouts.

[Build_Result.json](../QA/Canton_District/Build_Result.json) is explicitly the **initial pre-refinement snapshot** (96 mixed-lane pieces, 16 parcel pads). The current map has 76 and zero respectively: [slope refinement](../QA/Canton_District/Slope_Refinement.json) removed 20 high-grade western lane pieces, and [surface conformance](../QA/Canton_District/Surface_Conformance.json) removed 16 unsupported broad parcel pads. Current counts come from the [fresh native reload](../QA/Canton_District/Reload_Validation.json), not the initial snapshot.

## Milestone closure

| Milestone | State | Basis / remaining work |
|---|---|---|
| M01 technical UE | PASS, provisional source | Native R16 import, landscape collision/configuration and M01 automation pass. |
| M01 H1 city XY / H2 gate XY / historical Z | BLOCKED / BLOCKED / BLOCKED | Survey controls, local gate checks and dated datum-backed walking surface missing. |
| Day-01 owner budget | PENDING | [Versioned budget](../Docs/Terrain_Acceptance_Budget.md) is not approved. |
| M02 / Day 14 | PARTIAL | Seven intended open/staging routes pass the provisional navigation screen; near-threshold and closed-door transit queries detour. Pawn, nav clearance budget and warning resolution remain open. |
| M03 | PROVISIONAL | Paint/material and paving blockouts with fixed views; period assets and transitions open. |
| M04 / Days 18–19 | **INCOMPLETE / PROVISIONAL** | Growth cubes and wet cards only; debris, period planting, final wetness and runtime counts/draw calls missing. No approved scope reduction. |
| M05 handoff | PROVISIONAL HANDOFF COMPLETE | Reproducible evidence and open-work packet delivered. |
| Day 20 runtime / World Partition acceptance | BLOCKED | No Development fixed-route capture or runtime streaming/cell metrics. |

## Day 14 traversal and collision

The [fresh-load route matrix](../QA/Canton_District/Traversal_Route_Matrix.json) runs `BuildPaths`, zero-error/zero-warning map check, vertical floor traces at 2 m or finer spacing, seven intended open/staging navigation queries, and a separate **closed-door transit diagnostic**. The gate package models shut timber doors. The original south-to-north through-gate query starts inside its footprint and detours 2.583× around the outer wall. A [second attempted stop](../QA/Canton_District/Gate_Threshold_Attempt.json) about 2 m north of the gate face also detoured 2.905×. The chosen open-route endpoint is a staging point about **6 m north of the face**, not the threshold and not a through-passage claim. This is automated editor navigation/floor support, **not a pawn walk**. All intended-route floor traces hit; physical pawn traversal is `NOT_RUN`. The courtyard target is an open-ground GIS reference, not a built courtyard.

| Segment | Floor support | Nav directness | Pawn |
|---|---|---|---|
| Gate-side principal road → interior (145.01 m) | PASS | PASS, 1.00× | NOT RUN |
| Western mixed lane → junction | PASS | PASS, 1.00× | NOT RUN |
| Junction → eastern mixed lane | PASS | PASS, 1.00× | NOT RUN |
| Principal → mixed-lane intersection | PASS | PASS, 1.00× | NOT RUN |
| District-side gate staging (ends ~6 m north of closed gate) | PASS | PASS, 1.002× | NOT RUN |
| Covered-gutter crossing | PASS | PASS, 1.00× | NOT RUN |
| Mixed lane → courtyard reference | PASS | PASS, 1.00× | NOT RUN |

The original 100/100 principal station height/collision traces pass; maximum center error is 0 cm and sampled longitudinal edge deviation is 0.024 cm. A distinct [joint probe](../QA/Canton_District/Seam_Probe.json) measures **198 adjacent top-corner pairs** and a 1.33 cm maximum vertical step, rather than reusing the reload JSON as independent seam evidence. These checks establish local road floor support, **not threshold access or passage through the closed gate**. The [nav-to-floor diagnostic](../QA/Canton_District/Nav_to_Floor_Diagnostic.md) traces under each path corner; observed gaps reach 15.63 cm, with no approved clearance/quantisation budget or pawn test. The gate navigation export warns at 288,064 and 595,104 collision triangles; **no owner waiver has been issued**. Gate threshold access (`CANTON-M05-004`), nav-floor alignment (`CANTON-M05-006`) and collision complexity (`CANTON-M05-001`) remain open.

## Day 20 validation breakdown

| Check | State | Evidence / limitation |
|---|---|---|
| Scale and road geometry | PROVISIONAL PASS | [Reload result](../QA/Canton_District/Reload_Validation.json); gate only scale-placed. |
| Road floor collision | PASS on samples | 100 principal checks plus seven route floor-trace sets. |
| Gate approach / through-passage | STAGING PASS / THRESHOLD OPEN / DOORS CLOSED | Staging route is direct; 2 m threshold attempt detours 2.905×; closed-door transit detours 2.583×. No pawn run. |
| Gate collision complexity | OPEN, UNWAIVED | Two high-triangle navigation export warnings; runtime cost unknown. |
| Nav-to-road vertical alignment | MEASURED, OPEN | Nav-corner physics-floor gaps reach 15.63 cm; no approved clearance budget. |
| Nanite compatibility on declared Mac | UNAVAILABLE | [Wenmingmen package](../ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md) states Metal SM5 uses full-resolution raster fallback. Owner must approve Nanite as inapplicable or choose supported target hardware. |
| Lumen compatibility | NOT RUN | Project config enables Lumen methods; no district runtime visual/compatibility capture. |
| World Partition editor map load | PASS | Fresh-load map check and targeted cook. |
| World Partition runtime streaming / failed-cell count | NOT RUN / NOT CAPTURED | Needs Development fixed route and cell diagnostics. |
| Slab-to-slab geometric joints | PROVISIONAL MEASURED | [Distinct joint probe](../QA/Canton_District/Seam_Probe.json): 198 comparisons, 1.33 cm maximum vertical step. Final visible material/module joints remain OPEN. |
| Road/shoulder cross-section | BLOCKOUT OPEN | [Section view](../QA/Road_Edge_Blockout_Profile.svg): 7 cm underside gap and 11 cm shoulder lip; final kerb/skirt/transition unresolved. |
| Development p95/p99 frame time, GPU and RSS | BLOCKED | No Development/packaged fixed-route run. [Editor SceneCapture proxy](../QA/Canton_District/Performance_Probe.json) is diagnostic only. |
| Runtime draw calls and instance counts | NOT CAPTURED | Editor reload counts 38 proxies/14 cards but no runtime snapshot. |

The editor proxy used 1920 × 1080 High settings, a gate-to-interior camera path, 60 s warm-up and 180 s sample. It recorded 20,431 **editor tick intervals** (p95 9.72 ms, p99 10.48 ms) and 3.43 GB process peak RSS. These are not Development game frame/GPU times and cannot pass the 33.3 ms p95, 50 ms p99, 12 GiB RSS and zero failed-cell gate. Draw calls and runtime instances were not captured.

## Build and cook qualification

`./BuildEditor.command` passed for AuraEditor Mac Development after tightening the native count validator and adding joint measurement. `python3 Scripts/test_canton_source_manifest.py`, `Saved/TerrainTools/venv/bin/python Scripts/test_canton_heightmap_contract.py`, `Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py`, `python3 Scripts/test_canton_m01_readiness.py` and `python3 Scripts/test_canton_district_contract.py` pass. `zsh Scripts/run_canton_provisional_review.sh automation` passes one map-configuration and seven height/collision checks ([result](../QA/UE_Import_Screenshots/Automation_Validation.json)). The [fresh-load native review](../QA/Canton_District/Reload_Validation.json) and [distinct joint probe](../QA/Canton_District/Seam_Probe.json) pass their technical checks. The [route matrix](../QA/Canton_District/Traversal_Route_Matrix.json) now passes seven intended open/staging screens while retaining a failing closed-door transit diagnostic; it does **not** close Day 14.

**Targeted Canton Mac Development cook: PASS** — 994/994 packages, two provisional `.umap` files, zero error lines ([result](../QA/Canton_District/Cook_Result.json), [log](../Saved/Logs/CantonDistrictCookFinal.log)). The successful run temporarily excluded four project-wide `DirectoriesToAlwaysCook` entries; `Config/DefaultGame.ini` was restored byte-for-byte. **Normal unchanged project cook configuration: NOT VALIDATED.** This is not a full-project packaging or runtime pass. Exact cook arguments are in the machine result.

The [guarded headless runner](../Scripts/run_canton_district_check.py) checks fresh JSON, clean completed UE shutdown, known versus unknown log errors, map-check result and mode-specific assertions before normalizing UE's observed scripted-quit exit 1. [Failure-case tests](../Scripts/test_canton_district_check_runner.py) cover missing output, Python errors, map-check failure, incomplete shutdown, incomplete seam data and unexpected closed-gate passage. Its [live native review](../QA/Canton_District/Runner_review.json), [joint probe](../QA/Canton_District/Runner_seam.json) and [revised traversal](../QA/Canton_District/Runner_traversal.json) normalized raw exit 1 only after their checks passed. The earlier bad threshold queries remain failed evidence. `CANTON-M05-003` remains open for the underlying engine exit behavior.

## Four acceptance axes

| Axis | State | Required evidence |
|---|---|---|
| `Technical_UE` | Provisional map/geometry/cook pass; Day 14 and runtime blocked | Near-threshold clearance, pawn traversal, warning disposition, nav-floor budget, final art and Development fixed-route metrics. |
| `Historical_XY_City (H1)` | BLOCKED | ≥8 distributed independent surveyed checks; ≤15 m RMSE, ≤30 m worst. Current unsurveyed holdouts: 93.24 m RMSE, 132.155 m worst. |
| `Historical_XY_GateLocal (H2)` | BLOCKED | Wenmingmen local survey and ≥4 independent checks; ≤2 m RMSE, ≤4 m worst. |
| `Historical_Z` | BLOCKED | Dated late-Qing gate-approach walking surface tied to named benchmark and vertical datum. |

## One-to-one evidence index

| Required item | Direct evidence | State |
|---|---|---|
| Original map IDs, observation dates, rights/reuse | [Source register](../Data/Source_Register.csv) | Present; unknown dates identified |
| Local source SHA-256 and licence | [Artifact manifest](../Data/Artifact_Manifest.csv) | Present where acquired; link-only items have no invented local hash |
| CRS, local origin and diagnostic bounds | [Prototype contract](../Data/Canton_Prototype_Contract.json), [bounds](../Maps/District_Test_Bounds.json) | Present; historical metric position blocked |
| Fitted controls and independent H1 residuals | [Residual table](../QA/Map_GCP_Residuals.csv), [M01 review](M01_Terrain_Review.md) | Present; H1 fails |
| H2 local controls/residuals | [H2 request](../Data/H2_Local_Control_Request.md) | Missing surveyed inputs; blocked |
| Historical Z constraints / datum | [Vertical datum register](../Docs/Vertical_Datum_Register.md), [gate ground note](../Docs/Gate_Ground_Datum.md) | Blocked |
| Declared thresholds and owner approval | [Acceptance budget](../Docs/Terrain_Acceptance_Budget.md) | Thresholds present; approval pending |
| Raster source and R16 encoding | [Source register](../Data/Source_Register.csv), [heightmap metadata](../Export/Provisional/Heightmap_Metadata.json) | Modern-context D only |
| Historical confidence fields and raster | [Source register](../Data/Source_Register.csv), [confidence raster](../GIS/Provisional/Terrain_Confidence_PROVISIONAL.tif) | D/provisional |
| UE import and Edit Layer settings | [Import settings](../QA/UE_Import_Screenshots/Import_Settings.json), [native validation](../QA/Canton_District/Reload_Validation.json) | Present; correction layers unmodified |
| World Partition settings/map | [Import settings](../QA/UE_Import_Screenshots/Import_Settings.json), [two-level manifest](../Data/M02_M05_District_Artifacts.csv) | Both cooked maps and external actors frozen; runtime streaming missing |
| Wall/gate overlay and import screenshots | [M01 screenshots](../QA/UE_Import_Screenshots/), [gate view](M05_QA_Screenshots/gate_approach.png) | Provisional views, not surveyed overlay acceptance |
| North–south terrain profile | [CSV](../QA/Provisional/North_South_Modern_Context_Profile.csv), [SVG](../QA/Provisional/North_South_Modern_Context_Profile.svg) | Modern EGM2008 context only |
| District walk-through views | [M05 fixed views](M05_QA_Screenshots/) and [capture settings](../QA/Canton_District/Capture_Settings.json) | Representative stills; continuous pawn walk missing |
| Source vs prototype changes | [Change log](M05_Source_to_Prototype_Change_Log.md) | Present; D-confidence departures explicit |
| Day 14 traversal / map check | [Route matrix](../QA/Canton_District/Traversal_Route_Matrix.json), [threshold attempt](../QA/Canton_District/Gate_Threshold_Attempt.json), [nav-to-floor diagnostic](../QA/Canton_District/Nav_to_Floor_Diagnostic.md), [runner log](../Saved/Logs/CantonDistrictRunner_traversal.log) | 7/7 intended open/staging route screens; threshold/pawn/warnings still open |
| Road-joint and edge section | [Joint probe](../QA/Canton_District/Seam_Probe.json), [edge profile](../QA/Road_Edge_Blockout_Profile.svg) | Joint measured; visible edge blockout open |
| Native automation and cook logs | [Automation result](../QA/UE_Import_Screenshots/Automation_Validation.json), [cook result](../QA/Canton_District/Cook_Result.json), [cook log](../Saved/Logs/CantonDistrictCookFinal.log) | Scoped passes |
| Fixed performance configuration/results | [Editor proxy](../QA/Canton_District/Performance_Probe.json), [budget](../Docs/Terrain_Acceptance_Budget.md) | Editor diagnostic only; Development blocked |
| Runtime draw calls, instance counts, failed WP cells | [Open issues](../Data/Open_Issues.csv) | Not captured |
| Remaining provisional areas | [Open issues](../Data/Open_Issues.csv), [Days 11–20 ledger](../QA/Canton_Days_11_20_Execution.md) | Explicitly open |

The [district overview](M05_QA_Screenshots/district_overview.png), [authoritative gate approach](M05_QA_Screenshots/gate_approach.png), [main road](M05_QA_Screenshots/main_street.png), [mixed lane](M05_QA_Screenshots/mixed_lane.png), [courtyard reference](M05_QA_Screenshots/courtyard_edge.png), [dry](M05_QA_Screenshots/dry_ground.png) and [damp](M05_QA_Screenshots/after_rain_ground.png) views document the blockout. Every review view is now byte-identical to its [capture-settings](../QA/Canton_District/Capture_Settings.json) path and is mirrored automatically on a fresh review. [Capture QA](../QA/Canton_District/Capture_QA.json) verifies paired viewpoints. The [independent-audit disposition](M05_Independent_Audit_Disposition.md) accounts for each finding. The [open issue queue](../Data/Open_Issues.csv) prioritizes H1/H2/Z/owner evidence, gate threshold/clearance and pawn traversal, M04 completion, then the declared runtime World Partition/performance capture. [Citywide replication](../Docs/Citywide_Terrain_Replication.md) is a future contract; unbuilt full-city areas are out of scope.
