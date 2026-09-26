# Day 04 resources — Digitize the walled-city plan

Use this sheet while executing [Day 04](../Day-04-digitize-the-walled-city-plan.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Accepted georeferenced raster
- [ ] Wall/gate/road/waterway layer schemas
- [ ] Envelope and buffer calculation worksheet
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`GIS/Wall.geojson`; `GIS/Gates.geojson`; `GIS/Roads_Main.geojson`; `GIS/Waterways.geojson`; `QA/Extent_Check.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained layer extents, feature attribution, alignment captures, and envelope calculation
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
| Output identifiers | GIS attribution work area and extent gate record prepared |
| Status | Blocked |
| Reviewer | Pending |
| Open issues / next owner | No accepted georeference; no geometry or envelope measurement. |


## 2026-09-24 execution note

Status: **Blocked**. No accepted georeference; no geometry or envelope measurement. Read-only source/tool inspection and the validator are recorded in `QA/Canton_Days_01_05_Execution.md`. Historical outputs remain gated.

## 2026-09-24 continuation (supersedes the status above)

**Provisional work completed; acceptance blocked.** `Scripts/build_canton_provisional_layers.py` builds six isolated GeoJSON layers: two wall lines, four gate points, two main roads, two waterways, one landform and eight landmark markers. Inputs are preserved as map-pixel traces in `Data/Map_Pixel_Traces_Provisional.json`; the inspected overlay is `QA/Canton_Provisional_Trace_Overlay.jpg`. `Data/Extent_Provisional.json` and `QA/Extent_Check.md` show a 4032 m square fits the coarse wall with ≥200 m margin on every side. Root accepted GIS layers remain absent because the map transform fails Day 03.

## Native continuation — 2026-09-26

The separately scoped modern-context prototype now has a saved WP Landscape, locked Base_Imported layer, diagnostic material, source-aligned coarse markers and passing native height/collision, fresh-reload and automation checks. Historical gates remain unchanged. The original execution record above describes the earlier state; this continuation supersedes its technical import status.

See [current M01 decision](../../../../Review/M01_Terrain_Review.md), [exact commands](../../../../QA/Canton_Days_01_10_Execution.md), [native artifact hashes](../../../../Data/M01_Native_Artifacts.csv) and [source follow-up](../../../../QA/Canton_Blocker_Research_2026_09_26.md). The prototype contract is an engineering decision for isolated testing, not surveyed coordinate or historical datum approval.
