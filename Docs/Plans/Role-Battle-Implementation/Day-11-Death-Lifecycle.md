# Day 11 - Separate the Death Lifecycle

## Goal

Make death state authoritative and idempotent, with separate Player, Enemy, and Civilian policies.

## New files

- Source/Aura/Public/Combat/AuraCombatStateComponent.h
- Source/Aura/Private/Combat/AuraCombatStateComponent.cpp

## Files to modify

- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Private/AbilitySystem/Attributes/AuraAttributeSet.cpp
- Source/Aura/Public/AbilitySystem/AuraAbilityTypes.h

## Implementation steps

1. Add replicated states Alive, Dying, Dead, and Respawning.
2. Add a single server-side transition function for entering Dying.
3. Make fatal damage call that transition exactly once.
4. Move shared death animation, collision, dissolve, and movement shutdown into AuraCharacterBase.
5. Keep policy-specific consequences in the concrete actor or policy handler.
6. Preserve player respawn only in AAuraCharacter.
7. Preserve enemy loot/lifespan behavior only in AAuraEnemy.
8. Add civilian death behavior:
   - Stop AI.
   - Disable interaction.
   - Play civilian death presentation.
   - Do not grant standard enemy loot or XP.
   - Notify population manager and battle director.
9. Ensure damage after Dying does not create new rewards or death events.
10. Add killer and ability attribution to the death notification.
11. Check all callers of Die and make them use the state transition.

## Verification

- Repeated fatal hits create one death event.
- Player death still respawns the player.
- Enemy death still grants configured rewards once.
- Civilian death never invokes player respawn.
- Civilian death never receives enemy-only loot by default.
- Dead actors are not selectable or damageable.

## Completion gate

Death behavior is selected by death policy and is independent of inheritance assumptions. All three actor types pass repeated-hit tests.

