# Git daily mind maps — August 2026

August is the most recent migration/hardening period. The maps below compare each active day’s final `main` tree with the previous active-day checkpoint. The 2026-08-11 section explicitly describes the merge because it combines a runtime-asset LFS branch with incoming combat/beam code.

## 2026-08-01 — Projectile homing and snapshot refresh

Commits: `3e62fc6`, `a1762bd`, `b5e8c87`.

- Mind map: finish projectile behavior and synchronize graph-facing assets
  - Projectile
    - fixes collision behavior
    - adds homing support
    - adds/updates projectile tests
  - Montage graph node
    - moves montage data into the `PlayMontage` node definition
    - reduces ability-specific hidden montage configuration
  - Snapshots
    - refreshes Blueprint snapshots across AI, abilities, effects, actors, UI, and controllers
    - captures the post-migration content state
  - Diff surface
    - projectile/graph runtime code plus a very broad generated snapshot refresh.
  - Newcomer takeaway
    - projectile targeting/collision is runtime actor code; montage timing/data is graph-node definition; snapshots are the review mirror of the resulting assets.

## 2026-08-02 — Data-driven projectiles, pickups, metadata, and migration planning

Commits: `3deb301`, `2c8085b`, `3d8a423`, `a6293db`, `b668cac`, `9171b93`, `a533eea`.

- Mind map: remove Blueprint-only gameplay definitions from the pickup/projectile path
  - Projectile/pickup config
    - adds `ProjectileDefinitions.json`
    - adds `PickupDefinitions.json`
    - extends `AuraGameplayConfig`
    - updates projectile spawning and enemy/pickup consumers
  - Blueprint decoupling
    - adds the Gameplay-Blueprint-Decoupling migration plan/report
    - removes or empties old pickup/effect Blueprint assets where JSON/C++ now owns the behavior
    - adds a startup map/config checkpoint
  - Ability metadata
    - migrates ability metadata to runtime JSON
    - updates ASC, ability info, role info, game mode, and widget controllers
    - adds focused ability-info tests
  - Documentation/next moves
    - adds the next-moves document
    - clarifies GAS migration TODO/status
  - Collision/UI
    - blocks the Projectile collision channel in engine config
    - updates `AbilityInfo.json` UI metadata
  - Diff surface
    - a large cross-layer migration: JSON config + C++ loaders/controllers + Blueprint asset removal + documentation.
  - Newcomer takeaway
    - the direction is “data selects behavior, C++ executes behavior, UI reads the same data”; old Blueprint assets may be deleted intentionally because they are no longer the source of truth.

## 2026-08-03 — Enemy abilities, damage context, and smoke-test truthfulness

Commits: `44ebb25`, `4286f6e`, `1092517`, `e895b8b`, `47cfec4`, `4230438`, `c97a02a`, `183fe39`.

- Mind map: make the graph migration cover enemies and carry damage provenance
  - Enemy ability definitions
    - adds XML for enemy FireBolt, hit react, melee, and ranged attack
    - adds `EnemyAbilityConfig.json`
    - adds native enemy montage, hit-react, and melee-damage nodes
    - registers and tests the migrated enemy abilities
  - Damage direction/context
    - routes knockback from target direction
    - unifies `CauseDamage` with `ApplyDamage`
    - removes unused `TargetFromContext` and `ScatterRadius` declarations
    - populates damage context and seeds a projectile ASC
  - Montage execution
    - routes montage events through the graph node
    - removes dead declarations around ability definitions/data abilities
  - Smoke tests/docs
    - makes smoke tests exercise real graph/module paths
    - creates `GAS-Migration-Audit-2026-08-03.md`
    - updates migration issue/TODO memory and reports
  - Diff surface
    - enemy XML/config and native nodes, damage nodes/context, projectile/ASC setup, smoke infrastructure, and migration documentation.
  - Newcomer takeaway
    - damage now has an explicit path from producer to context to execution; enemy abilities use the same data-driven graph pattern as player abilities.

