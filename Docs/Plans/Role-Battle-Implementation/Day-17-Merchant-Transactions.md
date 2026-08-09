# Day 17 - Add Merchant Transactions

## Goal

Allow a player to buy one immutable offer unit from a specifically bound Civilian merchant through a replay-safe, all-or-nothing server transaction, with an explicit owner-client result and a basic merchant UI.

## BungeeMan Gun Skill checkpoint

Prove merchant requests remain a separate, replay-safe transaction path: FireGun input, projectile damage, and target callbacks cannot supply item, price, stock, range, or purchase authorization, and merchant UI state cannot alter the FireGun grant or cooldown.

## Prerequisite gate

- Day 14's PlayerController-owned interaction RPC and server range/line-of-sight revalidation pass in listen and dedicated modes.
- Day 15 resolves the fixture population member to one valid merchant definition and immutable offer snapshot.
- Day 16 owner-only PlayerState wallet/inventory, preflight APIs, and pawn-respawn tests pass.

## New runtime and test files

- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp
- Source/Aura/Public/Economy/AuraCommerceSubsystem.h
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp
- Source/Aura/Public/UI/WidgetController/MerchantWidgetController.h
- Source/Aura/Private/UI/WidgetController/MerchantWidgetController.cpp
- Source/Aura/Public/UI/Widget/AuraMerchantWidget.h
- Source/Aura/Private/UI/Widget/AuraMerchantWidget.cpp
- Source/Aura/Private/Tests/AuraCommerceTests.cpp
- Content/AutoTests/RoleBattleDay17Merchant.xml
- Content/Maps/Tests/RoleBattleMerchantTest.umap
- RunRoleBattleDay17NetworkSmoke.ps1

## New or modified UI assets

- Content/Blueprints/UI/Merchant/WBP_Merchant.uasset
- Content/Blueprints/UI/Merchant/WBP_Merchant.snapshot.json
- Content/Blueprints/UI/Merchant/WBP_MerchantOfferRow.uasset
- Content/Blueprints/UI/Merchant/WBP_MerchantOfferRow.snapshot.json
- Content/Blueprints/UI/WidgetController/BP_MerchantWidgetController.uasset
- Content/Blueprints/UI/WidgetController/BP_MerchantWidgetController.snapshot.json
- Content/Blueprints/UI/HUD/BP_AuraHUD.uasset
- Content/Blueprints/UI/HUD/BP_AuraHUD.snapshot.json

## Files to modify

- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Source/Aura/Public/UI/HUD/AuraHUD.h
- Source/Aura/Private/UI/HUD/AuraHUD.cpp
- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp
- Config/DefaultGame.ini

## Merchant instance and replication contract

1. Create `UAuraMerchantComponent` as a replicated default subobject on `AAuraCivilian` so component creation is stable across the network. Leave it inactive on ordinary civilians.
2. On the authority, activate it only when the spawned Civilian has the validated runtime `MerchantDefinitionId` copied from its exact Day 09 `memberOverrides[slotIndex].merchantDefinitionId`, together with its canonical `PopulationMemberId`. A row default or role-level economy profile must never activate a merchant.
3. Resolve immutable offer definitions from the authoritative economy registry. Store mutable per-instance stock under canonical `PopulationMemberId`, never actor name, pointer, network GUID, or merchant-definition ID alone.
4. Replicate a presentation snapshot containing offer ID, item ID/display data, configured grant quantity, authoritative displayed price, finite/unlimited state, current stock, availability, and stock revision. The server remains authoritative even though clients display those values.
5. Wake/flush dormancy when availability or stock changes. A late-relevant client must receive the current stock revision, not the initial definition value.
6. Disable interaction immediately when the merchant is dying, dead, unavailable, or no longer registered with the population manager.

## Request and response protocol

1. Keep and extend the Day 14 `UAuraInteractionComponent` default subobject on the player-owned `AAuraPlayerController`. The reliable purchase Server RPC is declared on that owned endpoint, never on the server-owned merchant actor/component.
2. At server login, generate an unpredictable server session nonce and replicate it owner-only to the interaction component. Rotate it on reconnect and when entering a new commerce world; rotation clears the client counter and invalidates the prior world's replay window.
3. A buy request contains only:
   - The current server session nonce.
   - A monotonically increasing `uint64` request ID.
   - The replicated merchant actor/reference selected through interaction.
   - Offer ID.
4. A request never contains an accepted price, item ID, grant quantity, currency result, stock result, role result, or inventory result. The server resolves all of those from the merchant binding and immutable registry.
5. Use a dedicated commerce request-ID stream in the interaction component so Day 14 Talk/Observe/Shelter IDs cannot create purchase gaps. Accept new commerce requests in monotonic order for the current nonce. Cache the last 256 terminal results per player session. An exact replay returns the cached result without mutation; an ID below the retained window, a gap, overflow, or a nonce other than the connection's current nonce returns a typed stale/session failure.
6. On disconnect, invalidate the nonce. A reconnected client refreshes authoritative state and must not silently resubmit an old-session purchase under a new ID.
7. Return completion through an explicit reliable owner Client RPC/delegate such as `ClientReceivePurchaseResult(SessionNonce, RequestId, ResultCode, WalletRevision, InventoryRevision, StockRevision)`. The result carries revisions only; replicated PlayerState wallet/inventory state remains the sole value source and the UI never overwrites it from a cached response. Unreal Server RPCs do not return values.
8. Sanitize failure codes. Do not reveal another player's inventory/balance or internal object paths.

