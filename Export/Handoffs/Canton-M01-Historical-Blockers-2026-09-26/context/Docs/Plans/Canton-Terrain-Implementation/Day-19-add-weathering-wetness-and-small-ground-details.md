# Day 19 — Add weathering, wetness and small ground details

**Milestone:** M04  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 19 resource sheet](Resources/Day-19-Resources.md)

## Goal

Complete the Day 19 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 18 plan](Day-18-add-urban-vegetation-with-controls.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Distribute local leaf litter, grit, small stone debris, puddles and dampness through PCG or ordinary decals/instances according to the Day 01 decision, rather than noisy full-map height edits.
- Test dry and after-rain scenarios; validate the wet ground placement against drainage and material boundaries.
- Capture representative before/after screenshots and a draw-call/instance-count snapshot.
**Deliverables:** `GroundDetails/PCG_Canton_Debris` or `GroundDetails/Canton_Debris_Instances` according to the Day 01 decision; `Materials/MI_Wetness_*`; `Review/M04_Weathering_QA.md`.
**Done when:** Localized details enhance readability without clutter or implausible uniform mud/wetness; no material/foliage collision issues.

## Required evidence

Retain before/after captures, placement masks, draw calls, instance counts, and collision result. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 20 plan](Day-20-m05-gate-district-integration-and-production-handoff.md).

