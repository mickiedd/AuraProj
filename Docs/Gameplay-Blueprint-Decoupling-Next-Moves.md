# AuraAbilityGraph Blueprint-Decoupling: Next Moves

Date: 2026-08-02  
Starting point: native projectile and pickup runtime is implemented; pickup Blueprint deletion is complete.

This document is the ordered implementation handoff for the remaining narrow migration. Do not delete legacy player or projectile packages out of order: the current Asset Registry manifest proves they still have consumers.

## 1. Finish the `AbilityInfo.json` runtime boundary

The principal cleanup blocker is not the active ability grant path; it is the legacy metadata API. Several UI, save, status, and debug paths still call `UAuraAbilitySystemLibrary::GetAbilityInfo()` and therefore keep `DA_AbilityInfo` and its legacy `GA_*` class references alive.

Migrate these consumers to `URuntimeAbilityInfo` and the active role/ability definitions:

- `UAuraWidgetController` and derived overlay/spell-menu controllers;
- `UAuraAbilitySystemComponent::IsPassiveAbility`;
- `UpdateAbilityStatuses`;
- `GrantAndEquipAllAbilities`;
- ability-description and slot-selection paths;
- `AAuraCharacter` save serialization;
- any Blueprint callers found by Asset Registry or Blueprint snapshot inspection.

Rules:

- `AbilityInfo.json` remains UI metadata: icon, material, level requirement, and ability tag.
- Ability type, input, cooldown, cost, and implementation class come from the active XML/role definition or the live ability spec—not duplicated JSON fields.
- Do not reintroduce a `TSubclassOf<UGameplayAbility>` field into `AbilityInfo.json`.
- Save data should persist stable ability tags, slots, status, and levels. Resolve the runtime definition/class when restoring.
- Config failure must report an actionable error and must not fall back silently to `DA_AbilityInfo`.

Acceptance:

- runtime code and active Blueprints no longer call `GetAbilityInfo()`;
- UI and spell-menu metadata still display correctly;
- level eligibility, equip/unequip, passive classification, descriptions, save, and restore work from tag/definition data;
- malformed or missing `AbilityInfo.json` fails predictably;
- an Asset Registry report shows that active maps/game modes no longer require `DA_AbilityInfo`.

## 2. Remove stale player Blueprint references

After step 1:

1. Clear the `GA_FireBolt` reference in `BP_AuraCharacter`.
2. Compile and resave `BP_AuraCharacter`.
3. Clear the deprecated `AbilityInfo` property on active GameMode Blueprint defaults.
4. Compile and resave the affected GameMode, HUD, character, and widget Blueprints.
5. Reload `StartupMap` and the normal startup/menu/gameplay transition.
6. Regenerate `Saved/AuraMigration/LegacyAbilityManifest.json`.

Do not modify enemy startup abilities or passive definitions in this step.

Acceptance:

- `BP_AuraCharacter` has no `GA_FireBolt` hard or soft reference;
- active GameMode defaults have no `DA_AbilityInfo` reference;
- editor startup and map load logs have no missing classes, packages, Blueprint compile warnings, or `Accessed None` errors.

## 3. Delete the legacy player GA/GE batch

Review each candidate using the machine-readable manifest. Expected candidates are:

- `GA_FireBolt`, `GA_FireGun`, `GA_FireBlast`, `GA_ArcaneShards`, `GA_Electrocute`;
- their player `GE_Cost_*` and `GE_Cooldown_*` packages;
- associated snapshots when the corresponding package is deleted.

For every asset, record:

| Asset | Referencers before | Replacement | Referencers after | Decision |
|---|---:|---|---:|---|
| Example player GA | `DA_AbilityInfo` | role config + XML + `UAuraDataAbility` | 0 | Delete |

Deletion gate:

- active player startup, LMB, unlock, equip, save, and restore paths use data definitions;
- Asset Registry, maps, config, strings, defaults, and data assets report zero referencers;
- focused automation and graph smoke tests pass before deletion;
- delete in a small player-only batch, then rebuild and rerun tests;
- do not include enemy, passive, attribute-initialization, or shared damage assets.

