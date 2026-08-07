# Day 13 - Add Population Lifecycle

## Goal

Make civilian and enemy population creation, death cleanup, and refill server-authoritative.

## New files

- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp

## Files to modify

- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Content/Config/PopulationSpawnTable.json

## Implementation steps

1. Move civilian registry and population counts into AuraPopulationManager.
2. Register each spawned member with population ID, role ID, zone ID, and member ID.
3. Listen for death notifications from AuraCombatStateComponent.
4. Remove dead actors from active AI and interaction registries.
5. Add corpse cleanup timing by role/profile.
6. Add civilian refill rules:
   - Delay.
   - Maximum population.
   - Spawn point validation.
   - Battle-phase restrictions.
7. Keep enemy respawn behavior compatible with the existing spawn table.
8. Ensure player respawn never uses the population manager.
9. Add a population snapshot for debug and future persistence.
10. Add server-only safeguards against duplicate refill tasks.

## Verification

- Killing one civilian reduces active population once.
- The corpse is cleaned up once.
- A replacement civilian appears only when the policy allows it.
- Active count never exceeds the configured maximum.
- Enemy respawn remains unchanged.
- Restarting the level rebuilds the population cleanly.

## Completion gate

Civilian lifecycle is managed by population policy rather than by player or enemy respawn code.

