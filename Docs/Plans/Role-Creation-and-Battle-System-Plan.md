# Role Creation and Battle System Plan

Status: Revised implementation contract; Day 01 headless gate executed conditionally, Days 02–04 implemented and verified, Day 05 pending

This plan is based on the current AuraProj code and configuration. It is intended to turn the existing Aura/BungeeMan prototype into a system that can support:

- Aura Girl: magic-focused combatant.
- BungeeMan: gun-focused combatant.
- Civilian: a non-combatant who observes, works, trades, flees, and can be killed during a conflict.

The locked first version treats Civilian as an ambient AI-controlled NPC and rejects it during player role selection. The same role definition may be reused for a playable civilian only after a later design explicitly adds player selection, input, progression, and transactional role switching.

## 1. Main architectural decision

The current project uses the word role for several different concepts at once. A role currently controls mesh, animation, weapon, attributes, abilities, and some death assets. Player and enemy behavior is controlled separately through inheritance and raw actor tags.

Keep these concepts separate:

| Concept | Example | Responsibility |
| --- | --- | --- |
| Role definition | Aura, BungeeMan, Civilian | Appearance, base attributes, loadout, interaction profile |
| Combat profile | Magic, Gun, Civilian | Attack capability, damage permissions, target rules |
| Faction | Player, Enemy, Civilian | Friend/foe relationships |
| Control type | Player, EnemyAI, CivilianAI | Input, AI, possession, replication behavior |
| Death policy | Death.PlayerRespawn, Death.EnemyLoot, Death.PopulationRespawn | What happens after fatal damage |
| Economy profile (capability only) | Economy.None, Economy.Ambient, Economy.CommerceCapable | Whether an actor may host work or commerce features; not a merchant instance ID |
| Population member data | WorkProfileId plus optional MerchantDefinitionId | Per-instance work, merchant, stable-member, and population data |

The resulting model should be:

    RoleDefinition
        -> visual presentation
        -> attributes and ability loadout
        -> equipment, selection, interaction, and economy profile/capability

    CombatIdentity
        -> faction
        -> control type
        -> combat profile
        -> combat permissions
        -> death policy

    PopulationMember
        -> stable PopulationId:SlotIndex member ID
        -> work and shelter profile
        -> optional MerchantDefinitionId

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
| Player role persistence | AuraCharacter, AuraPlayerState, Game/LoadScreenSaveGame | Refactor from one process-global slot into stable per-player records |
| Combat identity (Day 02 complete) | Combat/AuraCombatIdentityComponent and AuraCombatTypes | Keep the replicated component; replace temporary `Combat.Unassigned` through role application on Day 06 |
| Ability System Component and attributes | AuraCharacterBase, AuraPlayerState, AuraEnemy, AuraAttributeSet | Reuse for the first civilian vertical slice |
| Damage calculation | ExecCalc_Damage | Keep the calculation, replace the target permission gate |
| Damage application | AuraAbilitySystemLibrary, AuraProjectile, data-driven action nodes | Route all damage through combat rules |
| Data-driven abilities | Plugins/AuraAbilityGraph and Content/AbilityDefinitions | Use for Aura and BungeeMan |
| Enemy behavior | AuraEnemy, AuraAIController, Unreal Behavior Trees, BTService_FindNearestPlayer | Generalize hostile targeting; Day 10 introduces `BTService_FindNearestHostile`; do not copy enemy loot/death behavior to civilians |
| Pickup effects | AuraEffectActor and Content/Config/PickupDefinitions.json | Reuse for health/mana pickups |
| Building placement | AuraBuildingComponent and AuraPlacedBuildingActor | Reuse only if civilians can own or interact with buildings |
| Player/enemy spawning | AuraGameModeBase, AuraEnemySpawnVolume, spawn tables | Preserve existing enemy ownership; add a separate Civilian population manager |
| Save/load | Game/LoadScreenSaveGame and AuraGameModeBase world state | Split stable per-player data from one load-once world snapshot |

The current RoleConfig.json already contains Aura and BungeeMan. Their existing ability definitions are:

- Aura: FireBlast, ArcaneShards, Electrocute, with FireBolt as the LMB ability.
- BungeeMan: FireGun as the LMB ability, with a rifle mesh and Muzzle socket.

Day 02 has added the replicated combat-identity foundation, but its relationship behavior is intentionally a narrow Player/Enemy compatibility table and Player/Enemy profiles remain `Combat.Unassigned`. There is still no Civilian role/actor, general combat-rules service, commerce system, inventory, currency, Civilian AI, or Civilian population persistence.

## 3. Critical constraints in the current code

These should be fixed before adding the new role:

1. Role application does not reliably clear old abilities. ApplyRole only overwrites startup arrays when the incoming arrays are non-empty. An explicitly empty Civilian loadout could therefore leave Blueprint defaults or previously granted abilities active.

2. Weapon setup is inferred from the presence of an LMB ability. This is unsafe for Civilian. Equipment must be an explicit field, independent of attack capability.

