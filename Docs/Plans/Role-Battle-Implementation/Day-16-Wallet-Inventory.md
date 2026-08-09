# Day 16 - Add Wallet and Inventory

## Goal

Add server-authoritative wallet and inventory state owned by `AAuraPlayerState`. The state must replicate only to that PlayerState's owning connection, survive pawn replacement, initialize new-profile currency exactly once, and remain independent of the Ability System Component.

## BungeeMan Gun Skill checkpoint

Keep the FireGun grant, cooldown, and role equipment independent from owner-only wallet/inventory replication. Pawn replacement must preserve the server-owned BungeeMan skill ledger without exposing another player's gun or economy state to a non-owner.

## Prerequisite gate

- Day 15 has published one valid immutable authority registry with currency, stack, item-slot capacity, and item definitions.
- The server refuses economy initialization when that registry is not ready.
- Day 15 focused and full regression tests pass.

## New files

- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Private/Tests/AuraEconomyStateTests.cpp
- Content/AutoTests/RoleBattleDay16OwnerReplication.xml
- RunRoleBattleDay16NetworkSmoke.ps1

## Files to modify

- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraEconomyRegistrySubsystem.h

## Ownership and replication contract

1. Create wallet and inventory as default subobjects of `AAuraPlayerState`; do not attach either component to `AAuraCharacter`, a PlayerController, the ASC, or another pawn-lifetime object.
2. Set both components to replicate, but replicate balance, inventory stacks, initialization state needed by presentation, and subsequent deltas with `COND_OwnerOnly`.
3. Use an owner-only fast-array or equivalent delta-safe representation for inventory slots. A non-owning client may know that another PlayerState exists but must receive neither its balance nor its inventory entries.
4. Currency, quantities, and stack limits remain `int64` in component state, replication, delegates, arithmetic, tests, and later save records.
5. Expose const read APIs to the owning client for UI. Client-side values are observational and editing local memory never changes authority state.
6. Broadcast UI change delegates on the server after a committed mutation and on the owning client from `OnRep`/fast-array callbacks. Do not broadcast an intermediate preflight or partial transaction state.

## Mutation and preflight APIs

1. Provide side-effect-free authority preflight methods:
   - `CanCreditCurrency` and `CanDebitCurrency`.
   - `CanAddItem` and `CanRemoveItem`.
   - Capacity/slot calculation for a proposed item quantity.
2. Provide authority-only commit methods for credit, debit, add, and remove. They must:
   - Reject calls without server authority.
   - Reject zero or negative mutation amounts.
   - Reject invalid currency/item IDs.
   - Use checked `int64` arithmetic and respect the configured maximum balance.
   - Respect stack limits and the exact item-slot capacity contract from Day 15.
   - Return a typed result without changing state on failure.
3. Do not mark mutation methods as Server RPCs on the components and do not expose them as unrestricted Blueprint-callable client entry points. Day 17's player-owned interaction RPC is the only purchase entry point.
4. Keep `HasItem`, balance lookup, stack lookup, and capacity queries read-only; they are not mutation methods and may operate on the owning client's replicated view.
5. Give the Day 17 commerce subsystem a narrow server-only transaction interface that can perform preflight and commit without re-entering UI delegates between wallet and inventory changes.

## Initialization and lifecycle

1. Component constructors start with an empty inventory and zero balance. Constructors must not grant configured starting currency.
2. Add one server-only `AAuraPlayerState::InitializeEconomyForNewProfileOnce` path. It reads the validated Day 15 registry and grants the configured starting balance only when this connection's server-generated bootstrap state is `NewEphemeralSession`.
3. `NewEphemeralSession` is a per-connection, authority-created decision held on the PlayerState; it is never a GameInstance/process-global flag, client slot value, URL option, or shared first-load variable. Guard the transition once per PlayerState so two simultaneous clients can independently initialize without affecting one another.
4. Protect initialization with transient PlayerState state so repeated `PossessedBy`, `LoadProgress`, role application, and pawn respawn calls cannot grant again. Day 18 replaces this ephemeral bootstrap with the authenticated profile record/migration result before any persistent load.
5. On pawn death and replacement, reuse the same PlayerState components. Rebind UI observers to the new pawn/controller as necessary, but do not copy, reset, or reinitialize wallet/inventory state.
6. Add a development grant command only through an authority-owned GameMode path and compile/register it only in non-Shipping builds. It must use the same validated mutation API, reject client execution, and be retained solely as an automation fixture until the Day 20 Shipping audit.

## Inventory behavior

1. Store slots as item ID plus `int64` quantity; never store or replicate arbitrary asset paths.
2. Fill existing partial stacks in stable slot order, then append the minimum number of new slots.
3. Reject the entire add if all requested quantity cannot fit. Never add a partial quantity unless a future API explicitly requests partial behavior.
4. Remove from reverse stable slot order, erase empty slots deterministically, and reject the entire remove when aggregate quantity is insufficient.
5. Reject registry reloads that would make a live stack limit or item ID invalid; Day 15's immutable startup snapshot remains fixed for the match.

## Named tests

Native automation:

- `Aura.RoleBattle.Day16.Wallet.CheckedArithmetic`
- `Aura.RoleBattle.Day16.Wallet.StartingBalanceIdempotent`
- `Aura.RoleBattle.Day16.Inventory.StackingAndSlotCapacity`
- `Aura.RoleBattle.Day16.Inventory.AllOrNothingMutation`
- `Aura.RoleBattle.Day16.Inventory.InvalidDefinition`
- `Aura.RoleBattle.Day16.Authority.ClientMutationRejected`
- `Aura.RoleBattle.Day16.Lifecycle.PawnRespawnPreservesEconomy`
- `Aura.RoleBattle.Day16.Lifecycle.IndependentSessionInitialization`

The multi-process network smoke must run a server/listen host plus two remote clients and additionally assert:

- The owning client receives balance and stack updates.
- A second client receives no wallet balance or inventory entries for the first player.
- A remote client cannot execute the development grant or component mutation path.
- Destroying and replacing the pawn leaves balance, stacks, and initialization count unchanged.

`RunRoleBattleDay16NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`; both modes are required.

The runner enforces bounded startup/readiness/assertion/late-join/respawn/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, and stops only processes it created. It writes `Saved/Logs/Day16-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day16-{Listen|Dedicated}.json` with revision, commands, exit codes, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day16; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day16Automation.log'

& '.\RunRoleBattleDay16NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay16NetworkSmoke.ps1' -Mode Dedicated
```

## Verification

- Wallet and inventory are owned only by PlayerState and are present on server and owning client.
- Owner-only replication is proven with an observer client, not inferred from local PIE state.
- Starting currency is granted once for a new profile and not again after role reapplication or two pawn respawns.
- Negative, zero, oversized, overflowing, invalid-ID, over-stack, and over-capacity operations fail without state change.
- Inventory slot ordering and stack fill/removal are deterministic.
- The focused Day 16 native suite, Day 16 network smoke, and full `Aura` native suite pass.

## Completion gate

Each player has exactly one PlayerState-owned wallet and inventory. Only validated server code can mutate them, only the owning connection receives their contents, and both states survive pawn death/respawn without duplicate initialization.
