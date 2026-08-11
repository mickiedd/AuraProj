# Git daily mind maps — June 2026

Each section is a hand-authored summary of the diff from the previous active date to that day’s `main` tip. The project’s major June themes are mounted-state UX, building placement, the BehaviorU plugin, and broom/network architecture.

## 2026-06-01 — Mounted input guard

Commits: `ea6fb06` — Add mounted broom tag and block ability input.

- Mind map: prevent mounted characters from using incompatible abilities
  - State tag
    - adds a mounted-broom gameplay tag
  - Input boundary
    - player controller checks mounted state
    - blocks normal ability input while mounted
  - Vehicle integration
    - broom applies/removes the state around mounting
  - Diff surface
    - gameplay tags, controller input, and broom state now agree on the mounted restriction.
  - Newcomer takeaway
    - if an ability should be unavailable on the broom, begin with the mounted tag/input guard rather than changing every ability.

## 2026-06-02 — Mount state reaches the HUD

Commits: `0c60842` — Add mount state support and UI notifications.

- Mind map: tell the player when mount state changes
  - Interface
    - extends the player interface with mount-state access
  - Widget/controller
    - overlay widget controller receives mount changes
    - user widgets can react to the state
  - Content
    - updates the overlay Blueprint asset
  - Diff surface
    - mount state moves from gameplay/input-only into the UI notification path.
  - Newcomer takeaway
    - the mounted tag prevents actions; this commit gives UI code an explicit signal to explain that state to the player.

## 2026-06-03 — UGC building preview and placement

Commits: `e2ac461`, `82e17cd`.

- Mind map: add a player-controlled building placement loop
  - Preview
    - adds `AuraPlacementPreviewActor`
    - adds `PreviewMeshBase` groundwork/configuration
  - Placement
    - adds `AuraBuildingComponent`
    - adds `AuraPlacedBuildingActor`
    - connects player/controller input to preview and confirm behavior
  - World alignment
    - reads `LevelConfig.groundAltitude`
    - uses configured ground height instead of an implicit hard-coded plane
  - Diff surface
    - new building actors/components plus level configuration and player integration.
  - Newcomer takeaway
    - preview is temporary visual state, placed building is the committed actor, and ground altitude comes from level data.

## 2026-06-09 — Placement preview and collision refinement

Commits: `a383df4`, `a0dd79d`.

- Mind map: make building placement trustworthy before confirmation
  - Preview mesh
    - adds/updates `BP_PreviewMesh`
    - introduces `PreviewMeshBase`
  - Rotation
    - refines placement rotation and orientation handling
  - Collision
    - improves collision/overlap checks
    - keeps the preview actor and controller in sync with validation
  - Diff surface
    - building component, preview actor, player controller, and new preview base class are updated.
  - Newcomer takeaway
    - a red/invalid placement is produced by preview validation; the placed actor is not created until the validated confirmation path succeeds.

## 2026-06-10 — Placement options and BehaviorU plugin foundation

Commits: `f230929`, `d6f0ab9`, `c95124b`.

- Mind map: support behavior-driven gameplay while loosening an overstrict placement gate
  - Placement options
    - adds the option to center placement on the owner
    - disables the reachability check in `ConfirmPlacement` where it blocked valid placement
  - BehaviorU plugin
    - adds editor module and runtime module
    - adds XML graph editor/import/export UI and server bridge
    - adds behavior-tree, FSM, HTN, decorator, composite, and agent runtime types
    - adds a launcher project and plugin documentation
  - Diff surface
    - `AuraBuildingComponent` gets a small behavior change while the majority of the diff establishes the initial BehaviorU plugin surface.
  - Newcomer takeaway
    - BehaviorU is the AI/behavior runtime; its editor produces XML and its runtime agents execute those trees inside Aura.

## 2026-06-11 — BehaviorU commands and editor dependencies

Commits: `3aef963` — Add Behaviac command, icons, and network deps.

- Mind map: make the new plugin addressable from the editor
  - Commands
    - adds the plugin command type used by editor/runtime communication
  - Editor UX
    - adds toolbar/debug/reimport/server icons
  - Module dependencies
    - updates `.Build.cs` and project registration for the new command/network path
  - Diff surface
    - small integration layer on top of the large plugin foundation.
  - Newcomer takeaway
    - this is the command-and-discoverability bridge that lets the editor operate the BehaviorU server/runtime.

