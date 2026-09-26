# Day 04 — Digitize the walled-city plan

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 04 resource sheet](Resources/Day-04-Resources.md)

## Goal

Complete the Day 04 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 03 plan](Day-03-georeference-historic-city-map.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Trace wall centerline and gate points with source/date/confidence attributes.
- Digitize principal roads, historical water routes, important landform extents and major fixed landmarks into separate GIS layers.
- Calculate required bounding box and buffer; confirm that the proposed 4032 m square Landscape covers the actual wall polygon and needed working margin.
**Deliverables:** `GIS/Wall.geojson`; `GIS/Gates.geojson`; `GIS/Roads_Main.geojson`; `GIS/Waterways.geojson`; `QA/Extent_Check.md`.
**Done when:** Wall/gate layers align to the map and fit within the tested heightmap envelope; re-scope size if not.

## Required evidence

Retain layer extents, feature attribution, alignment captures, and envelope calculation. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 05 plan](Day-05-acquire-elevation-and-archaeological-constraints.md).

