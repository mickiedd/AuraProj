# Git daily mind maps — May 2026

Each section compares the tip of the previous active day with the tip of the date shown. Binary assets and generated snapshots are grouped by purpose so the behavioral change is visible without listing every asset filename.

## 2026-05-05 — Authority, project tooling, projectile behavior, and respawn

Commits: `96553ff`, `492eca4`, `9568207`, `5189d71`, `4e84dda`, `5fab335`, `221e87d`.

- Mind map: establish a multiplayer-safe combat baseline
  - Cross-platform/editor tooling
    - adds macOS build/editor/client/server commands
    - adds editor menu support and shared Unreal shell helpers
  - Server authority
    - documents server-authoritative multiplayer rules
    - requires ability activation to be accepted by the authoritative ability system
  - Projectile and ability behavior
    - refactors projectile impact effects
    - enables projectile replication
    - relaxes an input check that blocked valid activation
  - Player and enemy lifecycle
    - adds logging around fireball/ability behavior
    - adds fall-death handling
    - adds monster respawn from `MonsterSpawnTable.json`
    - keeps respawn work in `AuraGameModeBase`
  - Diff surface
    - build scripts, controller/ASC code, projectile actors, fireball ability code, attribute calculations, and game mode all change in one large authority pass.
  - Newcomer takeaway
    - the server is intended to own outcomes; clients request abilities and replicate the result. For a projectile bug, inspect activation authority, projectile replication, impact effects, and game-mode respawn in that order.

## 2026-05-06 — AI movement task and projectile diagnostics

Commits: `17e495f` — Add logs & enable projectile replication, relax input check.

- Mind map: expose and correct projectile execution
  - Projectile
    - explicitly enables replication on the projectile path
    - improves projectile logging
  - Target data
    - relaxes/adjusts mouse target validation so valid targets survive the input path
  - Ability initialization
    - updates FireBolt and damage ability setup
  - Diff surface
    - FireBolt asset, target-data task, projectile actor, damage ability, and player controller are aligned for a networked projectile.
  - Newcomer takeaway
    - a projectile has two independent concerns: obtaining target data and replicating the spawned actor. This day touches both.

## 2026-05-07 — AI navigation task and Blueprint snapshot tooling

Commits: `06802cd`, `061dcd8`, `447609a`.

- Mind map: make AI and editor state inspectable
  - Enemy AI
    - adds `BTTask_MoveToTarget`
    - improves nearest-player service diagnostics
    - adds enemy/controller debug logs
  - Snapshot export/import
    - adds editor-side Blueprint JSON snapshot export/import commands
    - prepares bulk asset inspection without opening every Blueprint manually
  - Player startup
    - restores keyboard focus to the game viewport when the player spawns
  - Diff surface
    - `AuraEditor` grows the snapshot tooling while AI controller, behavior-tree service/task, enemy, and game-mode integration become more observable.
  - Newcomer takeaway
    - behavior-tree movement is now a named C++ task, and editor snapshots provide a text representation useful for review and migration.

## 2026-05-08 — Bulk Blueprint and behavior-tree snapshot pipeline

Commits: `53f81f3`, `a3cd74b`, `4d8b20e`, `afc499b`.

- Mind map: turn visual assets into reviewable text
  - Bulk export
    - walks the Asset Registry
    - writes JSON snapshots for abilities, effects, actors, AI, checkpoints, UI, and controllers
  - Behavior trees
    - adds snapshots for enemy behavior trees
    - adds JSON import support for behavior-tree assets
  - Editor architecture
    - extracts JSON import into a reusable helper
    - keeps the editor module as the owner of export/import commands
  - Diff surface
    - hundreds of `Content/Blueprints/**/*.snapshot.json` files are added, while `AuraEditorModule.cpp` receives the export/import implementation.
  - Newcomer takeaway
    - snapshot files are not the runtime source of truth; they are a text mirror used to review, migrate, and diagnose Unreal assets.

## 2026-05-10 — Login menu, item spawns, portals, and dedicated-server launch control

Commits: `5af2b91`, `a4fa676`, `2a9002e`, `fe30f8c`, `bd8c859`, `df4af48`.

