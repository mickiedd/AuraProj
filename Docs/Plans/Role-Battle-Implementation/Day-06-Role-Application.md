# Day 06 - Apply Roles Authoritatively and Idempotently

## Goal

Apply one validated role during server login/spawn, publish its combat and non-combat runtime profiles, and grant its loadout exactly once even though the PlayerState-owned ASC survives pawn replacement.

Live role hot-swapping remains disabled. Day 6 builds enough grant ownership metadata to support safe future work, but it does not remove/cancel a live loadout or reverse already-applied instant attributes.

## Files to modify

### Role, identity, and replicated runtime state

- Source/Aura/Public/AbilitySystem/Data/RoleInfo.h
- Source/Aura/Private/AbilitySystem/Data/RoleInfo.cpp
- Source/Aura/Public/Combat/AuraCombatTypes.h
- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/AuraGameplayTags.h
- Source/Aura/Private/AuraGameplayTags.cpp
- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp

### Persistent ASC grant bookkeeping and save restore

- Source/Aura/Public/AbilitySystem/AuraAbilitySystemComponent.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp
- Source/Aura/Public/Game/LoadScreenSaveGame.h

### Tests and runner

- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp
- Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.h
- Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.cpp
- Source/Aura/Private/Game/LoadScreenSaveGame.cpp
- RunRoleBattleDay6NetworkSmoke.ps1

## Content and UI to inspect/regression-test

- Content/Config/RoleConfig.json
- Source/Aura/Private/UI/ViewModel/MVVM_LoadScreen.cpp
- Source/Aura/Private/UI/WidgetController/AuraWidgetController.cpp
- Source/Aura/Private/UI/WidgetController/OverlayWidgetController.cpp
- Source/Aura/Private/UI/WidgetController/SpellMenuWidgetController.cpp

These UI files should not become role-authority paths. Adjust them only if the replicated role/ability notification contract changes; record any such adjustment in the Day 6 evidence.

## Replicated runtime ownership

Keep combat and non-combat concepts separate:

- `FAuraCombatIdentity` receives the validated role’s faction, control type, combat profile, death policy, targetable, can-attack, can-be-damaged, and friendly-fire values.
- Add `FAuraAppliedRoleState` for role ID, entity type, economy capability/default classification (`economyProfile`), and interaction profile. Replicate it from `AAuraCharacterBase` with `OnRep_AppliedRoleState` so the future `AAuraCivilian` can use the same path without a PlayerState. This state never contains a concrete merchant/job/offer identity; `MerchantDefinitionId` belongs to stable population-member data added on Day 9/15.
- `AAuraPlayerState::CharacterRole` remains the persistent player/save/UI role ID, but only authority writes it. It must match the pawn’s applied role state.

Clients use the replicated applied role ID to load presentation from their local validated config. They never construct authoritative identity/economy state from local JSON. A mismatch or missing client definition produces a presentation error, not a client-authored fallback identity.

The authority-only identity mutation path added on Day 2 is reused after role validation. It must support a spawn-time update after `BeginPlay`, wake owner replication, and broadcast a server/client identity-changed notification. It is not exposed as a Blueprint setter or role-switch RPC.

## Spawn-time application transaction

Replace the ambiguous public `ApplyRole` flow with two clearly scoped paths:

- `ApplyRoleAtSpawn` (server only) validates, stages, publishes runtime state, initializes presentation/config caches, and returns a structured success/error result.
- `ApplyRolePresentation` (client/server cosmetic path) consumes an already-authorized applied role ID and never grants abilities, initializes attributes, or mutates identity.

Caller precondition: the ASC has already completed `InitAbilityActorInfo`/`AbilityActorInfoSet` for the current owner/avatar. Those calls bind GAS only; they do not initialize role attributes or grant a loadout. `ApplyRoleAtSpawn` exclusively owns the role attribute/grant work that follows.

Server order:

1. Resolve only the server-published Day 5 `URoleInfo` and validate the role ID.
2. Enforce actor-shell compatibility (`AAuraCharacter` requires Player/Control.Player; the Day 8 Civilian shell will require AmbientNPC/Control.CivilianAI).
3. Stage every asset, socket, array, identity, and applied-profile value before clearing current spawn-time caches. If staging fails, mutate nothing and grant nothing.
4. Snapshot the pre-transaction spawn-time caches/ledger, then run `ClearRoleRuntimeState` and `LoadRoleRuntimeState` from the immutable candidate.
5. Set the authority-local pending `FAuraAppliedRoleState` and `FAuraCombatIdentity`, then call `ApplyRolePresentation`; do not broadcast change delegates or force replication yet.
6. Initialize role attributes once on the persistent PlayerState ASC, or on the actor-owned ASC for non-player shells, and restore/reconcile saved abilities or grant the first-time loadout through the ledger.
7. If any post-stage step fails, restore the snapshot/ledger and publish no role, identity, presentation notification, attribute, or grant change.
8. On success, commit the PlayerState role (for a Player), mark the applied state/identity committed, broadcast once, and force replication only after the complete server transaction is valid.

`ClearRoleRuntimeState` clears body/AnimBP override, weapon mesh/attachment, all combat socket names, death presentation assets, startup class arrays, definition object arrays, LMB references, and cached profile data. Empty arrays mean empty. Equipment is decided only by the explicit equipment structure.

