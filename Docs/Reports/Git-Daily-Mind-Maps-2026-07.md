# Git daily mind maps — July 2026

This month moves from autonomous vehicles and world content into the AuraAbilityGraph migration. Each section describes the diff between active-day checkpoints and calls out source/config/assets separately when that distinction helps a newcomer.

## 2026-07-01 — Autonomous AuraCar

Commits: `b18ccb9` — Add autonomous AuraCar with BehaviorU waypoint driving.

- Mind map: reuse BehaviorU for a second autonomous vehicle
  - Vehicle
    - adds `AuraCar`
    - adds `AuraCarDriveComponent`
    - adds `AuraCarAgentComponent`
  - Behavior
    - adds `BT_AuraCarDrive.xml`
    - drives through waypoint behavior rather than hard-coded per-tick game logic
  - Content
    - adds the car Blueprint asset
  - Diff surface
    - new vehicle classes, agent component, behavior tree, and content asset.
  - Newcomer takeaway
    - broom and car share the pattern “vehicle actor + focused movement + BehaviorU agent + XML tree,” even though their movement details differ.

## 2026-07-03 — Sci-fi desert level configuration

Commits: `b51d752` — Added Sci-fi Desert Level.

- Mind map: register the next showcase environment
  - Level
    - updates default game configuration
    - adds level settings in `Content/Config/LevelConfig.json`
  - World data
    - records the Sci-fi Desert level and its tuning values
  - Diff surface
    - configuration-only registration; geometry/assets are consumed by the level setup rather than implemented in C++ here.
  - Newcomer takeaway
    - level behavior such as altitude and spawn/tuning values belongs in `LevelConfig`, not in vehicle or placement code.

## 2026-07-04 — Camera collision, jump tuning, and village generation

Commits: `bff0a79`, `f51205b`, `60bfbee`.

- Mind map: make the desert level traversable and populate it
  - Camera
    - enables collision testing for the camera
    - updates character Blueprint/configuration to avoid camera clipping
  - Jump
    - simplifies jump handling in character/controller code
    - reduces redundant controller logic
  - World generation
    - adds scripts to analyze/fix the showcase level
    - adds house/village population and unpopulation helpers
    - adds remote execution support
  - Diff surface
    - character/controller/config plus a new level-generation tooling set.
  - Newcomer takeaway
    - the camera collision flag protects the view, jump code controls player traversal, and the population scripts modify level content outside the runtime game loop.

## 2026-07-07 — Procedural house population revision

Commits: `9b023cd` — Update `populate_desert_houses.py`.

- Mind map: improve generated village layout
  - Generation logic
    - refines placement/scanning behavior in the house-population tool
    - expands handling for the desert showcase level
  - Diff surface
    - one tooling script changes; runtime C++ is untouched.
  - Newcomer takeaway
    - run/review this as world-authoring tooling, not as an in-game procedural system.

## 2026-07-08 — Random player starts

Commits: `f5ecbea` — Random PlayerStarts.

- Mind map: vary entry locations
  - Game mode
    - chooses random player-start locations
  - World-generation support
    - adjusts house-population script ignore/placement details
  - Diff surface
    - `AuraGameModeBase` owns runtime selection; the script change supports the authored environment.
  - Newcomer takeaway
    - this affects where players enter the world, not how portals or servers are selected.

## 2026-07-10 — Runtime cheats and editor access

Commits: `1a77dda` — Add `UAuraCheatManager` + editor menu for runtime cheat commands.

- Mind map: add controlled developer diagnostics
  - Cheat manager
    - adds `UAuraCheatManager`
    - adds runtime commands for local testing/inspection
  - Editor UX
    - adds an editor menu entry to invoke the commands
  - Input support
    - adds gamepad asset/diagnostic helpers
  - Disconnect handling
    - updates client disconnect integration used by test commands
  - Diff surface
    - player cheat/controller code, editor menu, client handling, and helper scripts.
  - Newcomer takeaway
    - cheats are test hooks, not normal gameplay; use them to reproduce state quickly while keeping authoritative runtime rules intact.

