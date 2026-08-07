# Role Creation and Battle System Plan

Status: Draft implementation plan

This plan is based on the current AuraProj code and configuration. It is intended to turn the existing Aura/BungeeMan prototype into a system that can support:

- Aura Girl: magic-focused combatant.
- BungeeMan: gun-focused combatant.
- Civilian: a non-combatant who observes, works, trades, flees, and can be killed during a conflict.

The recommended first version treats Civilian as an ambient AI-controlled NPC. The same role definition can later be reused for a playable civilian if that becomes necessary.

## 1. Main architectural decision

The current project uses the word role for several different concepts at once. A role currently controls mesh, animation, weapon, attributes, abilities, and some death assets. Player and enemy behavior is controlled separately through inheritance and raw actor tags.

Keep these concepts separate:

| Concept | Example | Responsibility |
| --- | --- | --- |
| Role definition | Aura, BungeeMan, Civilian | Appearance, base attributes, loadout, interaction profile |
| Combat profile | Magic, Gun, Civilian | Attack capability, damage permissions, target rules |
| Faction | Player, Enemy, Civilian | Friend/foe relationships |
| Control type | Player, EnemyAI, CivilianAI | Input, AI, possession, replication behavior |
| Death policy | PlayerRespawn, EnemyLoot, CivilianPopulation | What happens after fatal damage |
| Economy profile | None, None, Merchant, Worker | Currency, inventory, offers, work behavior |

The resulting model should be:

    RoleDefinition
        -> visual presentation
        -> attributes and ability loadout
        -> equipment and interaction profile

    CombatIdentity
        -> faction
        -> control type
        -> combat permissions
        -> death policy

    Actor shell
        -> AAuraCharacter for players
        -> AAuraEnemy for hostile enemy archetypes
        -> AAuraCivilian for civilian NPCs

Do not add Civilian to ECharacterClass. ECharacterClass is currently used by enemy ability selection and damage calculation curves. It should remain an enemy/stat archetype until there is a deliberate migration.

## 2. Current implementation map

The existing systems already provide a useful foundation:

| Existing system | Current location | Reuse |
| --- | --- | --- |
| Role parsing and asset lookup | Source/Aura/Public/AbilitySystem/Data/RoleInfo.h and corresponding Private files | Extend into a validated role definition |
| Role application | Source/Aura/Private/Character/AuraCharacterBase.cpp | Refactor into explicit loadout replacement |
| Player role persistence | AuraCharacter, AuraPlayerState, LoadScreenSaveGame | Keep for player roles |
| Ability System Component and attributes | AuraCharacterBase, AuraPlayerState, AuraEnemy, AuraAttributeSet | Reuse for the first civilian vertical slice |
| Damage calculation | ExecCalc_Damage | Keep the calculation, replace the target permission gate |
| Damage application | AuraAbilitySystemLibrary, AuraProjectile, data-driven action nodes | Route all damage through combat rules |
| Data-driven abilities | Plugins/AuraAbilityGraph and Content/AbilityDefinitions | Use for Aura and BungeeMan |
| Enemy behavior | AuraEnemy, AuraAIController, BehaviorU, BTService_FindNearestPlayer | Generalize hostile targeting; do not copy enemy loot/death behavior to civilians |
| Pickup effects | AuraEffectActor and Content/Config/PickupDefinitions.json | Reuse for health/mana pickups |
| Building placement | AuraBuildingComponent and AuraPlacedBuildingActor | Reuse only if civilians can own or interact with buildings |
| Player/enemy spawning | AuraGameModeBase, AuraEnemySpawnVolume, spawn tables | Extend into a population spawn system |
| Save/load | LoadScreenSaveGame and AuraGameModeBase world state | Extend for economy and population state |

The current RoleConfig.json already contains Aura and BungeeMan. Their existing ability definitions are:

- Aura: FireBlast, ArcaneShards, Electrocute, with FireBolt as the LMB ability.
- BungeeMan: FireGun as the LMB ability, with a rifle mesh and Muzzle socket.

