# Playable Candidate — Next 20 Logical Development Days

Date: 2026-08-29  
Scope: Days 21–40 after the Role/Battle first vertical slice  
Status: Planned execution contract, revised after deep review

Deep review: [local deep-review report](../Reports/Playable-Candidate-Deep-Review-2026-08-29.md)

## Mission

Move AuraProj from an architecture-complete prototype to a small, genuinely playable, explainable, supportable multiplayer game candidate. The target is not another twenty days of system breadth. The target is that a new player can launch, understand the loop, fight, run out of ammunition and reload, die and recover, interact with a merchant, see a meaningful wallet/inventory result, reconnect without losing intended state, and leave enough evidence for an engineer to diagnose a failure.

## Starting truth

- Start from revision `6e78692` or a later recorded candidate revision.
- The existing Role/Battle Days 01–20 arc is implemented locally with the release classification recorded in [the readiness review](../Reports/Playable-Candidate-Readiness-Review-2026-08-29.md).
- The production Online Subsystem/account gate remains external and must never be represented as passed by a fixture identity or `OnlineSubsystemNull`.
- The canonical test/playable world is the existing StartupMap/runtime-fixture path selected by `LevelConfig.json`; do not create a duplicate `RoleBattleCivilianTest.umap` solely to satisfy stale plan language.
- Civilian AI and marker objects remain runtime-created unless a product requirement proves that designer-authored assets are needed.

## Supported playable contract

The candidate must support both existing player roles, Aura and BungeeMan, on the canonical StartupMap fixture, in packaged local/LAN listen and dedicated topologies. The player journey is:

`launch → login → loading → controllable role → move/focus target → attack → observe authoritative result → interact/trade → earn/spend → die/recover → save → disconnect/reconnect or late join`

The candidate must retain the existing server-authoritative rules, owner-only economy, stable identity/persistence, Civilian population lifecycle, battle phases, and WebUI presentation boundary.

## Deep-review decisions frozen before implementation

Day 21 is a decision gate, not a discovery day. The following values are fixed for this candidate and must be written into the machine-readable scope manifest before Days 22–40 begin:

- **World and roles:** `/Game/Maps/StartupMap` is the canonical map; its runtime Civilian/marker fixtures are intentional. Aura and BungeeMan are the only supported roles. Local listen and dedicated packaged topologies are both required.
- **FireGun:** the active path remains `Content/AbilityDefinitions/FireGun.xml` through the existing server execution boundary. The candidate uses semi-auto semantics: one accepted shot per press, no held-input auto-repeat, server-defined cadence, and server-owned magazine/reserve/reload state. `UAuraFireGun` remains compatibility-only unless a later plan explicitly changes this decision.
- **Role applicability:** FireGun magazine/reserve/reload applies to BungeeMan only because Aura's active LMB definition is `FireBolt.xml`. Aura's HUD shows an explicit `NotApplicable` firearm state; it must not invent ammo values. Both roles still complete the common combat, interaction, reward, persistence, death, and recovery journey.
- **FireGun input recovery:** press/release are an edge-triggered owner input contract. The server clears a lost pressed state after a two-second input-lease timeout and emits a typed recovery result; a missing release cannot permanently disable firing.
- **Ammo and tutorial persistence:** wallet, inventory, role/loadout, magazine/reserve ammo, and tutorial completion persist as player-owned state across pawn replacement, reconnect, and a graceful server restart. A forced kill restores the last committed snapshot; uncommitted actions roll back. There is no offline ammo refill or offline player progression.
- **Reload persistence:** an in-flight reload is never serialized or auto-completed. Death, pawn replacement, disconnect, forced kill, and graceful shutdown cancel it; a checkpoint contains only completed magazine/reserve values, and restart/reconnect restores the last committed completed state.
- **Player recovery persistence:** a committed `Dead` or `Recovering` profile is normalized to one server-owned `Recovering → Alive` transition on the next valid join/restart; clients cannot turn a persisted life state directly into a respawn.
- **Reward:** `RoleBattle.CivilianLethalReward` pays `25 gold` once for each eligible authoritative lethal Civilian outcome attributed to the attacking player. Each outcome receives a server-issued `OutcomeCorrelationId`, and one reward transaction may commit per ID. The canonical purchase is offer `market_health_potion` for `25 gold`, granting one `health_potion`, and applies to both supported player roles. Client claims, actor tags, and client-supplied amounts are never inputs.
- **Merchant restock:** server UTC is the authoritative clock, with an injectable deterministic clock for tests. The finite `market_health_potion` stock refills to its configured `initialStock` of `20` after an elapsed interval of `600` seconds, using `EffectiveNow = max(observedUtc, LastObservedUtc)` and an inclusive interval boundary. `LastRestockAtUtc` and the last observed time are persisted; startup performs at most one refill. No offline earnings or unbounded catch-up is supported.
- **Diagnostics privacy:** logs use a stable redacted/hash representation of player identity plus provider type; raw external identifiers, credentials, tokens, and personal data never enter the candidate artifact.

