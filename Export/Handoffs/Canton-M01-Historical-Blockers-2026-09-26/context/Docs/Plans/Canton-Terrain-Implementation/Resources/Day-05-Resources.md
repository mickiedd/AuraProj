# Day 05 resources — Acquire elevation and archaeological constraints

Use this sheet while executing [Day 05](../Day-05-acquire-elevation-and-archaeological-constraints.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Bare-earth DEM/contours and archaeological references
- [ ] Evidence-quality schema from Day 01
- [ ] Primary/fallback gate-district scorecard
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`Sources/Elevation/`; `Data/Elevation_Constraints.csv`; `Docs/Vertical_Datum_Register.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained vertical-datum records, contamination notes, evidence-quality fields, and district-selection score
- [ ] Exact commands and arguments recorded
- [ ] Exit codes and log paths recorded
- [ ] Manual/visual checks identify fixed viewpoints, scale, and relevant settings
- [ ] Historical claims link to source-register rows
- [ ] Deviations and waived warnings have an owner and rationale

## Completion record

| Field | Value |
|---|---|
| Date / operator | 2026-09-24 / Codex |
| Git revision | c840ce2a22993c33491a5d0fa1c74add125cf3ce |
| Engine / tools | UE 5.5.4 (CL 40574608); Python 3.9.6; validator stdlib |
| Input revision(s) | MAP-001 scan SHA-256 ebab90db8e25cd87fea7ac4e5e6d3d2e45e4659fb35bff6b509aba6f62406bd7 |
| Automated checks | python3 Scripts/validate_canton_terrain_days_01_05.py — exit 0; inventory consistent, historical gates still blocked |
| Manual checks | Source documents and project manifest reviewed; no historical geometry/height approved |
| Output identifiers | Elevation constraint schema, source candidates and vertical datum register prepared |
| Status | Blocked |
| Reviewer | Pending |
| Open issues / next owner | No accepted bare-earth/surveyed source, datum-backed height, or district coordinate. Modern GLO-30 is DSM only. |


## 2026-09-24 execution note

Status: **Blocked**. No accepted bare-earth/surveyed source, datum-backed height, or district coordinate. Modern GLO-30 is DSM only. Read-only source/tool inspection and the validator are recorded in `QA/Canton_Days_01_05_Execution.md`. Historical outputs remain gated.

## 2026-09-24 continuation (supersedes the status above)

**Modern source and one stratigraphic candidate acquired; historical Z blocked.** `ELEV-002/003` are 720 × 520 GEDTM30 v1.1 bare-earth estimate and uncertainty crops (EGM2008 / EPSG:3855, CC BY 4.0); source URLs and SHAs are registered. The official 177-page Guangta/Chashu excavation report (`ARCH-003`) gave relative depths only. A 2021 primary stratigraphy paper (`ARCH-004`) reports a Qing layer at 5.92–6.22 m sea-level elevation, recorded as `QING-STRATUM-001` with blank terrain height and an unreconciled datum. A south-wall 200 × 200 m working district and east-gate fallback are in `Docs/Vertical_Datum_Register.md`, both D confidence and awaiting local surveyed checks and usable historical Z.

## 2026-09-24 datum follow-up

`DATUM-001` is the official Guangzhou-to-1985 National Height Datum relation, **H₁₉₈₅ = H_Guangzhou − 4.256 m**. It cannot yet be applied to `ARCH-004`: the paper does not identify which height system its “sea-level elevation” values use. `Docs/Vertical_Datum_Register.md` records this conditional conversion and the separate EGM2008 model datum. No historical terrain Z was promoted.

## Native continuation — 2026-09-26

The separately scoped modern-context prototype now has a saved WP Landscape, locked Base_Imported layer, diagnostic material, source-aligned coarse markers and passing native height/collision, fresh-reload and automation checks. Historical gates remain unchanged. The original execution record above describes the earlier state; this continuation supersedes its technical import status.

See [current M01 decision](../../../../Review/M01_Terrain_Review.md), [exact commands](../../../../QA/Canton_Days_01_10_Execution.md), [native artifact hashes](../../../../Data/M01_Native_Artifacts.csv) and [source follow-up](../../../../QA/Canton_Blocker_Research_2026_09_26.md). The prototype contract is an engineering decision for isolated testing, not surveyed coordinate or historical datum approval.