- Mind map: make the multi-server entry experience usable
  - Dedicated-server launch
    - adds configurable levels and server connection settings
    - adds editor launch-all and stop-all commands
    - adds matching Windows and command-file helpers
  - Login UI
    - adds the Login menu asset
    - refactors login behavior into `ULoginMenuWidget`
    - reduces controller responsibility by moving UI logic into the widget
  - World navigation
    - adds `LevelJumpPortrail` actor and Blueprint
    - reads `ItemSpawnTable.json` for item/portal locations
    - adds dungeon 2/3/5 portal spawn entries
  - Asset consistency
    - refreshes Blueprint snapshots after the new login assets exist
  - Diff surface
    - login controller/widget, game mode, portal actor, server scripts, editor commands, and runtime config move together from a single-server prototype toward orchestrated travel.
  - Newcomer takeaway
    - the login screen chooses/starts a server, the game mode owns item/portal spawning, and the widget owns login presentation.

## 2026-05-11 — Sprint and portal placement data

Commits: `9c40a90`, `0f40cb9`.

- Mind map: improve player traversal
  - Portal data
    - adds more configured dungeon portal locations
  - Player control
    - adds sprint input/state handling to `AuraPlayerController`
  - Diff surface
    - one data-only spawn-table extension and one controller feature; no new travel architecture is introduced.
  - Newcomer takeaway
    - portal locations are data-driven, while sprint remains a player-controller movement concern.

## 2026-05-12 — Game Server Manager and travel component

Commits: `546bdd2`, `e253944`.

- Mind map: separate server allocation from travel execution
  - Game Server Manager
    - adds `GameServerClient`
    - adds Python manager integration and start commands
    - exchanges level/server information with the external manager
  - Travel component
    - extracts travel logic into `ServerTravelComponent`
    - reduces `LoginPlayerController` responsibility
    - lets portals request travel through a focused component
  - Diff surface
    - game mode, login controller/widget, portal actor, editor, config, scripts, and new network client/component code are updated as one travel refactor.
  - Newcomer takeaway
    - allocation answers “which server should I use?”; `ServerTravelComponent` answers “how does this player travel there?”

## 2026-05-13 — Loading level and cross-server routing

Commits: `c22c06e` — Implement Loading level & server-travel routing.

- Mind map: give travel an intermediate state
  - Loading map
    - adds loading UI, `LoadingGameMode`, and `LoadingPlayerController`
  - Route handoff
    - extends `AuraGameInstance` to retain travel context
    - connects Login → Loading → destination server
    - updates portal and server-manager integration
  - Diff surface
    - the previously direct login/portal path gains a dedicated loading phase and state carrier.
  - Newcomer takeaway
    - a destination travel bug may appear in three places: Login chooses the route, Loading displays/continues it, and GameInstance preserves the handoff data.

## 2026-05-15 — Login asset and item location adjustment

Commits: `c2a3420` — Update LoginMenu asset and item spawn location.

- Mind map: align content with the new flow
  - UI
    - updates the Login menu Blueprint asset
  - Spawn data
    - adjusts an item/portal spawn location in `ItemSpawnTable.json`
  - Diff surface
    - content-only follow-up to the login/travel work; runtime architecture remains unchanged.
  - Newcomer takeaway
    - this is a placement/content correction, not a new subsystem.

## 2026-05-17 — Camera control and initial gameplay assets

Commits: `4ae15b0`, `f8cf72a`, `4e73aeb`, `9e65066`.

- Mind map: add a usable third-person camera around the first asset set
  - Camera input
    - adds right-mouse camera rotation
    - handles input-mode transitions while rotating
  - Screen-edge camera
    - adds edge-of-screen rotation
    - enforces a minimum edge border to avoid accidental rotation
    - stores related tuning in `LevelConfig`
  - Content baseline
    - imports the first large ability, character, pickup, UI, sound, and animation asset collection
  - Diff surface
    - player controller and camera configuration change alongside a large content import.
  - Newcomer takeaway
    - camera behavior is split between controller input and `LevelConfig`; the bulk asset commit supplies the visual/audio resources referenced by later abilities.

## 2026-05-18 — Broom vehicle enters the game

Commits: `af0eb74` — Add broom vehicle, assets and spawn.

- Mind map: introduce the mountable flying vehicle
  - Vehicle code
    - adds broom actor and movement/spawn integration
  - Content
    - adds broom mesh/Blueprint assets
  - Gameplay entry
    - makes the broom spawnable in the world
  - Diff surface
    - new vehicle runtime code and assets are added on top of the camera/player baseline.
  - Newcomer takeaway
    - later broom commits refine this actor’s movement, mounting, AI follow, and networking; this is the first playable vehicle checkpoint.