The Day 21 scope manifest also separates `scopeRevision` (the frozen contract/hash) from `sourceRevisionAtFreeze`. Implementation commits may advance the source revision; final sign-off must match the packaged source revision and unchanged scope hash.

No downstream plan may contain an unresolved `choose`, `normally`, or `if persistent` decision for these surfaces. A change requires a revised scope manifest and a new candidate revision.

## Implementation ownership and artifact convention

Each daily plan must name a logical owner surface, a checked-in or explicitly planned artifact, and a machine-readable gate. The planned additions below are outputs of this 20-day arc, not claims that the files already exist:

- `Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json` — Day 21 source-of-truth scenario and decision manifest.
- `Content/Config/PlayableCandidateManifest.json` — Day 34 canonical content manifest consumed by validators and runners.
- `RunPlayableCandidate.ps1` — Day 38 single entry point with `-Stage Fast|Candidate|External`, diagnostic `-Soak` selection, Day 40-only `-Finalize`, explicit stage/result codes, and bounded child-process ownership.
- `Saved/Reports/PlayableCandidate/<SourceRevision>/<RunId>/candidate-draft.json` — Day 38 provisional machine artifact; `candidate.json` is written once by Day 40 as the immutable final artifact. Markdown/SVG explain them but do not replace either machine record.
- `Docs/Reference/Playable-Candidate-Server-Operations.md` — Day 37 second-developer runbook, alongside the existing `Scripts/GameServerManager.py` and dedicated-server batch entry points.

Existing runners such as `RunReviewSmokeSuite2.ps1`, `RunRoleBattleDay18PersistenceSmoke.ps1`, and `RunRoleBattleDay19Multiplayer.ps1` are reused and wrapped; the plan does not create a parallel test framework.

## Dependency waves

| Wave | Days | Outcome |
| --- | --- | --- |
| Freeze and re-prove | 21–24 | One unambiguous playable contract, fresh post-tooling baseline, deterministic boot, and first-use guidance. |
| Complete the player loop | 25–31 | Authoritative ammo/reload/fire semantics, visible combat feedback, player recovery, reward/spend, and deterministic restock. |
| Make multiplayer state survivable | 32–33 | Persistence closure, late join/reconnect, and negative-path network hardening. |
| Make development supportable | 34–38 | Shared content validation, correlation-rich diagnostics, packaged visual QA, server operations, and candidate artifact production. |
| Candidate sign-off | 39–40 | Long soak, full local/LAN matrix, explicit provider-gated release status, and frozen limitations. |

## Daily execution contracts

| Day | Milestone | Contract |
| --- | --- | --- |
| 21 | Freeze the playable contract | [Day 21](Playable-Candidate-Implementation/Day-21-Freeze-Playable-Contract.md) |
| 22 | Re-prove after harness fixes | [Day 22](Playable-Candidate-Implementation/Day-22-Reprove-Post-Tooling-Baseline.md) |
| 23 | Canonical boot-to-game flow | [Day 23](Playable-Candidate-Implementation/Day-23-Canonical-Boot-Flow.md) |
| 24 | First-use/tutorial slice | [Day 24](Playable-Candidate-Implementation/Day-24-First-Use-Tutorial.md) |
| 25 | Authoritative ammunition model | [Day 25](Playable-Candidate-Implementation/Day-25-Authoritative-Ammunition.md) |
| 26 | Combat HUD completion | [Day 26](Playable-Candidate-Implementation/Day-26-Combat-HUD-Completion.md) |
| 27 | Minimum fire-mode semantics | [Day 27](Playable-Candidate-Implementation/Day-27-Minimum-Fire-Mode.md) |
| 28 | Player death and recovery | [Day 28](Playable-Candidate-Implementation/Day-28-Player-Death-Recovery.md) |
| 29 | Reward-to-merchant loop | [Day 29](Playable-Candidate-Implementation/Day-29-Reward-Merchant-Loop.md) |
| 30 | Merchant availability/restock MVP | [Day 30](Playable-Candidate-Implementation/Day-30-Merchant-Restock-MVP.md) |
| 31 | Persistence contract closure | [Day 31](Playable-Candidate-Implementation/Day-31-Persistence-Closure.md) |
| 32 | Late join and reconnect | [Day 32](Playable-Candidate-Implementation/Day-32-Late-Join-Reconnect.md) |
| 33 | Network abuse/error hardening | [Day 33](Playable-Candidate-Implementation/Day-33-Network-Abuse-Hardening.md) |
| 34 | Content/config validation gate | [Day 34](Playable-Candidate-Implementation/Day-34-Content-Validation.md) |
| 35 | Diagnostics package | [Day 35](Playable-Candidate-Implementation/Day-35-Diagnostics-Package.md) |
| 36 | Packaged visual-resolution QA | [Day 36](Playable-Candidate-Implementation/Day-36-Packaged-Visual-QA.md) |
| 37 | Dedicated-server operations contract | [Day 37](Playable-Candidate-Implementation/Day-37-Server-Operations.md) |
| 38 | Repeatable candidate pipeline | [Day 38](Playable-Candidate-Implementation/Day-38-Candidate-Pipeline.md) |
| 39 | Bounded soak and candidate matrix | [Day 39](Playable-Candidate-Implementation/Day-39-Long-Soak-Matrix.md) |
| 40 | Playable Candidate sign-off | [Day 40](Playable-Candidate-Implementation/Day-40-Playable-Candidate-Signoff.md) |