## 4. Port `GA_EnemyFireBolt`

`BP_FireBolt` cannot be deleted while `GA_EnemyFireBolt` uses it. Port the enemy path separately:

1. Snapshot its graph, defaults, montage/event behavior, targeting, damage, cooldown, and authority behavior.
2. Add an enemy XML definition or a documented native enemy ability path.
3. Reuse the `fireBolt` projectile definition unless measured enemy presentation/movement values differ; if they differ, add a separately named native definition.
4. Migrate enemy ability grants/AI integration.
5. Test targeting, hostile/friendly filtering, montage events, exactly-once damage, death, and server authority.
6. Remove `GA_EnemyFireBolt` only after its own zero-referencer report.

Do not bundle `GA_RangedAttack`, `GA_MeleeAttack`, or `GA_HitReact` into this deletion unless each has independently passed the same gates.

Acceptance:

- enemies spawn a native configured projectile without loading `GA_EnemyFireBolt` or `BP_FireBolt`;
- listen-server/client testing proves server-only damage and correct replicated presentation;
- AI and montage behavior match the recorded baseline.

## 5. Delete the projectile Blueprint batch

Expected final blockers:

- `BP_FireBall` becomes deletable after the legacy player `GA_FireBlast` batch is gone;
- `BP_FireBolt` becomes deletable after both legacy `GA_FireBolt` and `GA_EnemyFireBolt` are gone.

Before deletion, regenerate the Asset Registry report and require zero hard, soft, map, string, config, default-object, and management referencers. Delete each projectile package together with its stale snapshot, then rebuild and run the projectile suite after each small batch.

Acceptance:

- neither Blueprint package exists or appears in Asset Registry;
- active XML contains only `ProjectileDefinition` fields;
- editor startup, map reload, PIE, standalone, cook, and packaged launch report no missing packages;
- native FireBolt wall impact and FireBall outbound/return behavior remain covered by assertions.

## 6. Multiplayer and packaged-build gates

These gates are required before claiming the narrow migration complete:

1. Listen server plus one client: FireBolt against enemy, friend, source, and wall.
2. FireBlast outbound/return through world geometry and multiple targets.
3. Verify one server damage application per valid target and no client-side duplicate.
4. Exercise all six pickup definitions, including FireArea apply/remove behavior.
5. Kill an enemy and validate JSON-driven loot visibility and pickup interaction on both peers.
6. Reload or transition through `StartupMap` and a normal gameplay map.
7. Package Win64 Development, launch the executable, repeat a minimal projectile/pickup scenario, and inspect the complete packaged log.

Treat new errors, fatals, ensures, missing packages/classes, config failures, SetByCaller errors, duplicate effects, replication warnings, Blueprint compile warnings, and cook-exclusion warnings as failures.

Required commands:

```powershell
& 'C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat' AuraEditor Win64 Development '-Project=C:\Git\AuraProj\Aura.uproject' -WaitMutex -NoHotReloadFromIDE

& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -unattended -nop4 -nullrhi -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests Aura.Projectiles' '-TestExit=Automation Test Queue Empty' -log

& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -unattended -nop4 -nullrhi -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests Aura.Buffs' '-TestExit=Automation Test Queue Empty' -log

& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi -DisablePlugins=RiderLink -log
```

## 7. Final closeout

Update the implementation report with:

- final player/enemy/projectile deletion manifest;
- multiplayer scenarios and results;
- packaged build path and launch result;
- final build, automation, smoke, cook, and log counts;
- retained presentation assets;
- deferred passive/enemy work that remains outside the narrow claim.

The narrow migration is complete only when both projectile Blueprints and legacy player GA/cost/cooldown packages are deleted and unreferenced, while enemy/passive assets outside the migrated ownership boundary remain explicitly documented.