## 2026-08-04 — Electrocute visual endpoints and unified damage parameters

Commits: `da80879`, `5c57678`.

- Mind map: make the beam visually correct and make graph actions share one damage contract
  - Beam presentation
    - computes/updates visual start and end endpoints
    - enforces range behavior
    - updates cursor/target-data handling
  - Damage parameters
    - unifies parameter construction across ApplyDamage, CauseDamage, hitscan, projectile, shards, and beam actions
    - adds ability-definition support needed by the shared path
  - Validation
    - adds/updates beam and smoke-test registration in the graph module
    - updates migration audit/TODO records
  - Diff surface
    - Electrocute node/XML, target-data task, shared graph module registration, and all damage-producing action nodes.
  - Newcomer takeaway
    - the beam’s line is presentation, while the shared damage parameter object is gameplay attribution; both must be correct for a beam hit to be useful.

## 2026-08-05 — Modular beam nodes and dynamic spec tags

Commits: `779c09f`, `ff69b6e`.

- Mind map: turn beam behavior into reusable graph pieces
  - Modular beam
    - adds `ModularBeamNodes`
    - exposes reusable beam initialization/targeting/visual/damage operations
    - updates Electrocute XML and the original Electrocute node to use the modular path
  - Ability spec metadata
    - switches to `GetDynamicSpecSourceTags()`
    - aligns ASC/DataAbility/config/test code with runtime-generated source tags
  - Documentation
    - updates GAS ability documentation for the new beam model
  - Diff surface
    - large new beam node implementation/header plus smaller ASC/data/config metadata corrections.
  - Newcomer takeaway
    - Electrocute is becoming a composition of reusable beam nodes rather than one monolithic action; dynamic tags preserve identity when specs are created at runtime.

## 2026-08-06 — Runtime monster lookup and documentation organization

Commits: `4c15782`, `57fe4b6`.

- Mind map: improve debug spawning and make the docs navigable
  - Monster debug spawn
    - adds numeric-ID lookup in the spawn table
    - adds player-controller/cheat-manager command path
    - updates `AuraGameModeBase` and `MonsterSpawnTable.json`
  - Documentation layout
    - creates Plans, Reference, Reports, and Tracking categories
    - moves GAS/migration documents into the appropriate categories
    - adds `Docs/README.md`
  - Diff surface
    - runtime debug-spawn code/config plus documentation renames and index/readme updates.
  - Newcomer takeaway
    - numeric IDs are for fast test spawning; the docs folders now signal whether a file is a plan, reference, report, or live tracking list.

## 2026-08-07 — Role/Battle plan, NavMesh fallback, and Day 1 smoke

Commits: `a26d241`, `f711913`, `098d7be`.

- Mind map: begin a structured combat-system implementation program
  - Navigation tooling
    - adds editor NavMesh bounds-volume support
    - adds a documented remote Python editor-automation path
    - adds a fallback in `AuraBehaviorUAgentComponent`
  - Role/Battle plan
    - adds the 20-day implementation index
    - adds day-by-day plans from baseline through cleanup/release
    - adds the Role/Battle system plan and baseline report
  - Day 1 runtime
    - adds player-controller smoke runner
    - fixes respawn attribute initialization/application
    - adds Role/Battle tests and baseline validation
  - Diff surface
    - editor automation/tooling, large documentation plan set, controller/character/GAS initialization, and a new Day 1 smoke command.
  - Newcomer takeaway
    - this is the transition from broad GAS migration to a staged Role/Battle delivery plan; Day 1 proves the baseline before Day 2 identity and later combat rules.

## 2026-08-08 — Combat identity and Day 2 smoke

Commits: `2abcfcb`, `8721069`.

