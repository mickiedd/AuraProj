# Role Battle Baseline Report — 2026-08-07

## Result

The initial executable baseline recorded the assets and map state available to that run. The current-checkout follow-up now passes the complete headless Day 1 functional gate: Login and BungeeMan assets load, both damage directions reduce health, and two respawns preserve the original health/mana maxima. Only rendered/editor visual confirmation remains outside the headless smoke coverage.

No gameplay source code was changed during the initial baseline run. The Day 1 follow-up below contains the subsequent source fixes and verification results.

## Environment

- Original baseline repository: `D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj`
- Current follow-up checkout: `C:\Git\AuraProj`
- Baseline commit: `a26d241391202cd9195bafafcb7c651e3d8c3074`
- Current follow-up commit: `f7119130ff1e0dd7744044c43a5947d38ca81c51`
- Original baseline engine: Unreal Engine 5.5 at `D:\UE_5.5`
- Current follow-up engine: Unreal Engine 5.5 at `C:\Git\UnrealEngine-5.5`
- Build target: `AuraEditor Win64 Development`
- Working tree already contained unrelated documentation changes and untracked plan documents.

## Fixture and asset findings

`/Game/Maps/StartupMap` is the reproducible headless fixture. It contains the Aura player, `PlayerStart_1`, navigation data, `BP_Demon_Ranger_C_1`, and `BP_Shaman_C_1`. The map loaded successfully in game mode, possessed the Aura character, and possessed both enemy AI controllers.

The original baseline log reported that the configured editor/game startup map `/Game/Maps/Login.Login` was unavailable. In the current follow-up checkout, `Content/Maps/Login.umap` is present; a direct load reached `/Game/Maps/Login.umap` and reported zero map-check errors and warnings. `Content/Maps/StartupMap.umap` remains the reproducible gameplay fixture used for the checks below.

The original baseline log also reported unresolved BungeeMan role references. In the current follow-up checkout, all three configured asset paths are present:

- `/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan`
- `/Game/BungeeMan/Blueprints/ABP_Bungee.ABP_Bungee_C`
- `/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B.Assault_Rifle_B`

`FireGun.xml` is present and parses. The StartupMap follow-up log loaded the BungeeMan role and its `FireGun` definition. The rendered mesh/animation, rifle socket, projectile, damage, and cooldown behavior still require the requested visual and weapon pass.

## Checks executed

### Build

```powershell
& 'D:\UE_5.5\Engine\Build\BatchFiles\Build.bat' AuraEditor Win64 Development -Project='D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj\Aura.uproject' -WaitMutex
```

Result: passed, exit code `0`.

Non-fatal existing warnings include the preferred Visual Studio compiler version, missing AuraAbilityGraph dependency declarations, an Aura/AuraAbilityGraph circular build dependency, and GameplayAbilities deprecation warnings.

### Native automation

```powershell
& 'D:\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj\Aura.uproject' '-ExecCmds=Automation RunTests Aura; Quit' -unattended -nop4 -nullrhi -log
```

Result: passed, exit code `0`; the log reported `TEST COMPLETE. EXIT CODE: 0`.

### AuraAbilityGraph smoke test

```powershell
& 'D:\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi -log
```

Result: `22 passed, 0 failed`.

### Map load and headless gameplay

`StartupMap` loaded successfully in `-game` mode. The player was possessed by `BP_AuraPlayerController_C_0`; both enemy AI controllers were also possessed with authority.

The bounded AutoRun pass activated all four Aura abilities:

- `FireBlast`: successful activation and 12 fireball actors spawned.
- `ArcaneShards`: successful activation and shard spawning through the point collection.
- `Electrocute`: successful activation with cost/cooldown application.
- `FireBolt`: successful activation and projectile spawning.

The final bounded run recorded zero `TryActivateAbility FAILED` entries. It also recorded two player deaths followed by two server respawns, each after the configured five-second delay.

The respawn pass exposed a baseline defect: fallback attributes accumulated across respawns instead of resetting. The observed values were:

| State | Health | Mana |
| --- | ---: | ---: |
| Initial spawn | 100 / 100 | 50 / 50 |
| After first respawn | 200 / 200 | 100 / 100 |
| After second respawn | 300 / 300 | 150 / 150 |

The AutoRun pass did not provide clean damage attribution to an enemy. FireBolt projectiles hit a static mesh actor without an Ability System Component and were destroyed; no unambiguous player-to-enemy damage result was recorded. This keeps the enemy-damage and PvP-direction checks open.

The headless run was deliberately bounded and then terminated after evidence collection. Manual visual checks were therefore not performed.

## Day 1 follow-up implementation

The baseline defects that were actionable in the current checkout were addressed after the initial report:

