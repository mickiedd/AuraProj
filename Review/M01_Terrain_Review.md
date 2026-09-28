# M01 terrain review — 2026-09-26

**Technical prototype: Provisional, validated in UE. Historical acceptance: Blocked.** The first ten days now have an executable modern-context terrain pipeline, a saved World Partition map, source/encoding tests, fresh-process validation and native screenshots. This is the master plan's provisional fallback, not completion of the historically verified deliverables.

Operator/technical reviewer: Codex. Baseline `e3f8e2c9b74b5f3c782c3111857ae4d53ab37b7b`; existing landmark and memory changes preserved. UE 5.5.4 CL 40574608, Mac/Metal, Apple M4. Terrain Python 3.9.6 uses `Scripts/requirements-canton-terrain.txt`.

| Day | Implemented / reviewed | Remaining historical gate |
|---|---|---|
| 01 | Separate executable prototype coordinate/encoding contract | Historical origin, terrain zero and accuracy approval |
| 02 | Verified 1880 source selection retained | Survey date/numeric map scale remain unknown |
| 03 | Failed transform and independent residuals retained; renewed source search logged | Surveyed controls and four-quadrant checks |
| 04 | Coarse wall/gate GIS projected correctly into UE markers | Accepted GIS requires passing Day 03 |
| 05 | Modern EGM2008 DTM used with stored scale/offset; archaeological candidate excluded | Datum-backed late-Qing walking surface |
| 06 | Continuous modern-context float32 raster and full-area contamination mask | Historical corrections/breaklines unavailable |
| 07 | Full-area D-confidence raster; no holes in working raster | Historical interpolation/checkpoints unavailable |
| 08 | PNG and south-first R16, explicit mapping and automated audits | Surveyed landmark alignment remains unresolved |
| 09 | Saved WP Landscape, diagnostic material, locked base, native samples/collision and reload pass | Imported geometry remains D-confidence context |
| 10 | Provisional technical review and versioned package manifest | Historically accepted M01 cannot pass |

## What resolved the technical blockers

[Prototype contract](../Data/Canton_Prototype_Contract.json) fixes the working origin at E729400/N2557300 in EPSG:32649 and a **tool-only** zero of 0 m EGM2008. It leaves the historical contract untouched. No unnamed archaeological datum was converted, no survey residual threshold was reduced, and no candidate was relabelled a measured point.

The native importer reads the explicit little-endian, south-first R16 prepared from the north-up PNG. A test checks every code against the flipped PNG and checks the longitude/latitude-to-UTM overlay conversion independently. Source pixel centres span exactly 4032 m at 2 m spacing. Resampling does not improve the approximately 30 m source resolution.

Map: `/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL`. It has a main Landscape plus 16 streaming proxies, 256 components, 63 quads/section, 2 × 2 sections/component, scale 200/200/50 and actor origin 0/0/0. Landscape grid size is 4 components; import region size is 16 components. WP streaming is enabled. `Base_Imported` is locked, and the neutral diagnostic material is assigned. Gold wall bars and blue gate columns are coarse, non-colliding map markers, not reconstructed architecture.

Seven asymmetric checks (corners, centre and district locations) compare persisted edit-layer height codes and collision traces against the import bytes. The largest collision error is 0.0156879425 cm, within the declared 2 cm engineering tolerance. These are implementation checks, not surveyed landmark accuracy. The imported samples themselves match exactly.

## Validation and evidence

| Check | Result |
|---|---|
| AuraEditor Mac Development build | Exit 0 |
| `python3 Scripts/test_canton_source_manifest.py` | Original source/inventory integrity retained |
| `Saved/TerrainTools/venv/bin/python Scripts/test_canton_heightmap_contract.py` | 2 test methods pass, including 4 rejection cases |
| `Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py` | 2 test methods pass: full-array row order/hashes and overlay conversion |
| Native creation and fresh-process reload | 256 terrain + 256 collision components; 7 height/collision checks pass |
| `Aura.Canton.Provisional.MapConfiguration` | **Success**, 1 test, 0 errors, 0 warnings |
| `python3 Scripts/test_canton_m01_readiness.py` | 472 native artifacts hashed; technical Provisional, historical Blocked |
| Encoding | 2017², grayscale 16-bit, zero clips, max error 0.001953125 m |

[Exact commands and run notes](../QA/Canton_Days_01_10_Execution.md), [Unreal automation report](../QA/UE_Import_Screenshots/Automation_Report.json), [reload checks](../QA/UE_Import_Screenshots/Reload_Validation.json), [capture viewpoints](../QA/UE_Import_Screenshots/Capture_Settings.json).

