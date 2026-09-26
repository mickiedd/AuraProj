# Day 12 — Street gradients and built-area grading

**Milestone:** M02  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 12 resource sheet](Resources/Day-12-Resources.md)

## Goal

Complete the Day 12 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 11 plan](Day-11-urban-road-and-parcel-layout-plan.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Confirm the locked `Base_Imported` layer created on Day 09; create independent Landscape Edit Layers for urban grading, road corridors and drainage.
- Draft road elevations/intersections and representative block platforms from constraints; avoid flattening all of the city.
- At the gate interface, preserve original architectural dimensions and separately record any local ground correction.
- Treat the 2 m Landscape vertex grid as broad grading only. Build narrow lanes, thresholds, kerbs, gutters and small drains with splines, meshes or patches at the resolution required by their evidence and collision needs.
**Deliverables:** `UE/EditLayers_Base_Urban_Roads`; `QA/Street_Grade_Profiles.csv`; `Docs/Gate_Ground_Datum.md`.
**Done when:** Representative roads and plots meet without visible terrain discontinuities; grade decisions are traceable or provisional.

## Required evidence

Retain edit-layer ownership, before/after grade profiles, gate datum, and feature-resolution decisions. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 13 plan](Day-13-drainage-and-low-area-blockout.md).