Because hot swapping is disabled, an already initialized living actor/ASC with a different applied/granted role is rejected. Do not clear arbitrary live specs/effects or attempt to subtract instant attributes. A same-role respawn is the supported repeated path.

## Persistent ASC grant ledger

The Player ASC lives on `AAuraPlayerState`; therefore all duplicate prevention and role grant ownership live on `UAuraAbilitySystemComponent`, not the pawn.

Add a server-owned `FAuraRoleGrantLedger` containing:

- Granted role ID.
- Every role-granted `FGameplayAbilitySpecHandle`.
- Any role-startup `FActiveGameplayEffectHandle` that is actually removable.
- Initialization/reconciliation state.

Persist versioned provenance with every saved ability: `GrantSource` plus optional `GrantedRoleId`. A role grant removed by a later role-definition version remains identifiable and is not silently reclassified as progression. Legacy saves migrate through an explicit table of the shipped Aura/BungeeMan grant sets; an unknown former role grant is quarantined/rejected with a diagnostic rather than promoted to an unlock.

Every role-granted spec also receives the replicated dynamic source tag `GrantSource.Role`. Keep the role ID in the server ledger/map keyed by spec handle; arbitrary role IDs are not converted into native Gameplay Tags. Do not replace `FGameplayAbilitySpec::SourceObject`, because data-driven abilities require the `UAuraAbilityDefinition` source object and it is weak/non-replicated.

Refactor ASC grant helpers to accept a grant-source descriptor and return the handles they created. Before giving a spec, deduplicate by stable ability tag and expected source. Legacy class abilities, data-driven definitions, LMB, and passives all use the same rule. Passive activation and its cosmetic multicast happen once.

Save restore rules:

- Apply/authorize the saved role before restoring abilities.
- Resolve each saved stable ability tag and its persisted provenance once. Role-source entries must match `GrantedRoleId` and the current allowed role set; progression/unlock entries remain unowned by the role. Current loadout membership alone never rewrites provenance.
- Reconcile required role grants after restore: preserve saved level/slot/status, grant a missing required role ability once, and never duplicate an existing spec.
- A same-role pawn respawn reuses the persistent ledger and grants nothing; it only binds the new avatar and refreshes vitals/presentation.
- A different saved/requested role against an already initialized persistent ledger is rejected as an unsupported live switch.

Retire or fold the legacy `bStartupAbilitiesGiven` behavior into this ledger so two competing flags cannot disagree.

## Required automation

Add:

- `Aura.RoleBattle.Day6.AuraAppliedIdentityAndProfiles`
- `Aura.RoleBattle.Day6.BungeeAppliedIdentityAndProfiles`
- `Aura.RoleBattle.Day6.FreshCivilianDefinitionHasEmptyLoadout`
- `Aura.RoleBattle.Day6.ExplicitEquipmentOnly`
- `Aura.RoleBattle.Day6.SpawnGrantIdempotence`
- `Aura.RoleBattle.Day6.PersistentASCRespawnLedger`
- `Aura.RoleBattle.Day6.ExistingSaveGrantReconciliation`
- `Aura.RoleBattle.Day6.RemovedRoleGrantNotPromoted`
- `Aura.RoleBattle.Day6.RoleGrantSourceMetadata`
- `Aura.RoleBattle.Day6.ClientRoleMutationRejected`
- `Aura.RoleBattle.Day6.LiveRoleSwitchRejected`
- `Aura.RoleBattle.Day6.ApplicationFailureIsAtomic`

The Civilian test uses the UHT-visible, automation-only `AAuraRoleApplicationTestActor` fixture declared in `Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.{h,cpp}`. It proves a fresh compatible actor-owned ASC has actor info initialized before the transaction and that an empty configured loadout grants no LMB/offensive spec and equips no weapon. It must not apply Civilian over a live Aura/BungeeMan ASC; live switching is outside scope, and the fixture must not compile into Shipping.

The respawn test must replace the Player pawn at least twice while retaining one PlayerState ASC, then assert one spec per stable role ability tag, one passive activation, unchanged attribute maxima, matching role ledger, final pawn identity/profile replication, and no second grant notification.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day6; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day6Automation.log'

& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay6NetworkSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay6NetworkSmoke.ps1' -Mode Dedicated
```

The Day 6 runner supports `-Mode Listen` and `-Mode Dedicated`, uses isolated server/client save data, enforces bounded startup/assertion/respawn/teardown timeouts, returns nonzero on any assertion, process, crash, timeout, or missing-artifact failure, stops only processes it created, and writes `Saved/Logs/Day06-{Listen|Dedicated}-{Server|Client1|Client2}.log` plus `Saved/Reports/Day06-{Listen|Dedicated}.json`. It verifies Aura and BungeeMan final profiles (`Combat.Magic`/`Combat.Gun`), applied role/economy/interaction state, client presentation, server-only grants, two same-role respawns, an existing-save restore, client mutation rejection, and unsupported live-switch rejection. It restores or removes only fixtures it created.

## Completion gate

Aura and BungeeMan receive final server-owned identities and replicated applied profiles before gameplay grants; no combat role remains `Combat.Unassigned`. Empty loadouts and explicit equipment are deterministic. The persistent ASC ledger owns every role grant, save restoration and two respawns produce no duplicates, client mutation and live hot-switch attempts fail closed, and application failure publishes no partial identity/presentation/loadout. All named automation, UI regressions, and Day 4–6 smokes pass with recorded evidence.