3. Day 02 migrated `IsNotFriend` to a conservative identity-based Player/Enemy compatibility table. It intentionally rejects every Civilian pairing and is not the final relationship API; Day 03 must replace it with context-aware rules.

4. The replicated identity component now exists, but Player/Enemy combat profiles are deliberately `Combat.Unassigned`, and some compatibility filters still use raw actor tags. Day 06 must apply validated role-derived identity while later cleanup removes remaining tag assumptions.

5. AAuraCharacter death leads into the player respawn path. Civilian death must never invoke player respawn logic.

6. AAuraEnemy death includes enemy-specific loot, lifespan, and respawn assumptions. Civilian needs a separate death policy.

7. BTService_FindNearestPlayer is player/enemy-specific. It cannot be the basis for civilian behavior or general faction-aware targeting.

8. The targeting UI mainly recognizes EnemyInterface. Civilian needs targetable/interactable semantics without being treated as an enemy.

9. ExecCalc_Damage assumes the character-class damage coefficient setup is available. Civilian needs a safe combat/stat profile, even if it has no abilities.

10. Runtime role switching currently changes presentation and configuration but does not transactionally remove old ASC abilities, effects, or attributes. Lock role selection at spawn for the first version.

11. RoleConfig parsing uses required JSON getters for many fields. Invalid or incomplete civilian data should produce a validation error, not a load-time crash or partially configured role.

12. Damage context does not yet contain complete killer/instigator/ability attribution. This will be needed for kill feeds, civilian consequences, AI aggro, statistics, and future crime/reputation rules.

13. `UAuraDamageGameplayAbility::CauseDamage` applies a GameplayEffect spec directly. An exhaustive migration must cover this path as well as `ApplyDamageEffect`, projectiles, fireballs, data-driven action nodes, debuffs, and any client-provided target data.

14. The current in-game save path reads and writes one `UAuraGameInstance` slot. That is not a multiplayer player identity. Per-player progression must be keyed by an authenticated provider-validated `FUniqueNetIdRepl`, while population and merchant state belong to a separate authoritative world snapshot keyed by a validated server-owned `WorldPersistenceId` (campaign/shard/world slot), never by a client URL or map name alone.

15. Raw JSON and XML under `Content/Config` and `Content/AbilityDefinitions` are runtime inputs. Editor success does not prove they are present in packaged client and dedicated-server builds; staging and packaged-load checks are release requirements.

16. `AAuraGameModeBase` already owns table-spawned enemy respawn timers. The first Civilian population manager must not also refill those enemies. There must be one spawn/refill owner per member.

17. GameMode exists only on the server. Any battle phase or zone state clients must observe belongs on a replicated actor or GameState, with an idempotent late-join path.

18. Role and economy config reload must use parse -> aggregate errors -> validate -> atomically publish. A failed reload retains the last known-good registry instead of replacing it with null or partial data.

## 4. Role data model and configuration

### 4.1 Proposed role schema

`RoleConfig.json` has a required integer `roleDefinitionVersion`. Each entry uses full registered Gameplay Tag strings for identity/profile fields; the parser does not silently translate shorthand such as `Player` into `Faction.Player`. The snippets below emphasize role differences; the Day 05 contract is normative for the complete required-field set.

    role: "Aura"
    displayName: "Aura Girl"
    playerSelectable: true
    entityType: "Entity.Player"
    controlType: "Control.Player"
    combatProfile: "Combat.Magic"
    faction: "Faction.Player"
    canAttack: true
    canBeDamaged: true
    allowFriendlyFire: false
    deathPolicy: "Death.PlayerRespawn"
    economyProfile: "Economy.None"
    interactionProfile: "Interaction.Combatant"
    targetable: true
    equipment:
        weaponMesh: ...
        attachSocket: "WeaponHandSocket"
        tipSocket: ...
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
    playerSelectable: true
    entityType: "Entity.Player"
    controlType: "Control.Player"
    combatProfile: "Combat.Gun"
    faction: "Faction.Player"
    canAttack: true
    canBeDamaged: true
    allowFriendlyFire: false
    deathPolicy: "Death.PlayerRespawn"
    economyProfile: "Economy.None"
    interactionProfile: "Interaction.Combatant"
    targetable: true
    equipment:
        weaponMesh: ...
        attachSocket: "WeaponHandSocket"
        tipSocket: "Muzzle"
    startupAbilityDefinitions: []
    lmbAbilityDefinition: "/Game/AbilityDefinitions/FireGun.xml"

    role: "Civilian"
    displayName: "Civilian"
    playerSelectable: false
    entityType: "Entity.AmbientNPC"
    controlType: "Control.CivilianAI"
    combatProfile: "Combat.Civilian"
    faction: "Faction.Civilian"
    canAttack: false
    canBeDamaged: true
    allowFriendlyFire: false
    targetable: true
    deathPolicy: "Death.PopulationRespawn"
    economyProfile: "Economy.Ambient"
    interactionProfile: "Interaction.Civilian"
    startupAbilityDefinitions: []
    startupPassiveAbilityDefinitions: []
    lmbAbilityDefinition: ""

