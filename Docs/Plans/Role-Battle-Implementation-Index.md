# Role and Battle System: Daily Implementation Index

Status: Revised execution schedule; Day 01's headless gate was executed conditionally, Days 02–05 are implemented and verified, and Day 06 is next. Day 01's expanded damage inventory closed at the start of Day 04; only its rendered presentation follow-up carries into Day 07.

This is the execution schedule for `Role-Creation-and-Battle-System-Plan.md`. Each day file is an implementation contract: it identifies prerequisites, source/content/test scope, exact work, verification commands or fixtures, evidence to retain, and a completion gate.

Finished milestone mind maps are archived in [Docs/Reports/DayPlan-Archive](../Reports/DayPlan-Archive/README.md).

The schedule assumes one developer working in a stable Unreal project branch. A “day” is a logical milestone; it may take more or less than one calendar day.

## How to use this schedule

1. Read the current day file before editing code.
2. Confirm every prerequisite and the previous day's completion gate pass.
3. Keep changes inside the current milestone. If an unlisted file is genuinely required, update the day manifest before editing it and record why; do not silently expand scope.
4. Add the named automation and network coverage in the same milestone as the behavior it protects.
5. Compile and run every listed check before moving on.
6. Record the revision, commands, topology, fixture/map, result, and log/report paths.
7. Commit the milestone as one reviewable unit and record deviations in its day file or tracking document.

Do not skip gates. In particular, replicated combat state and conservative faction-aware damage must work before Civilian AI or Civilian death is added. A qualitative editor observation alone does not satisfy a network or persistence gate.

## Locked first-slice decisions

- Civilian is ambient AI only and is not player-selectable.
- Day 03 introduces the minimal replicated Alive/Dying/Dead/Respawning state used by rules and later death policies.
- Civilian damage is denied by default until the Day 12 authoritative battle-zone resolver explicitly permits it.
- Combat targeting, threat evaluation, cosmetic highlighting, and interaction are separate queries.
- The first Civilian AI uses Unreal Behavior Trees. BehaviorU remains unchanged for existing actors.
- `AuraPopulationManager` is introduced with spawning on Day 09 and owns Civilian population only in this slice. Existing enemy respawn remains under its current owner.
- The battle director is one server-spawned, replicated, always-relevant `AInfo`-style actor. GameMode owns authority; the replicated actor owns client-visible phase state.
- Interaction requests originate from a player-owned endpoint and use a distinct Interact input. Clicking an interactable never bypasses combat rules.
- Wallet and inventory live on `AAuraPlayerState` and replicate owner-only.
- Merchant identity belongs to a stable population member through `MerchantDefinitionId`; the shared Civilian role does not make every Civilian a merchant.
- Persistence uses authenticated, provider-validated `FUniqueNetIdRepl` player identity and separates per-player saves from one authoritative world snapshot keyed by a server-owned `WorldPersistenceId`.
- Listen-server and dedicated-server multi-process acceptance are both required before release.

## Cross-cutting BungeeMan Gun Skill track

The BungeeMan Gun Skill is **half-finished** in the current checkout. The configured path is present: `Content/Config/RoleConfig.json` points BungeeMan's `lmbAbilityDefinition` to `Content/AbilityDefinitions/FireGun.xml`; the XML defines the LMB input, zero mana cost, physical damage, 0.2 second cooldown, montage/event sequence, `fireGunBullet` projectile, and muzzle FX; and `Content/Config/ProjectileDefinitions.json` defines the native projectile. The Day 1 catalog test already proves the role, graph, weapon/socket, and projectile definition load.

The remaining work is runtime proof and closeout: exercise the configured `UAuraDataAbility` XML path (the role does not currently point at the parallel `UAuraFireGun` helper), prove server-only spawn and shared damage-rule/attribution behavior, confirm montage/event/cooldown/muzzle/impact presentation, and repeat those checks through respawn, late join, dedicated, packaged, persistence, and release gates. Every day below carries the relevant checkpoint; a checkpoint marked regression must not introduce a second gun-grant or a new client-authority path.

