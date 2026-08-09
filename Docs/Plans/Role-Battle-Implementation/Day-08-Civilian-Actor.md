# Day 08 - Add the Civilian Actor

## Goal

Create a replicated `AAuraCivilian` actor shell that reuses the common GAS, health, damage, combat-state, and role-presentation infrastructure without inheriting player or enemy behavior.

Day 08 consumes the validated Civilian entry produced by Day 05 and the deterministic role-application path from Day 06. It must not duplicate Civilian identity values in the actor class, add Civilian to `ECharacterClass`, or depend on the target/interaction interface introduced on Day 14.

## Prerequisite gate

- The Day 04 final damage boundary accepts a non-enemy target without requiring a Civilian `ECharacterClass` value.
- The Day 05 Civilian role passes validation and supplies entity type, faction, control type, combat profile, attack/damage flags, death policy, mesh, animation, explicit no-equipment state, attributes, and empty offensive loadout.
- Day 06 can clear and apply an empty role loadout deterministically and exposes a server-only role-derived identity initialization path.
- Day 07 multiplayer regression remains green.

## New files

- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Content/Blueprints/Character/Civilian/BP_AuraCivilian.uasset`
- `Content/Blueprints/UI/Character/WBP_CivilianDebugHealthBar.uasset`
- `Content/Maps/RoleBattleCivilianTest.umap`
- `RunRoleBattleDay8NetworkSmoke.ps1`

## Files to modify

- `Source/Aura/Public/Character/AuraCharacterBase.h`
- `Source/Aura/Private/Character/AuraCharacterBase.cpp`
- `Source/Aura/Public/Combat/AuraCombatIdentityComponent.h`
- `Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Config/RoleConfig.json` only if the Day 05 Civilian entry needs an asset-path correction discovered while building the actor fixture

`ExecCalc_Damage.cpp` is verification-only on Day 08. The safe non-`ECharacterClass` calculation path belongs to Day 04 and must not be reimplemented here.

## Concrete actor and replication contract

- `AAuraCivilian` derives directly from `AAuraCharacterBase`.
- The constructor creates exactly one `UAuraAbilitySystemComponent` and one `UAuraAttributeSet`. The ASC replicates with `Minimal` replication mode, matching the existing non-player avatar pattern.
- `RequestedCivilianRoleId` is an authority-only `ExposeOnSpawn`/class-default `FName` whose default is `Civilian`. It is only an input to Day 06 `ApplyRoleAtSpawn` and is never a second replicated role property or runtime switch API.
- Manually placed civilians use the fixed class-default request. A deferred population spawn sets the request before `FinishSpawning`. Day 06 `FAuraAppliedRoleState` is the one replicated applied-role source, and `OnRep_AppliedRoleState` drives presentation for existing and late-joining clients.
- `ApplyRoleAtSpawn` resolves the validated role definition identified by the requested role and copies its faction, control type, combat profile, death policy, `bTargetable`, `bCanAttack`, `bCanBeDamaged`, and friendly-fire policy into `FAuraCombatIdentity`. Do not hardcode a second Civilian identity table in `AAuraCivilian`.
- A role that is missing, is not an ambient Civilian entity, has `bCanAttack=true`, contains an offensive ability, or has an incompatible death policy fails closed on authority: log one actionable error, leave the ASC ungranted, disable damage/AI, and reject population registration.
- `AAuraCivilian` does not implement `EnemyInterface` and does not add Player, Enemy, or Civilian actor tags. Until Day 14, generic combat consumers use the existing combat identity/state components and `ICombatInterface` only.

## Initialization order

Use this exact order so server and client actor state cannot diverge:

1. The constructor creates the ASC, AttributeSet, replicated debug-health widget component, and fixed Civilian defaults. It does not grant abilities or apply Gameplay Effects.
2. A deferred server spawn sets `RequestedCivilianRoleId` before `FinishSpawning`. A placed actor uses the class default.
3. On every machine, call `AbilitySystemComponent->InitAbilityActorInfo(this, this)` and `AbilityActorInfoSet` once for this owner/avatar, then broadcast `OnAscRegistered`. These calls bind GAS only and grant/apply nothing.
4. On authority, call Day 06 `ApplyRoleAtSpawn` exactly once. That transaction exclusively resolves/validates the role, commits `FAuraAppliedRoleState` and combat identity, applies server presentation, initializes actor-owned role attributes, and reconciles the explicitly empty grant ledger. Do not repeat attributes or grants in `AAuraCivilian`.
5. Verify the committed `FAuraCombatIdentity`, positive vitals, and empty loadout match the applied role before allowing population registration or later AI.
6. On clients, `OnRep_AppliedRoleState` calls only Day 06 `ApplyRolePresentation`; it never initializes attributes, grants abilities, or mutates identity. The path is idempotent and late-join safe.
7. Bind health delegates after actor info is valid; authority binds after the transaction, while clients accept initial/delta attribute replication without reinitialization. The debug-health widget is cosmetic and hidden on dedicated servers.

`OnRep_AppliedRoleState` and late-join reconciliation may reapply presentation, but must never reinitialize attributes, grant abilities, or mutate identity on a client.

## Implementation steps

1. Add `AAuraCivilian` with replicated ASC/AttributeSet ownership and the role-ID contract above.
2. Reuse Day 06 `ApplyRoleAtSpawn`/`FAuraAppliedRoleState` and its replicated presentation hook without adding a Civilian-only replicated role field.
3. Read every Civilian identity field from the validated role definition.
4. Configure `BP_AuraCivilian` with no player possession, no enemy behavior tree, no loot table, and no player respawn callback.
5. Set collision for replicated character movement, visibility tracing, and the existing projectile channel. Do not add Day 14 highlight/interaction code.
6. Set `AIControllerClass` to null and `AutoPossessAI` to disabled for Day 08; Day 10 owns Civilian AI.
7. Add the debug health bar using the existing `UAuraUserWidget` controller pattern.
8. Ensure direct calls to `AddCharacterAbilities` cannot grant an inherited Blueprint default to an empty Civilian role.
9. Add rate-limited diagnostics containing actor, role ID, ASC owner/avatar, identity fields, Health/MaxHealth, authority, and net mode.
10. Add the native and network tests below.

## Native automation

Add these tests to `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`:

- `Aura.RoleBattle.Day8.CivilianRoleIdentity`
- `Aura.RoleBattle.Day8.CivilianASCInitialization`
- `Aura.RoleBattle.Day8.EmptyOffensiveLoadout`
- `Aura.RoleBattle.Day8.InvalidRoleFailsClosed`
- `Aura.RoleBattle.Day8.PresentationIdempotence`
- `Aura.RoleBattle.Day8.DefaultDamageDeniedAndTrustedFixtureAllowed`

The tests must assert that identity equals the Civilian role record, ASC owner/avatar are the Civilian, Health and MaxHealth are positive after authority initialization, no offensive/LMB spec exists, and a second presentation application changes no gameplay state.

## Listen-server and dedicated-server smoke

`RunRoleBattleDay8NetworkSmoke.ps1` must support `-Mode Listen` and `-Mode Dedicated` and use `Content/Maps/RoleBattleCivilianTest.umap`. Each mode starts one server and two clients, then:

1. Spawns one Civilian on authority.
2. Verifies exactly one replicated identity component, ASC, and AttributeSet.
3. Compares server/client role ID, every identity field, mesh, AnimBP, weapon-empty state, Health, and MaxHealth.
4. Proves the default policy rejects Civilian damage, then installs Day 03's `WITH_DEV_AUTOMATION_TESTS` authority-only trusted policy fixture, applies one allowed server-authored damage event, and observes the same resulting Health on both clients. A client attempt to supply the same permission remains rejected.
5. Confirms neither client possesses the Civilian or can activate an offensive ability.
6. Connects a late client after spawn and verifies it reconstructs the same role presentation and attributes.
7. Confirms no player respawn, enemy loot, or enemy AI path is entered.

The runner enforces bounded startup, assertion, late-join, and teardown timeouts; returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure; and stops only processes it created. It writes `Saved/Logs/Day08-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day08-{Listen|Dedicated}.json` with revision, commands, exit codes, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day8; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day08Automation.log'

& '.\RunRoleBattleDay8NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay8NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

One role-derived Civilian can be placed or server-spawned, has a correctly initialized replicated ASC and positive vitals, defaults to protected, receives valid damage only under the authority-side non-shipping policy fixture before Day 12, owns no offensive ability, never enters Player/Enemy-specific behavior, and reconstructs identical identity and presentation for existing and late-joining clients. All build, native, listen-server, and dedicated-server gates pass.
