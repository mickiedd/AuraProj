# Day 10 - Add Civilian AI

## Goal

Give civilians a small, predictable behavior loop: work, observe, flee, and seek shelter.

## New files

- Source/Aura/Public/AI/AuraCivilianAIController.h
- Source/Aura/Private/AI/AuraCivilianAIController.cpp
- Source/Aura/Public/AI/BTService_FindNearestThreat.h
- Source/Aura/Private/AI/BTService_FindNearestThreat.cpp
- Source/Aura/Public/AI/AuraCivilianBehaviorTypes.h

## Files to modify or create in content

- Civilian behavior tree or BehaviorU graph.
- Civilian blackboard or BehaviorU state data.
- Content/Config/CivilianWorkProfiles.json

## Implementation steps

1. Create CivilianAIController and assign it to AAuraCivilian.
2. Define civilian states:
   - Idle.
   - Wander.
   - Work.
   - Observe.
   - Flee.
   - Shelter.
   - Dead.
3. Add a threat query that uses AuraCombatRules and ignores friendly or protected actors.
4. Add a simple work point or observation point interface.
5. Add a short work/wander loop with randomized but bounded movement.
6. When a hostile actor enters the configured threat radius, switch to Flee.
7. Select a valid shelter or safe point using navigation.
8. Stop civilian attack tasks entirely; no attack node should exist in the civilian behavior graph.
9. Add an interruption rule for death and Dying state.
10. Add development logs for state changes and failed shelter selection.

## Verification

- Civilians wander or work when no threat is present.
- Civilians detect an Enemy in range.
- Civilians flee from an allowed hostile actor.
- Civilians stop fleeing when safe.
- Civilians do not target or attack any actor.
- Civilians stop behavior when dead.

## Completion gate

At least three civilians can run the behavior loop without attacking, getting stuck permanently, or selecting friendly actors as threats.

