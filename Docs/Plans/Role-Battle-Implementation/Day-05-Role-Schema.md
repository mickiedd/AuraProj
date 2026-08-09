# Day 05 - Add a Versioned, Validated Role Schema

## Goal

Make `RoleConfig.json` an explicit, versioned, safely parsed definition of Aura, BungeeMan, and Civilian. Invalid data must produce one aggregate report, never a fatal JSON getter, partial role, or silent overwrite. The server publishes a candidate only after full validation and retains the last known-good config when a reload fails.

Day 5 defines data; Day 6 is the only milestone that applies the validated role identity and loadout to a runtime actor.

## BungeeMan Gun Skill checkpoint

Make `lmbAbilityDefinition: /Game/AbilityDefinitions/FireGun.xml` a validated BungeeMan role contract. Validation must reject a missing/unparseable definition, invalid LMB input/tag, missing projectile definition, or incompatible weapon-tip socket atomically; a bad reload must not publish a partial BungeeMan Gun Skill.

## Files to modify

### Runtime schema, parser, tags, and publication

- Source/Aura/Public/AbilitySystem/Data/RoleInfo.h
- Source/Aura/Private/AbilitySystem/Data/RoleInfo.cpp
- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AuraGameplayTags.h
- Source/Aura/Private/AuraGameplayTags.cpp
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
- Source/Aura/Public/Game/LoadScreenSaveGame.h
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp

### UI/editor validation surfaces

- Source/Aura/Public/UI/ViewModel/MVVM_LoadSlot.h
- Source/Aura/Private/UI/ViewModel/MVVM_LoadSlot.cpp
- Source/Aura/Public/UI/ViewModel/MVVM_LoadScreen.h
- Source/Aura/Private/UI/ViewModel/MVVM_LoadScreen.cpp
- Source/AuraEditor/Private/AuraEditorModule.cpp

### Config, tests, and runner

- Content/Config/RoleConfig.json
- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp
- RunRoleBattleDay5ConfigSmoke.ps1

Inline JSON strings in `AuraRoleBattleTests.cpp` are the authoritative malformed/legacy test fixtures; do not add hidden external fixture files.

## Version 2 JSON contract

Add required top-level integer `roleDefinitionVersion`. Day 5 writes version `2`. The parser supports the shipped legacy/version-1 flat weapon fields only as an explicit migration input; it normalizes them into version-2 runtime data and reports deprecation. Version 2 rejects a role that mixes nested and legacy equipment fields, so precedence is never ambiguous.

### Legacy/version-1 migration contract

An omitted `roleDefinitionVersion` is detected as version `1`, not treated as version `2` and not silently discarded. An explicit `roleDefinitionVersion: 1` follows the same migration path and emits the same deprecation warning. Version 1 accepts the current flat fields, then produces the version-2 shape before validation/publication:

| Legacy input | Normalized version-2 field | Rule |
| --- | --- | --- |
| `weaponMesh` | `equipment.weaponMesh` | Copy the asset path; an empty value means no equipment. |
| `weaponSocket` | `equipment.attachSocket` | Copy the socket name; an empty value means unset. |
| `weaponTipSocket` | `equipment.tipSocket` | Copy the socket name; an empty value means unset. |
| `leftHandSocket`, `rightHandSocket`, `tailSocket` | Same named optional role fields | Preserve when present; empty values normalize to absent optional fields. |
| Missing ability arrays | The corresponding version-2 arrays | Normalize to empty arrays. |
| Missing optional strings | The corresponding version-2 optional strings | Normalize to empty strings/absent optional fields, never inferred from the mesh. |

For the two shipped legacy roles, missing identity/profile fields use this fixed compatibility map. These defaults apply only to version-1 migration and are not role-name inference for version 2:

| Legacy role | `entityType` | `controlType` | `combatProfile` | `faction` | `deathPolicy` | `economyProfile` | `interactionProfile` | Flags |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `Aura` | `Entity.Player` | `Control.Player` | `Combat.Magic` | `Faction.Player` | `Death.PlayerRespawn` | `Economy.None` | `Interaction.Combatant` | `playerSelectable=true`, `targetable=true`, `canAttack=true`, `canBeDamaged=true`, `allowFriendlyFire=false` |
| `BungeeMan` | `Entity.Player` | `Control.Player` | `Combat.Gun` | `Faction.Player` | `Death.PlayerRespawn` | `Economy.None` | `Interaction.Combatant` | `playerSelectable=true`, `targetable=true`, `canAttack=true`, `canBeDamaged=true`, `allowFriendlyFire=false` |

No other legacy role receives identity/profile defaults. A missing required legacy field outside this table is an aggregate validation error. A version-1 object containing `equipment` is rejected as an ambiguous mixed shape; version 2 must use `equipment` and must provide every required identity/profile/flag field explicitly. The normalized result records `detectedVersion: 1`, `publishedVersion: 2`, and the deprecation issue so callers and the editor can distinguish migrated data from authored version 2.