There is currently no civilian role, faction abstraction, commerce system, inventory, currency, civilian AI, or civilian population persistence.

## 3. Critical constraints in the current code

These should be fixed before adding the new role:

1. Role application does not reliably clear old abilities. ApplyRole only overwrites startup arrays when the incoming arrays are non-empty. An explicitly empty Civilian loadout could therefore leave Blueprint defaults or previously granted abilities active.

2. Weapon setup is inferred from the presence of an LMB ability. This is unsafe for Civilian. Equipment must be an explicit field, independent of attack capability.

3. IsNotFriend in AuraAbilitySystemLibrary currently treats Player and Enemy as the only meaningful groups. An actor without either tag can become hostile to both groups. Civilian must be handled through a real combat relationship query.

4. Player and enemy identity is currently represented by raw Actor tags and inheritance. Add a replicated combat identity component instead of adding more special-case tags.

5. AAuraCharacter death leads into the player respawn path. Civilian death must never invoke player respawn logic.

6. AAuraEnemy death includes enemy-specific loot, lifespan, and respawn assumptions. Civilian needs a separate death policy.

7. BTService_FindNearestPlayer is player/enemy-specific. It cannot be the basis for civilian behavior or general faction-aware targeting.

8. The targeting UI mainly recognizes EnemyInterface. Civilian needs targetable/interactable semantics without being treated as an enemy.

9. ExecCalc_Damage assumes the character-class damage coefficient setup is available. Civilian needs a safe combat/stat profile, even if it has no abilities.

10. Runtime role switching currently changes presentation and configuration but does not transactionally remove old ASC abilities, effects, or attributes. Lock role selection at spawn for the first version.

11. RoleConfig parsing uses required JSON getters for many fields. Invalid or incomplete civilian data should produce a validation error, not a load-time crash or partially configured role.

12. Damage context does not yet contain complete killer/instigator/ability attribution. This will be needed for kill feeds, civilian consequences, AI aggro, statistics, and future crime/reputation rules.

## 4. Role data model and configuration

### 4.1 Proposed role schema

Extend each RoleConfig.json entry with explicit identity and behavior fields. Names can be changed, but the separation should remain.

    role: "Aura"
    displayName: "Aura Girl"
    entityType: "Player"
    combatProfile: "Magic"
    faction: "Player"
    canAttack: true
    canBeDamaged: true
    deathPolicy: "PlayerRespawn"
    equipment:
        weaponMesh: ...
        weaponSocket: "WeaponHandSocket"
        weaponTipSocket: ...
    attributes:
        strength: 10
        intelligence: 15
        resilience: 10
        vigor: 10
    startupAbilityDefinitions: [...]
    startupPassiveAbilityDefinitions: [...]
    lmbAbilityDefinition: ...

    role: "BungeeMan"
    displayName: "BungeeMan"
    entityType: "Player"
    combatProfile: "Gun"
    faction: "Player"
    canAttack: true
    canBeDamaged: true
    deathPolicy: "PlayerRespawn"
    equipment:
        weaponMesh: ...
        weaponSocket: "WeaponHandSocket"
        weaponTipSocket: "Muzzle"
    startupAbilityDefinitions: []
    lmbAbilityDefinition: "/Game/AbilityDefinitions/FireGun.xml"

    role: "Civilian"
    displayName: "Civilian"
    entityType: "AmbientNPC"
    combatProfile: "Civilian"
    faction: "Civilian"
    canAttack: false
    canBeDamaged: true
    deathPolicy: "PopulationRespawn"
    economyProfile: "Merchant"
    interactionProfile: "Civilian"
    equipment:
        weaponMesh: ""
    startupAbilityDefinitions: []
    startupPassiveAbilityDefinitions: []
    lmbAbilityDefinition: ""

Use FName or Gameplay Tags for configurable identifiers. Avoid spreading hard-coded C++ enums for every future role, faction, job, or weapon.

Recommended required fields:

- role
- displayName
- entityType
- combatProfile
- faction
- canAttack
- canBeDamaged
- deathPolicy
- mesh
- animBlueprint
- attributes
- ability arrays

Recommended optional fields:

- equipment
- combat sockets
- death assets
- economyProfile
- interactionProfile
- workProfile
- spawnProfile
- voice/profile assets

### 4.2 Role parser and validation

Update RoleInfo loading to use safe getters and defaults for optional fields. Add a validation pass that reports all invalid roles together.

Validation should cover:

- Duplicate role IDs.
- Missing mesh or animation assets.
- Invalid animation blueprint classes.
- Invalid weapon socket names.
- A combat role with canAttack true but no valid attack ability.
- A Civilian role with any offensive LMB or startup ability.
- An entity with an equipment mesh but no valid equipment socket.
- Invalid ability definition paths.
- Invalid attribute values.
- Unsupported entity type, combat profile, faction, or death policy.
- A defaultRole that is not present or is not player-selectable.

IsRoleConfigured should become profile-aware. A player combat role needs a weapon/attack configuration; a civilian does not.

Add a RoleDefinitionVersion to the config. This will make future schema migrations safer.

### 4.3 ApplyRole refactor

Refactor ApplyRole into three explicit operations:

1. ClearRoleRuntimeState.
2. LoadRoleRuntimeState.
3. ApplyRolePresentation.

ClearRoleRuntimeState must clear:

- Body mesh and animation override.
- Weapon mesh and weapon socket state.
- Combat sockets and death assets.
- Startup ability arrays.
- Data-driven ability definition objects.
- LMB ability references.
- Role-specific cached attributes and profile data.

The first implementation should apply a role before abilities are granted. If runtime role switching is later required, add a RebuildRoleLoadout transaction that:

- Validates the new role on the server.
- Removes only abilities/effects granted by the previous role.
- Applies the new attributes and equipment.
- Grants the new role abilities.
- Replicates the result to clients.
- Rolls back if any asset or grant step fails.

Use an explicit role-granted source tag or ability set so the system knows which abilities may be removed. Do not remove arbitrary player abilities.

### 4.4 Specific role plans

#### Aura Girl

Use the existing Aura role as the first magic combat role.

Initial loadout:

- LMB: FireBolt.
- Active abilities: FireBlast, ArcaneShards, Electrocute.
- Existing intelligence-oriented attributes.
- Existing staff mesh and WeaponHandSocket.

Tasks:

- Rename only the display name if desired; retain the internal role ID Aura for save compatibility.
- Validate all ability definition paths and montage/socket requirements.
- Verify FireBolt, FireBlast, ArcaneShards, and Electrocute use the same faction-aware damage path.
- Decide whether legacy passive abilities remain startup grants or are migrated to the data-driven definitions.
- Add a clear magic combat profile rather than inferring magic behavior from CharacterClass.

#### BungeeMan

Use the existing BungeeMan role as the first gun combat role.

Initial loadout:

- LMB: FireGun.
- Rifle mesh.
- WeaponHandSocket and Muzzle combat socket.
- Physical damage profile.

Tasks:

- Validate the Muzzle socket and the FireGun projectile definition.
- Verify montage event Event.Montage.FireGun is reliable on server and client.
- Decide whether FireGun remains projectile-based or later gains a hitscan weapon mode.
- Add ammo/reload only after the basic damage loop is stable.
- Keep weapon attachment state separate from attack ability state.

#### Civilian

Create a civilian role definition with no offensive abilities and no weapon requirement.

Initial profile:

- AmbientNPC control type.
- Civilian faction.
- Can be damaged.
- Cannot attack.
- Common health and resistance attributes.
- No XP reward by default.
- No enemy loot table by default.
- PopulationRespawn or PermanentUntilPopulationReset death policy.
- Work/interaction profile such as Observer, Worker, or Merchant.

The civilian should use a separate AAuraCivilian actor. It may derive from AAuraCharacterBase so it can reuse health, damage, death, combat sockets, and GAS integration, but it should not derive from AAuraCharacter or AAuraEnemy.

