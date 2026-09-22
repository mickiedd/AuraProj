# Day 09 — Import and inspect UE5 Landscape

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 09 resource sheet](Resources/Day-09-Resources.md)

## Goal

Complete the Day 09 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 08 plan](Day-08-resample-encode-and-audit-heightmap.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Create a dedicated World Partition map; enable Landscape Edit Layers during creation and import the heightmap into a named `Base_Imported` layer at 200/200/50 XY/Z scale only if the metadata confirms that encoding. Lock the base layer immediately.
- Use 63 quads per section, 2 × 2 sections/component and 16 × 16 components; center/position the Landscape using the documented local-origin transform.
- Record World Partition grid/region sizes, `Flip Y Axis`, Landscape actor transform and all import settings. Apply a neutral diagnostic material; place wall and gate markers and inspect orientation, scaling, collision and northern slope direction.
**Deliverables:** `Content/Canton/Maps/L_Canton_WalledCity.umap`; `Content/Canton/Terrain/MI_Diagnostic`; `QA/UE_Import_Screenshots/`.
**Done when:** UE Landscape imports without distortion; `Base_Imported` exists and is locked; import/WP settings are captured; an automated map-load/configuration check passes; landmark overlay faces the intended direction and sits in the correct XY frame.

## Required evidence

Retain all import settings, edit-layer lock state, WP settings, actor transform, collision, and orientation captures. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 10 plan](Day-10-m01-source-and-terrain-acceptance-review.md).

