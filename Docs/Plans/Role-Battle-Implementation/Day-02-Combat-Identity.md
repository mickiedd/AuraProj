# Day 02 - Add Combat Identity

## Goal

Give every combat-capable actor an explicit, replicated identity instead of relying on raw Player and Enemy actor tags.

## New files

- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/Combat/AuraCombatTypes.h

## Files to modify

- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp

## Implementation steps

1. Define tags or strongly named identifiers for:
   - Faction.Player
   - Faction.Enemy
   - Faction.Civilian
   - Control.Player
   - Control.EnemyAI
   - Control.CivilianAI
   - Combat.Magic
   - Combat.Gun
   - Combat.Civilian
   - Death.PlayerRespawn
   - Death.EnemyLoot
   - Death.PopulationRespawn
2. Add AuraCombatIdentityComponent as a replicated ActorComponent.
3. Store faction, combat profile, control type, death policy, targetable state, can-attack, can-be-damaged, and friendly-fire policy.
4. Add getters on AuraCharacterBase so damage and targeting code can access identity without knowing the concrete class.
5. Attach the component to AuraCharacterBase or ensure every derived combat actor creates one in its constructor.
6. Initialize AuraCharacter with Player faction and Player control type.
7. Initialize AuraEnemy with Enemy faction and EnemyAI control type.
8. Keep existing Player and Enemy actor tags temporarily for compatibility, but stop adding new code that depends on them.
9. Replicate identity from the server. Clients may display it but may not set it.
10. Add logging for an actor with missing identity.

## Verification

- Log every spawned AuraCharacter and AuraEnemy identity.
- Confirm identity is identical on server and client.
- Confirm the existing combat behavior has not changed.
- Confirm a Civilian identity can be assigned in a unit test or temporary test actor even before AAuraCivilian exists.

## Completion gate

Every actor that can receive or cause damage has a valid identity on the server. Existing Player and Enemy gameplay still works.

