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

## Tracking

| Doc | Purpose |
|---|---|
| [GAS-Migration-TODOs.md](Tracking/GAS-Migration-TODOs.md) | Status tracker for the GAS → AuraAbilityGraph migration (living document). |
| [Gameplay-Blueprint-Decoupling-Next-Moves.md](Tracking/Gameplay-Blueprint-Decoupling-Next-Moves.md) | Ordered handoff of remaining decoupling steps. |

## Reports

| Doc | Purpose |
|---|---|
| [GAS-Migration-Audit-2026-08-03.md](Reports/GAS-Migration-Audit-2026-08-03.md) | Corrected audit of the TODO/pending-plan against the source tree. |
| [Gameplay-Blueprint-Decoupling-Migration-Report.md](Reports/Gameplay-Blueprint-Decoupling-Migration-Report.md) | Implementation report for the decoupling phases. |

## Reference

| Doc | Purpose |
|---|---|
| [GAS-Abilities-Documentation.md](Reference/GAS-Abilities-Documentation.md) | Implementation documentation for all data-driven abilities. |
