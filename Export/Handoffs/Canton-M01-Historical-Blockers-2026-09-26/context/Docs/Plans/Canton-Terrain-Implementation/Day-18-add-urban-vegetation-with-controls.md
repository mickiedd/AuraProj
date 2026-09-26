# Day 18 — Add urban vegetation with controls

**Milestone:** M04  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 18 resource sheet](Resources/Day-18-Resources.md)

## Goal

Complete the Day 18 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 17 plan](Day-17-terrain-road-blending-and-material-qa.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Create grass tufts, weeds, wall-edge moss and sparse courtyard plants using foliage/PCG masks tied to surface class and evidence.
- Use PCG only if the Day 01 load/cook smoke passed; otherwise use the documented foliage-instance fallback without changing the placement contract.
- Keep dense managed garden vegetation out of ordinary street areas unless specifically documented.
- Set exclusion masks around paved roads, gate access, drain covers and building foundations.
**Deliverables:** `Foliage/PCG_Canton_UrbanGrowth` or `Foliage/Foliage_Canton_UrbanGrowth` according to the Day 01 decision; `Data/Foliage_SpawnRules.json`; `QA/Vegetation_Exclusions.md`.
**Done when:** Vegetation is sparse and place-specific; paths and gate openings remain unobstructed.

## Required evidence

Retain spawn implementation choice, masks, rule parameters, counts, and route-clearance proof. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 19 plan](Day-19-add-weathering-wetness-and-small-ground-details.md).

