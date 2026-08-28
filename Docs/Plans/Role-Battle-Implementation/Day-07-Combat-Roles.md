# Day 07 - Evidence Gate for Aura and BungeeMan

## Goal

Prove with automation, rendered evidence, listen-server, dedicated-server, and packaged-build evidence that the role, identity, grant-ledger, combat-rule, damage-attribution, connection-scoped role request, existing single-profile save compatibility, UI, death, and respawn refactors preserve both existing combat roles.

The Civilian role definition already exists for schema/pre-grant testing. The Day 8 `AAuraCivilian` actor shell must not begin until this gate is fully green.

## BungeeMan Gun Skill checkpoint

This is the primary completion gate for the half-finished Gun Skill. Prove the configured XML path in a real activation: LMB input, target validation, face/montage/event ordering, one server projectile, physical damage and attribution, 0.2 second cooldown, replicated muzzle/impact presentation, respawn/late join, listen, dedicated, and packaged behavior. Resolve and document whether the parallel `UAuraFireGun` helper remains unused or is removed/reused; it must not create a second grant path.

## Files to inspect or adjust

### Role and ability data

- Content/Config/RoleConfig.json
- Content/Config/ProjectileDefinitions.json
- Content/AbilityDefinitions/FireBolt.xml
- Content/AbilityDefinitions/FireBlast.xml
- Content/AbilityDefinitions/ArcaneShards.xml
- Content/AbilityDefinitions/Electrocute.xml
- Content/AbilityDefinitions/FireGun.xml
- Config/DefaultGame.ini

### Runtime paths under regression

- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Player/AuraPlayerState.cpp
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AuraAbilityTypes.h
- Source/Aura/Private/AuraAbilityTypes.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp
- Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Private/Combat/AuraCombatStateComponent.cpp
- Source/Aura/Private/Combat/AuraCombatRules.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AbilityDefinition.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/DataAbility.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/PlayMontageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/WaitForMontageEventNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp

### UI/save paths under regression

- Source/Aura/Private/UI/ViewModel/MVVM_LoadScreen.cpp
- Source/Aura/Private/UI/ViewModel/MVVM_LoadSlot.cpp
- Source/Aura/Private/UI/WidgetController/AuraWidgetController.cpp
- Source/Aura/Private/UI/WidgetController/OverlayWidgetController.cpp
- Source/Aura/Private/UI/WidgetController/SpellMenuWidgetController.cpp
- Source/Aura/Public/Game/LoadScreenSaveGame.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp

### Tests and runners

- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Tests/TestDataAbility.cpp
- RunRoleBattleDay7ListenSmoke.ps1
- RunRoleBattleDay7DedicatedSmoke.ps1
- RunRoleBattleDay7PackagedSmoke.ps1

Do not make unrelated ability-system fixes under this milestone. A defect required to pass this matrix is in scope and must have a focused regression test; unrelated migration debt is recorded separately.

## Package-staging prerequisite

The ability definitions are raw XML read at runtime, not `.uasset` content. Add this packaging rule to `Config/DefaultGame.ini` alongside the existing Config/BehaviorTrees UFS rules:

```ini
+DirectoriesToAlwaysStageAsUFS=(Path="AbilityDefinitions")
```

The packaged runner must prove all five XML files appear in the staged/Pak manifest and load successfully through the same `/Game/AbilityDefinitions/*.xml` resolution used in editor. An editor-only pass is insufficient.

## Exact expected role contracts

### Aura

- Role ID `Aura`, `Combat.Magic`, `Faction.Player`, `Control.Player`, `Death.PlayerRespawn`, `Economy.None`, and `Interaction.Combatant` on server and clients.
- Staff attached to the configured body socket; `CombatSocket.Weapon` resolves to a valid staff-tip socket.
- Exactly one role-granted spec for each of FireBolt, FireBlast, ArcaneShards, and Electrocute; no BungeeMan role grant.
- FireBolt: LMB, Fire damage, mana 10, cooldown 5 seconds.
- FireBlast: Input 1, Fire damage, mana 25, cooldown 10 seconds.
- ArcaneShards: Input 2, Arcane damage, mana 20, cooldown 8 seconds.
- Electrocute: Input 3, Lightning damage, mana 5, cooldown 3 seconds.
- Every montage/event/socket required by each graph resolves on Aura’s mesh and completes or fails through the documented timeout/error path.

### BungeeMan

