# Day 13 — Drainage and low-area blockout

**Milestone:** M02  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 13 resource sheet](Resources/Day-13-Resources.md)

## Goal

Complete the Day 13 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 12 plan](Day-12-street-gradients-and-built-area-grading.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Place historically mapped drains, canals or low areas as reference splines; do not fabricate open channels where maps/photos offer no support.
- Model test ditch and puddle depressions only where the ground context calls for them.
- Check that road grading does not introduce dead-end depressions or water running uphill.
**Deliverables:** `GIS/Drainage_Locations.geojson`; `UE/Drainage_Blockout`; `QA/Drainage_Flow_Notes.md`.
**Done when:** Prototype drainage has plausible downhill flow; documented and invented demo features are clearly separated.

## Required evidence

Retain flow directions, low-point checks, documented/demo labels, and drainage captures. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 14 plan](Day-14-m02-geometry-review-and-blocker-cleanup.md).

