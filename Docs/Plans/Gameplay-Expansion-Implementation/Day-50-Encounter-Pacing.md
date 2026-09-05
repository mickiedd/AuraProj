# Day 50 — Encounter Pacing

Status: Inert contract foundation implemented; full runtime day remains Planned.
Depends on: Day 49 complete objective/space loop; Day 45 enemy counters/heavy-token owner; Day 46 minimal supplies; Day 42 retained-member/proxy contract.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

This increment adds an authority-only pacing reducer, a closed pacing-definition fixture, and all seven named native contract cases. It deliberately does not wire production AI/spawning, mission snapshots, HUD, supplies, health/damage scaling, or packaged play; the Day 46/49 runtime, anchor-survey, and evidence gates remain open.

## Player outcome

Alternate readable pressure and useful relief instead of producing nonstop waves or empty waiting.

## Exact change surfaces

- Extend `Source/Aura/Public/Gameplay/AuraEncounterCoordinator.h`, mission snapshots and authoritative outcome notifications.
- New: `Source/Aura/Public/Gameplay/AuraEncounterPacingTypes.h`, `AuraEncounterPacingPolicy.h` and implementation; `Content/Config/GameplayPacingDefinitions.json`.
- Existing: battle director phase transitions, enemy archetype costs, HUD combat state. New pacing never takes ownership of zone protection or damage.

## Data and authority contract

Use 2Hz authority scheduling with Build/Pressure/Recovery, distinct from battle policy phases. Intensity is a tuning signal, not a psychological measurement: maximum participant pressure from recent 5s health loss, nearby committed threats and Day 42 incapacitation state, normalized 0–1; Day 51 later contributes AwaitingRescue through the same input. Enter Recovery at >=0.75 or after 30s Pressure; minimum Build 10s and Recovery 8s, max Recovery 20s unless bounded choices/supply interactions await. Day 46 medkits/emergency terminals already exist; the Day 52 station is not required here. No hidden HP/damage scaling mid-encounter. Fixed required spawn roster must still finish; pacing only admits its queued slots, never substitutes objective count. Reuse Day 45 EncounterCoordinator heavy tokens, including targetable dormant proxies, rather than adding a second attack-budget owner.

## Numbered implementation steps

1. Define costs Raider=1, Lancer=2, Bulwark=3, Disruptor=3; wave admission budget 6 solo/10 pair, active caps 10/16. Validate recipes can complete within their objective deadline.
2. Record server-observed intensity inputs and clamp them; smooth over a two-second window and use 0.45 exit threshold to prevent rapid oscillation.
3. Schedule only legal pending leases; require validated offscreen anchors and reuse Day 45 encounter-owned heavy tokens/cue limits. Each heavy Windup accounts for every live player/proxy within 600 units of its committed shape, maximum two tokens per affected participant; the pacing policy cannot bypass or independently reset those tokens. During Recovery no new hostile admission; existing hostiles remain real.
4. Keep minimum quiet window after active threats stop; choice/resupply events pause admission, not the run deadline. Telegraph next Pressure before spawning.
5. Keep live dormant proxies in current-encounter membership, wipe checks and heavy-token accounting throughout their 60s lease; existing hits/DoTs/enemies and deadlines continue. When no connected Alive member remains, suspend only new admissions while a live proxy remains. After committed expiry/abandonment cleanup, recompute future admission at encounter boundaries without rewriting current enemies' health. A proxy death can still cause a normal wipe during grace.
6. Compare fixed schedule and adaptive schedule against identical seeds/outcome traces; tune to remove long empty spans and relentless pressure.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay50Tests.cpp`; namespace `Aura.Gameplay.Day50`.

- HysteresisPreventsChatter — oscillating intensity near threshold cannot switch phase every tick.
- BudgetAndLiveCaps — mixed costs/pending leases never exceed admission/live limits.
- RecoverySuppressesNewSpawns — no newly committed lease during relief/choice window; existing enemies and combat deadlines continue.
- RequiredRosterStillCompletes — recovery cannot permanently strand an objective's queued enemies.
- DisconnectNoHealthRewrite — live proxies retain current-encounter/threat membership and vulnerability; only new admission pauses without connected Alive members, and committed expiry changes future scheduling without mutating living enemies.
- HeavyBudgetOwnerPreserved — pacing cannot admit a Windup around the Day 45 per-player/proxy token cap or reset live tokens during phase changes.
- SeedAndInputsReproduceDecisions — recorded input trace produces the same ordered admission decisions.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 50 -Stage Fast -RunId d50-fast
./RunGameplayExpansion.ps1 -Day 50 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d50-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

PacingLab: synthetic intensity traces and actual mixed encounter in all nine lanes; injury spike, low resource/Day 46 emergency use, incapacitated member, targetable dormant proxy taking a committed hit/DoT, all-disconnected grace, lease expiry, blocked anchors and a 20-second idle detector. Include heavy-token saturation across a pacing transition.

## Failure and timeout semantics

Invalid/non-finite intensity input is clamped and logged; missing source must not create unlimited pressure. If queued required enemies cannot be admitted before objective deadline, fail/abort using its typed reason rather than silently awarding Clear. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-50.json, pacing-input-output.csv, spawn-budget-trace.json, fixed-vs-adaptive comparison and rendered pressure/recovery clips. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Both roles get visible recovery windows, fixed-roster objectives still terminate, encounter-owned spawn/heavy budgets hold under disconnect/load, and players can name when it is safe to choose/use the available supplies. Disconnect never grants immunity or pauses existing combat/deadlines, and Day 51/52 features are not required to satisfy this gate.

## Defer / anti-goals

No machine-learning director, emotional inference, dynamic damage cheating, endless horde mode or replacement of BattleDirector.