- Role ID `BungeeMan`, `Combat.Gun`, `Faction.Player`, `Control.Player`, `Death.PlayerRespawn`, `Economy.None`, and `Interaction.Combatant` on server and clients.
- Rifle attached to `WeaponHandSocket`; `CombatSocket.Weapon` resolves to the rifle’s `Muzzle` socket.
- Exactly one role-granted FireGun spec and no Aura role-granted offensive spec. Its configured non-LMB startup list is empty.
- FireGun: LMB, Physical damage, mana 0, cooldown 0.2 seconds, valid projectile definition, valid montage, and `Event.Montage.FireGun` observed on authority without double fire.

Both roles must reject same-faction damage in both directions, damage an Enemy, receive Enemy damage, attribute direct and delayed damage correctly, and survive save/reload plus two respawns without duplicate specs, passive activation, or attribute growth.

## Named automation

Add:

- `Aura.RoleBattle.Day7.AuraDefinitionContract`
- `Aura.RoleBattle.Day7.BungeeDefinitionContract`
- `Aura.RoleBattle.Day7.AbilityAssetMontageSocketValidation`
- `Aura.RoleBattle.Day7.ExactRoleGrantSets`
- `Aura.RoleBattle.Day7.FinalProfileAssertions`
- `Aura.RoleBattle.Day7.CostCooldownAndDamageTypes`
- `Aura.RoleBattle.Day7.SameFactionRejection`
- `Aura.RoleBattle.Day7.IsolatedSingleProfileSaveReloadAndRespawn`
- `Aura.RoleBattle.Day7.ClientCannotGrantOrDamage`
- `Aura.RoleBattle.Day7.PackagingConfigContract`

The asset validation test must distinguish body attach sockets from weapon tip sockets and validate AnimBP/mesh skeleton compatibility. The save test runs Aura and BungeeMan in separate single-profile server fixtures because authenticated per-player disk-save ownership is deferred to Day 18; each fixture performs reload and two pawn replacements and verifies the persistent ASC ledger/spec handles. `PackagingConfigContract` validates the UFS configuration before cook. The post-UAT packaged runner, not this pre-cook native test, owns the staged/Pak manifest assertion.

## Network topologies

`RunRoleBattleDay7ListenSmoke.ps1` starts one Aura listen host, exercises a death/respawn boundary, then connects one BungeeMan remote client late using its own local fixture directory and the Day 05 `Role=` request. It verifies both connection-scoped role/profile replications, late-join presentation/life state, cross-player friendly rejection, Enemy combat in both directions, montage/projectile observation, UI ability lists, and respawn. It does not treat either client's local slot as server progression identity.

`RunRoleBattleDay7DedicatedSmoke.ps1` builds/starts one dedicated server, connects Aura first, exercises a death/respawn boundary, and then connects BungeeMan late. It repeats the listen assertions, attempts client-originated invalid damage/grant/role mutations from both clients, and checks server authority plus matching client state. It stops only processes it creates.

`RunRoleBattleDay7PackagedSmoke.ps1` runs the staged Development dedicated server, an initial Aura client, and a late BungeeMan client; validates the staged/Pak XML manifest after `BuildCookRun`; then repeats the essential role/profile/grant/combat/respawn and invalid client-mutation checks without relying on source-tree Content files.

All three topology runners capture actual rendered Aura and BungeeMan body/AnimBP, weapon attachment, muzzle/tip, and montage frames from a non-NullRHI client. Retain deterministic captures at `Saved/RoleBattle/Day7/AuraRendered.png` and `Saved/RoleBattle/Day7/BungeeManRendered.png` plus the source client logs; these close Day 01's rendered debt.

Required logs/artifacts:

- `Saved/Logs/Day7Automation.log`
- `Saved/Logs/Day7AbilityGraph.log`
- `Saved/Logs/Day7ListenServer.log`
- `Saved/Logs/Day7ListenClient.log`
- `Saved/Logs/Day7DedicatedServer.log`
- `Saved/Logs/Day7DedicatedAuraClient.log`
- `Saved/Logs/Day7DedicatedBungeeClient.log`
- `Saved/Logs/Day7PackagedServer.log`
- `Saved/Logs/Day7PackagedAuraClient.log`
- `Saved/Logs/Day7PackagedBungeeClient.log`
- `Saved/RoleBattle/Day7/PackageManifest.txt`
- `Saved/RoleBattle/Day7/Evidence.md`
- `Saved/RoleBattle/Day7/AuraRendered.png`
- `Saved/RoleBattle/Day7/BungeeManRendered.png`