Role IDs and stable population/member IDs use validated `FName`/string contracts. Faction, control, combat, death, entity, interaction, and economy capability use registered Gameplay Tags. Avoid spreading hard-coded C++ enums for future roles, factions, jobs, or weapons.

Recommended required fields:

- role
- displayName
- playerSelectable
- entityType
- controlType
- combatProfile
- faction
- canAttack
- canBeDamaged
- targetable
- allowFriendlyFire
- deathPolicy
- economyProfile
- interactionProfile
- mesh
- animBlueprint
- attributes
- ability arrays

Recommended optional fields:

- equipment
- combat sockets
- death assets
- workProfile
- spawnProfile
- voice/profile assets

### 4.2 Role parser and validation

Load into a temporary registry using safe getters for every field. Aggregate all parse and validation errors, publish the registry only when the complete file is valid, and retain the last known-good registry after a failed editor reload. Server login/spawn fails closed when no valid registry has ever been published.

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
- A client-selected role whose `playerSelectable` value is false or whose entity/control type is not a player role.
- Unsupported `roleDefinitionVersion`, missing required full Gameplay Tags, and shorthand/unregistered tags.
- A role whose economy/interaction profile conflicts with its entity type.

IsRoleConfigured should become profile-aware. A player combat role needs a weapon/attack configuration; a civilian does not.

The server owns role selection and resolves only a validated role ID. Clients never submit asset paths, ability paths, tags, or role-definition payloads.

### 4.3 ApplyRole refactor

Refactor spawn-time role application into explicit operations:

1. Initialize/rebind ASC actor info for the current owner/avatar without applying attributes or grants.
2. Resolve, validate, and fully stage the server-approved role candidate.
3. Snapshot current spawn-time caches/ledger, then clear and load runtime caches from the staged candidate.
4. Set pending authority-local applied state/identity and apply presentation without broadcasting or forcing replication.
5. Initialize attributes and reconcile/grant the loadout once through the persistent ASC grant ledger.
6. Roll back the snapshot on any failure; otherwise commit PlayerState role/applied identity, broadcast once, and force replication only after the whole transaction succeeds.

ClearRoleRuntimeState must clear:

- Body mesh and animation override.
- Weapon mesh and weapon socket state.
- Combat sockets and death assets.
- Startup ability arrays.
- Data-driven ability definition objects.
- LMB ability references.
- Role-specific cached attributes and profile data.
- Cached interaction/economy profile and pending combat-identity fields.

The persistent PlayerState ASC records the applied role ID, grant generation, role-granted ability spec handles, and source metadata. Saved abilities also retain versioned `GrantSource` and optional `GrantedRoleId`; a removed former role grant cannot silently become a progression ability after a definition change. Repeated pawn initialization or progress loading reuses that ledger and must not duplicate abilities or effects. A pawn-local boolean is not sufficient.

The first implementation applies the role before abilities are granted and rejects a different role after live grants exist. A test may replace pending role data before the first grant, but it must not pretend that this is supported runtime hot-swapping. If runtime role switching is later required, add a `RebuildRoleLoadout` transaction that:

- Validates the new role on the server.
- Removes only abilities/effects granted by the previous role.
- Applies the new attributes and equipment.
- Grants the new role abilities.
- Replicates the result to clients.
- Rolls back if any asset or grant step fails.

Use an explicit role-granted source tag/source object plus recorded handles so the system knows which abilities may later be removed. Do not remove arbitrary player abilities, and do not clear live specs merely because an empty array was parsed.

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
- PopulationRespawn death policy for the first slice.
- Ambient interaction/economy capability; per-instance Observer/Worker work profiles come from population data.
- No merchant identity on the shared role. A stable population member becomes a merchant only when its validated `MerchantDefinitionId` is non-empty.

The civilian should use a separate AAuraCivilian actor. It may derive from AAuraCharacterBase so it can reuse health, damage, death, combat sockets, and GAS integration, but it should not derive from AAuraCharacter or AAuraEnemy.

## 5. Battle system plan

### Phase B0: Combat identity and relationship rules

Keep the replicated `AuraCombatIdentityComponent` delivered on Day 02 and apply it to every later combat source/target avatar. Transient projectiles/effect carriers use their source avatar's identity instead of owning a second identity.

Day 03 also introduces the minimal replicated `AuraCombatStateComponent` with Alive, Dying, Dead, and Respawning. Rules may therefore reject Dying/Dead actors before damage migration begins. Policy-specific death consequences are added on Day 11.

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
- CanCombatTarget(Source, Target, CombatTargetingContext).
- CanReceiveDamage(Target, DamageContext).

The result should include a reason for rejection, such as Friendly, InvalidTarget, Dead, Protected, or ProfileDenied.

