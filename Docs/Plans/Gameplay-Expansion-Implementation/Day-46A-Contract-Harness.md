# Day 46A — Deterministic Contract Harness

Status: Implemented as an inert validation milestone; the full Day 46 role-kit plan remains Planned.
Depends on: Days 42–45 mission, encounter, attack-timeline, status, interrupt, and lease contracts.
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md), and [Day 46 role-kit plan](Day-46-Role-Kits-and-Combos.md).

## Milestone intent

Compose the existing authority seams into a deterministic, test-only event harness before adding role-kit runtime behavior. The harness accepts explicit run identities and server timestamps, drives real mission, attack, encounter-lease, status, and interrupt-ledger APIs, and emits a value-only snapshot suitable for replay comparison.

## Included

- `AuraGameplayDay46Harness` and `AuraGameplayDay46Tests` under the private test tree.
- Deterministic replay of one cross-contract encounter stream.
- Generation, source-life, and driver-owner barriers for stale callbacks.
- Heavy-lease replay, conflict, expiry, release, token, and invalidation convergence.
- Lowest-live-serial support ownership promotion and coverage updates.
- Attack replacement isolation after explicit cancelled-state reset.
- Mission terminal transition and repeated cleanup idempotence.
- A mixed stale-callback storm that must leave the replacement snapshot unchanged.
- Offline source/config checks in `Scripts/test_gameplay_day46_contracts.py`.

## Explicitly deferred

This milestone does not add Focus Shot, Shock Trap, Exposed selection/consumption, run supplies or medkits, emergency-terminal runtime use, role input bindings, GAS/damage/movement integration, AI spawning, profile activation, or packaged-play evidence. Those remain governed by the full Day 46 plan and its later dependencies.

The status ledger currently provides exact-key application/removal and channel isolation. Atomic Exposed selection/consumption and globally comparable application ordering remain follow-up contract work before live combo behavior.

## Validation gate

The offline suite must pass together with the existing Days 42–43 and Days 44–45 definition suites. UE5.5 UHT/UBT/native automation and packaged visual evidence remain environment/evidence gates; `DAY40_PACKAGED_EVIDENCE_MISSING` and `ANCHOR_SURVEY_UNVERIFIED` stay fail-closed.
