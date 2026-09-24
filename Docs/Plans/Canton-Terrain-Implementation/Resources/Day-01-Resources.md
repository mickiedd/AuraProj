# Day 01 resources — Project datum, scope and source register

Use this sheet while executing [Day 01](../Day-01-project-datum-scope-and-source-register.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [ ] Installed Unreal build and resolved EngineAssociation
- [ ] Plugin inventory and minimal PCG load/cook fixture
- [ ] Source-register, artifact-manifest, coordinate-contract and acceptance-budget schemas
- [ ] Prior-day accepted artifacts or a recorded exception
- [ ] Cleanly identified repository revision and unrelated working-tree changes preserved

## Expected artifacts

`Docs/Terrain_Baseline.md`; `Data/Source_Register.csv`; `Data/Coordinate_Contract.json`; `Data/Artifact_Manifest.csv`; `Docs/Terrain_Acceptance_Budget.md`; `QA/PCG_Preflight.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [ ] Retained engine identity, plugin smoke/cook output, storage decision, and approved numeric budgets
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
| Input revision(s) | Repository HEAD c840ce2; pre-existing unrelated dirty files preserved |
| Automated checks | python3 Scripts/validate_canton_terrain_days_01_05.py — exit 0; inventory consistent, historical gates still blocked |
| Manual checks | Source documents and project manifest reviewed; no historical geometry/height approved |
| Output identifiers | Terrain baseline, coordinate contract, source/artifact registers, acceptance proposal and PCG preflight written |
| Status | Blocked |
| Reviewer | Pending |
| Open issues / next owner | Projected and vertical origins and budget approval missing; PCG cook not proven. Day 02 source research proceeds as a non-metric exception. |


## 2026-09-24 execution note

Status: **Blocked**. Projected and vertical origins and budget approval missing; PCG cook not proven. Day 02 source research proceeds as a non-metric exception. Read-only source/tool inspection and the validator are recorded in `QA/Canton_Days_01_05_Execution.md`. Historical outputs remain gated.
