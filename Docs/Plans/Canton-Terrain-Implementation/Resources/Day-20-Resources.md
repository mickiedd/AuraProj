# Day 20 resources — M05 gate-district integration and production handoff

Use this sheet while executing [Day 20](../Day-20-m05-gate-district-integration-and-production-handoff.md). Fill in actual identifiers and paths; bracketed values are not evidence.

## Required inputs

- [x] M01 and M02–M04 provisional candidates plus `Data/Open_Issues.csv`; historical acceptance exceptions are in `QA/Canton_Days_11_20_Execution.md`.
- [x] Apple M4 Mac mini, 16 GiB; 1920 × 1080 High, 200 m fixed route, 60 s warm-up and 180 s sample are declared in `Docs/Terrain_Acceptance_Budget.md`. Owner approval and Development metrics remain pending.
- [x] Automation, map check, fresh-load validation, navigation and cook command/log are indexed in `Review/M05_Final_Handoff.md`.
- [x] The M01 map is a validated provisional technical baseline, not accepted historical terrain; each downstream day is scoped in the execution ledger.
- [x] Working baseline `14041340a781dc8bbb417c6f36a9dd922b56a023`; unrelated Zhengdongmen/Xiaobeimen working-tree changes preserved.

## Expected artifacts

`Review/M05_Final_Handoff.md`; `Review/M05_QA_Screenshots/`; `Data/Open_Issues.csv`; `Docs/Citywide_Terrain_Replication.md`.

For every file or asset above, record owner, repository/artifact path, SHA-256 or UE package identifier, source inputs, generator/tool version, licence status, and whether it is **Verified**, **Provisional**, or **Blocked**.

## Evidence checklist

- [x] Engine/build identity, four-axis status, deviations and open blockers are in `Review/M05_Final_Handoff.md`; cook and Development performance outcomes are individually reported there.
- [x] Exact contract, build and cook commands are recorded in the handoff; UE capture scripts are in the package manifest.
- [x] Check results and log paths are recorded; Development performance is explicitly blocked until its required result exists.
- [x] Fixed camera settings are in `QA/Canton_District/Capture_Settings.json`; the gate is scale-only.
- [x] Original M01 source register and district D-confidence register keep source claims separate from the blockout.
- [x] Existing gate collision-complexity warning and final-art gaps have owners in `Data/Open_Issues.csv`.

## Completion record

| Field | Value |
|---|---|
| Date / operator | 2026-09-27 / Codex |
| Git revision | `14041340a781dc8bbb417c6f36a9dd922b56a023` working baseline; uncommitted district outputs |
| Engine / tools | UE 5.5.4 CL 40574608, Python 3.9.6 terrain environment, Apple M4 Mac mini |
| Input revision(s) | M01 provisional R16; `Data/Artifact_Manifest.csv`; district source-neutral D-confidence registers |
| Automated checks | Contract, editor build, fresh map load, map check, collision and navigation pass; cook result in M05 handoff |
| Manual checks | Four fixed material views, dry/damp pair and visual archive |
| Output identifiers | `/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL`; `Data/M02_M05_District_Artifacts.csv` |
| Status | Provisional technical district; historical H1/H2/Z blocked, owner approval pending, Development performance gate blocked |
| Reviewer | Codex technical review; historical and owner acceptance pending |
| Open issues / next owner | `Data/Open_Issues.csv`; survey, archaeology, environment art, performance and project owner |