Combat targeting, threat evaluation, cosmetic highlighting, and interaction are distinct operations. In particular, a Civilian with `bCanAttack=false` may still observe an actor that can damage it, and an interactable Civilian need not be a legal combat target.

Day 03 defines a rule-context/resolver contract but has no battle zones yet. Its default resolver denies Player/Enemy damage to Civilian. Day 12 installs the authoritative zone resolver and is the first milestone allowed to accept configured Civilian casualties. The final server application boundary resolves current context itself; client/caller-supplied zone or permission results are never trusted.

Replace every direct IsNotFriend check with this service. At minimum, cover:

- AuraAbilitySystemLibrary damage application.
- AuraProjectile.
- Projectile and hitscan action nodes.
- Beam and radial damage.
- Enemy target selection.
- Player target validation.
- Any future melee traces.

The server must be authoritative. A client may request an action, but the server validates source role, target relationship, life state, range/line of sight where applicable, cooldown, ability ownership, target data, and damage at the final boundary.

Suggested initial relationship matrix:

| Source | Player | Enemy | Civilian |
| --- | --- | --- | --- |
| Player | Friendly unless PvP is enabled | Hostile | Protected by default; Day 12 may allow damage in a resolved conflict zone |
| Enemy | Hostile | Friendly | Protected by default; Day 12 may allow damage in a resolved conflict zone |
| Civilian | Never attacks | Never attacks | Friendly |

Make this data-driven later. Do not hide the civilian decision inside projectile code.

### Phase B1: Targeting and interaction contracts

Add a generic `IAuraTargetableInterface` for highlight/selection data and a separate player-owned interaction request component. Do not make the target own the client-to-server RPC endpoint.

It should expose:

- IsTargetable.
- GetTargetKind.
- GetCombatIdentity.
- GetHealth/life state.
- GetTargetingPriority.
- GetInteractionTags or an interactable-provider reference, without treating those tags as combat relationship state.

Keep EnemyInterface for enemy-specific UI and behavior during migration, but stop using it as the universal combat target test.

Update AuraPlayerController targeting:

- Target hostile combatants when combat is allowed.
- Target civilians as interactable or observable actors.
- Compose UI from orthogonal relationship, target kind, life state, and interaction tags. A merchant remains a Civilian and a dead Enemy remains an Enemy target kind.
- Never let a client-only highlight imply that the server accepts the target.

Add a distinct Interact input. Its request originates on the owning PlayerController/component and sends only a target reference plus option ID. The server re-resolves target, option, range, line of sight, life state, and interaction permission. Attack affordance uses `CanDamage(Player, Target, ResolvedContext)`, never the target's `bCanAttack` value.

### Phase B2: Combat state and damage attribution

The state foundation introduced on Day 03 contains:

- Alive.
- Dying.
- Dead.
- Respawning.

Downed can be added later if the game needs it.

Fatal damage handling is idempotent. The authority transition returns whether the caller won Alive -> Dying. Only the winning path may emit death, XP, loot, population, corpse-cleanup, or battle notifications. Replicated `OnRep` presentation is idempotent so late joiners receive the same collision, movement, AI, interaction, and visual state.

Extend the custom GameplayEffectContext with attribution:

- Instigator actor/controller/player state.
- Source actor and weapon.
- Source role.
- Ability tag.
- Damage type.
- Hit location and timestamp if needed.
- Battle/event ID.

Every new field is added to `FAuraGameplayEffectContext::Duplicate`/`NetSerialize` and covered by a network serialization round-trip test. Existing inherited instigator/effect-causer data is reused instead of redundantly serializing unsafe pointers.

Use this information for:

- Kill feed.
- XP and reward decisions.
- Civilian death notifications.
- AI threat/aggro.
- Future reputation or crime rules.
- Analytics and replay/debug logging.

### Phase B3: Ability and weapon integration

Continue using AuraAbilityGraph for Aura and BungeeMan. Add the combat rule check at the final damage boundary, not only at the ability-selection layer.

The migration inventory explicitly includes `UAuraDamageGameplayAbility::CauseDamage`, `MakeDamageEffectParamsFromClassDefaults`, `AuraProjectile`, `AuraFireBall`, debuff ticks, every AuraAbilityGraph damage action, projectile parameter builders, and any other direct `ApplyGameplayEffectSpecToTarget/Self` path that can reduce health. A legacy path is either routed through the boundary, proven non-damaging, or removed with evidence; it is not omitted from the inventory.

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

Day 10 replaces `BTService_FindNearestPlayer` with `BTService_FindNearestHostile` while preserving the existing blackboard contract during asset migration. This is an explicit deliverable, not a Day 20 cleanup suggestion.

The service should:

- Query targetable actors.
- Use AuraCombatRules.
- Ignore dead, protected, or unreachable actors.
- Consider distance, line of sight, threat, and combat zone.
- Avoid selecting friendly civilians unless the battle configuration allows it.

Create `AAuraCivilianAIController` and a server-authoritative Unreal Behavior Tree/blackboard. BehaviorU is not used for the first Civilian slice. The exact assets, keys, tasks, services, decorators, controller startup, and authority guards are part of the Day 10 manifest.

