# Role Battle Day 2 - Combat Identity Report

Date: 2026-08-08 (Asia/Shanghai)

## Status

Complete. Day 2's implementation, native automation, standalone regression, and real server/client replication gates pass.

## Implemented

- Added `FAuraCombatIdentity`, containing faction, control type, combat profile, death policy, targetable, can-attack, can-be-damaged, and friendly-fire state.
- Added the replicated, server-owned `UAuraCombatIdentityComponent` with generic `AActor` lookup, validation, client `OnRep` logging, and rate-limited missing-identity diagnostics.
- Registered Player, Enemy, Civilian, control, combat-profile, and death-policy native gameplay tags. `GameplayTags` is now a public Aura module dependency because the public identity structure exposes `FGameplayTag`.
- Added one identity component to `AAuraCharacterBase`. Runtime default builders resolve native tags after startup initialization, avoiding invalid tag values baked into early class default objects.
- Configured exact Player and Enemy defaults. `Combat.Unassigned` remains the explicit transitional profile until the role schema/application work on Days 5-6.
- Migrated `UAuraAbilitySystemLibrary::IsNotFriend` from Player/Enemy actor tags to the bounded Day 2 identity truth table, including null, self, Civilian, missing, and invalid identity rejection.
- Migrated `BTService_FindNearestPlayer` from PlayerController/tag fallback selection to valid, targetable `Faction.Player` identity selection while preserving its blackboard contract.
- Added three Day 2 native automation tests and a repeatable hidden two-process network smoke runner.

`AuraEffectActor.cpp` and its pickup-specific `bApplyEffectsToEnemies` filter remain unchanged and explicitly deferred to Day 20. `AuraPlayerState` remains unchanged by Day 2.

## Verification evidence

| Gate | Result | Evidence |
| --- | --- | --- |
| AuraEditor build | Pass, exit 0 | `build_test.bat` completed the Development Editor target |
| Focused Day 2 automation | 3/3 passed, exit 0 | `Saved/Logs/Day2Automation.log`; `Aura.RoleBattle.Day2.*` |
| Full native Aura automation | 17/17 passed, exit 0 | `Saved/Logs/Day2AuraRegression.log` |
| Day 1 runtime regression | Pass, exit 0 | `Saved/Logs/Aura-backup-2026.08.07-16.16.33.log`; damage directions and two stable respawns |
| Day 2 server/client network smoke | Pass, exit 0 | `Saved/Logs/Day2NetworkServer.log` and `Saved/Logs/Day2NetworkClient.log` |
| AuraAbilityGraph smoke | 22/22 passed, exit 0 | `Saved/Logs/Aura.log` |
| Diff whitespace check | Pass | `git diff --check` reports no errors |

The network client received matching Player and Enemy identities through `OnRep_Identity`. The server recorded a successful client join and both enemy behavior services selected the replicated Player with `Candidates=1`.

## Commands

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day2; Quit' '-TestExit=Automation Test Queue Empty' -log

& '.\RunRoleBattleDay1Smoke.bat'
& '.\RunRoleBattleDay2NetworkSmoke.ps1'
& '.\RunSmokeTest.bat'
```

## Non-blocking existing log noise

- Commandlet automation logs contain the existing AuraEditor toolbar `RegisterMenus` errors; all requested automation tests still complete successfully with exit code 0.
- The dedicated-server network fixture warns that `StartupMap` has no `levelId` mapping for the existing GSM ready notification. The server still accepts the client, completes login/join, replicates identities, and runs identity-based enemy targeting. This warning predates and is outside Day 2 combat identity scope.

## Completion gate

Pass. Source/target combat avatars have valid server-owned identities, clients receive matching identities, migrated relationship/targeting paths no longer read Player/Enemy actor tags, and every required regression and smoke gate is green.
