# Day 01 — Project datum, scope and source register

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 01 resource sheet](Resources/Day-01-Resources.md)

## Goal

Complete the Day 01 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- Approved master-plan baseline and repository state.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Lock study period (1880–1900), walled-city polygon, local XY orientation, world origin convention, centimetre-to-metre conversion, target Unreal version and asset naming.
- Resolve the project's GUID-based Unreal association to an installed engine version. Audit required plugins and prove a minimal PCG smoke asset can load and cook; if PCG is unavailable, record ordinary foliage instances as the approved fallback.
- Create a source register: archive ID, observation date, locality, scan resolution, map scale if known, copyright/licence, redistribution status, retrieval location, SHA-256, location accuracy, vertical datum, measurement reliability, temporal relevance, modern-change risk, reconstruction method and historical confidence A–D.
- Define storage policy for original scans, large GeoTIFFs, derived rasters and UE assets, including Git LFS/external-artifact rules and reproducible tool versions.
- Declare georeference acceptance thresholds: independent check-point count and distribution, RMSE, maximum residual and a stricter gate-district tolerance tied to the narrowest geometry being aligned. Declare target hardware, build configuration, resolution/scalability, fixed performance route and frame-time/memory/streaming thresholds.
- Collect existing city-gate mesh dimensions and pivot conventions as *asset data*, not as surveyed historical ground elevations.
**Deliverables:** `Docs/Terrain_Baseline.md`; `Data/Source_Register.csv`; `Data/Coordinate_Contract.json`; `Data/Artifact_Manifest.csv`; `Docs/Terrain_Acceptance_Budget.md`; `QA/PCG_Preflight.md`.
**Done when:** Scope, coordinate and acceptance contracts are approved; the exact UE/plugin path is proven; storage/licence rules are recorded; all candidate sources carry provenance and separate evidence-quality fields.

## Required evidence

Retain engine identity, plugin smoke/cook output, storage decision, and approved numeric budgets. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 02 plan](Day-02-historical-wall-and-landmark-source-acquisition.md).

