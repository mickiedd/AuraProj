# Day 06 — Build preliminary historical elevation model

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 06 resource sheet](Resources/Day-06-Resources.md)

## Goal

Complete the Day 06 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 05 plan](Day-05-acquire-elevation-and-archaeological-constraints.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Reconcile vertical datums before combining data; select a documented local terrain-zero reference.
- Use DEM for broad morphology only; tag modern cut/fill, roads, basements and reclamation as potential contamination.
- Construct north-hill / south-lowland conceptual grade and source-informed breaklines; do not invent specific nineteenth-century spot heights.
**Deliverables:** `GIS/Base_Elevation_float32.tif`; `GIS/Historic_Breaklines.geojson`; `QA/Modern_Change_Mask.tif`.
**Done when:** Prototype elevation model is spatially continuous, and every historical correction is traceable to evidence or flagged provisional.

## Required evidence

Retain datum conversion, correction provenance, change mask, and breakline parameters. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 07 plan](Day-07-interpolate-historical-terrain-and-confidence-surface.md).