## 2026-06-12 — BehaviorU launcher delivery

Commits: `4c8def4` — Add BehaviacLauncher project and build updates.

- Mind map: provide a buildable companion launcher
  - Native launcher
    - adds the launcher project file and source
    - checks in the generated executable/PDB used by the local workflow
  - Build integration
    - updates launcher build command and ignore rules
  - Diff surface
    - plugin tooling and build artifacts change; game runtime behavior is unchanged.
  - Newcomer takeaway
    - the launcher is editor support, not the in-game behavior executor.

## 2026-06-15 — BehaviorU patch and editor cleanup

Commits: `d512e54` — Patch BehaviorU Updates.

- Mind map: stabilize the first launcher/editor handoff
  - Launcher
    - refreshes native binary/PDB outputs
    - simplifies the launcher project configuration
  - Editor
    - removes obsolete editor module surface
  - Workspace cleanup
    - records a local Visual Studio index artifact from the build
  - Diff surface
    - focused plugin/editor patch following the launcher introduction.
  - Newcomer takeaway
    - if the launcher compiles but the editor menu is stale, this is the historical checkpoint where those pieces were synchronized.

## 2026-06-17 — WebView launcher display fixes

Commits: `7d53e52` — Fix WebView display inside the app issues.

- Mind map: make the embedded BehaviorU UI visible
  - Launcher/WebView
    - updates native launcher startup/display behavior
    - adds the WebView2 loader/runtime pieces required by the embedded UI
  - Editor configuration
    - adds workspace settings supporting the launcher workflow
  - Runtime data
    - refreshes WebView user-data/cache files generated by the launcher
  - Diff surface
    - source/executable plus WebView2 runtime/cache content; the intent is display reliability, not gameplay behavior.
  - Newcomer takeaway
    - WebView problems live in the launcher/editor layer; do not start by changing Aura gameplay code.

## 2026-06-18 — Runtime debug panel

Commits: `4a2769f` — Fix Errors, and add runtime-debug.js.

- Mind map: inspect behavior execution while the game runs
  - Runtime debug UI
    - adds `runtime-debug.js`
    - expands the embedded debug panel and fixes UI errors
  - Editor integration
    - updates the BehaviorU page/app resources
  - Ignore rules
    - prevents more local WebView/cache noise from being treated as source
  - Diff surface
    - BehaviorU editor JavaScript and its embedded runtime data are updated.
  - Newcomer takeaway
    - the runtime debug panel is the place to inspect agent/tree state; it is separate from Unreal’s normal log output.

## 2026-06-19 — Enemy BehaviorU agent

Commits: `e32daf4` — Test enemy behavior tree.

- Mind map: connect an Aura enemy to BehaviorU
  - Agent component
    - adds `AuraBehaviacAgentComponent`
    - binds enemy actor behavior to the BehaviorU agent runtime
  - Enemy integration
    - updates `AuraEnemy` and build dependencies
  - Tree/debug
    - updates test enemy tree and runtime-debug support
  - Diff surface
    - new Aura-side agent component and enemy wiring are the meaningful behavioral changes; WebView files are incidental runtime data.
  - Newcomer takeaway
    - the tree asset declares behavior, the BehaviorU runtime executes it, and the Aura agent component connects execution to an enemy actor.

## 2026-06-21 — Critical BehaviorU/Aura fixes and broom AI

Commits: `ecef223`, `d5c3b33`, `28720f9`.

- Mind map: make state-machine exits and broom AI safe enough to use
  - BehaviorU/FSM
    - makes `OnExit` reachable through `ExitState()`
    - casts the current task to the FSM state task before exiting
    - repairs decorator/composite behavior and logging edge cases
  - Broom AI
    - adds `AuraBroomAgentComponent`
    - adds `BT_BroomFollowPlayer.xml`
    - connects broom follow behavior to the vehicle
  - GAS/network cleanup
    - fixes replication/lifetime issues identified during integration
    - removes tracked WebView user data from the repository
  - Diff surface
    - large plugin cleanup plus new broom agent/tree; the two small FSM commits correct compile/runtime exit handling on top.
  - Newcomer takeaway
    - broom follow is a BehaviorU-driven agent, while the FSM fixes ensure an active state can leave cleanly instead of calling an inaccessible or wrong task method.