| Day | BungeeMan Gun Skill checkpoint |
| --- | --- |
| 01 | Record the FireGun wiring, active XML path, assets, projectile, and the missing activation/presentation evidence. |
| 02 | Replicate BungeeMan's `Combat.Gun` identity and preserve it for FireGun source attribution. |
| 03 | Make FireGun target/damage decisions use the authoritative combat rules and life-state checks; friendly targets remain rejected. |
| 04 | Inventory and migrate FireGun projectile damage through the final server boundary with source ability/type attribution. |
| 05 | Validate BungeeMan's `lmbAbilityDefinition` reference and the FireGun contract atomically with the role schema. |
| 06 | Grant exactly one FireGun data ability, bind `InputTag.LMB`, equip the rifle, and keep the grant across pawn replacement. |
| 07 | Close the functional, rendered, listen, dedicated, and packaged FireGun evidence gate. |
| 08 | Keep Civilian empty-handed and without FireGun; verify the gun cannot damage a Civilian under the Day 8 default-deny rules. |
| 09 | Ensure population spawning never inherits a player FireGun grant or weapon and retains stable target identity. |
| 10 | Ensure Civilian threat/flee behavior reacts only to an authoritative permitted FireGun hit and never grants AI a player skill. |
| 11 | Verify a lethal FireGun hit produces one attributed death transition and one policy dispatch. |
| 12 | Resolve FireGun damage against the authoritative battle-zone/phase policy, including the explicit Civilian casualty opt-in. |
| 13 | Verify FireGun-caused Civilian death updates one population slot and cannot trigger duplicate cleanup/refill. |
| 14 | Keep FireGun attack targeting separate from Interact; validate target descriptors, friendly rejection, and server re-resolution. |
| 15 | Keep FireGun and its projectile data outside economy registry ownership and transaction inputs. |
| 16 | Keep the FireGun grant and cooldown/respawn behavior independent from owner-only wallet and inventory replication. |
| 17 | Prove merchant interaction/transaction state cannot be mutated or bypassed by FireGun damage or input. |
| 18 | Persist and restore BungeeMan's role and FireGun grant exactly once without duplicating the ASC ledger entry. |
| 19 | Run FireGun authority, replication, late-join, reconnect, concurrency, privacy, and anti-cheat checks in both topologies. |
| 20 | Package and release-test the complete FireGun XML/projectile/assets path, with no unresolved helper/path ambiguity. |

## Daily sequence

| Day | Milestone | Main result |
| --- | --- | --- |
| 01 | Baseline | Existing Aura/BungeeMan behavior, environment, and known gaps are recorded reproducibly |
| 02 | Combat identity | Combat avatars have replicated faction, profile, control, capability, and death-policy identity |
| 03 | Combat rules and state foundation | Replicated life state and one context-aware relationship/permission API exist; Civilian damage defaults to denied |
| 04 | Authoritative damage migration | Every native, projectile, beam, radial, hitscan, melee, and graph path uses the final server boundary and serialized attribution |
| 05 | Versioned role schema | RoleConfig is safely parsed, fully validated, and atomically published with explicit identity, selection, equipment, and profile fields |
| 06 | Deterministic role application | Role presentation and identity apply together, while persistent ASC grant bookkeeping prevents duplication |
| 07 | Combat-role and package regression | Aura and BungeeMan pass listen/dedicated and packaged-data regression with recorded evidence |
| 08 | Civilian actor | `AAuraCivilian` has role-derived identity, replicated presentation, GAS health, and no offensive loadout |
| 09 | Civilian population foundation | A single population manager loads validated data and spawns stable-ID Civilian members authoritatively |
| 10 | Civilian AI and markers | Server-only UE Behavior Trees support work, observe, threat detection, flee, and shelter selection |
| 11 | Death policies | Exactly-once, policy-dispatched Player/Enemy/Civilian death and late-join presentation work |
| 12 | Battle director and zones | Replicated phases and deterministic server-resolved zone policy control Civilian protection and battle attribution |
| 13 | Civilian lifecycle | The existing population manager owns Civilian corpse cleanup and policy-based refill without touching enemy respawn |
| 14 | Targeting and interaction | Orthogonal target data and a player-owned, server-validated Interact route work without conflating attacks |
| 15 | Economy data | Versioned items, merchants, offers, and currency data load through a validated read-only registry |
| 16 | Player wallet and inventory | Owner-only, server-authoritative PlayerState economy survives pawn replacement |
| 17 | Merchant transactions | Stable Civilian merchants support replay-safe, atomic purchases and explicit client results |
| 18 | Player/world persistence | Per-player role/economy and world population/merchant state save, migrate, reconcile, and reload separately |
| 19 | Multiplayer hardening | Mandatory listen and dedicated two-client security, replication, lifecycle, reconnect, and concurrency matrices pass |
| 20 | Cleanup and release | Cleanup is followed by full rebuild, automation, cook, packaged-data, listen, and dedicated regression evidence |

## Evidence required at every completion gate

