# Day 05 - Extend the Role Schema

## Goal

Make RoleConfig explicit enough to represent Aura Girl, BungeeMan, and Civilian without inferring equipment or behavior from ability arrays.

## Files to modify

- Source/Aura/Public/AbilitySystem/Data/RoleInfo.h
- Source/Aura/Private/AbilitySystem/Data/RoleInfo.cpp
- Content/Config/RoleConfig.json
- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp

## Implementation steps

1. Add role fields for:
   - Entity type.
   - Combat profile.
   - Faction.
   - Can attack.
   - Can be damaged.
   - Death policy.
   - Economy profile.
   - Interaction profile.
2. Create a dedicated equipment structure containing weapon mesh and socket names.
3. Preserve existing legacy fields during migration so current saved roles remain readable.
4. Add Civilian to RoleConfig with:
   - No startup offensive abilities.
   - No LMB ability.
   - No required weapon mesh.
   - Civilian faction and profile.
   - Population respawn death policy.
5. Keep Aura and BungeeMan internal role IDs stable for save compatibility.
6. Replace fatal JSON getters with safe parsing for optional fields.
7. Add a validation result containing role ID, field name, and error message.
8. Validate all role definitions at startup and when the editor reload sentinel is used.
9. Make IsRoleConfigured profile-aware:
   - Player combat roles require attack configuration.
   - Civilian does not require a weapon or attack ability.
10. Reject invalid role configuration on the server before login or spawn.

## Verification

- RoleConfig loads all three roles.
- Civilian loads with empty ability and equipment fields.
- Missing optional Civilian fields do not crash loading.
- Invalid mesh, animation, ability, or socket paths produce readable validation errors.
- The configured default role is still Aura.

## Completion gate

RoleConfig is the single source of truth for the three role definitions, and a Civilian role can load without accidentally inheriting a weapon or ability.

