# Documentation Index

> Index of the migration and implementation docs for the data-driven GAS rewrite.
> Reorganized 2026-08-06 into `Plans/`, `Reports/`, `Tracking/`, and `Reference/`.

## Where to start

- **New to the system** → [Reference/GAS-Abilities-Documentation.md](Reference/GAS-Abilities-Documentation.md) describes the data-driven architecture and every implemented ability.
- **What's left to do** → [Tracking/GAS-Migration-TODOs.md](Tracking/GAS-Migration-TODOs.md) is the living status tracker.
- **About to implement a migration** → read [Plans/Gameplay-Blueprint-Decoupling-Migration-Plan.md](Plans/Gameplay-Blueprint-Decoupling-Migration-Plan.md) first; it lists its own prerequisite reading.

## Plans

| Doc | Purpose |
|---|---|
| [GAS-DataDriven-Rewrite-Plan.md](Plans/GAS-DataDriven-Rewrite-Plan.md) | Implementation plan & status for the data-driven ability rewrite (implemented). |
| [GAS-DataDriven-Rewrite-Plan-Pending.md](Plans/GAS-DataDriven-Rewrite-Plan-Pending.md) | Pending phases and known issues not yet started. |
| [Gameplay-Blueprint-Decoupling-Migration-Plan.md](Plans/Gameplay-Blueprint-Decoupling-Migration-Plan.md) | Plan to remove remaining Blueprint gameplay/definition UAssets (in progress). |
| [Buff-DataDriven-Migration-Plan.md](Plans/Buff-DataDriven-Migration-Plan.md) | Data-driven buff/pickup system via `GameplayEffects.json` (implemented). |

| [Role-Creation-and-Battle-System-Plan.md](Plans/Role-Creation-and-Battle-System-Plan.md) | Architecture plan for Aura Girl, BungeeMan, Civilian, combat, AI, and economy. |
| [Role-Battle-Implementation-Index.md](Plans/Role-Battle-Implementation-Index.md) | Twenty-day implementation sequence with one executable plan per day. |
| [Playable-Candidate-Implementation-Plan-2026-08-29.md](Plans/Playable-Candidate-Implementation-Plan-2026-08-29.md) | Next twenty logical days: turn the integrated slice into a player-ready local/LAN candidate with embedded support gates. |

## Tracking

| Doc | Purpose |
|---|---|
| [GAS-Migration-TODOs.md](Tracking/GAS-Migration-TODOs.md) | Status tracker for the GAS → AuraAbilityGraph migration (living document). |
| [Gameplay-Blueprint-Decoupling-Next-Moves.md](Tracking/Gameplay-Blueprint-Decoupling-Next-Moves.md) | Ordered handoff of remaining decoupling steps. |
| [Role-Battle-Issue-Dispositions.md](Tracking/Role-Battle-Issue-Dispositions.md) | Canonical disposition of the reviewed Role/Battle issue list and active follow-up risks. |

## Reports

| Doc | Purpose |
|---|---|
| [GAS-Migration-Audit-2026-08-03.md](Reports/GAS-Migration-Audit-2026-08-03.md) | Corrected audit of the TODO/pending-plan against the source tree. |
| [Gameplay-Blueprint-Decoupling-Migration-Report.md](Reports/Gameplay-Blueprint-Decoupling-Migration-Report.md) | Implementation report for the decoupling phases. |
| [Role-Battle-Vertical-Slice-2026-08-28.md](Reports/Role-Battle-Vertical-Slice-2026-08-28.md) | Day 20 local verification report and explicit production-release blocker. |
| [Playable-Candidate-Readiness-Review-2026-08-29.md](Reports/Playable-Candidate-Readiness-Review-2026-08-29.md) | Local analysis of the finished arc plus the independent in-app ChatGPT roadmap review. |
| [Playable-Candidate-Deep-Review-2026-08-29.md](Reports/Playable-Candidate-Deep-Review-2026-08-29.md) | Deep local review and correction of the Days 21–40 contracts, with the bounded handoff limitation recorded. |
| [DayPlan-Archive](Reports/DayPlan-Archive/README.md) | Detailed, manually authored mind maps for every finished Role/Battle day plan. |
| [Change-Archive](Reports/Change-Archive/README.md) | Calendar-day and job-level visual change records. |

## Reference

| Doc | Purpose |
|---|---|
| [GAS-Abilities-Documentation.md](Reference/GAS-Abilities-Documentation.md) | Implementation documentation for all data-driven abilities. |
| [Remote-Python-Editor-Automation.md](Reference/Remote-Python-Editor-Automation.md) | Runbook: drive the editor via UE Remote Execution (`Scripts/remote_run.py`) — add navmesh, spawn actors, recook for the server. |
| [Role-Battle-Economy-Schema.md](Reference/Role-Battle-Economy-Schema.md) | Version 1 identity, persistence, commerce, and staged-data schema contract. |
| [Role-Battle-Vertical-Slice-Test-Procedure.md](Reference/Role-Battle-Vertical-Slice-Test-Procedure.md) | Reproducible Listen/Dedicated, persistence, network, and release verification procedure. |
