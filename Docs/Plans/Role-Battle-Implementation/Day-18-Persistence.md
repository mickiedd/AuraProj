# Day 18 - Add Persistence

## Goal

Persist each player's validated profile independently from one server-owned world snapshot. Restore player role/progression/wallet/inventory before pawn role/loadout initialization, and restore population plus per-instance merchant stock exactly once before population spawning.

The existing `UAuraGameInstance::LoadSlotName/LoadSlotIndex` pair is a local menu selection, not a multiplayer player identity or a server-wide persistence key.

## Prerequisite gate

- Day 13 provides canonical `PopulationMemberId` values and a deterministic server snapshot.
- Day 17 provides stable per-instance merchant stock and publishes only complete transaction state.
- The Day 17 native and two-remote-client network gates pass before save schemas change.
- Select and configure the deployment's authenticated Online Subsystem provider so it supplies a valid `FUniqueNetIdRepl` for remote connections. `OnlineSubsystemNull`, display names, and locally chosen slot names are not release persistence identities; if no authenticated provider is available, the persistent-profile portion of Day 18 remains blocked.

## New files

- Source/Aura/Public/Game/AuraPlayerProfileIdentity.h
- Source/Aura/Private/Game/AuraPlayerProfileIdentity.cpp
- Source/Aura/Public/Game/AuraSaveTypes.h
- Source/Aura/Private/Game/AuraSaveTypes.cpp
- Source/Aura/Public/Game/AuraPlayerSaveGame.h
- Source/Aura/Private/Game/AuraPlayerSaveGame.cpp
- Source/Aura/Public/Game/AuraWorldSaveGame.h
- Source/Aura/Private/Game/AuraWorldSaveGame.cpp
- Source/Aura/Public/Game/AuraPersistenceManifestSaveGame.h
- Source/Aura/Private/Game/AuraPersistenceManifestSaveGame.cpp
- Source/Aura/Public/Game/AuraPersistenceSubsystem.h
- Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp
- Source/Aura/Private/Tests/AuraPersistenceTests.cpp
- Source/Aura/Private/Tests/Fixtures/RoleBattleLegacyV0.sav
- Content/AutoTests/RoleBattleDay18Persistence.xml
- RunRoleBattleDay18PersistenceSmoke.ps1

## Files to modify

- Aura.uproject
- Config/DefaultEngine.ini
- Content/Config/ServerConnection.json
- Source/Aura/Aura.Build.cs
- Source/Aura/Public/Game/LoadScreenSaveGame.h
- Source/Aura/Private/Game/LoadScreenSaveGame.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Game/AuraGameInstance.h
- Source/Aura/Private/Game/AuraGameInstance.cpp
- Source/Aura/Public/Game/LoginPlayerController.h
- Source/Aura/Private/Game/LoginPlayerController.cpp
- Source/Aura/Public/Game/LoadingPlayerController.h
- Source/Aura/Private/Game/LoadingPlayerController.cpp
- Source/Aura/Public/Game/ServerTravelComponent.h
- Source/Aura/Private/Game/ServerTravelComponent.cpp
- Source/Aura/Public/Game/GameServerClient.h
- Source/Aura/Private/Game/GameServerClient.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp
- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp
- Source/Aura/Public/Economy/AuraCommerceSubsystem.h
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp

The stale `Source/Aura/Public/Save` and `Source/Aura/Private/Save` paths are not used; the existing save class is under `Source/Aura/{Public,Private}/Game`.

## Persistence ownership and identity

1. `UAuraPersistenceSubsystem` is an authority-only `UGameInstanceSubsystem` and performs all runtime load/save operations across world travel. Clients never choose save paths, slot names, profile records, migration sources, or world IDs.
2. The first slice has one production identity source: the authenticated `FUniqueNetIdRepl` delivered to `PreLogin`/`InitNewPlayer` by the configured Online Subsystem. Require a valid ID of the configured provider type and authenticated login state, then canonicalize `{ProviderName, UniqueId}` into `FAuraPlayerProfileId`. The custom Game Server Manager remains endpoint routing only and does not mint a second profile/session-token identity.
3. Never key persistence by PlayerState `PlayerId`, player name, controller index, network GUID, IP address, remote port, client-supplied slot string, URL option, or actor name. If no authenticated unique ID can be validated, fail persistent-profile login closed or use an explicitly ephemeral non-saving profile; never silently merge it into another player's save.
4. Store the canonical profile identity only in server-owned connection/PlayerState state. Use a stable server-side hash/encoding for the player-save slot name so raw credentials or platform tokens are not filenames.
5. Reject a second concurrent connection for the same persistent profile unless an explicit handoff policy safely completes the old session first.
6. Define one validated authority-owned `WorldPersistenceId` (campaign/shard/world slot) from server deployment configuration, not from a client URL, map name alone, or client option. Include it in the world record, manifest key, and generation-specific slot/path names; reject missing, malformed, or changed IDs. The load-once guard is keyed by `(WorldPersistenceId, authoritative world generation)` so subsystem reuse during travel cannot suppress a new world restore.
7. The network tests inject two distinct server-approved fixture `FUniqueNetId` values and reuse each identity for reconnect; they do not simulate identity with display names. The fixture provider exists only under `WITH_DEV_AUTOMATION_TESTS` and can never satisfy a Shipping login.
8. Add the `OnlineSubsystem`/identity-interface dependency plus the selected deployment provider plugin/configuration. `OnlineSubsystemNull` may support unrelated local networking but cannot satisfy the release persistence gate. If the production provider or authenticated status is unavailable, persistent login fails closed; no test provider ships as a fallback credential system.