- Mind map: give every combatant a stable role/identity boundary
  - Combat identity
    - adds `AuraCombatIdentityComponent`
    - adds combat identity/types and gameplay tags
    - assigns/reads identity on characters and enemies
    - updates nearest-player AI queries to respect the new identity data
  - Tests
    - adds Role/Battle identity tests
    - adds the Day 2 network smoke runner/report
  - Plans
    - expands Day 2 identity notes
    - clarifies Day 3 rules and Day 20 cleanup references
  - Diff surface
    - new component/types/tags and consumers, test harness, and detailed implementation-plan expansion.
  - Newcomer takeaway
    - identity answers “what combat role/faction does this actor represent?”; it is distinct from display name and from the later combat-rule state.

## 2026-08-09 — Combat rules/state, smoke enforcement, and damage inventory

Commits: `da4b6e1`, `60d68d0`, `43f95bd`, `679fe3a`.

- Mind map: move Role/Battle from identity into authoritative combat behavior
  - Plan refinement
    - expands all 20 day plans with executable scope, dependencies, validation, and release notes
    - improves the Day 2 network smoke runner
  - Smoke policy
    - documents listen-server and dedicated-server runs for the early days
    - records expected logs/validation per day
  - Combat rules/state
    - adds combat rule/context types
    - adds `AuraCombatRules`
    - adds `AuraCombatStateComponent`
    - updates character/enemy/AI code to use the rule/state layer
    - adds Day 3 network smoke coverage
  - Damage inventory
    - inventories damage producers and migration targets
    - adds Day 4 inventory report and tests
  - Diff surface
    - major C++ combat-rule/state implementation plus extensive plans/smoke scripts/reports.
  - Newcomer takeaway
    - identity is static-ish actor meaning; rules/state answer what that actor may do now. The smoke policy deliberately checks both listen and dedicated modes.

## 2026-08-10 — Damage boundary and attribution hardening

Commits: `38763af` — Harden damage boundary and preserve attribution.

- Mind map: make every damage producer enter one safe, attributable boundary
  - Boundary validation
    - validates target/source actors before damage application
    - prevents invalid/non-combat actors from crossing into the damage path
  - Attribution
    - preserves source/causer/instigator information through the damage context
    - aligns native graph nodes and legacy projectile/fireball producers
  - Attribute execution
    - hardens attribute-set and execution-calculation handling
    - updates damage ability/library/types for the common contract
  - Validation
    - adds `RunRoleBattleDay4DamageSmoke.ps1`
    - expands Role/Battle tests around damage producers and boundaries
    - updates the Day 4 inventory report
  - Diff surface
    - 20 files across graph nodes, projectiles, character base, GAS library/attributes/exec calc, smoke runner, and tests.
  - Newcomer takeaway
    - a damage bug should be debugged at the producer, boundary validation, context attribution, and execution calculation layers; this day ties those layers together.

## 2026-08-11 — Runtime asset LFS merge, safe avatar lookup, and modular beam fixes

Commits: `22573dc`, `fb10b85`, `5fe427a` (merge), `9588a92`, `1c32b8e`.

- Mind map: combine a source fix with repository storage repair
  - Incoming combat/beam branch (`fb10b85`)
    - hardens safe avatar lookup in graph/test code
    - adds modular beam target/endpoint fixes
    - updates damage/beam tests and the existing archive record
  - Runtime asset branch (`22573dc`)
    - tracks oversized runtime snapshots/assets with Git LFS
    - adds `.gitattributes` rules and LFS pointer files
  - Merge (`5fe427a`)
    - combines the incoming code changes with the asset-storage branch
    - preserves both code behavior and the large runtime asset set
  - LFS follow-up (`9588a92`, `1c32b8e`)
    - tracks the remaining oversized snapshot correctly
    - archives the final snapshot-size fix
  - Diff surface
    - source/config/tests and 2,747 runtime asset/snapshot paths are represented in the merge; the storage change is repository plumbing, while the avatar/beam change is runtime behavior.
  - Newcomer takeaway
    - if a large `.uasset`/snapshot appears as a tiny pointer, check Git LFS; if a beam test cannot find an actor, check safe avatar lookup. These are separate concerns joined by the merge.
