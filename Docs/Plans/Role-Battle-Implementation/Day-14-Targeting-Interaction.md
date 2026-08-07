# Day 14 - Add Targeting and Interaction

## Goal

Let players distinguish enemies, civilians, merchants, shelters, and dead actors through shared targeting and interaction contracts.

## New files

- Source/Aura/Public/Combat/AuraTargetableInterface.h
- Source/Aura/Private/Combat/AuraTargetableInterface.cpp
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp

## Files to modify

- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Existing highlight and cursor UI code.

## Implementation steps

1. Define a targetable interface exposing target type, targetable state, combat identity, health/death state, and interaction options.
2. Implement it on AAuraCharacterBase or the actors that can be targeted.
3. Keep EnemyInterface for enemy-only behavior during migration.
4. Replace player-controller checks that only recognize EnemyInterface.
5. Add target states:
   - Hostile.
   - Friendly.
   - Civilian.
   - Merchant.
   - Shelter.
   - Downed or Dead.
6. Add AuraInteractionComponent for server-validated interaction requests.
7. Validate range, line of sight, target state, and faction before showing an actionable prompt.
8. Add civilian highlight and interaction UI without showing an attack prompt when canAttack is false.
9. Keep client highlight cosmetic; server validates every interaction.
10. Add a temporary civilian “Talk” or “Observe” interaction before commerce is implemented.

## Verification

- Enemy shows hostile targeting.
- Civilian shows civilian/interaction targeting.
- Dead civilian cannot be interacted with.
- A client cannot interact outside range or line of sight.
- Civilian never receives a misleading attack prompt.

## Completion gate

Targeting and interaction are distinct concepts. Civilians can be selected as interactive actors without becoming enemies.