## 2026-07-11 — BehaviorU rename and travel endpoint caching

Commits: `a25ad15`, `82adbc8`.

- Mind map: stabilize names and cross-server login
  - Plugin rename
    - renames Behaviac modules/files/launcher to BehaviorU
    - updates Aura component names, project dependencies, XML/editor JavaScript, and icons
    - removes stale launcher/build artifacts
  - Login travel
    - caches the resolved cross-server travel endpoint in `GameInstance`
    - avoids resolving the same endpoint repeatedly during a flow
  - Diff surface
    - very large source-level rename plus a focused login-flow state fix.
  - Newcomer takeaway
    - “BehaviorU” is the current name; a stale `Behaviac` include/path is usually a rename issue. Travel endpoint caching lives in `GameInstance`.

## 2026-07-12 — Headless automation, crouch animation, and test plugin

Commits: `28eb13e`, `040ac0f`, `2440fa9`, `12bd5a0`, `006d868`.

- Mind map: make automated multiplayer testing possible without a visible client
  - AuraAutoTest plugin
    - adds editor panel and runtime subsystem
    - loads XML test definitions
    - runs agents/nodes and writes structured reports
    - registers BehaviorU nodes for test execution
  - Test control
    - adds `StopRun`
    - introduces `Aborted` result/status
    - fixes host-actor name collisions
  - NullRHI login
    - adds headless auto-login
    - adds `TransferToRandomPlayer` cheat
    - adds `RunClientNullRHI.bat`
  - Animation
    - adds crouch and crouch-walk assets/states to `ABP_Aura`
    - extends `AuraCharacterAnimInstance`
  - Rename completion
    - finishes Behaviac → BehaviorU names in Aura glue/editor/tree assets
  - Diff surface
    - new automation plugin, headless launch path, animation content, and final rename cleanup.
  - Newcomer takeaway
    - XML defines the test, AuraAutoTest executes it, NullRHI supplies a headless client, and BehaviorU supplies behavior nodes; `Aborted` distinguishes a stopped run from a completed failure.

## 2026-07-14 — Headless stress-run behavior

Commits: `871d6db`, `049e9d6`, `c9046cf`.

- Mind map: make automated runs resemble a real player session
  - Auto-run
    - adds stress-test auto-run for a NullRHI client
    - reduces the test XML to a reusable auto-run scenario
  - Natural movement
    - adds movement timing/behavior that looks less synthetic
    - adds random skill use
  - Headless targeting
    - adds a cursor fallback for environments without a real mouse cursor
    - updates target-data acquisition and player-controller helpers
  - Documentation/config cleanup
    - clears stale plugin `DocsURL`
  - Diff surface
    - AutoTest runtime/agent, target-data, controller, editor launch, and XML test definition all change.
  - Newcomer takeaway
    - headless runs need explicit target-data fallback; “no cursor” is an environment constraint, not automatically an ability failure.

## 2026-07-17 — Mounted camera and GAS lifetime hardening

Commits: `703e825`, `3d3ed05`.

- Mind map: make mounts and effects safer
  - Mounted camera
    - ignores the camera collision channel on the broom mesh
    - keeps mounted view third-person instead of letting the broom occlude it
  - Ability/effect lifetime
    - hardens effect actor cleanup
    - adjusts beam spell lifetime/state
    - removes unsafe or obsolete damage-calculation cleanup paths
    - improves attribute set and auto-test lifetime handling
  - Diff surface
    - broom collision plus GAS ability/effect actor/controller code and BehaviorU auto-test configuration.
  - Newcomer takeaway
    - camera collision is a mesh-channel issue; ability lifetime is an object/cleanup issue. They meet in mounted gameplay but should be debugged separately.

## 2026-07-19 — JSON role configuration and config/build cleanup

Commits: `e9e790b`, `ccaf478`, `892aac8`, `204b2f7`.

