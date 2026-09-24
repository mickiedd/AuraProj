# Canton terrain acceptance budget — proposal

**Not yet approved.** These values are review targets, not passed results.

| Gate | Proposed numeric target | Evidence required |
|---|---:|---|
| City map independent checks | At least 8, distributed across four quadrants and perimeter/interior | Separate controls and holdouts, dated persistent feature references |
| City map holdout RMSE / worst residual | ≤ 15 m / ≤ 30 m | Per-point horizontal vector and residual CSV |
| Selected 200 × 200 m gate district | At least 4 independent local checks; RMSE ≤ 2 m; worst ≤ 4 m | Tolerance is no more than half a proposed 8 m clear opening; replace with measured narrowest geometry if smaller |
| Landscape envelope | Measured wall plus ≥ 200 m working margin on every side | Projected polygon bounds and calculated buffer, no clipping |
| Height source | Every non-null Z has units, datum, date, location and reliability | Vertical datum register and source-linked constraints |

Performance target is likewise pending target hardware approval: Development or packaged build at 1920 × 1080, High scalability, fixed 200 m district route, 60 s warm-up then 180 s capture; p95 frame time ≤ 33.3 ms, p99 ≤ 50 ms, process resident memory ≤ 12 GiB, and zero failed World Partition streaming cells. Record GPU/CPU/RAM, engine build, route, draw calls and instance counts when this gate is run. No benchmark has been run.

The numeric georeference proposal must be reviewed against the actual gate opening and map scan; reducing fitted-point error alone does not approve a transform.
