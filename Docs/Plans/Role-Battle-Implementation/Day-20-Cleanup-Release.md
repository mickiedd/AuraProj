# Day 20 - Finish the First Vertical Slice

## Goal

Remove transitional assumptions, document the completed slice, and prepare the system for future roles and larger populations.

## Files to inspect or modify

- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Player/AuraPlayerController.cpp
- Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp
- Content/Config/RoleConfig.json
- Docs/Tracking/GAS-Migration-TODOs.md
- Docs/README.md

## Implementation steps

1. Search for remaining direct combat checks based on Player and Enemy actor tags.
2. Replace safe remaining cases with identity/rules access, or document them as intentional compatibility code.
3. Search for every caller of IsNotFriend and confirm it now delegates to AuraCombatRules.
4. Search for any role application path that does not clear previous state.
5. Search for any Civilian path that enters AAuraCharacter player respawn or AAuraEnemy loot logic.
6. Remove temporary debug grants, test commands, and development-only logs from production paths.
7. Add configuration comments or schema documentation for new RoleConfig and economy fields.
8. Update GAS-Migration-TODOs.md with issues discovered during the implementation.
9. Update Docs/README.md with the daily plan and completed vertical-slice status.
10. Capture a final test map or test procedure that reproduces:
    - Aura combat.
    - BungeeMan combat.
    - Civilian work/flee/death.
    - Battle phase changes.
    - Merchant purchase.
    - Save/load.
11. Review whether role hot-swapping is still disabled; leave it disabled until transactional ASC cleanup is implemented.
12. Create a follow-up backlog for ammo/reload, reputation, richer businesses, civilian schedules, and crowd optimization.

## Final definition of done

- Aura Girl and BungeeMan are stable data-driven combat roles.
- Civilian is an independent non-combat AI actor.
- Faction and battle-zone rules control all damage.
- Death policies are separate and idempotent.
- Civilian population refill is authoritative.
- Merchant economy works and persists.
- Existing player/enemy behavior is not regressed.
- Multiplayer security checks pass.
- The next features have clear extension points.

## Completion gate

The first role/battle/business vertical slice is documented, tested, and ready for the next feature cycle.