## 5. Battle system plan

### Phase B0: Combat identity and relationship rules

Add a replicated AuraCombatIdentityComponent to every combat-capable actor.

Minimum data:

- Faction ID.
- Control type.
- Combat profile.
- Targetable flag.
- CanAttack.
- CanBeDamaged.
- Friendly-fire policy.
- Death policy.
- Optional team ID or temporary allegiance override.

Create a centralized AuraCombatRules service or library with functions similar to:

- CanDamage(Source, Target, DamageContext).
- GetRelationship(Source, Target).
- CanTarget(Source, Target, TargetingContext).
- CanReceiveDamage(Target, DamageContext).

The result should include a reason for rejection, such as Friendly, InvalidTarget, Dead, Protected, or ProfileDenied.

Replace every direct IsNotFriend check with this service. At minimum, cover:

- AuraAbilitySystemLibrary damage application.
- AuraProjectile.
- Projectile and hitscan action nodes.
- Beam and radial damage.
- Enemy target selection.
- Player target validation.
- Any future melee traces.

The server must be authoritative. A client may request an action, but the server validates source role, target relationship, range, cooldown, ability ownership, and damage.

Suggested initial relationship matrix:

| Source | Player | Enemy | Civilian |
| --- | --- | --- | --- |
| Player | Friendly unless PvP is enabled | Hostile | Hostile only in configured conflict zones; otherwise denied or protected |
| Enemy | Hostile | Friendly | Hostile if the battle scenario allows civilian casualties |
| Civilian | Never attacks | Never attacks | Friendly |

Make this data-driven later. Do not hide the civilian decision inside projectile code.

### Phase B1: Targeting and interaction contracts

Add a generic ICombatTargetInterface or ITargetableInterface for actors that can be selected or damaged.

It should expose:

- IsTargetable.
- GetTargetType.
- GetCombatIdentity.
- GetHealth/death state.
- GetTargetingPriority.
- GetInteractionOptions.

Keep EnemyInterface for enemy-specific UI and behavior during migration, but stop using it as the universal combat target test.

Update AuraPlayerController targeting:

- Target hostile combatants when combat is allowed.
- Target civilians as interactable or observable actors.
- Show different cursor/highlight states for enemy, civilian, merchant, downed, and dead.
- Never let a client-only highlight imply that the server accepts the target.

Add range, line-of-sight, dead-state, and safe-zone checks to target validation.

### Phase B2: Combat state and damage attribution

Introduce a replicated combat/death state with at least:

- Alive.
- Dying.
- Dead.
- Respawning.

Downed can be added later if the game needs it.

Fatal damage handling should be idempotent. Only the server may transition an actor into Dying or Dead. Repeated damage after the first fatal event must not grant additional XP, loot, or death notifications.

Extend the custom GameplayEffectContext with attribution:

- Instigator actor/controller/player state.
- Source actor and weapon.
- Source role.
- Ability tag.
- Damage type.
- Hit location and timestamp if needed.
- Battle/event ID.

Use this information for:

- Kill feed.
- XP and reward decisions.
- Civilian death notifications.
- AI threat/aggro.
- Future reputation or crime rules.
- Analytics and replay/debug logging.

### Phase B3: Ability and weapon integration

Continue using AuraAbilityGraph for Aura and BungeeMan. Add the combat rule check at the final damage boundary, not only at the ability-selection layer.

For every attack ability, verify:

- The source owns the ability.
- The source role permits the ability.
- The source is Alive and not stunned/dead.
- The target relationship permits the damage.
- The target is alive and damageable.
- Damage is applied only on the server.
- The damage context contains attribution.

Before adding many new abilities, resolve the known migration issues documented in Docs/Tracking/GAS-Migration-TODOs.md, especially:

- Knockback direction inconsistency.
- Montage success when no montage is configured.
- Montage event delegate handling.
- Active graph validation.
- Cooldown scaling.
- Hitscan impulse direction.

These issues become more expensive once three entity types depend on the same ability path.

### Phase B4: AI targeting and civilian behavior

