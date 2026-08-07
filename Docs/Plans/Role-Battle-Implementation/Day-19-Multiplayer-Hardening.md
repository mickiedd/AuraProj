# Day 19 - Harden Multiplayer Behavior

## Goal

Run the complete first vertical slice under server authority and close security, replication, and lifecycle gaps.

## Files to inspect or modify

- All files changed during Days 02 through 18.
- Plugins/AuraAutoTest.
- Existing automation test modules and network test maps.
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Private/Projectile/AuraProjectile.cpp
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp

## Implementation steps

1. Run a listen-server test with:
   - Two Players.
   - At least one Enemy.
   - At least five Civilians.
   - One Civilian merchant.
2. Run the same test on a dedicated server if the project setup supports it.
3. Attempt invalid client actions:
   - Select an unapproved role.
   - Grant an ability.
   - Apply client-authored damage.
   - Damage a friendly actor.
   - Buy with a forged price.
   - Buy outside range.
   - Replay a purchase request.
4. Confirm the server rejects each invalid action.
5. Confirm role, identity, health, combat state, civilian behavior, merchant offers, currency, and inventory replicate correctly.
6. Test disconnect and reconnect during combat, death, and purchase.
7. Test simultaneous damage and simultaneous purchase requests.
8. Test dead civilian cleanup while a client is looking at or interacting with it.
9. Add automated coverage for the relationship matrix, death idempotence, and commerce atomicity.
10. Record performance for the chosen civilian population size.

## Required acceptance tests

- Player versus Enemy damage succeeds.
- Friendly damage fails.
- Allowed civilian damage succeeds.
- Protected civilian damage fails.
- Civilian death produces one event.
- Player respawn still works.
- Enemy rewards still work once.
- Merchant purchase succeeds once.
- Forged client data never changes server state.

## Completion gate

The vertical slice passes on a server-authoritative multiplayer test with no known client authority bypass.