[Overview](../QA/UE_Import_Screenshots/overview.png) · [Northern terrain](../QA/UE_Import_Screenshots/north-hills.png) · [South-gate working district](../QA/UE_Import_Screenshots/south-gate.png). All show modern-context terrain and D-confidence overlays. No period buildings or historically validated street grades are implied.

## Remaining evidence and scope

Every raster cell remains confidence D with unassessed modern contamination. Day 03 still has only three unsurveyed holdouts: 93.24 m RMSE, 132.155 m worst and no NE coverage. The historical terrain zero is null. The [targeted source follow-up](../QA/Canton_Blocker_Research_2026_09_26.md) did not supply the missing survey packet.

Obtain the specific controls and dated heights in [Control Survey Handoff](../QA/Control_Survey_Handoff.md), then create a new versioned transform, historical terrain and acceptance review. Existing failed evidence and archived reports remain immutable. Do not place production roads from these markers. Days 11–20 were not executed in this first-ten-day continuation; later prototype work can proceed only with an explicit provisional scope.

[Native artifact snapshot](../Data/M01_Native_Artifacts.csv) includes the map **and external actor packages**, scripts, settings and evidence. It is a provisional freeze, not an accepted historical base. [Archived visual summary](../Docs/Reports/Change-Archive/2026-09-26-canton-native-terrain-validation.svg).

## Four-axis completion audit — 2026-09-26

The revised plan's gates were checked against the actual first-ten-day artifacts. The technical prototype has a reproducible pass for its provisional scope. Historical M01 remains blocked, so Days 01–10 cannot be reported as historically complete.

| Axis | Decision | Evidence and next required input |
|---|---|---|
| `Technical_UE` | **Pass, provisional scope** | Native saved map, fresh reload, one UE automation test with zero failures/warnings, 256 Landscape and collision components, seven exact height checks, and encoding/coordinate tests. The original map package and source hashes are frozen in the native manifest. |
| `Historical_XY_City (H1)` | **Blocked** | Five OSM centroid fit candidates and three unsurveyed independent checks are below the ≥8 distributed holdout contract. Their 93.24 m RMSE and 132.155 m worst residual fail the 15 m / 30 m budget. A new survey-capable backbone and measured fit/holdout points are required. [H1 source comparison](../Data/H1_Source_Comparison.csv). |
| `Historical_XY_GateLocal (H2)` | **Blocked** | Wenmingmen is the primary named candidate and Zhengdongmen is fallback based on an [official approximate location account](https://www.gz.gov.cn/zlgz/whgz/content/post_8929405.html) and project asset availability. Neither has an independent local control network or accepted metric bounds. The current unnamed squares remain diagnostic. [Candidate register](../Data/Gate_District_Candidates.csv). |
| `Historical_Z` | **Blocked** | No dated 1880–1900 walking surface tied to an exact point, named benchmark and reconciled datum. `QING-STRATUM-001` remains a non-terrain candidate. |
| Day 01 `owner_approval` | **Pending** | The working budget is versioned; historical `Verified` remains prohibited until owner approval is recorded. |

Day 01's technical conventions and working budget, Day 02 source selection, Day 05 named-candidate selection, and Days 06–10 provisional engineering outputs are reviewable. Day 03 H1, Day 04 accepted GIS, Day 05 H2/Z, and the historical Day 10 exit remain open on missing primary evidence. No accepted H1 raster, root historical GIS layers, H2 bounds or historical heightmap was published.

Current run: `python3 Scripts/test_canton_source_manifest.py` passed with 56 registered artifacts and zero integrity errors; the heightmap and native import contracts each passed two tests; `python3 Scripts/test_canton_m01_readiness.py` checked 472 native artifacts and reported the four gate states above. [Measured-evidence request](../QA/Control_Survey_Handoff.md) identifies what an external survey/archive owner must return.

The final public-source pass confirmed that the [Guangzhou Municipal Archives describes the 1907 map as surveyed](https://www.gzdaj.gov.cn/gzjs/zdjm/content/post_138362.html), but the surfaced page does not provide the original scan, control grid or point uncertainty needed for H1. The [municipal historic-gate account](https://www.gz.gov.cn/zlgz/whgz/content/post_8929405.html) supports named candidate selection but gives approximate street locations rather than H2 control coordinates. The [archaeological notice](https://wglj.gz.gov.cn/xxgk/bmwj/ywxx/wwl/content/post_10742941.html) describes multi-period layers; the available report still lacks the required datum-backed late-Qing walking surface. These leads remain requests, not accepted measurements.
