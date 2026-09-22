# Day 08 — Resample, encode and audit heightmap

**Milestone:** M01  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 08 resource sheet](Resources/Day-08-Resources.md)

## Goal

Complete the Day 08 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 07 plan](Day-07-interpolate-historical-terrain-and-confidence-surface.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Set world envelope, origin, orientation and grid-spacing metadata; resample elevation to a 2017 × 2017 grid for the proposed 4032 m coverage.
- Encode to single-channel 16-bit PNG with a fixed height-to-code mapping and matching UE Z scale; preserve float32 source raster.
- Test decoder round-trip, axis direction, extreme elevations, clipping and image bit depth using a small reproducible script.
- Add a coordinate round-trip test from projected GIS metres to local UE centimetres and back, using independent landmarks at the corners and within the gate district.
**Deliverables:** `Export/Canton_1890_Height_2017_R16.png`; `Export/Heightmap_Metadata.json`; `Tools/Validate_Heightmap.py`; `QA/Heightmap_Encode_Report.md`.
**Done when:** PNG is exactly 2017 × 2017, uint16, single-channel and reversible within expected quantization error; no elevations clip; the GIS↔UE coordinate round trip meets the Day 01 tolerance.

## Required evidence

Retain bit depth, dimensions, minimum/maximum codes, clipping test, quantization error, and coordinate round trip. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 09 plan](Day-09-import-and-inspect-ue5-landscape.md).

