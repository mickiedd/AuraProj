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

**Current baseline (2026-09-29):** All seven fresh navigation checks and all seven packaged physical pawn routes pass, including the endpoint about 0.6 m outside the closed door. Maximum nav-corner floor gap is 1.42 cm; current map checks have zero errors/warnings.

**Remaining focus:** Preserve the closed-door proxy and source bridge, grounded 2.67% outer approach, 300 cm nav tiles and 5/1 cm voxels. Review the explicit capsule/6 cm clearance criteria and retain H2/Z blockers. Re-run the packaged routes after geometry changes. Evidence: QA/Canton_Continuation/Runtime_traversal.json and QA/Canton_District/Traversal_Route_Matrix.json.

**Work**
- Walk all intended open prototype roads with the actual pawn, including the district-side threshold approach; fix mesh/landscape seams and foundation gaps. Keep the authored gate closed and test its obstruction separately.
- Check collision and navigation on road grades; compare the layout with the georeferenced map overlay.
- Record corrections that alter evidence-backed geometry separately from implementation fixes.
- Run an automated district traversal/collision/navigation smoke and a commandlet map check. Fix or explicitly waive every warning relevant to the prototype map.
**Deliverables:** `Review/M02_Roads_Review.md`; `QA/Road_Nav_Collision.md`; `Review/M02_Screenshots/`.
**Done when:** The 200 × 200 m prototype supports physical pawn traversal of the intended open network and resolved district-side threshold approach, meets approved nav-floor/capsule criteria, and has no unresolved implementation geometry blockers or unwaived relevant collision warnings. Historical road/gate alignment is reported separately and remains Provisional/Blocked wherever H2 or source evidence is unresolved.

## Required evidence

Retain walk route, collision/navigation results, map-check output, warnings/waivers, corrected seams, and the separate H2 historical-alignment status. Screenshots support the record but do not replace machine-readable metadata or automated checks.

## Stop/go rule

Stop and classify the day **Blocked** when a required source, datum, coordinate transform, plugin, map, or validation path is unavailable. Use **Provisional** only when the uncertainty is explicitly mapped and the downstream work can remain isolated from verified data. Do not silently substitute invented historical measurements.

## Handoff

Update the resource checklist with artifact paths, checksums, commands, exit codes, reviewer decision, and open issues. The accepted handoff feeds [Day 15 plan](Day-15-author-master-ground-materials.md).

# Provisional route-screen clarification (2026-09-28)

The 1.5× navigation detour and <8% two-metre grade figures are working engineering diagnostics in `Docs/Terrain_Acceptance_Budget.md`, pending owner approval. The Wenmingmen test asset has closed doors; a through-gate query is an obstruction diagnostic, while the passing open route currently ends at staging about 6 m north of its face. The attempted ~2 m near-threshold endpoint failed; actual threshold access remains open. No such screen replaces a physical pawn walk or closes Day 14 by itself. Gate collision-export warnings remain unresolved and unwaived.

## Implementation continuation — 2026-09-29

Use [the current continuation record](../../../QA/Canton_Continuation/README.md) and [current M05 handoff](../../../Review/M05_Final_Handoff.md) for the candidate implementation and validation. Earlier baseline descriptions below/above remain pre-continuation context where not replaced. The owner approved the M4/Metal raster target and closed gate. Historical H1/H2/Z and period-art source approval remain separate gates.