Every Day 7 runner uses bounded build/startup/assertion/late-join/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, package, or missing-artifact failure, and stops only processes it created. The retained `Saved/Logs/Day7*.log`, `Saved/RoleBattle/Day7/Evidence.md`, and packaged manifest are summarized under `Saved/Reports/Day07-{Listen|Dedicated|Packaged}.json`.

## Exact execution commands

Run from the repository root:

```powershell
& '.\build_test.bat'
& '.\BuildDedicatedServer.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day7Automation.log'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" '.\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi '-abslog=Saved/Logs/Day7AbilityGraph.log'

& '.\RunRoleBattleDay1Smoke.bat'
& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay4DamageSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay5ConfigSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay6NetworkSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay6NetworkSmoke.ps1' -Mode Dedicated
& '.\RunRoleBattleDay7ListenSmoke.ps1'
& '.\RunRoleBattleDay7DedicatedSmoke.ps1'

$projectPath = (Resolve-Path '.\Aura.uproject').Path
$savedPath = (Resolve-Path '.\Saved').Path
$archivePath = Join-Path $savedPath 'RoleBattleDay7Package'
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$projectPath" -nop4 -utf8output -build -cook -stage -pak -archive "-archivedirectory=$archivePath" -map=/Game/Maps/StartupMap -server -client -serverplatform=Win64 -platform=Win64 -serverconfig=Development -clientconfig=Development

& '.\RunRoleBattleDay7PackagedSmoke.ps1' -ArchivePath $archivePath
```

Record every exit code, engine/commit identifier, runner-generated process IDs, exact log path, save-fixture path, and package manifest in `Saved/RoleBattle/Day7/Evidence.md`. A timeout, missing expected assertion, unexpected crash, or manually killed process is a failure, not a pass inferred from partial log text.

## Evidence matrix

Leave applicable result cells blank until the corresponding artifact has been inspected. Enter `Pass` or `Fail` plus an artifact reference; never prefill expected results as observed results. A literal `N/A` may be predeclared only when that topology cannot contain the artifact by definition.

| Test | Expected | Listen result | Dedicated result | Packaged result | Evidence |
| --- | --- | --- | --- | --- | --- |
| Aura visual/equipment/socket | Aura mesh/AnimBP, staff, valid tip |  |  |  |  |
| Aura final profiles | Magic/Player/PlayerRespawn/None/Combatant |  |  |  |  |
| Aura exact grants | FireBolt + three active definitions, once each |  |  |  |  |
| Aura cost/cooldown/damage | Exact values above |  |  |  |  |
| Bungee visual/equipment/socket | Bungee mesh/AnimBP, rifle, Muzzle |  |  |  |  |
| Bungee final profiles | Gun/Player/PlayerRespawn/None/Combatant |  |  |  |  |
| Bungee exact grants | FireGun once; no Aura role grants |  |  |  |  |
| Bungee cost/cooldown/damage | 0 mana, 0.2 s, Physical |  |  |  |  |
| Same-faction rejection | Aura/Bungee cannot damage each other |  |  |  |  |
| Enemy combat | Both roles deal/receive authoritative damage |  |  |  |  |
| Attribution | Direct and periodic source/role/ability/type correct |  |  |  |  |
| Client mutation rejection | No client role/grant/damage authority |  |  |  |  |
| New-player persistence | Role/grants/attributes correct after save/reload |  |  |  |  |
| Existing-save persistence | Stable role restored with no duplicate specs |  |  |  |  |
| Two respawns | Same profiles; stable vitals and grant ledger |  |  |  |  |
| UI role/ability state | Local and remote displays match server state |  |  |  |  |
| Late join | Delayed client reconstructs role/identity/life/presentation |  |  |  |  |
| Rendered presentation | Captures prove body/AnimBP/weapon/socket/montage |  |  |  |  |
| XML package staging | Five definitions in manifest and runtime-loadable | N/A: no package | N/A: no package |  |  |

`N/A` is permitted only where the expected column explicitly states that the check is inapplicable, as for package contents in unpackaged listen/dedicated runs. Every other topology cell must be `Pass` or `Fail` with evidence; mutation probes run in all three topologies.

## Completion gate

Day 7 passes only when every applicable matrix cell is `Pass` with a valid artifact reference, every predeclared `N/A` has its stated reason, every named automation and prior smoke exits zero, both rendered captures are retained, a delayed client reconstructs current role/identity/life/presentation, the package contains and loads all ability XML, server/client final profile assertions show `Combat.Magic` and `Combat.Gun`, exact grant sets remain idempotent through the isolated compatibility saves and two respawns, and no client can author role, grant, or damage state. Any unexplained blank/N/A blocks the Day 8 Civilian actor.
