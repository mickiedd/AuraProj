# Day 10 — M01 source and terrain acceptance review

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 10 resource sheet](Resources/Day-10-Resources.md)

## Goal

Complete the Day 10 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 09 plan](Day-09-import-and-inspect-ue5-landscape.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Overlay traced walls and gates against imported terrain and independently check major control-point elevations. Run H1 city checks and, for the selected named gate district, H2 local checks separately.
- Mark historical-evidence gaps directly on the GIS confidence map and UE map; report `Technical_UE`, `Historical_XY_City`, `Historical_XY_GateLocal`, and `Historical_Z` separately as Verified / Provisional / Blocked.
- Freeze the versioned provisional/verified raster variants, source register and coordinate contracts; produce a short review packet and decision log. Do not freeze failed-transform-derived UTM district bounds as historical truth.
- Run `Scripts/test_canton_source_manifest.py`, `Scripts/test_canton_heightmap_contract.py` and the UE map-load/configuration automation test; record exact commands and outputs.
**Deliverables:** `Review/M01_Terrain_Review.md`; `Review/M01_Screenshots/`; `Data/M01_Uncertainty_Log.csv`.
**Done when:** `Technical_UE` may pass if import/scales/coords are valid and uncertainty is explicit. Historical M01 is `Verified` only when H1, H2 and Historical_Z each pass and Day 01 `owner_approval` is approved. Missing evidence leaves the corresponding axis Provisional/Blocked without invalidating the technical prototype.

## Required evidence

Retain exact test commands, exit codes, logs, screenshots, Day 01 approval state, four-axis decisions, and unresolved uncertainty. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 11 plan](Day-11-urban-road-and-parcel-layout-plan.md).
