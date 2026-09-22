# Day 02 — Historical wall and landmark source acquisition

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 02 resource sheet](Resources/Day-02-Resources.md)

## Goal

Complete the Day 02 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 01 plan](Day-01-project-datum-scope-and-source-register.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Locate high-resolution maps dated within 1880–1900; separately register nearby-date maps as secondary references.
- Record city-wall perimeter, gate locations, Zhenhai Tower and surviving street junctions with source pointers.
- Mark unverified gate names/positions as provisional and record disagreements between maps.
**Deliverables:** `Sources/HistoricMaps/`; `Data/Historic_Map_Selection.md`; `Data/Landmark_Candidates.csv`.
**Done when:** At least one traceable historical map is selected; gaps and date mismatches are explicitly logged.

## Required evidence

Retain archive identifiers, dates, licences, scan metadata, and conflicting source notes. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 03 plan](Day-03-georeference-historic-city-map.md).

