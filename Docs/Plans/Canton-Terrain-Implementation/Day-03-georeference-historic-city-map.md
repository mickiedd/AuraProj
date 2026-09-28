# Day 03 — Georeference historic city map

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 03 resource sheet](Resources/Day-03-Resources.md)

## Goal

Complete the Day 03 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 02 plan](Day-02-historical-wall-and-landmark-source-acquisition.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Current baseline (2026-09-29):** The retained affine fails H1: three unsurveyed holdouts, 93.24 m RMSE / 132.155 m worst.

**Remaining focus:** Acquire measured controls and ≥8 distributed independent holdouts, build a new transform and compare period geometry. Preserve failed outputs; no production XY promotion until H1 passes. Owner: survey/GIS owner; CANTON-H1-001.

**Work**
- Choose a projected metric CRS suitable for Guangzhou; archive its EPSG code and coordinate transform to a local UE origin.
- Build **H1 (city historical frame)** from demonstrably persistent, precisely defined control points. Reserve spatially distributed points as independent checks and avoid forcing an archive map onto modern realigned streets. Use MAP-001 as a historical comparison layer unless its own metric quality is independently demonstrated.
- Prefer a survey-capable geometry backbone (including a clearly tagged near-period source if necessary) and compare it against in-period maps to detect temporal change. Test affine/polynomial/local transformations as appropriate; record per-point residuals, holdout residuals, distortion zones and rejected points with reasons. Do not select a higher-order transform merely because it reduces fitted-point error.
- H1 keeps the city acceptance budget. The strict gate-district budget belongs to H2 and is not waived merely because H1 passes.
**Deliverables:** `GIS/Canton_Historic_Georef_H1.tif`; `QA/Map_GCP_Residuals_H1.csv`; `Docs/Map_Distortion_Notes.md`; `Data/H1_Source_Comparison.csv`.
**Done when:** H1 opens at the documented coordinate origin and independent city checks meet the Day 01 city RMSE/maximum-error/spatial-coverage budget. Otherwise classify affected areas Provisional or Blocked rather than passing the transform.

## Required evidence

Retain the metric-backbone rationale, MAP-001 comparison result, fitted and independent H1 residuals, transform parameters, rejected-point reasons, and distortion map. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 04 plan](Day-04-digitize-the-walled-city-plan.md).
