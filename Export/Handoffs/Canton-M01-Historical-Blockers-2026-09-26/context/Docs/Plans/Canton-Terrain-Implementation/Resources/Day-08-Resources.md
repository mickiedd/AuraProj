# Day 08 resources — Resample, encode and audit heightmap

Use this sheet while executing [Day 08](../Day-08-resample-encode-and-audit-heightmap.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Accepted float32 elevation and confidence rasters
- [ ] 2017 x 2017 encoding contract
- [ ] GIS-to-UE coordinate round-trip fixtures
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`Export/Canton_1890_Height_2017_R16.png`; `Export/Heightmap_Metadata.json`; `Tools/Validate_Heightmap.py`; `QA/Heightmap_Encode_Report.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained bit depth, dimensions, minimum/maximum codes, clipping test, quantization error, and coordinate round trip
- [ ] Exact commands and arguments recorded
- [ ] Exit codes and log paths recorded
- [ ] Manual/visual checks identify fixed viewpoints, scale, and relevant settings
- [ ] Historical claims link to source-register rows
- [ ] Deviations and waived warnings have an owner and rationale

## Completion record

| Field | Value |
|---|---|
| Date / operator | 2026-09-26 / Codex |
| Git revision | `e3f8e2c9b74b5f3c782c3111857ae4d53ab37b7b`; pre-existing landmark edits preserved |
| Engine / tools | UE 5.5.4 CL 40574608 inspected; Python 3.9.6 with pinned terrain environment |
| Input revision(s) | Existing 46-artifact source manifest; provisional export metadata records source and coordinate hashes |
| Automated checks | See `Review/M01_Terrain_Review.md` for commands, results and limitations |
| Manual checks | Three native views and archived summary inspected; modern-context only |
| Output identifiers | `Data/M01_Provisional_Artifacts.csv` and `Data/M01_Native_Artifacts.csv` |
| Status | Provisional export and UE row-order audit passed |
| Reviewer | Codex technical review; historical/coordinate approval outstanding |
| Open issues / next owner | Survey/GIS owner: `QA/Control_Survey_Handoff.md`; UE owner: import/configuration evidence |


## Execution — 2026-09-26

Produced isolated 2017 × 2017 R16 PNG and metadata. Audited PNG IHDR, hashes, no-data, clipping, quantization, raster alignment and five analytic corner/centre frame checks. These checks do not substitute for surveyed landmarks or an editor orientation test.

**Recorded downstream exception (Codex):** the master plan’s DEM/datum fallback permits an isolated provisional heightfield. Days 06–08 use modern EGM2008 context with a tool-only 0 m reference. This does not change `Data/Coordinate_Contract.json`, supply historical Z, or waive production acceptance. Day 10 is a blocked-readiness review without Day 09 acceptance.

Output paths and SHA-256 are inventoried in [M01 provisional artifacts](../../../../Data/M01_Provisional_Artifacts.csv). All are project-authored tooling/review or CC BY 4.0 derivatives of ELEV-002; owner: AuraProj terrain. [Review and validation](../../../../Review/M01_Terrain_Review.md).

## Native continuation — 2026-09-26

The separately scoped modern-context prototype now has a saved WP Landscape, locked Base_Imported layer, diagnostic material, source-aligned coarse markers and passing native height/collision, fresh-reload and automation checks. Historical gates remain unchanged. The original execution record above describes the earlier state; this continuation supersedes its technical import status.

See [current M01 decision](../../../../Review/M01_Terrain_Review.md), [exact commands](../../../../QA/Canton_Days_01_10_Execution.md), [native artifact hashes](../../../../Data/M01_Native_Artifacts.csv) and [source follow-up](../../../../QA/Canton_Blocker_Research_2026_09_26.md). The prototype contract is an engineering decision for isolated testing, not surveyed coordinate or historical datum approval.
