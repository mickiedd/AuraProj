# AuraProj Playable Candidate Readiness Review — 2026-08-29

## Purpose

This report records the local project review, the status of the completed Role/Battle implementation arc, and an independent in-app ChatGPT review used to shape the next 20 logical development days.

The conclusion is deliberately conservative: AuraProj has a strong, server-authoritative technical slice, but it is not yet a player-ready release. The next milestone should turn the existing systems into a small, understandable, supportable **Playable Candidate** instead of adding another broad subsystem.

## Evidence reviewed

- `Docs/Plans/Role-Battle-Implementation-Index.md` and all twenty day contracts in `Docs/Plans/Role-Battle-Implementation/`.
- `Docs/Reports/DayPlan-Archive/`, `Docs/Reports/Role-Battle-Vertical-Slice-2026-08-28.md`, and the job-level change archive.
- `Docs/Tracking/Role-Battle-Review-Findings-2026-08-28.md`, `Docs/Tracking/Role-Battle-Issue-Dispositions.md`, and `Docs/Tracking/GAS-Migration-TODOs.md`.
- `Docs/Plans/Role-Creation-and-Battle-System-Plan.md`, `Docs/Plans/Role-Battle-UI-Incremental-Plan.md`, and the vertical-slice test procedure.
- Current source/config/tooling surfaces: native Aura and AuraEditor modules, AuraAbilityGraph, AuraWebUI, AuraAutoTest, BehaviorU, JSON/XML definitions, smoke runners, Config Studio, and the visual Behavior Tree debugger.
- Current repository revision: `6e78692` (`Apply Role/Battle review fixes and tools`), Unreal Engine 5.5, Win64. The worktree was clean before these planning artifacts were added.

## Current readiness classification

| Classification | Status | Evidence and boundary |
| --- | --- | --- |
| Internal engineering build | **Usable now** | Local Development/Shipping client/server builds, localhost/LAN gameplay, design testing, packaged-content testing, and the existing native/plugin harness are available. |
| Local/LAN Playable Candidate | **Target of Days 21–40** | Requires a bounded boot-to-game loop, onboarding, minimum firearm state, visible death/recovery/economy/persistence behavior, packaged visual QA, deterministic server operations, and a post-tooling multiplayer/soak rerun. |
| Public authenticated multiplayer release | **BLOCKED** | Production Online Subsystem/App ID, two authorized accounts with distinct stable `FUniqueNetIdRepl` identities, and the packaged Shipping listen/dedicated two-remote-client matrix are not provisioned on this host. |

The external release blocker is not a reason to stop local work, but it must remain visibly blocked. Development fixtures and `OnlineSubsystemNull` cannot be reported as production identity evidence.

## Other plan families already completed or in progress

The Role/Battle arc depends on several adjacent plans. They were included in the review so the new candidate roadmap does not reopen work that is already good enough or accidentally hide work that is still incomplete.

| Plan family | Current reading | Consequence for Days 21–40 |
| --- | --- | --- |
| Buff and Pickup data-driven migration | Implemented and validated. Runtime definitions and pickup/effect behavior already have a data-driven path and focused contract coverage. | Reuse the existing registry and authority boundary; do not create a second pickup/effect data system. |
| In-game WebUI HUD | Complete for the player-facing Day 01–18 slice. Login/loading/HUD pages, bounded three-panel geometry, state replay, and narrow command gates are documented and tested. | Finish onboarding, ammo/death/merchant visuals, packaged-resolution captures, and late-join/reconnect presentation on the existing WebUI path. |
| GAS/AuraAbilityGraph rewrite | Core runtime is implemented; graph, XML, native damage, and import/reload issues are largely resolved. Legacy asset/source cleanup, optional projectile conversion, and passive migration remain open. | Fix only cleanup that blocks the supported candidate or exposes a defect. Do not let legacy removal displace player-loop work. |
| Gameplay Blueprint decoupling | In progress. The data-driven boundary is real, but remaining old packages and compatibility code still require reference/reload/cook decisions. | Treat this as a bounded maintenance stream behind the candidate gates, not as a reason to expand gameplay scope. |
| Role Creation/Battle master plan and UI incremental plan | The first-slice architecture and player-facing signal contracts are the authoritative design baseline. | Extend the existing seams—role, ability, weapon, economy, persistence, identity, and WebUI—rather than adding parallel ownership paths. |