- `AAuraPlayerState` now records whether its persistent Ability System Component has received initial attributes.
- `AAuraCharacter::LoadProgress` initializes default/save attributes and startup abilities only once per persistent player state. Later pawn possessions refill current health/mana without reapplying additive default effects, preventing the observed 200/100 and 300/150 growth.
- All aggregate SetByCaller attribute GameplayEffect specs now receive zero defaults before selective values are assigned. A fresh `StartupMap` game-mode load no longer reports the baseline `GetMagnitude ... magnitude had not yet been set by caller` errors.

The checked-in `RunRoleBattleDay1Smoke.bat` driver now repeats the functional regression on `StartupMap`: it validates BungeeMan asset/FireGun wiring, applies live player-to-enemy and enemy-to-player damage, forces two server respawns, and checks the persistent ASC vitals after each replacement pawn.

### Follow-up verification

```powershell
& '.\build_test.bat'
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' '-ExecCmds=Automation RunTests Aura; Quit' -unattended -nop4 -nullrhi -log
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi -log
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" '.\Aura.uproject' -game /Game/Maps/StartupMap -unattended -nop4 -nullrhi -nosound '-ExecCmds=Quit' -log
& '.\RunRoleBattleDay1Smoke.bat'
```

Results: editor build passed; native automation exited `0` with all Aura tests successful, including the Day 1 role, respawn-guard, and attribute-default tests; AuraAbilityGraph smoke test exited `0` with `22 passed, 0 failed`; `StartupMap` loaded and the follow-up log contained zero unassigned SetByCaller magnitude errors. The remaining compiler/build warnings are the pre-existing plugin dependency, circular dependency, toolchain, and GameplayAbilities deprecation warnings.

The current-checkout Login recheck used the configured map path and reached `LoadMap(/Game/Maps/Login.umap)`. The log reported `Map check complete: 0 Error(s), 0 Warning(s)`. The commandlet did not return within the bounded 120-second window after issuing `Cmd: Quit`, so the process was stopped after the successful load/map-check evidence was collected.

### Day 1 bounded runtime smoke

```powershell
& '.\RunRoleBattleDay1Smoke.bat'
```

Result: pass. The archived log `Saved/Logs/Aura-backup-2026.08.07-15.22.15.log` records BungeeMan mesh/animation/rifle/Muzzle wiring, `FireGun` and `fireGunBullet` resolution, player-to-enemy damage `100 -> 90`, enemy-to-player damage `100 -> 90`, and two respawn checks at `Health=100/100` and `Mana=50/50`. The smoke runner exits after `[Day1Smoke] PASS`.

## Combat-path inventory

The current relationship and damage paths are distributed as follows:

| Path | Relationship check | Damage application |
| --- | --- | --- |
| `AuraProjectile.cpp` | Server-side `IsNotFriend` | Applies damage on projectile hit |
| `AuraFireBall.cpp` | Server-side `IsNotFriend` | Applies damage on fireball hit |
| `EnemyMeleeDamageNode.cpp` | Authority-side `IsNotFriend` | Applies melee damage |
| `ApplyDamageNode.cpp` | No direct relationship check | Calls shared `ApplyDamageEffect` |
| `CauseDamageNode.cpp` | No direct relationship check | Calls shared `ApplyDamageEffect` |
| `ElectrocuteBeamNode.cpp` | No direct relationship check in the damage tick path | Applies beam damage on authority |
| `HitscanTraceNode.cpp` | No direct relationship check | Applies trace damage |
| `ModularBeamNodes.cpp` | No direct relationship check | Applies optional authority-side beam damage |
| `SpawnShardsNode.cpp` | No direct relationship check | Applies radial damage on authority |
| `AuraAbilitySystemLibrary::ApplyDamageEffect` | No relationship validation | Builds and applies the GameplayEffect |

`IsNotFriend` itself is implemented in `AuraAbilitySystemLibrary.cpp` using the `Player` and `Enemy` actor tags. Other tag-based filtering remains in `AuraEffectActor.cpp` and `BTService_FindNearestPlayer.cpp`. Because the shared `ApplyDamageEffect` helper does not enforce relationship validation, indirect damage nodes can bypass the relationship policy unless their callers validate the target first.

## Remaining blockers and follow-up

1. Repeat the combat checks with a rendered/editor session for visual confirmation of Aura and Bungee presentation, including the rifle muzzle FX and montage timing.

## Gate status

| Gate | Status |
| --- | --- |
| Repository and engine baseline recorded | Pass |
| Editor build | Pass |
| Native automation | Pass |
| AuraAbilityGraph smoke test | Pass: 22/22 |
| Startup fixture load | Pass: `StartupMap` |
| Aura ability activation | Pass: all four observed |
| Enemy AI possession | Pass: two enemies observed |
| Respawn | Pass: bounded runtime smoke, 2/2 respawns with unchanged vitals |
| BungeeMan load/presentation | Headless asset/config/projectile wiring: Pass; rendered presentation open |
| Clean enemy/PvP damage attribution | Pass: live fixture, both directions reduced health |
| Full plan completion gate | Pass for headless functional Day 1; rendered/editor visual check remains open |