Each role has these required fields:

- `role`: stable internal `FName`; retain `Aura` and `BungeeMan` for save compatibility.
- `displayName`: non-empty user-facing text.
- `entityType`: full Gameplay Tag string, initially `Entity.Player` or `Entity.AmbientNPC`.
- `controlType`: full Gameplay Tag string such as `Control.Player` or `Control.CivilianAI`.
- `combatProfile`: full Gameplay Tag string such as `Combat.Magic`, `Combat.Gun`, or `Combat.Civilian`.
- `faction`: full Gameplay Tag string such as `Faction.Player` or `Faction.Civilian`.
- `deathPolicy`: full Gameplay Tag string such as `Death.PlayerRespawn` or `Death.PopulationRespawn`.
- `economyProfile`: full Gameplay Tag string used only as a capability/default classification, initially `Economy.None`, `Economy.Ambient`, or `Economy.CommerceCapable`.
- `interactionProfile`: full Gameplay Tag string such as `Interaction.Combatant` or `Interaction.Civilian`.
- `playerSelectable`, `targetable`, `canAttack`, `canBeDamaged`, and `allowFriendlyFire`: explicit booleans.
- `mesh`, `animBlueprint`, and all four numeric primary attributes.
- `startupAbilities`, `startupPassiveAbilities`, `startupAbilityDefinitions`, and `startupPassiveAbilityDefinitions`: arrays that are present even when empty.
- `unlockableAbilities`: a present array of progression definitions/classes that are valid for the role but are never granted merely because the role spawns.
- `lmbAbility` and `lmbAbilityDefinition`: present strings; at most one may be non-empty.

`equipment` is optional and contains `weaponMesh`, `attachSocket`, and `tipSocket`. Absence means no equipment. Combat socket names not owned by equipment remain explicit optional role fields. Death presentation, work/spawn/voice profiles, and other future content remain optional safe-parsed fields.

Gameplay Tag strings are stored in JSON exactly as registered; do not accept shorthand such as `"Player"` and do not infer `Control.Player` from `Entity.Player`. Register the supported Entity, Economy, and Interaction tags through `FAuraGameplayTags`. Unknown or category-mismatched tags are validation errors.

`economyProfile` does not identify a concrete merchant, job, offer list, or population member. In particular, do not add or infer `Economy.Merchant` as instance identity. A stable `MerchantDefinitionId`, when applicable, belongs to population-member data introduced by the Day 9/15 population and economy work, not to the shared Civilian role definition.

`playerSelectable` is authoritative metadata. The default role must exist, be valid, use `Entity.Player`/`Control.Player`, and be player-selectable. Civilian is ambient-only and must set `playerSelectable: false`.

Civilian's offensive `unlockableAbilities` set is empty. Validate every unlockable class/path, stable ability tag, role compatibility, and duplicate against startup/LMB entries, while keeping unlockable/progression provenance separate from spawn grants.

## Ownership and compatibility rules

RoleConfig is the source of the role-derived combat profile, faction, control, death policy, flags, economy capability/default classification, and interaction profile. The actor shell supplies a compatibility constraint, not an alternate value: for example, `AAuraCharacter` accepts only Player entity/control roles, while the future `AAuraCivilian` accepts only AmbientNPC/CivilianAI roles. Day 6 rejects a mismatch rather than choosing one source silently.

Saved games continue to store the stable role ID, not a serialized copy of the role definition. A saved unknown, invalid, or non-player-selectable role is rejected with an actionable login error; it is never silently replaced by Aura.

Before this gate closes, select and record real project asset paths for the Civilian mesh and compatible Anim Blueprint. Do not silently reuse Aura/BungeeMan assets as a placeholder. If those assets are not yet available, Day 5 remains blocked with that explicit prerequisite.

## Safe aggregate parser and validator

Define `FAuraRoleValidationIssue` with severity, role ID (or top-level scope), JSON field/path, and message. Define a load result containing candidate `URoleInfo`, detected version, all issues, and `bCanPublish`.

Requirements:

1. Use `TryGet*` for every field, including required fields. Missing, wrong-type, non-finite, or malformed values append issues and parsing continues where safe.
2. Detect duplicate role IDs before insertion into `TMap`; no last-entry-wins behavior.
3. Keep original asset/path strings long enough to report exact failures.
4. Validate mesh and AnimBP loading and skeleton compatibility.
5. Validate the body attach socket on the body mesh and the tip socket on the weapon mesh; a socket is a name, not an asset path.
6. Validate startup, passive, definition, LMB, and unlockable ability class/definition paths; duplicate ability tags/input slots; offensive capability; grant category; and mutually exclusive LMB fields. Unlockables are validated but never spawn-granted.
7. A role with `canAttack: true` needs at least one valid offensive grant. A role with `canAttack: false` must have no offensive LMB/startup definition.
8. Validate supported entity/control/profile/faction/death/economy/interaction tags and compatible combinations.
9. Validate default-role existence/selectability and all asset/numeric requirements.
10. Report all issues in one pass. Warnings may publish; any error makes `bCanPublish` false.
11. Cover omitted-version legacy input, explicit version-1 input, both shipped role mappings, missing optional fields, mixed equipment rejection, and deterministic normalized output in the inline test fixtures.

