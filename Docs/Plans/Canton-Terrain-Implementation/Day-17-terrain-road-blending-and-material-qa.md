# Day 17 — Terrain/road blending and material QA

**Milestone:** M03  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 17 resource sheet](Resources/Day-17-Resources.md)

## Goal

Complete the Day 17 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 16 plan](Day-16-build-modular-historical-stone-paving.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Blend Landscape ground with modular paving using appropriate skirts, decals or masked transition meshes; avoid painting a fake sharp stone edge on soil.
- Compare dry and wet material variants under consistent lighting; audit roughness/normal intensity at eye level.
- Capture four representative scenes: main stone street, mixed lane, gate approach and courtyard edge.
**Deliverables:** `Roads/MI_Transition_*`; `Review/M03_Material_Atlas.png`; `Review/M03_Material_QA.md`.
**Done when:** Four scenes show distinct readable surfaces and convincing transitions at human scale, without unverified claims of exact historic soil colour.

## Required evidence

Retain fixed-lighting material atlas, dry/wet settings, normals/roughness review, and transition captures. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 18 plan](Day-18-add-urban-vegetation-with-controls.md).

