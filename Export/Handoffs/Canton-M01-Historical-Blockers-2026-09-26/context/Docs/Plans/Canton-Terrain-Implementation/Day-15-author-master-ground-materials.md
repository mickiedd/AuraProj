# Day 15 — Author master ground materials

**Milestone:** M03  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 15 resource sheet](Resources/Day-15-Resources.md)

## Goal

Complete the Day 15 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 14 plan](Day-14-m02-geometry-review-and-blocker-cleanup.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Build a Landscape material with restrained layer count: natural soil, compacted earth, mixed pebble/earth, grass/weed soil, damp earth and optional exposed stone.
- Prepare physically based material instances with adjustable tint/roughness; treat monochrome photos as shape/placement evidence, not reliable colour sampling.
- Document scale, texel density, blend masks and texture licence/provenance.
**Deliverables:** `Terrain/M_Canton_Landscape`; `Terrain/MI_*`; `Data/Material_Evidence_Register.csv`.
**Done when:** The Landscape material compiles and the six planned surface families can be painted without obvious repetition at walking height.

## Required evidence

Retain shader compile result, layer cost, texture provenance, scale, and walking-height captures. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 16 plan](Day-16-build-modular-historical-stone-paving.md).

