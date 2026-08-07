# Day 07 - Validate Aura and BungeeMan

## Goal

Prove that the role refactor did not break the two existing combat roles before adding Civilian.

## Files to inspect or adjust

- Content/Config/RoleConfig.json
- Content/AbilityDefinitions/FireBolt.xml
- Content/AbilityDefinitions/FireBlast.xml
- Content/AbilityDefinitions/ArcaneShards.xml
- Content/AbilityDefinitions/Electrocute.xml
- Content/AbilityDefinitions/FireGun.xml
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp

## Implementation steps

1. Validate Aura’s mesh, animation blueprint, staff socket, montage assets, and all four ability definitions.
2. Validate BungeeMan’s mesh, animation blueprint, WeaponHandSocket, Muzzle socket, montage event, and FireGun projectile.
3. Confirm Aura’s mana costs, cooldowns, damage types, and ability tags.
4. Confirm BungeeMan’s physical damage and cooldown.
5. Verify both roles use AuraCombatRules rather than actor-tag logic.
6. Verify server-only ability grants and server-only damage application.
7. Test role persistence across:
   - New player.
   - Existing save.
   - Reload.
   - Respawn.
8. Test death and respawn for both roles.
9. Record any existing data-driven ability migration issues rather than silently fixing unrelated systems.

## Verification matrix

| Test | Aura | BungeeMan |
| --- | --- | --- |
| Visual loadout | Pass | Pass |
| LMB ability | FireBolt | FireGun |
| Active ability list | Pass | Empty as configured |
| Damage type | Fire/magic paths | Physical |
| Friendly target rejection | Pass | Pass |
| Enemy damage | Pass | Pass |
| Player respawn | Pass | Pass |
| Save/load role | Pass | Pass |

## Completion gate

Aura and BungeeMan pass the matrix in a multiplayer test. Only then should Civilian be introduced.