Replace BTService_FindNearestPlayer with a generic faction-aware FindNearestHostile service for hostile AI.

The service should:

- Query targetable actors.
- Use AuraCombatRules.
- Ignore dead, protected, or unreachable actors.
- Consider distance, line of sight, threat, and combat zone.
- Avoid selecting friendly civilians unless the battle configuration allows it.

Create an AAuraCivilianAIController and a civilian behavior tree or BehaviorU graph with states such as:

- Idle.
- Wander.
- Work.
- Observe conflict.
- Flee.
- Seek shelter.
- Interact.
- Dead.

Civilian AI should not be implemented as an enemy with its attack nodes removed. Its movement, perception, fear, workplace, and death transitions are different.

For the first slice, use a simple state machine:

- Peace: perform a work or observation schedule.
- Nearby threat: move to a shelter or flee point.
- Direct attack: flee and notify nearby civilians.
- Dead: stop behavior and notify the population/workplace system.

Add navigation and shelter markers before building complex crowd simulation.

### Phase B5: Battle director and conflict zones

Add a server-authoritative AuraBattleDirector or equivalent GameMode subsystem.

Responsibilities:

- Current battle phase: Peace, Alert, Conflict, Cleanup.
- Active conflict zones.
- Faction relationship overrides per zone.
- Enemy and civilian spawn/despawn rules.
- Battle event IDs.
- Safe zones and protected civilians.
- Notifications for AI, UI, rewards, and persistence.

This is preferable to scattering conflict state across individual actors.

Start with one configured conflict zone. Its data should define:

- Which factions may damage each other.
- Which civilian casualties are allowed.
- Enemy wave/spawn table.
- Civilian population table.
- Respawn or population refill policy.
- Safe/shelter locations.

### Phase B6: Death, rewards, and population lifecycle

Keep death policy-specific behavior outside the common damage calculation.

Player:

- Preserve current player respawn flow.
- Preserve player progression and save behavior.
- Ensure the role is reapplied before the new ability loadout is granted.

Enemy:

- Preserve enemy loot, XP, lifespan, and respawn behavior.
- Move the decision behind the death policy rather than relying only on AAuraEnemy class checks.

Civilian:

- Stop AI and interaction.
- Play death animation, dissolve, or ragdoll according to the role/profile.
- Do not invoke player respawn.
- Do not grant enemy loot or XP unless explicitly configured.
- Notify the battle director, workplace, nearby civilians, and population manager.
- Remove or hide the actor after the configured corpse duration.
- Refill the population after a delay or at the next world reset if configured.

The fatal damage path should be:

    Damage request
        -> CanDamage
        -> damage calculation
        -> health reduction
        -> one server-side death transition
        -> policy-specific rewards and cleanup
        -> replicated state and notifications

### Phase B7: Business, inventory, and economy

There is no business system in the current project. Treat it as a separate, server-authoritative system rather than attaching arbitrary currency logic to an ability or pickup.

Minimum first version:

- Currency component or player wallet.
- Inventory component with item IDs and quantities.
- Merchant component on a civilian or shop actor.
- Static merchant offer data.
- Purchase request and validated transaction response.
- Save/load for wallet and inventory.

Suggested data:

- ItemDefinitions.json.
- MerchantDefinitions.json.
- Optional EconomyConfig.json.

Suggested runtime types:

- AuraCurrencyComponent.
- AuraInventoryComponent.
- AuraMerchantComponent.
- FAuraMerchantOffer.
- FAuraCommerceTransaction.

Every transaction must be validated on the server for:

- Merchant availability.
- Player distance and interaction permission.
- Offer availability and stock.
- Price and currency balance.
- Inventory capacity.
- Duplicate request protection.
- Atomic debit and grant.

For the first merchant slice:

- A Civilian has a Merchant economy profile.
- The merchant has a small static offer list.
- The player interacts with the civilian.
- The server validates and completes a purchase.
- Currency and inventory survive save/reload.

Later systems can add supply chains, civilian businesses, shop destruction, worker schedules, and conflict-driven price or stock changes.

