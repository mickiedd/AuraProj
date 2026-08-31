# Day 49 — Civilian Rescue and Mission Space

Status: Planned; not implemented by this planning job.  
Depends on: Day 48 Escort reducer, Day 43 arrangement A, existing Civilian reservations/lifecycle.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Make moving through the world and protecting civilians meaningful, with two readable arrangements and three playable mission templates.

## Exact change surfaces

- Existing: `Source/Aura/Private/World/AuraPopulationManager.cpp`, `Private/AI/AuraCivilianAIController.cpp`, `BTTask_CivilianMoveTo.cpp`, `BTTask_FindCivilianDestination.cpp`, existing activity/shelter markers.
- New: `Source/Aura/Public/Gameplay/AuraEscortObjectiveComponent.h` and implementation; `Content/Config/GameplayEscortDefinitions.json`; add arrangement B to `GameplayArenaLayouts.json`.
- Existing: `Content/Config/BattleZones.json`, `CivilianWorkProfiles.json`, combat policy resolver; Day 48 objectives. Scope gameplay-policy overrides separately; do not change old fixture policies.

## Data and authority contract

Reserve two existing designated civilian member IDs at preparation; escorts remain owned by PopulationManager for spawn/death, while a mission activity lease owns destination. Suppress their normal work/flee scheduling only while the lease is valid. Players cannot damage civilians. Only these reserved escorts may be attacked by mission enemies under an explicit run policy. No permanent death/economy/reputation mutation; successful return releases the lease and restores normal work. Arrangement B reuses verified assets/anchors, not a new biome.

## Numbered implementation steps

1. Validate preparation can reserve both required live civilians; otherwise remain in hub with EscortUnavailable. Bind mission ID/generation to reservations and override only designated movement intent.
2. Create a reachable escorted path with two shelter waypoints and cover. Civilians move only while an alive participant is within 800 units; stop safely otherwise. Arrival counts only inside the registered destination volume.
3. Add escort health/progress and threat feedback; use existing non-combat actor identity, never grant attack abilities. Make enemy targeting of escorts explicit and limited.
4. Implement path-stall repair using requested-movement time since last actual route progress. Accumulate only while movement is requested with an Alive escorting participant within 800 units; pause that stall clock during WaitingForEscort or an intentional shelter wait, while objective/run deadlines continue. Detect/first retry at 5s of accumulated requested movement, retry again at 7s, then abort technically no later than 9s if actual progress has not resumed. A successful new path alone does not reset the progress clock. Never teleport to complete. Civilian lethal outcome fails the objective and goes through existing lifecycle once.
5. Survey/layout arrangement B and test both arrangements for role reachability, cover, objective LOS, hub exclusion and spawn constraints.
6. Conduct the first combined gameplay review: each role tries all three templates with augments. Capture whether movement/escort objectives change tactics; fix unreadable/blocked routes before adding pacing.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay49Tests.cpp`; namespace `Aura.Gameplay.Day49`.

- ReservationPreventsTwoOwners — normal work/other mission cannot steal the escort lease.
- EscortPolicyScoped — enemy can damage only designated run escorts; player damage remains denied.
- EscortArrivalExactlyOnce — two overlap events count one member; both required members must arrive.
- StallAbortBounded — a requested but blocked path terminates within nine seconds of requested-movement time since last actual progress, with detection/first retry at 5s and second retry at 7s; a path-query success without movement cannot reset the clock.
- IntentionalEscortWaitIsNotStall — leaving the 800-unit escort range or intentionally waiting at shelter for more than nine seconds pauses the stall clock without technical abort; returning resumes requested movement while objective/run deadlines have continued.
- ReleaseRestoresCivilianWork — success/failure/abort releases destination and delegates.
- LayoutsReachableByBothRoles — verified capsules/nav paths/relay LOS pass in A and B.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 49 -Stage Fast -RunId d49-fast
./RunGameplayExpansion.ps1 -Day 49 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d49-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

RescueRelayLive: two civilians, two shelter waypoints, 800-unit follow range, out-of-range/shelter wait longer than nine seconds followed by return, forced corridor blockage while movement remains requested, and one civilian casualty; all nine lanes in layout A, solo/mixed lane coverage in B before full Day 55 matrix.

## Failure and timeout semantics

Reservation timeout 5s aborts preparation without resource change. A path stall has a nine-second requested-movement deadline since last actual progress, not nine additional seconds after detection. WaitingForEscort/intentional shelter waits pause only that stall clock; objective/run deadlines never pause. Escort death fails gameplay normally; navigation/actor disappearance is technical abort with no reward. Cleanup must not fight the existing population respawn timer. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-49.json, escort-reservations.json, nav-and-policy-report.json, arrangement-A-B screenshots, three-template walkthroughs and checkpoint-play-observations.md. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All three templates are playable by either role and same-role pairs; escort ownership, scoped vulnerability and cleanup work; observed space/route decisions justify the added objective.

## Defer / anti-goals

No civilian schedules overhaul, crime/reputation, permanent settlement casualties, procedural terrain, vehicles or broom-dependent objectives.
