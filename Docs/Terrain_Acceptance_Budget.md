# Canton terrain acceptance budget — working version

| Field | Value |
|---|---|
| `working_acceptance_budget_versioned` | `true` |
| `working_budget_version` | `2026-09-28-h1-h2-district-v2` |
| `owner_approval` | `pending` |

This versioned budget may govern technical prototyping while owner approval is pending. No historical axis may be marked `Verified` until `owner_approval` is changed to `approved` with reviewer identity and date. Numeric targets remain review targets until that approval is recorded; they are not passed results.

## Independent status axes

| Axis | Scope | Pass condition |
|---|---|---|
| `Technical_UE` | Heightmap encoding, GIS↔UE transform implementation, map load, collision, navigation, cook and performance | Relevant automated and visual gates pass on the declared engine/hardware configuration |
| `Historical_XY_City (H1)` | City-scale historical metric frame | H1 control/holdout budget below passes |
| `Historical_XY_GateLocal (H2)` | Chosen named gate district and its local alignment | Independent local control budget below passes |
| `Historical_Z` | Late-Qing walking/ground surface | Dated ground surface is tied to an exact location, named benchmark and explicit reconciled vertical datum |

`Technical_UE = Pass` may coexist with any historical axis being `Provisional` or `Blocked`. Historical M01 is `Verified` only when H1, H2, and Historical_Z pass and `owner_approval` is `approved`.

## Numeric gates

| Gate | Working numeric target | Evidence required |
|---|---:|---|
| H1 city fit controls | At least 5 | Precisely defined persistent feature points with dated source, authority, CRS/datum, method and uncertainty |
| H1 city independent holdouts | At least 8; at least 2 in each quadrant; perimeter and interior represented | Controls and holdouts are disjoint; per-point horizontal vectors and residual CSV |
| H1 city holdout RMSE / worst residual | ≤ 15 m / ≤ 30 m | Reserved holdout results, transform version and distortion notes |
| H2 named gate district | At least 4 independent local checks | Local survey, conservation or archaeological geometry independent of the failed MAP-001 transform |
| H2 gate RMSE / worst residual | ≤ 2 m / ≤ 4 m | Tighten worst residual to ≤ half the narrowest source-measured alignment feature when that width is known |
| Landscape envelope | Measured wall plus ≥ 200 m working margin on every side | Projected polygon bounds and calculated buffer, no clipping |
| Historical height source | Every non-null Z has units, datum, date, exact location and reliability | Vertical datum register and source-linked constraints |

## Provisional district engineering screens (owner approval pending)

These versioned working numbers control **engineering smoke tests only**. They do not approve a historical road gradient, a gate opening, or Day 14 completion. The district is blocked from acceptance until the owner approves this budget and a pawn has traversed the intended open network.

| Screen | Working value | Interpretation |
|---|---:|---|
| Direct navigation detour ratio for an intended open route | ≤ 1.5 × straight-line distance | Diagnostic route screen in `ReviewCantonDistrictTraversal.py`. A closed gate is **not** an intended open route; its through-gate query is retained separately as an obstruction diagnostic. |
| Absolute 2 m road step grade | < 8% | Engineering comfort screen in `test_canton_district_contract.py`, calculated on the provisional R16; not a historical road-grade claim. |
| Mixed-lane refinement trigger | > 12% observed on the modern-context raster | Twenty western lane pieces were removed to keep the prototype walkable. This is a design intervention, not evidence of a period street boundary. |

The 1.5 and 8% figures were introduced during prototyping and were not in the earlier budget. This revision makes them explicit without backdating approval. No numeric screen substitutes for capsule clearance, nav-to-road height measurement, or the physical pawn walk.

Current failed-transform-derived 200 × 200 m UTM boxes are diagnostic only. Select the prototype district by named historical feature and source coverage, then derive final metric bounds after H2 passes. MAP-001 remains period layout/provenance evidence; its failed affine stays immutable and cannot establish H2 precision.

Proposed target hardware is this host's Mac mini (Apple M4, 10 CPU cores, 16 GB unified memory, Metal 4), observed with `system_profiler SPHardwareDataType SPDisplaysDataType`. Performance target is pending approval: Development or packaged build at 1920 × 1080, High scalability, fixed 200 m district route, 60 s warm-up then 180 s capture; p95 frame time ≤ 33.3 ms, p99 ≤ 50 ms, process resident memory ≤ 12 GiB, and zero failed World Partition streaming cells. Record engine build, route, draw calls and instance counts when this gate is run. No benchmark has been run.

The numeric georeference budget must be reviewed against the actual gate opening and map scan; reducing fitted-point error alone does not approve a transform. A near-period survey, including a 1907 source, must be tagged with its actual date and compared with in-period evidence before it supports 1880–1900 geometry.

`QING-STRATUM-001` retains a published Qing cultural-layer elevation range with an unspecified sea-level datum realization. Its `candidate_stratum_not_terrain` status excludes it from the height-source acceptance gate and all terrain interpolation. A usable terrain Z still requires a dated ground surface and a reconciled vertical datum. Modern DTM may support broad morphology only and remains D-confidence/provisional for historical Z.