## 6. Recommended source and content changes

### New runtime code

- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/Combat/AuraCombatRules.h
- Source/Aura/Private/Combat/AuraCombatRules.cpp
- Source/Aura/Public/Combat/AuraCombatStateComponent.h
- Source/Aura/Private/Combat/AuraCombatStateComponent.cpp
- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Public/AI/AuraCivilianAIController.h
- Source/Aura/Private/AI/AuraCivilianAIController.cpp
- Source/Aura/Public/AI/BTService_FindNearestHostile.h
- Source/Aura/Private/AI/BTService_FindNearestHostile.cpp
- Source/Aura/Public/Battle/AuraBattleDirector.h
- Source/Aura/Private/Battle/AuraBattleDirector.cpp
- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp

Names may follow the existing project naming conventions, but the responsibilities should stay separate.

### Existing code to modify

- RoleInfo: parse and validate the extended role schema.
- AuraCharacterBase: explicit role state clearing/application and shared combat identity access.
- AuraCharacter: player role validation, role loadout initialization, player death policy.
- AuraPlayerState: server-authoritative role selection and role replication.
- AuraEnemy: combat identity, target selection, death policy, and reward attribution.
- AuraAbilitySystemLibrary: replace IsNotFriend with AuraCombatRules.
- AuraProjectile: server-side target permission and attribution.
- ExecCalc_Damage: safe profile/curve lookup and context attribution.
- AuraAttributeSet: idempotent death transition and profile-aware rewards.
- AuraPlayerController: generic targetable/interactable UI.
- AuraAIController and behavior services: faction-aware targeting.
- AuraGameModeBase: population spawning and policy-based respawn.
- LoadScreenSaveGame: currency, inventory, and any selected civilian population state.

### New or extended content data

- Content/Config/RoleConfig.json: add Civilian and the explicit profile fields.
- Content/Config/PopulationSpawnTable.json: civilian and enemy population definitions.
- Content/Config/MerchantDefinitions.json: merchant offers and stock policies.
- Content/Config/ItemDefinitions.json: item identity, stack limits, and prices if needed.
- Content/Config/BattleZones.json: conflict and safe-zone rules.
- Content/AbilityDefinitions: only add civilian interaction abilities if an ability graph is actually useful; civilians should not receive offensive abilities.

## 7. Implementation order

Use the following order for the first vertical slice:

1. Capture a clean baseline with existing Aura and BungeeMan abilities, damage, death, save/load, and multiplayer PIE tests.
2. Add CombatIdentity and AuraCombatRules without changing visible behavior for existing Player/Enemy combat.
3. Route projectile, ability, beam, radial, and melee damage through AuraCombatRules.
4. Refactor RoleInfo and ApplyRole so empty loadouts explicitly clear state and equipment is independent from LMB ability.
5. Add the Civilian role configuration and validation.
6. Add AAuraCivilian with a simple idle/wander/work behavior and common health/damage support.
7. Add civilian target selection, civilian damage, and civilian-specific death policy.
8. Add generic hostile targeting and verify Enemy behavior still targets players correctly.
9. Add a small battle zone with one enemy spawn group and one civilian population group.
10. Add civilian flee/shelter behavior and battle director notifications.
11. Add a minimal merchant, wallet, inventory, and validated purchase flow.
12. Add persistence for wallet/inventory and the required population state.
13. Add UI for role selection, faction-aware targeting, civilian interaction, commerce, and battle events.
14. Only after this slice is stable, consider role hot-swapping, ammo/reload, advanced civilian schedules, reputation, and large-scale crowd optimization.

## 8. Test and acceptance matrix

### Role tests

- RoleConfig loads Aura, BungeeMan, and Civilian.
- Duplicate or invalid roles are rejected with actionable errors.
- Aura receives only its intended role abilities.
- BungeeMan receives FireGun and the correct rifle/socket configuration.
- Civilian receives no offensive ability, no LMB attack, and no weapon by inference.
- Role selection is validated by the server.
- Save/load restores a valid player role.
- A role cannot inject arbitrary asset or ability paths from the client.

