# Day 20 — M05 gate-district integration and production handoff

**Milestone:** M05  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 20 resource sheet](Resources/Day-20-Resources.md)

## Goal

Complete the Day 20 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 19 plan](Day-19-add-weathering-wetness-and-small-ground-details.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Current baseline (2026-09-29):** Targeted two-map cook and provisional handoff exist; Development performance, WP streaming and pawn evidence are absent.

**Remaining focus:** Build a runnable Development candidate with the declared pawn/route; run Lumen and agreed Nanite/fallback compatibility checks, capture runtime metrics, and validate the intended delivery cook configuration. Re-freeze both maps/external actors and all evidence after final changes. Editor proxy timings and the temporary cook exclusion cannot close runtime or normal-project packaging. Owners: build/performance lead / reviewer; CANTON-M05-002, CANTON-M05-003, CANTON-M05-005.

**Work**
- Reuse the existing Wenmingmen integration; preserve architectural scale and closed-door intent, record nonhistorical asset fixes, and validate the resolved district-side threshold approach. A through-gate route requires a separately approved design change.
- Validate scale, collision, Lumen/Nanite compatibility as used, World Partition streaming and visual seams. Run the fixed Day 01 route on the declared target hardware/build/resolution/scalability after a defined warm-up; capture frame-time percentiles, memory, draw calls, instance counts and streaming failures against the declared thresholds.
- Run the focused Python contracts, UE automation/map check and a Development cook of both current Canton maps (or their explicitly versioned successors). Record exact arguments and cook configuration. Validate the intended delivery configuration; retain any scoped cook exclusion as a limitation and do not claim normal full-project packaging from it.
- Publish terrain data, source confidence, screenshots, known deviations, open blocker list and next-stage citywide replication rules.
**Deliverables:** `Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.
**Done when:** The closed test gate has a physically accessible district-side threshold connected to the traversable 200 × 200 m open district; source/heightmap/map-load/collision/navigation/cook checks pass; the fixed Development performance route meets its approved thresholds and runtime WP checks pass; the handoff reports `Technical_UE`, H1, H2 and Historical_Z separately. If runtime acceptance fails or is missing, Day 20 remains blocked and only a provisional handoff may be delivered. A technically complete district may remain historically Provisional/Blocked; all full-city areas not yet built or validated remain explicitly out of scope.

## Required evidence

Retain engine/build identity, all commands and exit codes, cook log, fixed-route metrics, four-axis status, deviations, and open blockers. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds production handoff and the explicitly out-of-scope citywide expansion.
