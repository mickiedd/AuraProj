# Role/Battle Day 16 - Wallet and Inventory

Date: 2026-08-28

## Intent

Add a server-authoritative wallet and inventory owned by `AAuraPlayerState`, with owner-only replication, checked `int64` arithmetic, deterministic all-or-nothing inventory mutations, one-time new-profile initialization, and pawn-respawn persistence.

## Changed behavior

- `AAuraPlayerState` now owns replicated `UAuraCurrencyComponent` and `UAuraInventoryComponent` default subobjects. The components are independent of the Ability System Component and pawn lifetime.
- Currency balance, currency ID, revision, inventory fast-array slots, inventory revision, and initialization state replicate with `COND_OwnerOnly` where applicable. Inventory slots contain only item IDs and `int64` quantities.
- Currency and inventory mutation APIs refuse non-authority callers, invalid definitions, invalid amounts, overflow, insufficient funds/items, and over-capacity changes. Inventory fills existing stacks in stable order, appends deterministic slots, and commits only after a complete candidate succeeds.
- New-profile starting currency is guarded by a transient PlayerState initialization state/count. `LoadProgress` calls it before role application, while repeated possession and pawn replacement do not grant again.
- The non-Shipping GameMode development grant is routed through the same validated authority APIs. The process-level probe deliberately attempts the command from both clients and asserts no client-side grant is applied.
- The Day 16 probe mutates a known item on both server-side PlayerStates, destroys/restarts one pawn, and verifies balance, item quantity, and initialization count survive. Client logs identify their owning PlayerState so the smoke can assert the observer receives no foreign economy replica.

## Validation

- `build_test.bat`: AuraEditor Win64 Development passed after the final probe/privacy hardening.
- All repository Python contracts: 11/11 passed.
- Focused native automation: 8/8 Day 16 tests passed.
- Full `Aura` native automation: 170/170 passed, 0 failed.
- `RunRoleBattleDay16NetworkSmoke.ps1 -Mode Listen`: passed.
- `RunRoleBattleDay16NetworkSmoke.ps1 -Mode Dedicated`: passed.
- `git diff --check`: passed; pre-existing unrelated standalone BT debugger files were preserved.

## Illustration

[Day 16 wallet/inventory owner-only flow](2026-08-28-role-battle-day-16-wallet-inventory.svg)