- Idle.
- Wander.
- Work.
- Observe conflict.
- Flee.
- Shelter.

Interact is a Day 14 player request, not an `EAuraCivilianActivity`. Dying/Dead/Respawning remain Day 03 combat life states outside the AI activity enum and stop the tree.

Civilian AI should not be implemented as an enemy with its attack nodes removed. Its movement, perception, fear, workplace, and death transitions are different.

For the first slice, use a simple state machine:

- Peace: perform a work or observation schedule.
- Nearby threat: move to a shelter or flee point.
- Direct attack: flee and notify nearby civilians.
- Non-Alive combat state: stop behavior; the Day 11 global death event notifies population/workplace subscribers.

Add work, observation, flee, and shelter marker actors/interfaces before wiring the behavior tree. The threat service evaluates `CanDamage(CandidateThreat, Civilian, ServerResolvedContext)` directly in that direction, rather than asking whether the non-attacking Civilian can target the candidate. The potential source need not itself be targetable. Before Day 12, the flee branch is exercised only through the Day 03 authority-only non-shipping trusted policy fixture; production remains default-deny.

### Phase B5: Battle director and conflict zones

Add one server-spawned `AAuraBattleDirector` derived from an always-relevant replicated `AInfo`-style actor. GameMode creates and authoritatively drives it; clients and late joiners observe replicated phase/event state through the director. Do not rely on a GameMode-only subsystem for client-visible state.

Responsibilities:

- Current battle phase: Peace, Alert, Conflict, Cleanup.
- Active conflict zones.
- Faction relationship overrides per zone.
- Civilian population phase/refill policy and notifications to the existing population manager.
- Battle event IDs.
- Safe zones and protected civilians.
- Notifications for AI, UI, rewards, and persistence.

This is preferable to scattering conflict state across individual actors.

Zone policy is resolved at the final server damage boundary from the current target/hit location. If any containing zone is safe, discard non-safe candidates; within the remaining safe set choose highest priority, then lexicographically smallest stable Zone ID. If no safe zone contains the target, apply the same priority/tie rule to all containing conflict zones. Callers cannot assert their own zone or accepted permission. Source-zone restrictions, when enabled, are an additional deny and never override target protection.

Start with one configured conflict zone. Its data should define:

- Which factions may damage each other.
- Which civilian casualties are allowed.
- Existing enemy-group reference for scenario setup, without transferring enemy respawn ownership.
- Civilian population table.
- Respawn or population refill policy.
- Safe/shelter marker references created on Day 10.

### Phase B6: Death, rewards, and population lifecycle

Keep death policy-specific behavior outside the common damage calculation.

Day 11 adds one GameMode-owned dispatcher/strategy keyed by `DeathPolicyTag`. Concrete actors provide policy dependencies or presentation hooks, but they do not independently decide which policy applies. Only the successful Alive -> Dying winner may dispatch, and the dispatcher publishes one global neutral server death event. The Day 09 population manager binds once immediately; the Day 12 battle director binds once when introduced, so Day 11 has no compile-time dependency on a future concrete director or per-actor subscription race.

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
- Publish the neutral death event to currently registered subscribers such as the population/workplace systems and nearby-civilian notifier; the Day 12 director subscribes once it exists.
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

For the first slice, `AuraPopulationManager` owns only Civilian registration, corpse cleanup, and refill. Existing table-spawned enemy timers and `OnDestroyed` bindings remain owned by `AAuraGameModeBase`. A later migration may unify them only by transferring and canceling the old owner atomically.

### Phase B7: Business, inventory, and economy

There is no business system in the current project. Treat it as a separate, server-authoritative system rather than attaching arbitrary currency logic to an ability or pickup.

Minimum first version:

- Currency and inventory components owned by `AAuraPlayerState` and replicated owner-only.
- Inventory entries use validated item IDs and integer quantities; capacity means a configured number of occupied item stacks/slots in the first slice.
- Merchant component on a stable Civilian population member whose optional `MerchantDefinitionId` is valid.
- Static merchant offer data.
- Per-instance stock keyed by stable population member ID.
- Purchase request from a player-owned endpoint and an explicit owner-only result keyed by request ID.
- Separate per-player wallet/inventory saves and authoritative world merchant-stock/population save.

Suggested data:

- ItemDefinitions.json.
- MerchantDefinitions.json.
- EconomyConfig.json.

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
- Required role/interaction eligibility.
- Duplicate request protection.
- Atomic debit and grant.

Currency, price, stock, and quantity use bounded integer types (`int64` for currency/prices and validated integer quantities for stock/items); floating-point money is not used. Offer buy price is the single authority for purchases. Item sell value is omitted until selling is implemented.