## 2026-06-22 — Decorator logging correction

Commits: `c4befa5` — Fix Bugs for Decorator Logging.

- Mind map: make behavior diagnostics match behavior execution
  - Logging
    - fixes decorator log output
    - updates the broom follow tree/config used by the diagnostic path
  - Project configuration
    - adjusts the relevant game setting for logging/debug execution
  - Diff surface
    - small BehaviorU decorator and tree/config correction.
  - Newcomer takeaway
    - a wrong decorator log can make a correct tree look broken; verify the logging fix before changing tree conditions.

## 2026-06-23 — Network loss handling and richer blackboard inspection

Commits: `b462f1e`, `d8fefa0`.

- Mind map: handle failures and expose AI state
  - Network
    - adds custom `AuraNetDriver` and `AuraNetConnection`
    - adds heartbeat detection
    - detects a server lost during an active match, not only during login
    - routes the result to `AuraClientDisconnectHandler`
  - BehaviorU debug
    - expands the runtime blackboard panel
    - exposes more variables and node state in the WebView UI
  - Diff surface
    - network driver/connection/heartbeat/client UI and `runtime-debug.js` all change in parallel.
  - Newcomer takeaway
    - connection loss flows through the custom net connection/heartbeat into the client disconnect handler; AI state inspection is a separate WebView debug branch.

## 2026-06-24 — Persistent broom follow and node status visualization

Commits: `a2ebdb8` — Add persistent BT follow thrust + editor runtime-node status viz.

- Mind map: make broom follow continuous and visible
  - Broom follow
    - keeps thrust/follow state persistent across behavior ticks
    - separates vehicle follow behavior from agent orchestration
  - Editor visualization
    - shows runtime node status in the graph/debug UI
    - updates `index.html`, `app.js`, `graph.js`, and `runtime-debug.js`
  - Diff surface
    - broom agent/vehicle and BehaviorU editor debug surfaces are synchronized.
  - Newcomer takeaway
    - if the broom stops between tree ticks, inspect persistent follow/thrust state; if the tree is running but unclear, inspect the node-status view.

## 2026-06-26 — Broom mount/follow separation

Commits: `65a272b` — Add dismount back-off, fixed-height follow, and SRP refactor of BT follow.

- Mind map: separate movement responsibilities
  - Mount/dismount
    - adds a back-off movement when dismounting
  - Follow behavior
    - follows at a fixed height
    - removes vehicle-specific follow details from the agent component
  - Single responsibility
    - moves vehicle mechanics into `AuraBroomVehicle`
    - leaves the BehaviorU agent focused on commands/state
  - Diff surface
    - four broom source files are reorganized; the behavioral contract stays “follow the player at controlled height.”
  - Newcomer takeaway
    - agent code requests follow; vehicle code performs movement and dismount mechanics.

## 2026-06-27 — Broom component split and autonomous follow smoothing

Commits: `00d978b`, `ed18bff`.

- Mind map: turn the broom into focused components
  - Component split
    - extracts follow, motion, mount, movement, and later flight-driver responsibilities
    - removes dead portal/GSM code from `LevelJumpPortal`
  - Autonomous follow
    - updates position every tick
    - applies Z-floor/fixed-height constraints
    - eases approach instead of snapping
  - Diff surface
    - `AuraBroomVehicle.cpp` shrinks while focused components and headers are added; follow movement becomes smoother and easier to test.
  - Newcomer takeaway
    - start at the component matching the symptom: mount, follow, motion, movement, or flight driver.

## 2026-06-28 — Flight driver and vertical movement

Commits: `5fb26aa`, `b795d9e`.

- Mind map: complete the broom flight control boundary
  - Flight driver
    - extracts flight-specific movement into `AuraBroomFlightDriverComponent`
    - keeps the main vehicle actor focused on composition
  - Vertical input
    - adds ascend/descend commands to the player controller
    - connects them to broom flight state
  - Root/mesh synchronization
    - sweeps the mesh for collision-safe movement
    - re-syncs the actor root after movement
  - Diff surface
    - player input, flight driver, movement, follow, and vehicle composition all change as one final broom architecture pass.
  - Newcomer takeaway
    - player input supplies vertical intent, the flight driver applies it, movement handles collision/root synchronization, and follow remains its own component.
