# Day 04 - Migrate All Damage Producers

## Goal

Route every damage source through AuraCombatRules and make damage authoritative and attributable.

## Files to locate

Use the source tree search for:

- IsNotFriend
- ApplyDamageEffect
- FDamageEffectParams
- IncomingDamage
- SetByCaller
- DamageType

## Files to modify

- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Private/Projectile/AuraProjectile.cpp
- Source/Aura/Private/AbilitySystem/Attributes/AuraAttributeSet.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp
- All AuraAbilityGraph action files that create damage parameters.

## Implementation steps

1. Change ApplyDamageEffect to validate source and target through AuraCombatRules before creating the damage effect.
2. Keep a second validation at the server-side final application boundary.
3. Update AuraProjectile overlap/hit handling to:
   - Ignore invalid, dead, and friendly targets.
   - Validate the source actor on the server.
   - Pass source and target identity into the damage context.
4. Update beam, radial, projectile, melee, and hitscan action nodes.
5. Ensure no client-only action directly changes target health.
6. Add source actor, source controller/player state, ability tag, and damage type to the custom damage context.
7. Add a safe damage-calculation profile for actors that do not have a current enemy CharacterClass curve. Do not add Civilian to ECharacterClass.
8. Ensure damage events are rejected after the target enters Dying or Dead.
9. Preserve existing blocked, critical, resistance, knockback, and debuff behavior.
10. Add debug logging behind a development flag for rejected damage.

## Verification

- FireBolt damages Enemy and not Player.
- FireGun damages Enemy and not Player.
- Enemy ability damages Player and not Enemy.
- Every projectile and graph-based damage path rejects friendly targets.
- A civilian test actor can receive accepted damage when configured.
- Damage context identifies the correct source and ability.

## Completion gate

No damage producer bypasses AuraCombatRules. Existing Aura and BungeeMan combat still works on the server and produces correct damage attribution.