## Atomic publication and rejection behavior

- Startup timing: `AAuraGameModeBase::InitGame` parses and validates into a candidate before any `PreLogin`, and publishes it only when `bCanPublish` is true. `BeginPlay` is too late for a login prerequisite. If there has never been a good config, mark the authoritative role service unavailable.
- Reload sentinel: keep the current server and client config alive while loading the candidate. On any validation error, retain the last good pointer/cache, log the aggregate report once, and expose failure to the editor UI. Never assign `nullptr` or a partial candidate before success is known.
- Per-connection role request: the travel/login flow carries one `Role=<StableRoleId>` connection option sourced from its selected local slot/default. `PreLogin` parses that ID, rejects a missing/unknown/non-player-selectable/non-Player role, and never accepts client asset paths, tags, ability lists, or saved progression. `InitNewPlayer` re-parses and revalidates the same options, stores one `PendingAcceptedRoleId` on that connection's PlayerState, and gates `HandleStartingNewPlayer`/`RestartPlayer` until it exists. Day 06 consumes it once. No client shares or reads the process-global `UAuraGameInstance::LoadSlotName/LoadSlotIndex` as server player ownership.
- Persistence boundary: until Day 18 introduces authenticated stable profile identity, the accepted role is connection-scoped. Local save directories used by Day 07 are isolated fixture inputs and do not authorize server progression or identify another player. Production multiplayer disk-save ownership is not claimed on Day 05.
- Client UI: local parsing supports presentation only. The existing load-slot UI validates the saved/default role ID, excludes/rejects a non-player-selectable ID, and displays the server rejection reason through the existing login-alert path. Day 05 does not add a role-picker widget; a future picker may submit another validated role ID through the same request contract.
- Existing actors are not hot-swapped by an editor reload. The candidate applies to later logins/spawns; live switching remains disabled.

## Required automation

Add:

- `Aura.RoleBattle.Day5.ValidVersion2Schema`
- `Aura.RoleBattle.Day5.LegacyVersion1Migration`
- `Aura.RoleBattle.Day5.AggregateValidationErrors`
- `Aura.RoleBattle.Day5.DuplicateRoleIds`
- `Aura.RoleBattle.Day5.FullGameplayTagContract`
- `Aura.RoleBattle.Day5.EquipmentAndSocketValidation`
- `Aura.RoleBattle.Day5.CivilianEmptyLoadout`
- `Aura.RoleBattle.Day5.DefaultRoleSelectable`
- `Aura.RoleBattle.Day5.AtomicReloadRetainsLastGood`
- `Aura.RoleBattle.Day5.ServerRejectsInvalidConfigOrRole`
- `Aura.RoleBattle.Day5.ConnectionScopedRoleRequest`
- `Aura.RoleBattle.Day5.LoadScreenRoleValidation`
- `Aura.RoleBattle.Day5.SavedRoleIdCompatibility`

The aggregate-error fixture must omit required fields, use wrong JSON types, duplicate a role, use category-mismatched tags, and reference invalid assets/abilities; the test passes only if all expected issues are returned without an assertion/crash. The atomic-reload test must prove object identity/data from the last good config remains available after a bad candidate.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day5; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day5Automation.log'

& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Dedicated
```

The Day 5 config smoke supports `-Mode Listen` and `-Mode Dedicated`. It enforces bounded startup/reload/assertion/teardown timeouts, returns nonzero on any assertion, process, crash, timeout, or missing-artifact failure, stops only processes it created, and writes `Saved/Logs/Day05-{Listen|Dedicated}-{Server|Client1|Client2}.log` plus `Saved/Reports/Day05-{Listen|Dedicated}.json`. It verifies `InitGame` publication before `PreLogin`, valid startup, rejected invalid startup, failed sentinel reload retaining the prior config, successful sentinel reload publishing atomically, two simultaneous connections retaining different accepted role IDs, invalid-role login rejection, and saved/default role validation without a new picker. It restores the original RoleConfig and sentinel state in a `finally` block and edits no user save permanently.

## Completion gate

Version-2 RoleConfig loads Aura, BungeeMan, and Civilian with full Gameplay Tags, explicit flags, and separated spawn/unlockable ability categories; real Civilian presentation assets are valid; version-1 input migrates deterministically; all malformed input yields aggregate diagnostics without crashing; a bad reload retains the last good config; `InitGame` publishes before login; and two connections can retain different server-validated pending roles without using the process-global save slot. The existing UI accepts only a valid saved/default player role and adds no fictitious picker. All named tests and both Day 4 and Day 5 smokes pass with recorded artifacts.
