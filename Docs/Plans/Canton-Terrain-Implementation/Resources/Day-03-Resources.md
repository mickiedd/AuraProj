# Day 03 resources — Georeference historic city map

Use this sheet while executing [Day 03](../Day-03-georeference-historic-city-map.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Selected scan at original resolution
- [ ] Projected CRS definition and local-origin transform
- [ ] Control/check-point split and residual budget
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`GIS/Canton_Historic_Georef.tif`; `QA/Map_GCP_Residuals.csv`; `Docs/Map_Distortion_Notes.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained fitted and independent residuals, transform parameters, rejected-point reasons, and distortion map
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
| Output identifiers | EPSG:32649 proposed; empty residual schema and distortion/status record |
| Status | Blocked |
| Reviewer | Pending |
| Open issues / next owner | No surveyed persistent controls or independent holdouts; no georeferenced TIFF. |


## 2026-09-24 execution note

Status: **Blocked**. No surveyed persistent controls or independent holdouts; no georeferenced TIFF. Read-only source/tool inspection and the validator are recorded in `QA/Canton_Days_01_05_Execution.md`. Historical outputs remain gated.
