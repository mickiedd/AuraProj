# Day 02 resources — Historical wall and landmark source acquisition

Use this sheet while executing [Day 02](../Day-02-historical-wall-and-landmark-source-acquisition.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Archive catalog access and licence terms
- [ ] Day 01 source-register schema
- [ ] Persistent-landmark candidate worksheet
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`Sources/HistoricMaps/`; `Data/Historic_Map_Selection.md`; `Data/Landmark_Candidates.csv`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained archive identifiers, dates, licences, scan metadata, and conflicting source notes
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
| Manual checks | Map image visually opened at original 7017x4384; labels/wall visible |
| Output identifiers | Original 7017x4384 map copy, checksum, selection record and four image-only landmark candidates |
| Status | Provisional |
| Reviewer | Pending |
| Open issues / next owner | 1880/1890 edition discrepancy and scale unresolved. Day 03 may prepare schemas only; no metric transform. |


## 2026-09-24 execution note

Status: **Provisional**. 1880/1890 edition discrepancy and scale unresolved. Day 03 may prepare schemas only; no metric transform. Read-only source/tool inspection and the validator are recorded in `QA/Canton_Days_01_05_Execution.md`. Historical outputs remain gated.

## 2026-09-24 continuation (supersedes the status above)

**Provisional.** The British Library identifies `MAP-001` as page 41 of the 1880 second edition of *The Canton Guide* (BL `001954731`), resolving the Commons filename conflict. The map's actual survey date and scale remain unverified. Eight icon-to-OSM landmark matches are registered as candidate controls in `Data/Map_Control_Candidates.csv`; none is a surveyed mark. Source SHA and holding-library link are in `Data/Source_Register.csv`.

## 2026-09-24 source-selection closeout

**Verified for Day 02 source acquisition only.** The plan's done condition is a traceable map with gaps and date mismatches logged. `MAP-001` meets it: holding-library ID, publication date, original-resolution scan, source URL, rights statement and SHA are recorded. `QA/Map_Scale_Inspection.md` shows the printed scale bar has no legible units; actual survey date and numeric scale remain explicit exceptions. The wall, gates and landmarks have source-linked provisional pixel traces. This decision does **not** approve metric georeferencing; Day 03 remains blocked by independent residuals and unsurveyed candidate controls.