- Git revision and dirty-worktree note.
- Exact build/test command and exit code.
- Named automation tests and summary.
- Map/fixture and server topology for runtime checks.
- Client/server log or dated report paths.
- Explicit skipped checks and blockers; a skip is not a pass.
- For replicated state, a remote-client and late-join observation where relevant.
- For persistence, before/after values and the stable player/world/member identifiers used.

## Recommended commit names

- Day 01 - Establish role battle baseline
- Day 02 - Add combat identity
- Day 03 - Add combat rules and state foundation
- Day 04 - Route all damage through authoritative rules
- Day 05 - Add versioned role schema
- Day 06 - Apply role identity and loadouts deterministically
- Day 07 - Validate and package Aura and BungeeMan roles
- Day 08 - Add replicated Civilian actor
- Day 09 - Add Civilian population manager and spawning
- Day 10 - Add server-authoritative Civilian behavior
- Day 11 - Add exactly-once death policies
- Day 12 - Add replicated battle director and zones
- Day 13 - Add Civilian population lifecycle
- Day 14 - Add player-owned targeting and interaction
- Day 15 - Add validated economy definitions
- Day 16 - Add PlayerState wallet and inventory
- Day 17 - Add replay-safe merchant transactions
- Day 18 - Add per-player and world persistence
- Day 19 - Harden multiplayer behavior
- Day 20 - Complete packaged vertical-slice release

## Important project rules

- Do not add Civilian to `ECharacterClass`.
- Do not add new raw Player/Enemy/Civilian actor-tag checks.
- Do not infer equipment from the presence of an LMB ability.
- Do not grant role abilities from client input.
- Do not use `AAuraEnemy` for Civilian behavior or Civilian death.
- Do not implement runtime role switching until a transactional role-granted ASC cleanup path exists.
- Do not trust client-provided zone, price, item, damage, range, line-of-sight, or target-permission results.
- Do not place client-visible replicated state only in GameMode or an unreplicated subsystem.
- Do not give one population member two spawn/refill owners.
- Do not call a Server RPC through a server-owned Civilian component; route it through the owning player.
- Do not store multiplayer progression in one process-global save slot.
- Do not mark release complete without verifying loose JSON/XML staging in packaged client and dedicated-server builds.

## Detailed day files

- [Day 01 - Establish the baseline](Role-Battle-Implementation/Day-01-Baseline.md)
- [Day 02 - Add combat identity](Role-Battle-Implementation/Day-02-Combat-Identity.md)
- [Day 03 - Add combat rules and state foundation](Role-Battle-Implementation/Day-03-Combat-Rules.md)
- [Day 04 - Migrate all damage producers](Role-Battle-Implementation/Day-04-Damage-Migration.md)
- [Day 05 - Extend and version the role schema](Role-Battle-Implementation/Day-05-Role-Schema.md)
- [Day 06 - Refactor role application and grants](Role-Battle-Implementation/Day-06-Role-Application.md)
- [Day 07 - Validate Aura and BungeeMan](Role-Battle-Implementation/Day-07-Combat-Roles.md)
- [Day 08 - Add the Civilian actor](Role-Battle-Implementation/Day-08-Civilian-Actor.md)
- [Day 09 - Add Civilian population spawning](Role-Battle-Implementation/Day-09-Civilian-Spawning.md)
- [Day 10 - Add Civilian AI and markers](Role-Battle-Implementation/Day-10-Civilian-AI.md)
- [Day 11 - Separate death policies](Role-Battle-Implementation/Day-11-Death-Lifecycle.md)
- [Day 12 - Add the battle director](Role-Battle-Implementation/Day-12-Battle-Director.md)
- [Day 13 - Add Civilian population lifecycle](Role-Battle-Implementation/Day-13-Population-Lifecycle.md)
- [Day 14 - Add targeting and interaction](Role-Battle-Implementation/Day-14-Targeting-Interaction.md)
- [Day 15 - Add economy definitions](Role-Battle-Implementation/Day-15-Economy-Data.md)
- [Day 16 - Add PlayerState wallet and inventory](Role-Battle-Implementation/Day-16-Wallet-Inventory.md)
- [Day 17 - Add merchant transactions](Role-Battle-Implementation/Day-17-Merchant-Transactions.md)
- [Day 18 - Add player/world persistence](Role-Battle-Implementation/Day-18-Persistence.md)
- [Day 19 - Harden multiplayer](Role-Battle-Implementation/Day-19-Multiplayer-Hardening.md)
- [Day 20 - Finish the packaged vertical slice](Role-Battle-Implementation/Day-20-Cleanup-Release.md)
