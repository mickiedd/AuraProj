# Day 30 — Merchant Availability and Restock MVP

Status: Planned  
Depends on: Day 29 reward-to-merchant loop

## Goal

Make current merchant stock behavior deterministic across time, restart, and persistence without expanding into a business simulator.

## Work

- Freeze a fixed stock plus one bounded restock interval for the current merchant; availability windows are out of scope.
- Use server UTC as the authoritative time source, inject a deterministic clock for tests, clamp backwards clock movement to the last observed time, and cap offline catch-up to one restock interval at startup.
- Keep stock keyed by stable population member/merchant identity and committed with existing transaction revisions.
- Expose unavailable, restocking, and sold-out states through the merchant panel.

## Detailed execution contract

### Files to inspect or modify

- **Merchant authority:** `Source/Aura/Public/Economy/AuraMerchantComponent.h`, `AuraCommerceSubsystem.h`, `AuraEconomyRegistrySubsystem.h`, and implementations.
- **World identity/persistence:** `Source/Aura/Public/World/AuraPopulationTypes.h`, `AuraPopulationManager.h`, `AuraWorldSaveGame.h`, `AuraPersistenceSubsystem.h`, and the Day 18 manifest path.
- **Content/UI:** `Content/Config/MerchantDefinitions.json`, merchant widget/controller, and WebUI merchant state.
- **New planned output:** `Docs/Reference/Playable-Candidate-Merchant-Restock-Contract.md`, `Source/Aura/Private/Tests/AuraRoleBattleDay30Tests.cpp`, `RunPlayableCandidateDay30Restock.ps1`, and `day-30-restock.json`.

### Restock contract

Use one fixed stock definition and one configured `RestockInterval`. Server UTC is the source of truth; tests inject a deterministic clock. Persist `LastRestockAtUtc`, `StockRevision`, and stock entries by canonical `PopulationMemberId`. If the clock moves backward, clamp to the last observed time. On startup, offline elapsed time may produce at most one restock interval; no offline currency, reward, or player progression is generated.

### Detailed steps

1. Read the existing Day 17 stock/transaction schema and identify the stable merchant member and offer revision fields; do not create a second merchant identity.
2. Add fixed stock/restock fields and schema validation for positive interval, bounded quantities, stable offer IDs, and no duplicate stock entries.
3. Inject a server/test clock and implement elapsed-time evaluation with backward-clock clamp and one-interval offline catch-up.
4. Run restock only on the authority, serialize it with purchase transactions, increment `StockRevision` only after commit, and make repeated evaluation idempotent.
5. Persist/restore stock, last-restock timestamp, and revision under the existing world `WorldPersistenceId`; never derive stock from client time.
6. Expose available/restocking/sold-out/unavailable states through replicated merchant presentation and WebUI without allowing client state writes.
7. Test purchase at boundary, concurrent last-stock purchase, restart before/after interval, forced kill, backward clock, duplicate tick, wrong world ID, and late join.
8. Publish the exact offline policy to Day 31 and block any broad business-simulation addition.

### Named automation and commands

- Native tests: `RestockDefinition`, `InjectedClock`, `PurchaseBoundary`, `ConcurrentLastStock`, `RestartRestore`, `ForcedKillRollback`, `BackwardClockClamp`, `OneIntervalCap`, `WorldIsolation`, and `PresentationStates`.
- Run `RunPlayableCandidateDay30Restock.ps1 -Mode Listen` and `-Mode Dedicated`; retain deterministic clock inputs and server/world logs.
- The JSON report must prove no duplicate/rollback stock, no cross-world contamination, and explicit restock result codes for every transition.

## Validation and evidence

- Use a deterministic test clock or injected time source for purchase, restock, restart, and concurrent requests.
- Verify stock never duplicates, rolls back incorrectly, or crosses `WorldPersistenceId` boundaries.
- Record the chosen offline timer rule for Day 31 persistence.

## Deep-review closure

- **Owner surfaces:** `Source/Aura/Public/Economy/AuraMerchantComponent.h`, `AuraCommerceSubsystem.h`, `Content/Config/MerchantDefinitions.json`, and the world persistence record keyed by canonical `PopulationMemberId`.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Merchant-Restock-Contract.md` and `day-30-restock.json` containing stock revision, `LastRestockAtUtc`, injected-clock cases, restart behavior, and offline catch-up disposition.
- **Gate:** the deterministic clock proves no duplicate or rollback stock under purchase, concurrent requests, restart, and clock-backward cases; the persisted world record makes the same decision after restart; no offline progression beyond one interval occurs.

## Completion gate

The supported merchant has predictable availability/restock behavior and an explicit restart/offline rule; no ambiguous wall-clock behavior remains in the candidate contract.

## Defer

Do not add schedules, employees, multiple business types, or rich simulation.