The request originates through the player-owned Day 14 interaction endpoint. The server resolves the merchant component, stable member ID, immutable offer, current stock, and player state. A server-issued session nonce plus monotonic client sequence scopes request IDs; a bounded server cache returns the same result for an active-session replay. Validation and commit execute on the game thread without yielding, all preflight checks complete before mutation, and UI/save delegates fire only after the complete debit/stock/item commit succeeds.

For the first merchant slice:

- One Civilian population member has a `MerchantDefinitionId`; other members sharing the Civilian role do not become merchants.
- The merchant has a small static offer list.
- The player interacts with the civilian.
- The server validates and completes a purchase.
- Currency, inventory, population identity, and per-instance merchant stock survive save/reload or follow an explicitly tested restock policy. The first slice chooses persistence.

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
- Source/Aura/Public/AI/BTService_FindNearestThreat.h
- Source/Aura/Private/AI/BTService_FindNearestThreat.cpp
- Source/Aura/Public/AI/BTService_FindNearestHostile.h
- Source/Aura/Private/AI/BTService_FindNearestHostile.cpp
- Source/Aura/Public/World/AuraCivilianActivityMarker.h
- Source/Aura/Private/World/AuraCivilianActivityMarker.cpp
- Source/Aura/Public/World/AuraCivilianWorkMarker.h
- Source/Aura/Private/World/AuraCivilianWorkMarker.cpp
- Source/Aura/Public/World/AuraCivilianObservationMarker.h
- Source/Aura/Private/World/AuraCivilianObservationMarker.cpp
- Source/Aura/Public/World/AuraCivilianShelterMarker.h
- Source/Aura/Private/World/AuraCivilianShelterMarker.cpp
- Source/Aura/Public/World/AuraPopulationManager.h
- Source/Aura/Private/World/AuraPopulationManager.cpp
- Source/Aura/Public/Combat/AuraDeathPolicyDispatcher.h
- Source/Aura/Private/Combat/AuraDeathPolicyDispatcher.cpp
- Source/Aura/Public/Combat/AuraTargetableInterface.h
- Source/Aura/Private/Combat/AuraTargetableInterface.cpp
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp
- Source/Aura/Public/Interaction/AuraInteractionPolicy.h
- Source/Aura/Private/Interaction/AuraInteractionPolicy.cpp
- Source/Aura/Public/Battle/AuraBattleDirector.h
- Source/Aura/Private/Battle/AuraBattleDirector.cpp
- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp
- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp
- Source/Aura/Public/Economy/AuraCommerceSubsystem.h
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp

Names may follow the existing project naming conventions, but the responsibilities should stay separate.

### Existing code to modify

- RoleInfo: parse and validate the extended role schema.
- AuraCharacterBase: explicit role state clearing/application and shared combat identity access.
- AuraCharacter: player role validation, role loadout initialization, player death policy.
- AuraPlayerState: server-authoritative role selection, persistent ASC grant ledger, owner-only wallet/inventory, and stable profile identity.
- AuraEnemy: combat identity, target selection, death policy, and reward attribution.
- AuraAbilitySystemComponent: role-granted handle/source bookkeeping and idempotent persistent-ASC initialization.
- AuraAbilitySystemLibrary: replace IsNotFriend with AuraCombatRules and make config publication retain-last-good.
- AuraDamageGameplayAbility, AuraProjectile, and AuraFireBall: final server permission and attribution boundary.
- AuraAbilityTypes: serialize attribution and battle event data in the custom effect context.
- ExecCalc_Damage: safe profile/curve lookup and context attribution.
- AuraAttributeSet: idempotent death transition and profile-aware rewards.
- AuraPlayerController: generic targetable/interactable UI.
- AuraAIController and behavior services: faction-aware targeting.
- AuraGameModeBase: spawn the population manager/director, keep existing enemy respawn ownership, validate the server-owned `WorldPersistenceId`, and orchestrate load-once world restore.
- Game/LoadScreenSaveGame: versioned per-player and world snapshot types; no nonexistent `Save/` source directory.

### New or extended content data

- Content/Config/RoleConfig.json: add Civilian and the explicit profile fields.
- Content/Config/PopulationSpawnTable.json: Civilian population definitions, stable slots, work/zone data, and optional merchant IDs.
- Content/Config/MerchantDefinitions.json: merchant offers and stock policies.
- Content/Config/ItemDefinitions.json: item identity, stack limits, and prices if needed.
- Content/Config/BattleZones.json: conflict and safe-zone rules.
- Content/AbilityDefinitions: only add civilian interaction abilities if an ability graph is actually useful; civilians should not receive offensive abilities.
- Config/DefaultGame.ini: stage both `Content/Config` and `Content/AbilityDefinitions` runtime files and verify them after cook/package.
- Named native automation tests plus multi-process listen/dedicated runners and dated reports are part of each daily manifest, not deferred to Day 19.

## 7. Implementation order

The normative 20-milestone order is the daily implementation index:

1. Record the baseline and all direct/indirect damage paths; carry rendered/package gaps explicitly.
2. Add replicated combat identity without changing Player/Enemy behavior.
3. Add replicated life-state foundation, context-aware rules, and default-deny Civilian policy.
4. Route every damage producer and client target-data path through the final server boundary with serialized attribution.
5. Publish a versioned, fully validated role registry atomically.
6. Apply presentation and combat identity together and add persistent ASC grant bookkeeping.
7. Prove Aura/BungeeMan behavior in listen, dedicated, and packaged-data fixtures.
8. Add role-derived, replicated `AAuraCivilian` with GAS health and no offensive loadout.
9. Introduce the single Civilian population manager, stable member IDs, and validated spawning.
10. Add marker data, server-authoritative UE Behavior Trees, inverse threat evaluation, and the generic hostile-target service.
11. Add exactly-once policy-dispatched death on the Day 03 state foundation.
12. Add the replicated director and deterministic authoritative zone resolver; this is the first milestone that may permit Civilian casualties.
13. Extend the existing manager with Civilian corpse cleanup/refill while leaving enemy respawn under its existing owner.
14. Add orthogonal targeting UI and a distinct player-owned, server-validated Interact input/endpoint.
15. Publish a versioned, read-only economy registry and stable merchant/member binding.
16. Add owner-only PlayerState wallet/inventory with idempotent new-profile initialization.
17. Add replay-safe, atomic merchant purchase protocol, dynamic stock, UI, and focused tests.
18. Add stable per-player saves and a separate load-once world snapshot with migration/reconciliation.
19. Pass mandatory listen and dedicated two-client security, late-join, reconnect, concurrency, privacy, lifecycle, and performance matrices.
20. Perform cleanup, then rerun builds/automation/cook/package/config staging/listen/dedicated checks and publish a dated final report.

Only after this slice passes may role hot-swapping, ammo/reload, advanced Civilian schedules, reputation, unified enemy/Civilian population ownership, or large-scale crowd optimization begin.

## 8. Test and acceptance matrix

### Role tests

- RoleConfig loads Aura, BungeeMan, and Civilian.
- Duplicate or invalid roles are rejected with actionable errors.
- A failed initial load blocks server login/spawn; a failed reload retains the prior valid registry.
- Aura/BungeeMan/Civilian resolve full registered identity tags, with Aura=`Combat.Magic`, BungeeMan=`Combat.Gun`, and Civilian not player-selectable.
- Aura receives only its intended role abilities.
- BungeeMan receives FireGun and the correct rifle/socket configuration.
- Civilian receives no offensive ability, no LMB attack, and no weapon by inference.
- Role selection is validated by the server.
- Save/load restores a valid player role.
- A role cannot inject arbitrary asset or ability paths from the client.
- Repeated pawn initialization, respawn, and progress loading do not duplicate role-granted specs on the persistent ASC.

### Combat tests

- Player damages hostile Enemy.
- Enemy damages Player.
- Enemy damages Civilian when the battle zone allows civilian casualties.
- Player damages Civilian only when the configured conflict rule allows it.
- Player and Enemy do not damage friendly actors.
- Civilian cannot initiate an attack.
- Dead actors cannot receive additional effective damage, XP, loot, or death events.
- Native `CauseDamage`, projectile, fireball, beam, radial, hitscan, melee, debuff, and data-driven graph paths all use the same final server relationship boundary.
- Forged client target data, range/LOS, target, zone, or permission context cannot change authoritative health.
- Damage attribution serializes and round-trips the correct source, ability, damage type, and battle event.
- Safe-zone precedence, overlapping-zone priority/ties, boundary crossing, default policy, and caller-forged context have deterministic tests.

### Lifecycle tests

- Player death uses player respawn.
- Enemy death uses enemy reward and respawn policy.
- Civilian death never calls player respawn.
- Civilian death never grants enemy loot unless configured.
- Civilian population refill obeys the battle zone policy.
- Civilian stable member IDs survive refill/reload and never collide or duplicate.
- Existing enemy respawn creates exactly one replacement and is never scheduled by the Civilian population manager.
- Remote clients and late joiners show the same role visuals, combat state, collision, AI/interaction shutdown, and battle phase.

### Economy tests

- A valid merchant purchase debits currency and grants an item atomically.
- Insufficient currency, stock, capacity, range, and invalid offer requests fail.
- Required role/interaction eligibility is enforced from server-owned definitions.
- Replayed purchase requests do not double-spend or duplicate items.
- Simultaneous requests serialize to valid stock/balance results and failed commits emit no partial UI/save notification.
- Wallet/inventory are owner-only and survive pawn respawn, two-player save isolation, disconnect/reconnect, and old-save migration.
- Per-instance merchant stock persists by stable population member ID.

### Network tests

- Run both a listen host plus remote client and a dedicated server plus two remote clients, with at least one Enemy, five Civilians, and one stable Civilian merchant.
- Confirm clients cannot authoritatively apply damage, choose a target they cannot reach, grant currency, or select an invalid role.
- Confirm non-owners cannot read private wallet/inventory state and cannot call interaction/commerce RPCs through server-owned targets.
- Confirm role, combat state, health, death, battle phase, population, merchant stock, commerce results, and reconnect state replicate correctly, including late join.
- Record concurrency, network latency/loss, server tick/frame time, population size, duration, platform, and explicit pass thresholds.

