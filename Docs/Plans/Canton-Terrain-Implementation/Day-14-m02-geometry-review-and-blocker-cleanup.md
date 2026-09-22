# Day 14 — M02 geometry review and blocker cleanup

**Milestone:** M02  
**Master plan:** [Canton 1880–1900 UE5 Terrain 20-Day Plan](../Canton_1880_1900_UE5_Terrain_20_Day_Plan.md)  
**Resources:** [Day 14 resource sheet](Resources/Day-14-Resources.md)

## Goal

Complete the Day 14 scope below as an independently reviewable increment. Preserve source provenance and never promote a provisional reconstruction to verified historical evidence.

## Entry gate

- [Day 13 plan](Day-13-drainage-and-low-area-blockout.md) and its accepted deliverables.
- Record git status, git revision, the resolved Unreal Engine version, operator/tool versions, and the working data revision before changing outputs.
- Confirm every required input in the resource sheet exists, is licensed for the intended use, and carries a checksum or stable repository identifier.

**Work**
- Walk all prototype roads from gate to district interior; fix mesh/landscape seams and foundation gaps.
- Check collision and navigation on road grades; compare the layout with the georeferenced map overlay.
- Record corrections that alter evidence-backed geometry separately from implementation fixes.
- Run an automated district traversal/collision/navigation smoke and a commandlet map check. Fix or explicitly waive every warning relevant to the prototype map.
**Deliverables:** `Review/M02_Roads_Review.md`; `QA/Road_Nav_Collision.md`; `Review/M02_Screenshots/`.
**Done when:** 200 × 200 m prototype supports continuous traversal and its principal road/gate alignment has no unresolved geometry blockers.

## Required evidence

Retain walk route, collision/navigation results, map-check output, warnings/waivers, and corrected seams. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 15 plan](Day-15-author-master-ground-materials.md).

