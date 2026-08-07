# Day 06 - Refactor Role Application

## Goal

Make role application deterministic, explicit, and safe for empty Civilian loadouts.

## Files to modify

- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp

## Implementation steps

1. Split ApplyRole into:
   - ClearRoleRuntimeState.
   - LoadRoleRuntimeState.
   - ApplyRolePresentation.
2. Clear all previous role visual, weapon, socket, death-asset, ability-definition, and LMB state before loading the new role.
3. Apply equipment only from the explicit equipment structure.
4. Stop using the presence of DefaultLMBAbility or LMBAbilityDefinition as the weapon decision.
5. Make an empty ability list mean “this role has no abilities,” not “keep old Blueprint defaults.”
6. Add a runtime field indicating whether role abilities have already been granted.
7. Add a role-grant source tag or equivalent tracking metadata to every role-granted ASC ability.
8. Prevent duplicate grants when ApplyRole or progress loading runs twice.
9. Lock role changes to the server and to the login/spawn phase for now.
10. Preserve current PlayerState role replication and client presentation updates.
11. Update progress loading so role application happens before role ability grants.
12. Add an explicit error path if a role cannot be applied.

## Verification

- Applying Aura gives Aura visuals and Aura loadout.
- Applying BungeeMan gives the rifle and FireGun.
- Applying Civilian removes a previous weapon and leaves no offensive ability.
- Reapplying the same role does not duplicate abilities.
- Loading a saved role and initializing progress grants the expected loadout once.
- A client cannot call a role-changing path successfully.

## Completion gate

Role application is deterministic for non-empty and empty loadouts. No Civilian test can fire a previous Aura or BungeeMan ability.

