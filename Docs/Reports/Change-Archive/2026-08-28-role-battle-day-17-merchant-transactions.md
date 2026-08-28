# Role/Battle Day 17 - Merchant Transactions

Date: 2026-08-28

## Intent

Add a replay-safe, server-authoritative merchant purchase loop bound to the specific Civilian population member the player is interacting with. A valid request resolves immutable server offer data, validates the requester and merchant state, and atomically updates stock, wallet, and inventory while returning the result only to the requesting owner.

## Changed behavior

- `UAuraMerchantComponent` is a replicated, stable Civilian component keyed by the population member ID and resolved merchant definition. Its replicated presentation contains immutable offer identity plus current stock and revision; ordinary Civilian roles do not become merchants implicitly.
- `UAuraCommerceSubsystem` is authority-only and owns merchant registration, stock revisions, session nonce rotation, offer resolution, validation, replay caching, and purchase mutation. Client payloads cannot choose price, item, quantity, eligibility, or stock outcome.
- The purchase endpoint lives on the owning player's `UAuraInteractionComponent`. Requests contain only the session nonce, monotonic request ID, merchant actor reference, and offer ID. The owner receives a typed result; a bounded 256-entry cache returns exact replays without reapplying mutations and rejects stale or gapped requests.
- Currency debit, inventory grant, and finite-stock decrement use silent validated mutations followed by publication only after all changes succeed. Before-images restore all three state domains on failure, and restoration failure is surfaced as `RollbackFailed` rather than hidden as an ordinary business rejection.
- Merchant availability closes immediately when the Civilian leaves the Alive state. The native merchant widget/controller provides a BP-ready compatibility surface and routes purchases through the owned interaction component; the gameplay HUD remains WebUI-first per the project UI rule.

## Validation

- `build_test.bat`: AuraEditor Win64 Development passed after the final life-state, range, rollback, authority-guard, and contention-probe hardening.
- All repository Python contracts: 12/12 passed.
- Focused native automation: 13/13 Day 17 commerce tests passed.
- Full `Aura` native automation: 183/183 tests passed, 0 failed.
- `RunRoleBattleDay17NetworkSmoke.ps1 -Mode Listen`: passed with two-client exactly-once contention, owner result correlation, replay/privacy checks, merchant-death UI closure, crash scan, and bounded teardown.
- `RunRoleBattleDay17NetworkSmoke.ps1 -Mode Dedicated`: passed with the same assertions.
- `git diff --check`: passed; only existing line-ending warnings were reported.

The network runner force-terminates only the processes it creates after bounded assertions, so its child-process exit fields are `-1` by design; the runner itself returned success and all report assertions were true. The existing `StartupMap.umap` is used as the live fixture because it already contains the configured population; no fake binary map or `.uasset` placeholders were added. The native widget/controller and JSON snapshots preserve the project’s WebUI-first presentation contract.

## Illustration

[Day 17 replay-safe merchant transaction flow](2026-08-28-role-battle-day-17-merchant-transactions.svg)

## Evidence

- Focused log: `Saved/Logs/Day17Automation-Final.log`
- Full-suite log: `Saved/Logs/AuraFullAutomation-Day17-Final.log`
- Listen report: `Saved/Reports/Day17-Listen.json`
- Dedicated report: `Saved/Reports/Day17-Dedicated.json`

## Completion gate

The merchant purchase business loop is implemented and verified end to end: stable Civilian binding, immutable authority offer resolution, replay-safe all-or-nothing mutation, owner-only result/state handling, deterministic last-stock contention, and immediate unavailability on merchant death.