## Completed Role/Battle arc analysis

The twenty existing plans form a coherent authority-first dependency chain. The table below records what each plan achieved and what still matters after it.

| Day | Finished-plan result | Remaining implication |
| --- | --- | --- |
| 01 | Established the baseline, fixed additive-stat/respawn drift, and proved the headless Aura/BungeeMan functional path. | Rendered presentation was intentionally carried into the later role/package gate; do not mistake headless success for visual readiness. |
| 02 | Added replicated combat identity and validation with network coverage. | Identity is now a safe seam for rules, UI, persistence, and future roles. |
| 03 | Added replicated life state and shared relationship/permission rules with default-deny Civilian damage. | Every new action must preserve server-side rule resolution and non-Alive rejection. |
| 04 | Routed native and graph damage through the authoritative boundary and serialized attribution. | New weapon state must use the same boundary; no client-side ammo or damage truth. |
| 05 | Added versioned, aggregate role parsing and atomic last-known-good publication. | Future config must follow parse → validate → publish, with actionable errors and no partial registry. |
| 06 | Added atomic role application and persistent ASC grant bookkeeping across pawn replacement. | Role hot-swapping remains disabled until a real transactional cleanup/rollback path exists. |
| 07 | Covered Aura/BungeeMan regression, listen/dedicated/package paths, and the active `FireGun.xml` route. | The latest revision repaired runner composition and XML discovery; the full long post-tooling sweep still needs fresh evidence. |
| 08 | Added a role-derived, replicated Civilian actor with GAS health and no offensive loadout. | Civilian is a stable non-combatant gameplay surface, not a second player/enemy hierarchy. |
| 09 | Added validated population data, an authority-owned manager, stable member slots, reservation rollback, and spawn ordering. | Keep the canonical StartupMap/runtime-fixture contract; do not add assets only to repair old wording. |
| 10 | Added server-authoritative Civilian work/observe/threat/flee/shelter behavior and the hostile-target migration. | Runtime-created behavior assets are acceptable unless designer authoring becomes a product requirement. |
| 11 | Added exactly-once death transitions and policy dispatch for player, enemy, and Civilian lifecycles. | Player death/recovery still needs a player-facing, visually proven loop distinct from Civilian death. |
| 12 | Added replicated battle phases and deterministic zone policy, making Civilian casualty permissions explicit. | The battle director must be replayable for late join and reflected in usable UI. |
| 13 | Added Civilian corpse cleanup, delayed refill, stable IDs, and explicit population ownership. | Offline timer semantics and large-crowd scaling remain deferred. |
| 14 | Added orthogonal targeting plus a player-owned, validated Interact route. | Onboarding must teach target vs. attack vs. Interact without relying on developer knowledge. |
| 15 | Added a versioned read-only economy registry and stable merchant binding. | A minimum reward-to-spend loop is still needed for the game to feel complete. |
| 16 | Added owner-only PlayerState wallet/inventory with authoritative mutation and respawn persistence. | The UI and reconnect path must make ownership and revision-backed updates obvious to players and testers. |
| 17 | Added replay-safe, atomic merchant transactions with authoritative results and UI state. | Restock/time semantics need only the minimum deterministic behavior required by the playable loop. |
| 18 | Added separate authenticated player saves and a load-once authoritative world snapshot with migration/reconciliation. | The final provider boundary remains unproven; corruption, restart, late-join, and visual replay need a candidate gate. |
| 19 | Added listen/dedicated security, replication, late-join, reconnect, concurrency, and performance matrices. | Re-run the long packaged matrix after the new supervisors, per-run world IDs, and result composition changes. |
| 20 | Completed cleanup and local release verification: builds, native/plugin automation, staged data, package checks, and UDP launch passed. | Formal release is still blocked by provider/accounts and the dependent packaged Shipping two-client matrix. |

The sequence is architecturally sound. Its main weakness is that system completion outpaced player-facing comprehension and durable operational evidence.

## Project-level assessment

### Strong foundation