## Server validation and atomic commit

`UAuraCommerceSubsystem` is an authority-only `UWorldSubsystem`. It does not replicate and is not a client RPC endpoint; it serializes transactions, stock records, and bounded session replay state for its world. Replicated presentation remains on merchant/player components.

For each new request, resolve the player from the RPC-owning controller and validate on the server:

1. The nonce/request sequence and replay cache.
2. Player is alive, connected, and permitted to interact.
3. Merchant actor is the registered stable population member, is alive/available, and is bound to the requested merchant definition.
4. Current distance and line of sight using server transforms and collision.
5. Offer exists on that merchant; ignore any client display payload.
6. Required role, interaction, faction, and other offer eligibility tags.
7. Finite stock is available.
8. Wallet preflight can debit the registry price without underflow.
9. Inventory preflight can add the configured item quantity without overflow, invalid stacks, partial add, or slot-capacity failure.

After all checks pass:

1. Execute stock decrement, wallet debit, and item grant in one synchronous, no-yield authority function on the game thread.
2. Do not invoke Blueprint, latent, async, save, UI, or re-entrant delegates between mutations.
3. Use transaction-local before-images or equivalent rollback-safe internal commit APIs. An unexpected invariant failure restores wallet, inventory, and stock before returning failure.
4. Publish delegates/replication, advance stock revision, and cache/send the success result only after every mutation commits.
5. Serialize two players competing for the last unit through the commerce subsystem; exactly one may commit. No explicit cross-thread lock substitutes for keeping the whole transaction on the authority game thread.

Selling, variable client-selected quantities, dynamic pricing, and client-authored discounts are out of scope.

## UI flow

1. Open the merchant widget only from a validated interaction target, but keep highlight/prompt presentation cosmetic.
2. Build rows from the replicated merchant presentation snapshot. Clearly show price, configured item quantity, current finite/unlimited stock, eligibility, and pending state.
3. Clicking Buy allocates the next local request ID and sends only the request contract above.
4. Correlate the Client result by nonce and request ID. Do not infer success from prediction or from one replicated field arriving before another.
5. Refresh balance/inventory from their owner-only replication and stock from the merchant stock revision. Close or disable the UI when the merchant dies, leaves relevancy, becomes unavailable, or interaction range is lost.

## Named tests

Native automation:

- `Aura.RoleBattle.Day17.Commerce.SuccessAtomic`
- `Aura.RoleBattle.Day17.Commerce.InsufficientFunds`
- `Aura.RoleBattle.Day17.Commerce.FullInventory`
- `Aura.RoleBattle.Day17.Commerce.OutOfRangeAndLineOfSight`
- `Aura.RoleBattle.Day17.Commerce.DeadOrUnavailableMerchant`
- `Aura.RoleBattle.Day17.Commerce.SoldOut`
- `Aura.RoleBattle.Day17.Commerce.Eligibility`
- `Aura.RoleBattle.Day17.Commerce.ForgedPayloadIgnored`
- `Aura.RoleBattle.Day17.Commerce.ReplayReturnsCachedResult`
- `Aura.RoleBattle.Day17.Commerce.ReplayAfterLaterSuccessDoesNotRewind`
- `Aura.RoleBattle.Day17.Commerce.StaleNonceAndRequest`
- `Aura.RoleBattle.Day17.Commerce.RollbackLeavesNoPartialState`
- `Aura.RoleBattle.Day17.Commerce.LastStockContention`

The Day 17 network smoke must use two remote clients and assert the Client result correlation, exactly-once wallet/item/stock deltas, non-owner privacy, merchant-death UI closure, replay behavior, and contention for one remaining stock unit.

`RunRoleBattleDay17NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`; both modes are required.

The runner enforces bounded startup/request/assertion/late-join/replay/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, and stops only processes it created. It writes `Saved/Logs/Day17-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day17-{Listen|Dedicated}.json` with revision, commands, exit codes, request IDs, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day17; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day17Automation.log'

& '.\RunRoleBattleDay17NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay17NetworkSmoke.ps1' -Mode Dedicated
```

## Verification

- A valid remote-client purchase debits the authoritative price, grants the configured quantity, and decrements that merchant instance's stock exactly once.
- Every failed request leaves currency, inventory, and stock byte-for-byte unchanged.
- Forged price/item/quantity/eligibility/display data is ignored or rejected and never influences authority state.
- Exact replay returns the same result without a second mutation; stale nonce/request IDs fail safely.
- Two players competing for one stock unit produce one success and one sold-out result.
- An ordinary Civilian never exposes merchant interaction merely because its role has an economy capability.
- The focused Day 17 suite, network smoke, and full `Aura` native suite pass.

## Completion gate

The first business loop works end to end through a player-owned request endpoint: stable Civilian merchant binding, immutable server offer resolution, replay-safe atomic commit, owner Client result, replicated UI state, and no client authority over price or inventory outcome.
