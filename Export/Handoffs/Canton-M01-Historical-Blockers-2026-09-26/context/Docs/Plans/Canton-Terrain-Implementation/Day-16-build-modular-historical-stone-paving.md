# Day 16 — Build modular historical stone paving

**Milestone:** M03  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 16 resource sheet](Resources/Day-16-Resources.md)

## Goal

Complete the Day 16 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 15 plan](Day-15-author-master-ground-materials.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Create reusable stone-slab sets for primary roads, plus selected kerbs, steps, foundation strips and drainage edges.
- Use source-backed slab arrangement where documented; define pivots, snap grid, collision and Nanite usage by mesh.
- Test road-to-earth edge transitions, individual stone heights and slope changes at street intersections.
**Deliverables:** `Roads/SM_StoneSlab_*`; `Roads/BP_RoadSegment_Test`; `QA/StoneRoad_Seams.md`.
**Done when:** The test street is traversable with no floating stones, Z-fighting, visible grid seams or oversized European-style cobble pattern.

## Required evidence

Retain module dimensions, pivots, snap grid, collision/Nanite decisions, and seam captures. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 17 plan](Day-17-terrain-road-blending-and-material-qa.md).

