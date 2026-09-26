# Canton terrain acceptance budget — proposal

**Not yet approved.** These values are review targets, not passed results.

| Gate | Proposed numeric target | Evidence required |
|---|---:|---|
| City map independent checks | At least 8, distributed across four quadrants and perimeter/interior | Separate controls and holdouts, dated persistent feature references |
| City map holdout RMSE / worst residual | ≤ 15 m / ≤ 30 m | Per-point horizontal vector and residual CSV |
| Selected 200 × 200 m gate district | At least 4 independent local checks; RMSE ≤ 2 m; worst ≤ 4 m | Tighten worst residual to ≤ half the narrowest source-measured alignment feature when that width is known |
| Landscape envelope | Measured wall plus ≥ 200 m working margin on every side | Projected polygon bounds and calculated buffer, no clipping |
| Height source | Every non-null Z has units, datum, date, location and reliability | Vertical datum register and source-linked constraints |

Proposed target hardware is this host's Mac mini (Apple M4, 10 CPU cores, 16 GB unified memory, Metal 4), observed with `system_profiler SPHardwareDataType SPDisplaysDataType`. Performance target is pending approval: Development or packaged build at 1920 × 1080, High scalability, fixed 200 m district route, 60 s warm-up then 180 s capture; p95 frame time ≤ 33.3 ms, p99 ≤ 50 ms, process resident memory ≤ 12 GiB, and zero failed World Partition streaming cells. Record engine build, route, draw calls and instance counts when this gate is run. No benchmark has been run.

The numeric georeference proposal must be reviewed against the actual gate opening and map scan; reducing fitted-point error alone does not approve a transform.

`QING-STRATUM-001` retains a published Qing cultural-layer elevation range with an unspecified sea-level datum realization. Its `candidate_stratum_not_terrain` status excludes it from the height-source acceptance gate and all terrain interpolation. A usable terrain Z still requires a dated ground surface and a reconciled vertical datum.