## Save layout

1. `UAuraPlayerSaveGame` is one versioned record per validated profile and contains:
   - Save schema version and profile-record generation.
   - Validated role ID.
   - Existing level, XP, attributes, points, and ability data.
   - Currency ID and `int64` wallet balance.
   - Deterministically ordered inventory slots with item ID and `int64` quantity.
2. `UAuraWorldSaveGame` is one versioned record for the configured server world/campaign and contains:
   - Save schema version and world-record generation.
   - Validated server-owned `WorldPersistenceId` plus map/world identity for diagnostics (map name alone is not a slot key).
   - Population entries keyed by canonical `PopulationMemberId`, retaining `PopulationId` and `PopulationSlotIndex` for validation, plus alive/dead state and remaining refill delay.
   - Merchant stock keyed by canonical `PopulationMemberId`, including `MerchantDefinitionId`, deterministic offer-stock entries, and stock revision.
3. Do not duplicate player wallet/inventory in the world save, and do not put shared population/merchant state in an individual player's save.
4. `UAuraPersistenceManifestSaveGame` uses alternating manifest slots (`A`/`B`) with generation, `WorldPersistenceId`, record list, and checksum. Write the inactive slot only after every generation-specific player/world record is durable and verified; then atomically replace/rename that manifest. On load, validate both manifests and select the highest valid generation, retaining the prior valid slot when the newest manifest is torn or corrupt. It contains no gameplay state.
5. Serialize maps as arrays sorted by canonical ID so equivalent state produces deterministic records and tests.
6. Persist remaining refill duration and resume it after load; the first slice does not advance civilian respawn timers while the server is offline.
7. Validate every loaded role, item, currency, population, merchant, offer, quantity, balance, and stock value against the current authoritative registries before publishing runtime state.

## Versioning and legacy migration

1. Add explicit save versions for the legacy `ULoadScreenSaveGame`, new player save, and new world save. Missing legacy version is version zero; do not initialize a missing field to the current version and thereby skip migration.
2. Capture the checked-in version-zero fixture before changing serialization. The migration test must deserialize that real pre-economy save, not construct a current class and pretend it is old.
3. When no new player record exists, migrate a server-authorized legacy slot mapped to that profile:
   - Preserve valid role, level, XP, attributes, points, and abilities.
   - Initialize the configured currency and starting balance exactly once.
   - Initialize inventory empty.
4. Legacy world actor data migrates into the one world record only once. A dedicated server must never accept a client's local legacy slot as shared world authority.
5. Reject corrupt and unsupported future versions without partially applying them. Record actionable migration errors and retain the prior valid save.
6. Mark migration complete only after the new record is written successfully. A failed write must be safely retryable without a second starting-balance grant.

## Authority load/save order

1. At server world startup:
   - Load RoleConfig.
   - Construct the Day 09 population manager and load population/work definitions without spawning.
   - Load and publish the Day 15 economy registry after cross-validating per-member merchant bindings.
   - Read the world snapshot file exactly once into an unpublished candidate and perform only framing/schema/checksum/version checks; do not call it semantically valid before current zones, placed registrations, population, and economy definitions are available.
   - Enter `StartPlay`, create the deferred Day 12 director/zone candidate, call `Super::StartPlay`, and reconcile placed volume/marker registrations.
   - Jointly validate the zone and world-snapshot candidates against current population/work/economy definitions and discovered registrations. Any error publishes neither candidate and spawns nothing.
   - Publish the director, give the now-valid single snapshot to `AuraPopulationManager`, and reconcile saved members/merchants with current definitions exactly once.
   - Release population spawning with restored alive/dead/refill and stock state only after the dispatcher, director, economy registry, validated snapshot, definitions, and registrations are ready.
   - Mark world restore complete.
