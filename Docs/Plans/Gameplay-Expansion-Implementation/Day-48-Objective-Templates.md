# Day 48 — Objective Templates

Status: Planned; not implemented by this planning job.  
Depends on: Day 43 encounter membership/two-cell sequence, Day 47 boundary choices, and Day 44/46 ordered interaction cancellation.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Deliver Assault, Rescue Relay and Sabotage through three reusable objective verbs.

## Exact change surfaces

- New: `Source/Aura/Public/Gameplay/AuraObjectiveTypes.h`, `AuraObjectiveComponent.h`, `AuraMissionInteractable.h` with private implementations; `Content/Config/GameplayObjectiveDefinitions.json`.
- Extend `GameplayMissionDefinitions.json`, Day 42 mission reducer and Day 43 encounter lease events.
- Existing: `Source/Aura/Public/Combat/AuraTargetableInterface.h`, `Public/Player/AuraPlayerController.h`, native targeting/interaction controller and registered scene marker conventions.

## Data and authority contract

Three reducer types only: Clear accepts registered deaths, Escort accepts designated civilian arrivals, InteractHold accepts validated uninterrupted channel completion. Stable objective/target IDs, run generation and revisions key inputs. Generalize Day 43's existing two-cell progression into an acyclic ordered objective sequence with required versus optional flags, deadlines and extraction prerequisite; preserve its current-cell identity and exactly-once CellCompleted contract consumed by Day 47. Rescue's full AI integration arrives Day 49; its Day 48 movement-arrival fixture is explicitly diagnostic until then. Accepted evade cancels the owner's channel even if subsequent movement is blocked; rejected/duplicate evade never cancels or advances it.

## Numbered implementation steps

1. Add schema/reference/DAG checks and explicit state Pending/Active/Completed/Failed/Cancelled over the existing Day 43 two-cell owner. Preserve exactly-once boundary events/augment admission; future/inactive objectives ignore valid but out-of-order events.
2. Implement Assault second-cell finite Clear using coordinator membership; no global enemy actor scan can complete it.
3. Implement Sabotage's two relays, each with one owning 3s channel. Validate 200-unit distance, LOS, Alive state and current target revision throughout; damage, accepted evade or disconnect cancels. Follow Day 44's validate-then-cancel ordering, including a fully blocked accepted evade and no channel mutation from rejected/duplicate evade.
4. Implement Escort reducer with exact designated member IDs and shelter-volume authority checks; do not let client arrival flags or unrelated civilians count.
5. Connect HUD current action/progress/deadline and contextual failure reasons immediately. All templates use the same result/extraction pipeline.
6. Run objective teardown/race/ordering tests and verify optional cache branch failure never completes or blocks unrelated required objectives.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay48Tests.cpp`; namespace `Aura.Gameplay.Day48`.

- ForeignOutcomeIgnored — another run's death/arrival/channel cannot advance counters.
- ObjectiveDAGRejected — cycles, missing successor and unreachable required objective fail validation.
- ChannelRaceSingleOwner — two players starting one relay produce one lease; second sees Busy.
- ChannelCancelNoProgress — damage/death/accepted evade/disconnect/LOS loss cancels without completing; fully blocked accepted evade still cancels and rejected/duplicate evade changes nothing.
- EscortIdentityRequired — fixture arrival of wrong/duplicate member cannot satisfy rescue.
- ExtractionRequiresAllPrimary — optional success cannot bypass a missing required objective.
- GeneralizedSequenceKeepsChoices — replacing the simple Clear sequence with objective data preserves current-cell identity and exactly two Day 47 offers with no duplicate completion.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 48 -Stage Fast -RunId d48-fast
./RunGameplayExpansion.ps1 -Day 48 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d48-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

ObjectiveLab: all templates and nine lanes; event order permutations, two relay contenders, blocked LOS, blocked/invalid/duplicate evade during channel, duplicate death/arrival, destruction before completion and both existing augment boundaries. Rescue live gameplay completion is deferred explicitly to Day 49, not marked PASS here.

## Failure and timeout semantics

Per-objective budgets: Clear 180s, Escort 240s, each relay objective group 180s; global 20-minute limit still wins. Destroyed required target yields TechnicalObjectiveFailure and abort; no auto-completion. Invalid DAG blocks world readiness. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-48.json, objective-transition-table.json, malformed-DAG-results.json, interaction-cancel-traces.json and HUD progress captures; label synthetic escort fixture evidence. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All three reducers and actual Clear/Sabotage play pass; live Rescue Relay remains a named Day 49 integration gate. No objective can be completed by a client claim or stale/foreign event.

## Defer / anti-goals

No general quest language, branching campaign, arbitrary designer scripting, map travel or persistent world quest state.
