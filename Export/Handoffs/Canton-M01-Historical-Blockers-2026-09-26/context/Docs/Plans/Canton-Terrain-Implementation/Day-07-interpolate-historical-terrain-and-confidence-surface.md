# Day 07 — Interpolate historical terrain and confidence surface

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 07 resource sheet](Resources/Day-07-Resources.md)

## Goal

Complete the Day 07 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 06 plan](Day-06-build-preliminary-historical-elevation-model.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Interpolate between control points while constraining hills, road-grade logic and documented water-flow directions.
- Separate inferred reconstruction from measured reference with a confidence/uncertainty raster and a feature-level log.
- Inspect for steps, sink artifacts, unrealistically steep urban blocks and residual modern building shapes.
**Deliverables:** `GIS/Historic_Elevation_float32.tif`; `GIS/Terrain_Confidence.tif`; `QA/Elevation_Checkpoints.csv`.
**Done when:** Terrain has no unexplained spikes or missing values; measured checkpoints fall within their source-specific uncertainty.

## Required evidence

Retain interpolation parameters, checkpoint errors, uncertainty raster, and artifact inspection. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 08 plan](Day-08-resample-encode-and-audit-heightmap.md).