## 2026-05-20 — Broom idle hover

Commits: `349cea3` — Add idle hover behavior to broom vehicle.

- Mind map: give the vehicle a stable resting state
  - Idle motion
    - adds hover behavior when the broom is not actively flying
    - keeps the vehicle visually alive instead of motionless
  - Diff surface
    - focused broom vehicle behavior change; no new input or network boundary.
  - Newcomer takeaway
    - idle hover is presentation/vehicle-state behavior, separate from later follow and flight-driver logic.

## 2026-05-22 — Dedicated-server broom baseline

Commits: `8008973`, `ef211a2`.

- Mind map: make broom play compatible with a server-authoritative session
  - Dedicated-server tooling
    - adds build/run scripts for dedicated server use
    - fixes command-line/server startup details
  - Broom authority
    - moves important broom state toward server ownership
    - repairs vehicle behavior under the dedicated-server path
  - Diff surface
    - scripts, server build configuration, and broom actor/controller code change together.
  - Newcomer takeaway
    - client visuals are not the authority for broom movement; test it with the dedicated-server commands before trusting PIE-only behavior.

## 2026-05-23 — Camera-facing broom rotation

Commits: `a818f1f` — Keep broom facing camera; smooth yaw interpolation.

- Mind map: improve mounted camera readability
  - Orientation
    - keeps the broom aligned with the camera-facing direction
    - interpolates yaw rather than snapping
  - Diff surface
    - focused vehicle orientation update in broom movement code.
  - Newcomer takeaway
    - this changes how the mount feels, not how the server allocates or replicates it.

## 2026-05-24 — Development build/run ergonomics

Commits: `1be4411` — Use `cmd /k` and switch BuildCookRun to DebugGame.

- Mind map: make server iteration easier
  - Build mode
    - switches BuildCookRun to `DebugGame`
  - Shell behavior
    - keeps command windows open with `cmd /k` for inspection
  - Diff surface
    - build/run script configuration only; runtime gameplay is unchanged.
  - Newcomer takeaway
    - when a local server command exits, this change is intended to leave the window available so its error output can be read.

## 2026-05-25 — Runtime configuration relocation

Commits: `ffeba7f` — Move runtime config to Content/Config and update paths.

- Mind map: establish one runtime configuration home
  - File layout
    - moves `LevelConfig.json` and `ServerConnection.json` from `Config/` to `Content/Config/`
  - Runtime paths
    - updates game mode, login controller, portal, editor, and server-manager lookups
  - Diff surface
    - file moves are content-preserving, but every consumer path is updated.
  - Newcomer takeaway
    - when adding runtime JSON, start in `Content/Config`; a stale `Config/` lookup is a likely failure mode.

## 2026-05-26 — Portal naming, tuning, and network diagnostics

Commits: `451fe2f`, `0c75214`, `9cd4896`.

- Mind map: clean up mounted travel and local iteration
  - Portal naming
    - renames `LevelJumpPortrail` to `LevelJumpPortal`
    - updates config, game mode, travel, and editor references
  - Editor/runtime tuning
    - adds console variables
    - disables editor CPU throttling for more predictable testing
  - Diagnostics
    - lowers noisy broom logs
    - updates network settings and connection-related output
  - Diff surface
    - a source-level rename plus config/editor tuning; behavior is intended to remain equivalent except for diagnostics and naming.
  - Newcomer takeaway
    - use `LevelJumpPortal` in new code and treat the console/network settings as test-environment controls rather than gameplay rules.

## 2026-05-27 — Mount animation state

Commits: `69adfaa`, `3239e0f`, `748ddab`.

- Mind map: represent sitting on the broom in animation
  - Animation assets
    - adds sitting and side-sitting animations
    - updates `ABP_Aura`
  - Runtime animation state
    - adds `AuraCharacterAnimInstance`
    - exposes mount state to the animation Blueprint
    - connects broom mounting to character animation state
  - Diff surface
    - animation assets, character base, broom actor, and anim-instance C++/Blueprint code change together.
  - Newcomer takeaway
    - the vehicle owns the mount event, the character/anim instance owns the visible pose, and `ABP_Aura` consumes that state.