### Combat tests

- Player damages hostile Enemy.
- Enemy damages Player.
- Enemy damages Civilian when the battle zone allows civilian casualties.
- Player damages Civilian only when the configured conflict rule allows it.
- Player and Enemy do not damage friendly actors.
- Civilian cannot initiate an attack.
- Dead actors cannot receive additional effective damage, XP, loot, or death events.
- Projectile, beam, radial, hitscan, and melee paths all use the same relationship rule.
- Damage attribution identifies the correct source, ability, and damage type.

### Lifecycle tests

- Player death uses player respawn.
- Enemy death uses enemy reward and respawn policy.
- Civilian death never calls player respawn.
- Civilian death never grants enemy loot unless configured.
- Civilian population refill obeys the battle zone policy.
- Client and server show the same role visuals and death state.

### Economy tests

- A valid merchant purchase debits currency and grants an item atomically.
- Insufficient currency, stock, capacity, range, and invalid offer requests fail.
- Replayed purchase requests do not double-spend or duplicate items.
- Wallet and inventory survive save/reload.

### Network tests

- Run a dedicated-server or listen-server test with at least one player, one enemy, and multiple civilians.
- Confirm clients cannot authoritatively apply damage, choose a target they cannot reach, grant currency, or select an invalid role.
- Confirm role, combat state, health, death, and commerce results replicate correctly.

## 9. Decisions to make before implementation

The following choices materially affect the design:

1. Is Civilian ambient-only in the first release, or can a player select it?
   Recommended: ambient-only first.

2. Can players damage civilians?
   Recommended: protected outside conflict zones, configurable inside conflict zones.

3. Can enemies damage civilians?
   Recommended: yes in configured conflict zones, otherwise no.

4. Are civilian deaths permanent, delayed population respawns, or reset on level reload?
   Recommended: delayed population respawn for the first slice.

5. Does a civilian death reward the killer?
   Recommended: no XP and no standard enemy loot; use explicit battle rewards later.

6. Does every civilian use GAS?
   Recommended: yes for the first slice to reuse health, damage, replication, and death integration. Optimize to a lightweight health component only if population scale requires it.

7. Can players switch roles during a session?
   Recommended: role locked at login/match spawn until loadout cleanup is implemented.

8. Does BungeeMan need ammo and reload immediately?
   Recommended: no. Stabilize FireGun damage and targeting first.

## 10. Things to avoid in the first implementation

- Do not add Civilian as another ECharacterClass value.
- Do not add more raw Player/Enemy/Civilian tag checks throughout abilities.
- Do not make Civilian inherit from AAuraEnemy just to reuse AI.
- Do not infer equipment from whether an ability exists.
- Do not allow an empty role array to mean “keep Blueprint defaults.”
- Do not add runtime role switching before role-granted ASC cleanup exists.
- Do not put merchant transactions inside Gameplay Effects without a separate server transaction boundary.
- Do not let clients choose role assets, abilities, prices, damage, or inventory results.
- Do not build advanced crowd simulation before the simple civilian lifecycle is correct.

## 11. First vertical-slice definition of done

The first implementation is successful when:

- Aura and BungeeMan can use their current data-driven abilities.
- A Civilian can spawn, wander or work, flee from a threat, and interact as a non-combatant.
- Combat permissions come from a shared faction rule instead of IsNotFriend tag logic.
- Players and enemies can damage civilians only under the configured battle-zone rule.
- Civilian death is replicated, idempotent, does not respawn the player, and does not grant enemy rewards by default.
- Enemy targeting still works after the relationship refactor.
- Role selection and ability grants are server-validated.
- A simple civilian merchant can complete a validated purchase.
- Wallet and inventory can be saved and restored.
- The result works in a multiplayer test with server-authoritative damage and commerce.

This gives the project a stable base for later additions such as civilian reputation, witnesses, faction wars, weapon inventory, advanced business simulation, role switching, and large populations.
