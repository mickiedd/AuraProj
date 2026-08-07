# Day 12 - Add the Battle Director

## Goal

Centralize conflict phase, battle-zone rules, safe zones, and battle events.

## New files

- Source/Aura/Public/Battle/AuraBattleDirector.h
- Source/Aura/Private/Battle/AuraBattleDirector.cpp
- Source/Aura/Public/Battle/AuraBattleZoneTypes.h
- Source/Aura/Private/Battle/AuraBattleZoneTypes.cpp
- Content/Config/BattleZones.json

## Files to modify

- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Combat/AuraCombatRules.h
- Source/Aura/Private/Combat/AuraCombatRules.cpp

## Implementation steps

1. Create a server-owned AuraBattleDirector or equivalent GameMode subsystem.
2. Define battle phases:
   - Peace.
   - Alert.
   - Conflict.
   - Cleanup.
3. Define a battle zone with:
   - Zone ID.
   - Bounds or volume.
   - Allowed faction relationships.
   - Protected actors or civilian policy.
   - Safe and shelter locations.
   - Enemy and civilian population references.
4. Make AuraCombatRules accept battle-zone context.
5. Add a single test zone where Enemy can damage Civilian.
6. Add a safe zone where Civilian cannot be damaged.
7. Replicate phase and zone state through GameState or a replicated director actor.
8. Add battle event IDs to damage and death attribution.
9. Add server notifications for phase transitions.
10. Do not add reward, reputation, or complex wave logic yet.

## Verification

- Peace phase protects civilians according to configuration.
- Conflict phase permits the configured civilian casualty rule.
- Actors outside a battle zone use the default relationship policy.
- Clients display the same phase and zone state.
- Damage rejection includes the correct zone/protection reason.

## Completion gate

Changing the battle phase changes combat permissions through one authoritative system, without adding special cases to projectile or ability code.

