# Day 08 - Add the Civilian Actor

## Goal

Create a civilian actor shell that reuses common health and damage infrastructure without inheriting player or enemy behavior.

## New files

- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp

## Files to modify

- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp

## Implementation steps

1. Derive AAuraCivilian from AAuraCharacterBase, not AAuraCharacter or AAuraEnemy.
2. Create or assign a replicated Ability System Component and AuraAttributeSet using the simplest existing non-player pattern.
3. Assign Civilian faction, Civilian control type, Civilian combat profile, and population death policy.
4. Set canAttack false and canBeDamaged true.
5. Add a civilian role ID or role lookup path so its mesh and animation come from RoleConfig.
6. Add no startup offensive abilities and no LMB ability.
7. Add a safe health/resistance initialization path that does not require an enemy-only CharacterClass entry.
8. Implement generic targetable access without implementing EnemyInterface.
9. Implement a temporary idle tick or placeholder behavior that does not attack.
10. Add a civilian health bar or debug health display for development.
11. Ensure the civilian does not enter player XP, enemy loot, or player respawn code.

## Verification

- A civilian can be spawned manually in the editor or by a temporary test command.
- The configured mesh and animation apply.
- The civilian has health and can receive valid damage.
- The civilian cannot activate an offensive ability.
- The civilian does not receive player input.
- The civilian does not use enemy loot or player respawn behavior.

## Completion gate

One civilian can exist in the map, replicate, receive damage, and remain a non-combatant without crashing or entering Player/Enemy-specific paths.