2. Remove the current pattern where each `AAuraCharacter::PossessedBy` calls general world loading. A guard must also reject any accidental second world-restore request.
3. For each connection, resolve the validated profile and load/migrate its player record before `HandleStartingNewPlayer`/pawn grant initialization. Apply role, progression, wallet, and inventory to PlayerState first; only then spawn/possess the pawn and grant role abilities.
4. A reconnect with the same validated profile creates a new PlayerState populated from that profile record before pawn initialization. It receives a new Day 17 session nonce and cannot replay the old connection's requests.
5. Save only on authority at defined checkpoints, successful purchase completion, orderly logout, and requested world checkpoint/travel. Check and report every save result.
6. For a checkpoint affecting player and world state, capture one immutable server snapshot, write generation-specific player record(s) and the world record under the validated `WorldPersistenceId`, verify every write, and update the inactive manifest slot last. Load only the highest generation named by a valid manifest. A torn/failed checkpoint leaves the prior manifest generation authoritative; orphan generation files are ignored and later cleaned safely.
7. The Day 17 no-yield transaction publishes only committed state, so persistence must snapshot either the state before a request starts or the fully committed state after it ends, never debit-only/item-only intermediate state.

## Population and merchant reconciliation

1. For a saved stable member still present in definitions, restore its state exactly once.
2. For a newly configured member absent from an older world save, create the configured default once.
3. For a removed/unknown saved member, log and omit it; never spawn a duplicate fallback actor.
4. Restore merchant stock before activating merchant interaction. Validate merchant definition and offer IDs; apply an explicit migration default for newly added offers.
5. Restarting or late-joining clients consumes replicated restored state and never triggers another population/world load.

## Named tests

Native automation:

- `Aura.RoleBattle.Day18.Save.LegacyV0Migration`
- `Aura.RoleBattle.Day18.Save.FutureVersionRejected`
- `Aura.RoleBattle.Day18.Save.Int64RoundTrip`
- `Aura.RoleBattle.Day18.Save.PlayerIsolation`
- `Aura.RoleBattle.Day18.Save.LoadBeforePawnInitialization`
- `Aura.RoleBattle.Day18.Save.WorldLoadsOnce`
- `Aura.RoleBattle.Day18.Save.PopulationReconciliation`
- `Aura.RoleBattle.Day18.Save.MerchantStockRoundTrip`
- `Aura.RoleBattle.Day18.Save.CommittedTransactionOnly`
- `Aura.RoleBattle.Day18.Save.TornCheckpointRecovery`
- `Aura.RoleBattle.Day18.Save.CorruptNewestManifestFallsBack`
- `Aura.RoleBattle.Day18.Save.AuthenticatedIdentityRequired`
- `Aura.RoleBattle.Day18.Save.ProviderMismatchRejected`
- `Aura.RoleBattle.Day18.Save.DuplicateProfileConnectionRejected`
- `Aura.RoleBattle.Day18.Save.WorldPersistenceIdIsolation`

The cross-process persistence smoke must assert:

- Two validated profiles purchase different items/balances, disconnect, and reconnect to their own records with no cross-contamination.
- Reusing a display name cannot select another profile's save.
- An old connection nonce is rejected after reconnect.
- A missing/invalid provider identity, provider mismatch, and duplicate active profile connection are rejected without opening or cross-writing a save.
- Two server instances with different validated `WorldPersistenceId` values write isolated world/manifest generations and cannot load or overwrite one another's state.
- Civilian death/refill and merchant stock restore once without duplicate population actors.
- Loading occurs before role abilities or starting balance can be granted.

Use isolated automation save directories/slot prefixes that include the validated `WorldPersistenceId`; delete only the test artifacts created by the runner.

`RunRoleBattleDay18PersistenceSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`, always launches two remote clients with distinct validated fixture identities, and reconnects both identities. Both modes are required.

The runner enforces bounded startup/identity/load/reconnect/assertion/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, and stops only processes it created. It writes `Saved/Logs/Day18-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day18-{Listen|Dedicated}.json` with revision, commands, exit codes, profile/world identifiers, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day18; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day18Automation.log'

& '.\RunRoleBattleDay18PersistenceSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay18PersistenceSmoke.ps1' -Mode Dedicated
```

## Verification

- Existing version-zero saves migrate once and still restore Aura or BungeeMan plus existing progression.
- Player A can never load or overwrite Player B's wallet, inventory, role, or progression.
- Wallet, inventory, and per-instance merchant stock restore with exact `int64` values.
- World state loads once before population spawning and never from pawn possession.
- A civilian population does not duplicate after load, reconnect, late join, or level restart.
- Corrupt/future-version data fails closed without partially initialized runtime state.
- The focused Day 18 suite, persistence network smoke, and full `Aura` suite pass.

## Completion gate

Versioned server persistence cleanly separates validated per-player records from one shared world record. Two players and reconnect restore isolated role/economy state, while population and per-instance merchant stock restore deterministically exactly once before gameplay initialization.
