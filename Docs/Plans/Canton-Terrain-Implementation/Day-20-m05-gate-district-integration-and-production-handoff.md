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

**Work**
- Place one existing historical gate as an integration test; preserve architectural scale, record any nonhistorical asset fixes and align gate threshold to its modeled approach.
- Validate scale, collision, Lumen/Nanite compatibility as used, World Partition streaming and visual seams. Run the fixed Day 01 route on the declared target hardware/build/resolution/scalability after a defined warm-up; capture frame-time percentiles, memory, draw calls, instance counts and streaming failures against the declared thresholds.
- Run the focused Python contracts, UE automation/map check and a Development cook containing `L_Canton_WalledCity`; record exact commands, engine identity and output logs.
- Publish terrain data, source confidence, screenshots, known deviations, open blocker list and next-stage citywide replication rules.
**Deliverables:** `Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.
**Done when:** A test gate connects cleanly to a traversable 200 × 200 m district; source/heightmap/map-load/collision/navigation/cook checks pass; the fixed performance route meets its predeclared thresholds or is explicitly blocked; all full-city areas not yet built or validated remain explicitly out of scope.

## Required evidence

Retain engine/build identity, all commands and exit codes, cook log, fixed-route metrics, deviations, and open blockers. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds production handoff and the explicitly out-of-scope citywide expansion.

