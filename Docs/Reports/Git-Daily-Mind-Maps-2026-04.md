# Git daily mind maps — April 2026

These maps are a human reading of the `main` branch history. Each date compares the tip of the previous active date with the tip of the date shown; no-commit calendar days are intentionally omitted. The `Diff surface` branch names the important areas changed, while `Newcomer takeaway` explains why the work mattered.

## 2026-04-17 — Project foundation

Commits: `c710ada` — Init Commit.

- Mind map: repository starts
  - Unreal project shell
    - game module and targets
    - initial project configuration
  - Gameplay foundation
    - character, player, enemy, ability-system, UI, checkpoint, and input namespaces
    - initial interfaces and data classes
  - Editor foundation
    - project and solution scaffolding
    - baseline content/assets
  - Diff surface
    - no earlier project checkpoint exists; this is the root snapshot rather than an incremental feature diff
  - Newcomer takeaway
    - this commit establishes the vocabulary used by almost every later change: Aura characters, GAS abilities, enemies, maps, UI, and editor tooling.

## 2026-04-26 — Editor and login-game bootstrap

Commits: `9f48b2f`, `6533a06`, `9014740`, `5031c06`, `dc1c4ec`, `6ba8251`.

- Mind map: make the project playable from a login entry point
  - Editor module
    - adds `AuraEditor`
    - registers the first editor toolbar/menu integration
  - Blueprint/content baseline
    - imports the initial gameplay, AI, ability, UI, and player assets
    - updates `.gitignore` and `Aura.uproject`
  - Login flow
    - makes Login the default/editor startup map
    - adds Login `GameMode`, automatic connection, and launch helpers
  - Player identity
    - displays player names
    - assigns unique names and a disambiguation token when names collide
  - Diff surface
    - from an empty/root project to a runnable Unreal client with a login map and large initial content set
  - Newcomer takeaway
    - the project changes from a code/content skeleton into a game that can start at Login, connect, and identify multiple players.

## 2026-04-27 — Connection diagnostics and basic movement

Commits: `76f0ee9`, `1ddd314`.

- Mind map: make early multiplayer behavior observable
  - Login connection
    - adds connection logging
    - adds timeout handling so failed login attempts do not hang silently
  - Server configuration
    - reads server settings instead of relying only on hard-coded values
  - Player movement
    - adds jump input and the corresponding controller wiring
  - Diff surface
    - Login/client control code and server-config parsing change; the first player-facing movement input is added.
  - Newcomer takeaway
    - when debugging startup, inspect connection logs/timeouts first; once connected, jump is the first explicit movement action introduced here.

## 2026-04-28 — Player locomotion, monster spawning, and server-owned respawn

Commits: `12331ba`, `8ab48b2`, `e9d642c`.

- Mind map: add the first gameplay loop around the player
  - Locomotion
    - adds crouch input and crouch state support
  - Monster population
    - introduces a monster spawn table
    - adds a location HUD toggle to expose spawn/debug information
  - Death lifecycle
    - moves player death/respawn responsibility into `GameMode`
    - keeps respawn authority on the server-side game rules layer
  - Diff surface
    - controller/input code, spawn configuration, HUD/debug state, and `AuraGameModeBase` all move from setup toward runtime gameplay.
  - Newcomer takeaway
    - `GameMode` is now the place to understand authoritative player lifecycle; the spawn table and HUD toggle explain where monsters come from and how to inspect them.

## 2026-04-29 — Ability input and logging refinement

Commits: `fcbe3c8` — Improve ability input handling and logging.

- Mind map: make ability activation easier to diagnose
  - Input routing
    - tightens ability input handling
    - clarifies how input reaches gameplay abilities
  - Diagnostics
    - adds or improves activation logs
    - gives later GAS work a visible trail when activation fails
  - Diff surface
    - player/controller ability-input paths and logging change without introducing a new gameplay system.
  - Newcomer takeaway
    - when a spell does not fire, follow the controller-to-ability input path and its logs before debugging the spell’s projectile or effect.