- Mind map: replace a hard-coded role asset with runtime data
  - Role schema
    - adds `Content/Config/RoleConfig.json`
    - adds `RoleInfo` data structures and JSON loading helpers
    - routes character/player/game-mode/load-screen code through the role data
  - Config layout
    - moves item/monster spawn tables into `Content/Config`
    - updates game-mode consumers
  - Build
    - adds the `ModelViewViewModel` module dependency required by load-slot role assignment
  - Repository cleanup
    - removes tracked RiderLink developer plugin files
    - ignores `/Plugins/Developer/`
  - Diff surface
    - role runtime data, UI/load-slot consumers, config paths, build dependencies, and plugin tracking all change.
  - Newcomer takeaway
    - `RoleConfig.json` is the runtime role source; `RoleInfo` is the typed C++ representation; the old `DA_RoleInfo` path is being replaced.

## 2026-07-20 — Role reload and combat interface data

Commits: `25064a7`, `d0d1f6d`.

- Mind map: make role iteration faster and expose combat state
  - Editor reload
    - adds a “Reload Role Config” editor tool
    - uses a runtime sentinel so the game can observe the reload request
    - re-reads `RoleConfig.json`
  - Combat interface
    - exposes `IsInAir` through `CombatInterface`
    - updates animation/character base usage
  - BungeeMan role
    - equips/configures the role with the Aura staff
  - Diff surface
    - editor module, game mode/config loader, role data, combat interface, and animation instance.
  - Newcomer takeaway
    - use the editor reload tool for data iteration; use `CombatInterface` for cross-character combat facts such as airborne state.

## 2026-07-21 — Data-driven LMB skill and weapon detachment

Commits: `9811b80` — Make LMB skill data-driven and detach BungeeMan from the Aura staff.

- Mind map: let role config select the basic attack
  - Role data
    - adds/updates the configured LMB skill and role fields
  - Character base
    - reads the role-selected skill instead of assuming the Aura staff ability
    - detaches BungeeMan’s LMB behavior from the staff-specific path
  - Diff surface
    - `RoleConfig.json`, `RoleInfo`, ability-system library, and character-base ability setup.
  - Newcomer takeaway
    - a role now chooses its basic skill from data; changing the LMB skill should start in `RoleConfig.json`, not in a hard-coded character branch.

## 2026-07-26 — AuraAbilityGraph plugin and data-driven abilities

Commits: `94d165d` — Add AuraAbilityGraph plugin, data-driven abilities, BungeeMan gun, and JSON ability info.

- Mind map: introduce a graph runtime beside Blueprint abilities
  - Plugin
    - adds `AuraAbilityGraph` runtime/editor modules
    - registers ability definitions and node types
    - adds XML/JSON import and an editor web UI/server
  - Graph nodes
    - sequence/composition
    - play montage and wait-for-montage-event
    - target data, hitscan, damage, projectile spawning, and gun FX
  - Ability data
    - adds `AbilityInfo.json`
    - adds first graph-backed FireGun/BungeeMan assets
    - connects ability costs/cooldowns and tags to graph definitions
  - Runtime integration
    - adds bullet actor and FireGun ability
    - updates GAS library/ASC/character base to execute data-driven definitions
  - Diff surface
    - large new plugin plus role/config/content and GAS integration; Blueprint assets are snapshot-updated to reflect the new graph-backed path.
  - Newcomer takeaway
    - XML/JSON describes the ability; AuraAbilityGraph parses it into nodes; node tasks invoke GAS/actors. This is the foundation for later migration of FireBlast, ArcaneShards, Electrocute, and enemy abilities.

## 2026-07-27 — Graph context and mana routing

Commits: `b99e948` — Thread Definition through context, fix Sequence recursion, route mana via SetByCaller GE.

- Mind map: make graph execution carry its own definition and resource data
  - Execution context
    - passes the ability `Definition` through node/task context
    - removes hidden reliance on global/implicit ability data
  - Sequence
    - fixes recursive execution behavior
    - prevents a sequence from repeatedly re-entering itself incorrectly
  - Mana cost
    - routes cost through a SetByCaller GameplayEffect
    - aligns graph costs with GAS resource modification
  - Diff surface
    - graph context/composite execution plus GAS cost application.
  - Newcomer takeaway
    - the graph definition travels with the execution; mana is not manually subtracted by a node, it is expressed through a GAS effect.

