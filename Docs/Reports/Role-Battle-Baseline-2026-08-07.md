# Role Battle Baseline Report — 2026-08-07

## Result

The executable baseline is complete for the assets and map currently present in the checkout. The full plan gate is not green because the BungeeMan presentation assets are absent, the configured login map is absent, and a clean player-to-enemy damage attribution pass could not be established in the available headless fixture.

No gameplay source code was changed during this baseline run. The changes from this run are documentation only.

## Environment

- Repository: `D:\Users\zhouzhiqiang\Documents\GitHub\AuraProj`
- Baseline commit: `a26d241391202cd9195bafafcb7c651e3d8c3074`
- Engine: Unreal Engine 5.5 at `D:\UE_5.5`
- Build target: `AuraEditor Win64 Development`
- Working tree already contained unrelated documentation changes and untracked plan documents.

## Fixture and asset findings

`/Game/Maps/StartupMap` is the reproducible headless fixture. It contains the Aura player, `PlayerStart_1`, navigation data, `BP_Demon_Ranger_C_1`, and `BP_Shaman_C_1`. The map loaded successfully in game mode, possessed the Aura character, and possessed both enemy AI controllers.

The configured editor/game startup map is `/Game/Maps/Login.Login`, but `Content/Maps/Login.umap` is not present. `Content/Maps/StartupMap.umap` is present and was used for the checks below.

The BungeeMan role references are unresolved in this checkout:

- `/Game/BungeeMan/SKM_BungeeMan.SKM_BungeeMan`
- `/Game/BungeeMan/Blueprints/ABP_Bungee.ABP_Bungee_C`
- `/Game/MilitaryWeapDark/Weapons/Assault_Rifle_B.Assault_Rifle_B`

`FireGun.xml` is present and parses, but there is no valid BungeeMan mesh, animation blueprint, or rifle asset with which to perform the requested visual or weapon pass.

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

1. Restore or provide the BungeeMan mesh, animation blueprint, and rifle assets, then run the Bungee role load and weapon presentation checks.
2. Restore `/Game/Maps/Login` or update the startup configuration to a valid map after confirming the intended product behavior.
3. Fix respawn attribute initialization so health and mana reset to the intended baseline rather than accumulating.
4. Resolve the GameplayEffect magnitude errors logged during startup/spawn (`GetMagnitude ... magnitude had not yet been set by caller`).
5. Add or select a fixture with a valid enemy Ability System Component and run a clean player-to-enemy and enemy-to-player damage-direction pass.
6. Repeat the combat checks with a rendered/editor session for visual confirmation of Aura and Bungee presentation.

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
| Respawn | Pass: two respawns observed; attribute reset defect recorded |
| BungeeMan load/presentation | Blocked: required assets absent |
| Clean enemy/PvP damage attribution | Open: fixture/result insufficient |
| Full plan completion gate | Not yet pass |