- The server-first rule is consistently applied to damage, roles, spawning, economy, persistence, and replicated state.
- The data-driven seams are explicit: JSON registries, XML ability graphs, role-derived identity, PlayerState economy, stable population/member IDs, and separate player/world saves.
- WebUI is correctly treated as presentation and command transport; the native runtime remains authoritative. The three bounded HUD panels preserve native level input.
- The project has unusually strong test infrastructure for this maturity: focused native automation, graph smoke, XML AutoTest, Python contracts, listen/dedicated runners, packaged data checks, and editor tools.
- The latest review-fix revision improved the harness itself with bounded editor supervision, explicit exit status, isolated child processes, per-run persistence identity, composed prerequisite status, exhaustive ability-definition discovery, and an editor-launchable BT debugger.

### Biggest blockers to a convincing playable build

1. **The player loop is under-explained.** Login, role receipt, targeting, attack, Interact/Trade, death/recovery, save state, and reconnect need a first-use path and readable feedback.
2. **BungeeMan lacks a minimum firearm state.** An always-available `FireGun.xml` path is a prototype capability, not a complete gun loop. Small authoritative magazine/reserve ammo, reload, and required fire-mode semantics are high value; durability is not.
3. **Rendered behavior is under-gated.** Automated logic does not prove that loading clears, HUD regions are readable, WebUI input does not block the world, targeting feedback is visible, or late-join/death/merchant state initializes correctly in a packaged build.
4. **The post-tooling packaged/network sweep is not yet fresh.** The harness is stronger, but the full evidence must be rerun after those changes before using it as a candidate baseline.
5. **The release boundary is external.** Real provider/account identity and public-network behavior cannot be simulated honestly by local fixtures.

### Planning/documentation debt

The review found drift between historical day manifests and the implementation: the canonical Civilian fixture is a `StartupMap` alias, AI/marker objects are runtime-created, several test-file paths are stale, and most evidence under `Saved/` is gitignored. The latest revision fixed the runner/manifest issues that were actionable without changing gameplay. The next plan treats the actual canonical runtime as the contract and asks for tracked candidate manifests/results rather than recreating missing assets for historical wording.

The remaining data-driven GAS cleanup, passives, optional projectile conversion, and legacy asset removal are important maintainability work, but they should only enter this milestone when they block a supported playable path or expose a real defect.

## External/browser review result

The in-app ChatGPT review was completed as an independent architecture/roadmap pass. It could not independently inspect the repository; its advice was based on the scoped evidence packet above. Its key recommendations were:

- Rename the next arc as a **Playable Candidate** milestone rather than “Days 21–40 of more systems.”
- Re-prove the current packaged/network baseline before building more features.
- Prioritize boot determinism, first-use onboarding, minimum authoritative ammo/reload, combat HUD feedback, player death/recovery, reward-to-merchant flow, late join/reconnect, and visual packaged QA.
- Make harness correctness, machine-readable candidate artifacts, correlation IDs, config validation, deterministic server operations, and three-layer gates (fast, candidate, release) part of the product workflow.
- Treat `StartupMap`/runtime AI fixture drift as a documentation contract issue unless designers actually need authored assets.
- Ship internal Development/Shipping builds as “Aura Playable Development Build”; target a local/LAN Playable Candidate; keep public authenticated multiplayer explicitly BLOCKED until the provider/account matrix passes.
- Defer role hot-swapping, durability, full reputation/crime, rich schedules, broad crowd optimization, large legacy cleanup, cloud orchestration, and further test-framework expansion.

## Decision

The next executable plan is [Playable Candidate — Days 21–40](../Plans/Playable-Candidate-Implementation-Plan-2026-08-29.md). It preserves the existing authority and data-driven architecture, makes the player-visible loop the priority, and embeds support-system work into the same gates so future development is faster and safer.

## Deep plan review follow-up

The generated Days 21–40 contracts were subsequently reviewed in depth against the local repository and the completed Role/Battle plan families. A second in-app ChatGPT handoff was attempted with a scoped packet, but the page timed out twice and inspection confirmed that no packet was sent, so no external deep-review result is claimed. The local fallback closed the actionable gaps: Day 21 now freezes all cross-system decisions; each daily contract names owner surfaces, artifacts, and machine-readable gates; Day 38 names the candidate pipeline entry point; Day 39 uses bounded soak thresholds; and Day 40 separates local/LAN PASS from public-provider BLOCKED. See the [deep review report](Playable-Candidate-Deep-Review-2026-08-29.md).