## Embedded support-system track

Support work is part of the daily gates, not a separate “later” project.

### Three release layers

1. **Fast gate:** build the relevant target, validate JSON/XML/HTML contracts, run focused native/Python/plugin tests, and reject malformed content quickly.
2. **Candidate gate:** build/package Editor, Game, and Server as applicable, validate staged manifests, launch packaged local/LAN topology, run selected gameplay smoke, and publish machine-readable results plus hashes.
3. **Release gate:** use the configured production provider, two real authorized accounts, stable distinct `FUniqueNetIdRepl` identities, and the packaged public listen/dedicated two-remote-client matrix. This layer may remain BLOCKED while the first two layers pass.

### Non-negotiable evidence chain

`revision → build configuration → content/config manifest → test result JSON/XML → package hash → server/client logs → visual captures → candidate decision`

Markdown and SVG records explain the result for humans; the machine-readable candidate artifact is authoritative.

### Safe extension seams

- Role definition → validated role state → granted abilities/items.
- Ability graph → server-validated gameplay invocation and shared damage boundary.
- Weapon data → authoritative ammo/reload/fire semantics.
- Economy definitions → merchant instance → atomic transaction result.
- Versioned persistence schema → player/world reconciliation.
- Identity provider → stable player profile identity.
- WebUI → presentation of replicated state and narrow validated commands, never gameplay authority.

## Global completion gate

Days 21–40 are complete only when:

- A clean packaged local/LAN journey passes for Aura and BungeeMan on listen and dedicated topologies. A listen lane is one packaged host plus one packaged remote client; a dedicated lane is one packaged server plus two packaged remote clients.
- The mandatory matrix covers four lanes (Aura/listen, BungeeMan/listen, Aura/dedicated, BungeeMan/dedicated), with a fresh journey and late-join, reconnect, graceful restart, and forced-kill recovery cases in every lane. Aura's firearm ammo/reload checkpoints are explicitly `NotApplicable` because its active weapon is FireBolt.
- The player can understand the full loop without developer-only instructions.
- FireGun ammo/reload/fire semantics are authoritative, replicated, persisted as designed, and visible in the HUD.
- Player death/recovery, merchant reward/spend, persistence, late join, reconnect, and owner privacy are visually and functionally evidenced.
- No runner hangs, orphaned processes, false-success aggregates, shared persistence IDs, malformed staged definitions, or unexplained failures remain.
- Every mandatory stage has a bounded timeout, nonzero failure propagation, and a machine-readable result; no candidate gate is satisfied by a warning-only or partial aggregate.
- Server operation is reproducible by a second developer from the documented command line and artifacts.
- Local/LAN Playable Candidate status is explicit. Public authenticated multiplayer is **PASS only** if the provider/account matrix ran; otherwise it is **BLOCKED** with the exact external preflight listed.

For this milestone, a local candidate is **PASS** only when all four mandatory lanes pass with zero P0/P1 defects, zero unexplained blocking warnings, zero data loss/duplication/privacy violations, and no stage exceeding its documented timeout. The external provider/account gate is reported independently and cannot turn a local PASS into a local FAIL or a missing provider into a false PASS.

## Anti-goals and deferrals

Do not expand this milestone with role hot-swapping, weapon durability, full reputation/crime, rich civilian schedules, broad business simulation, large-crowd optimization without a measured violation, wholesale legacy GAS/Blueprint/passive/projectile cleanup, duplicate authored fixture maps/assets, cloud orchestration, or a new test framework. Fix a deferred area only when it blocks the supported contract or produces a confirmed defect.