## 2026-07-28 — Graph editor launcher and server

Commits: `a931d41`, `5d06db9`.

- Mind map: make AuraAbilityGraph usable as an editor tool
  - Web editor/server
    - adds graph editor UI/server support
    - serves definitions and node metadata
  - Native launcher
    - adds `AuraAbilityGraphLauncher`
    - adds startup/build support for the companion editor process
  - Diff surface
    - plugin editor/server and launcher are layered on top of the runtime graph implementation.
  - Newcomer takeaway
    - the launcher is the bridge to the graph web UI; the runtime plugin still owns execution inside Unreal.

## 2026-07-29 — Graph actions, data-driven effects, waits, and repository cleanup

Commits: `54a7e90`, `5c1251f`, `c0eb1e7`, `5c0d41a`, `38b082a`, `144d96b`, `2918713`, `c6eff50`, `df52ad2`.

- Mind map: fill in the graph’s gameplay vocabulary and prepare migration work
  - Targeting/editor
    - adds `FaceTarget` action node
    - adds FireBolt/FireGun XML definitions
    - adds graph editor toolbar command/icon
  - Damage effects
    - adds a C++ default damage GameplayEffect fallback
    - adds JSON-driven `GameplayEffects.json`
    - updates `AuraEffectActor`, ability definitions, GAS library, and graph editor data
  - Timing
    - adds a reusable `Wait` action node
    - adds an explicit one-second wait before FireBolt’s montage event
  - Migration planning
    - adds pending GAS rewrite plan/memory and updates the ability audit report
    - adds build/test helper scripts
  - Repository cleanup
    - removes WebView2 user-data/cache files from tracking
    - leaves only source/config/editor content relevant to the launcher
  - Diff surface
    - graph node runtime/editor, XML/JSON definitions, GAS effect fallback, documentation/plans, and large generated cache cleanup.
  - Newcomer takeaway
    - graph nodes are small verbs (`FaceTarget`, `Wait`, damage, spawn); definitions compose them, while JSON/C++ effect fallback supplies the actual GameplayEffect behavior.

## 2026-07-30 — FireBolt validation and Phase 3 ability migration

Commits: `5186ea8`, `a47c56e`, `6b93792`, `04eba36`, `e998c3b`, `de69a23`, `3791f0b`.

- Mind map: move more player abilities into XML and make the workflow repeatable
  - FireBolt
    - fixes the graph definition and attribute initialization
    - adds graph smoke-test support and build smoke command
  - Registry/cost safety
    - adds an ability-definition registry
    - makes cost checks client-safe while keeping application authoritative
  - Phase 3 migration
    - ports FireBlast, ArcaneShards, and Electrocute to XML
    - adds `SpawnShards` and initial Electrocute beam actions
  - Build/test tooling
    - improves build/run/test batch scripts
    - documents the smoke-test log location
  - Lifetime
    - marks cooldown-duration XML scaling as fixed
    - prevents PIE leaks with weak captures and cache clearing
  - Diff surface
    - graph registry/nodes/definitions, ability system library/assets, smoke scripts, and editor lifetime cleanup.
  - Newcomer takeaway
    - a migrated ability needs three pieces: an XML definition, registered native nodes, and a smoke path proving it can load/execute; editor PIE cleanup prevents test runs from retaining stale objects.

## 2026-07-31 — Graph documentation and Electrocute lifecycle tests

Commits: `89a5449`, `c67ff78`.

- Mind map: document and test the migrated ability system
  - Electrocute
    - hardens beam lifecycle and target handling
    - updates XML and beam node behavior
  - Graph runtime tests
    - adds combat-avatar and data-ability test fixtures
    - tests sequence/montage/beam execution boundaries
  - Documentation
    - adds GAS ability documentation
    - adds migration TODO tracking
  - Diff surface
    - graph runtime/module, Electrocute/other XML definitions, tests, and two large documentation reports.
  - Newcomer takeaway
    - use the docs/TODOs to see what is migrated versus pending, and use the graph tests to understand the expected execution lifecycle.
