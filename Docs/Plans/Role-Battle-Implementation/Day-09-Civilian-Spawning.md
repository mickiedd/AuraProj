# Day 09 - Add Civilian Population Spawning

## Goal

Spawn civilians from data rather than hand-placing every actor.

## New files

- Content/Config/PopulationSpawnTable.json
- Source/Aura/Public/World/AuraPopulationSpawnDefinition.h
- Source/Aura/Private/World/AuraPopulationSpawnDefinition.cpp
- Source/Aura/Public/World/AuraCivilianSpawnVolume.h
- Source/Aura/Private/World/AuraCivilianSpawnVolume.cpp

## Files to modify

- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Content/Config/LevelConfig.json

## Implementation steps

1. Define a population entry containing:
   - Population ID.
   - Role ID.
   - Actor class.
   - Initial count.
   - Maximum count.
   - Spawn volume or marker set.
   - Respawn policy placeholder.
   - Work profile.
2. Add a civilian spawn volume that validates navigation and collision before spawning.
3. Make spawning server-only.
4. Keep spawned civilians in a population registry owned by the GameMode or a dedicated server subsystem.
5. Add an initial population entry for a small test group.
6. Assign each spawned civilian a stable population member ID.
7. Prevent duplicate spawning when the map reloads or the editor reload sentinel runs.
8. Add debug visualization for valid and rejected civilian spawn points.
9. Do not implement full respawn yet; record deaths for Day 13.

## Verification

- A test map spawns the configured number of civilians on the server.
- Clients receive the civilians.
- Spawn points avoid invalid navigation and overlapping actors.
- Civilians use the Civilian role configuration.
- Restarting the match does not duplicate the population.

## Completion gate

The project can create a controlled civilian population from data with a stable registry and no hand-authored actor dependency.

