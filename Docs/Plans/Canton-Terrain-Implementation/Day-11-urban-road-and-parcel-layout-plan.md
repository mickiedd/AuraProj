# Day 11 — Urban road and parcel-layout plan

**Milestone:** M02  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 11 resource sheet](Resources/Day-11-Resources.md)

## Goal

Complete the Day 11 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 10 plan](Day-10-m01-source-and-terrain-acceptance-review.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Import road centre lines and city-wall geometry as non-rendered reference overlays.
- Classify roads: principal stone-slab streets, smaller mixed-earth/pebble lanes, gate approaches and special courtyard areas; link classifications to photo/source evidence.
- Lay out a 200 × 200 m representative district with one gate interface and adjoining street/plots.
**Deliverables:** `GIS/Road_Classifications.geojson`; `Data/Road_Surface_Register.csv`; `Maps/District_Test_Bounds.json`.
**Done when:** All prototype roads have source/date and surface-class labels; uncertain assignments are tagged.

## Required evidence

Retain district bounds, road classifications, evidence links, and uncertain assignments. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 12 plan](Day-12-street-gradients-and-built-area-grading.md).

