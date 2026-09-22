# Day 05 — Acquire elevation and archaeological constraints

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 05 resource sheet](Resources/Day-05-Resources.md)

## Goal

Complete the Day 05 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 04 plan](Day-04-digitize-the-walled-city-plan.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Locate the best available bare-earth DEM or surveyed contours and inspect age, spatial resolution, vertical datum, accuracy and evidence of modern earthworks.
- Collect source-backed archaeological site levels, historic contours, hill/terrace constraints, historic water references and gate-approach slope observations.
- Create a point/line elevation schema with measured value, range, vertical datum, reference, observation date, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D; do not fill missing values.
- Select the 200 × 200 m gate district now, based on source coverage, gate-asset readiness and achievable georeference accuracy. Record a ranked fallback district in case the preferred district fails the evidence gate.
**Deliverables:** `Sources/Elevation/`; `Data/Elevation_Constraints.csv`; `Docs/Vertical_Datum_Register.md`.
**Done when:** Every height value has units, origin and source; modern accuracy is not conflated with historical confidence; DEM coverage/uncertainty is documented; primary and fallback prototype districts are selected.

## Required evidence

Retain vertical-datum records, contamination notes, evidence-quality fields, and district-selection score. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 06 plan](Day-06-build-preliminary-historical-elevation-model.md).

