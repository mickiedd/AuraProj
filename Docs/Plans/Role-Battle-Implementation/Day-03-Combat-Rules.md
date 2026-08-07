# Day 03 - Add Combat Relationship Rules

## Goal

Create one authoritative answer to "may this source target and damage this target?"

## New files

- Source/Aura/Public/Combat/AuraCombatRules.h
- Source/Aura/Private/Combat/AuraCombatRules.cpp

## Files to modify

- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/Combat/AuraCombatTypes.h
- Source/Aura/Public/AuraGameplayTags.h
- Source/Aura/Private/AuraGameplayTags.cpp
- Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp

## Implementation steps

1. Define a relationship result with:
   - Relationship type: Friendly, Hostile, Neutral, Protected.
   - CanTarget.
   - CanDamage.
   - Rejection reason.
2. Add AuraCombatRules functions:
   - GetRelationship.
   - CanTarget.
   - CanDamage.
   - CanReceiveDamage.
3. Use combat identity, not class casts and not actor tags, as the primary input.
4. Add the initial relationship matrix:
   - Player versus Enemy: hostile.
   - Enemy versus Player: hostile.
   - Player versus Player: friendly unless PvP is enabled.
   - Enemy versus Enemy: friendly.
   - Civilian versus everyone: no attack capability.
   - Civilian targetability and damage permission: controlled by battle-zone policy.
5. Ensure the rules reject:
   - Missing source or target.
   - Self-targeting unless explicitly allowed.
   - Dead or dying targets.
   - Non-damageable targets.
   - Friendly targets.
   - Protected-zone targets.
6. Replace the Day 2 `IsNotFriend` compatibility truth table with a wrapper that calls AuraCombatRules.
7. Replace the Day 2 Player-faction filter in `BTService_FindNearestPlayer` with `AuraCombatRules::CanTarget`; keep the existing service name and blackboard contract until the later generic `FindNearestHostile` replacement.
8. Add a small automated test or table-driven test for every row in the relationship matrix.

## Verification

Test the following without changing the projectile code yet:

- Player can target Enemy.
- Enemy can target Player.
- Player cannot damage another Player.
- Enemy cannot damage another Enemy.
- Civilian cannot attack.
- A protected civilian is rejected.
- An unprotected civilian is accepted only when the rule allows it.
- Enemy AI still writes the same target and distance blackboard keys after switching from the temporary Day 2 Player filter to `CanTarget`.

## Completion gate

There is one relationship API that returns a reason for every accepted or rejected target. `IsNotFriend` and enemy target acquisition delegate to it, and no new feature adds a direct faction comparison outside this API.