### Package and release tests

- Build Editor, Game, and Server targets from the recorded revision.
- Run focused and full native automation plus AuraAbilityGraph and role/battle multi-process smokes after final cleanup.
- Cook and package client and dedicated server.
- Verify `Content/Config` JSON and `Content/AbilityDefinitions` XML load from staged packaged paths.
- Launch packaged client/listen and packaged dedicated/two-client fixtures and retain logs.
- Day 20 updates the master status, daily index, documentation index, and a dated final report only after all post-cleanup gates pass.

## 9. Locked first-slice decisions

1. Civilian is ambient-only and `playerSelectable=false`.
2. Players and enemies cannot damage Civilians under the default policy. Day 12 may permit casualties only inside an authoritatively resolved configured conflict zone; safe zones always protect.
3. Civilian death uses delayed population refill, stable member IDs, no XP, and no standard enemy loot.
4. Every first-slice Civilian uses GAS to reuse replicated health, damage, and effect integration. Optimization to a lightweight component requires profiling and a later migration.
5. Roles are locked after login/spawn grants. Runtime role hot-swapping remains disabled until a transactional, handle-based ASC cleanup/rollback system exists.
6. BungeeMan has no ammo/reload in this slice.
7. Unreal Behavior Trees are the first-slice Civilian AI framework. BehaviorU remains unchanged for existing users.
8. The Civilian population manager owns only Civilians. Existing enemy spawn/refill remains under its current GameMode/volume owners.
9. The battle director is a replicated always-relevant actor driven by server authority, not client-visible state hidden in GameMode.
10. Interact is a separate input routed through a player-owned endpoint. LMB remains an attack/ability input.
11. A merchant is a stable population member with `MerchantDefinitionId`; it is not every actor using the Civilian role.
12. Wallet/inventory live on PlayerState and replicate owner-only.
13. Per-player saves use authenticated provider-validated `FUniqueNetIdRepl` identity. World population and merchant stock use one separate authoritative world snapshot keyed by `WorldPersistenceId`, loaded once per authoritative world generation before spawning.
14. Both listen and dedicated multi-process matrices plus packaged client/server data loading are mandatory release gates.

## 10. Things to avoid in the first implementation

- Do not add Civilian as another ECharacterClass value.
- Do not add more raw Player/Enemy/Civilian tag checks throughout abilities.
- Do not make Civilian inherit from AAuraEnemy just to reuse AI.
- Do not infer equipment from whether an ability exists.
- Do not allow an empty role array to mean “keep Blueprint defaults.”
- Do not add runtime role switching before role-granted ASC cleanup exists.
- Do not put merchant transactions inside Gameplay Effects without a separate server transaction boundary.
- Do not let clients choose role assets, abilities, prices, damage, or inventory results.
- Do not trust client-provided target, zone, range, line-of-sight, permission, or completed-transaction results.
- Do not put client-visible phase/death state only in GameMode or transient multicast events.
- Do not call interaction or purchase Server RPCs on a server-owned Civilian component.
- Do not give Civilian/enemy population members multiple spawn or refill owners.
- Do not use one process-global save slot as multiplayer player identity.
- Do not build advanced crowd simulation before the simple civilian lifecycle is correct.

## 11. First vertical-slice definition of done

The first implementation is successful when:

- Aura and BungeeMan can use their current data-driven abilities.
- Aura, BungeeMan, and Civilian resolve the expected replicated role-derived combat identity; Civilian is not selectable by a player.
- A Civilian can spawn, wander or work, flee from a threat, and interact as a non-combatant.
- All effective damage paths, including native `CauseDamage`, use the final server rule boundary instead of direct tag logic.
- Players and enemies can damage civilians only under the configured battle-zone rule.
- Civilian death is replicated and late-join safe, emits exactly one event/reward decision, does not respawn the player, and does not grant enemy rewards by default.
- Civilian population refill uses stable IDs and never duplicates or interferes with existing enemy respawn.
- Enemy targeting still works after the relationship refactor.
- `BTService_FindNearestHostile` replaces the temporary Player-specific service without changing required blackboard behavior.
- Role selection and persistent-ASC ability grants are server-validated and idempotent across respawn/load.
- A simple civilian merchant can complete a validated purchase.
- Wallet/inventory are private to the owning client; per-instance merchant stock, population, and per-player progression save and restore under separate stable identities.
- Listen and dedicated two-client tests pass server-authoritative damage, interaction, commerce, late join, reconnect, concurrency, and privacy checks.
- Cooked/packaged client and server load staged RoleConfig/economy/population/battle JSON and AbilityDefinitions XML successfully after final cleanup.

This gives the project a stable base for later additions such as civilian reputation, witnesses, faction wars, weapon inventory, advanced business simulation, role switching, and large populations.
