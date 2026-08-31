# Day 54 — Captain Boss and Extraction

Status: Planned; not implemented by this planning job.  
Depends on: Days 45–46 counterplay, Day 50 pacing, Day 51 recovery, Day 53 settlement.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

End each mission with a readable skill test that uses the mechanics players have learned.

## Exact change surfaces

- New: `Source/Aura/Public/Gameplay/AuraBossPhaseComponent.h` and implementation; `Content/Config/GameplayBossDefinitions.json`; three boss attack XML definitions under `Content/AbilityDefinitions/`.
- Extend enemy archetype component, attack timeline, encounter leases, mission definitions, registered extraction target and `GameplayArenaLayouts.json` with a validated emergency terminal inside each arrangement's boss arena.
- Reuse existing enemy mesh/animation/VFX assets after content validation; add only clearly identified missing cue assets, native boss HUD and debrief evidence.

## Data and authority contract

One captain with sweep, line charge and interruptible reinforcement channel. Phase2 begins once at <=50% max health; cue/lock/recovery contracts do not shorten below Day44 minima. Max four adds within overall live cap, summoned under encounter leases. Boss never becomes permanently immune and no attack requires a specific role. A surveyed emergency terminal remains reachable inside the boss arena with finale gates closed, in both phases; it uses the existing supply owner, 3s interruptible channel and shared 30s per-member cooldown, with no safe-zone immunity. After accepted boss death, extraction lasts 30s within the 20-minute run deadline. At least one alive connected participant must channel at the extraction pad for 3s; success settles eligible members, including dead teammates, exactly once.

## Numbered implementation steps

1. Build a deterministic phase reducer over accepted health thresholds/actions; duplicate damage callbacks cannot retrigger phase-entry effects or spawn extra adds.
2. Implement sweep and line charge through the attack timeline/collision boundary. Expose safe arcs/lanes and a 1.2s recovery opportunity; no unavoidable room-wide hit.
3. Implement a 2s reinforcement channel that either interrupts cleanly or admits up to four legal adds; failed add placement does not deadlock the boss or manufacture kill credit.
4. Integrate boss gating after the second cell for every template, without erasing the earlier primary reward flag. Validate the arena's emergency terminal with gates closed and all phase/hazard geometry active; neither role may need ammo/mana to reach or activate it. Switching from a cell terminal to the boss terminal does not reset its existing personal cooldown. Boss death cancels pending attack/channel/spawn callbacks.
5. Register extraction pad and HUD countdown; define ordering when boss death, player wipe and timeout share a frame: process accepted combat deaths, then wipe, then deadline, then extraction completion; terminal state wins once.
6. Play each pattern with both roles and same-role pairs, including zero-resource emergency recovery and one participant dead; capture failure causes and iteration notes.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay54Tests.cpp`; namespace `Aura.Gameplay.Day54`.

- PhaseCrossingOnce — burst crossing 50% enters Phase2 one time.
- BossDeathCancelsPendingAttack — no postmortem sweep/charge/add spawn.
- BossAddsRespectLeases — four-add and total-cap limits hold with spawn failures.
- EveryPatternHasCounter — timing/collision fixture demonstrates safe response for either role.
- BossArenaResourceRecovery — zero-resource Aura/BungeeMan can reach and finish the existing emergency interaction with finale gates closed in both phases; no cooldown reset, permanent inventory mutation or immunity shortcut.
- ExtractionRaceOneSettlement — wipe/deadline/extraction race yields one ordered terminal result.
- DeadTeammateEligiblePayout — successful survivor extraction uses Day53 eligibility without resurrecting dead members.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 54 -Stage Fast -RunId d54-fast
./RunGameplayExpansion.ps1 -Day 54 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d54-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

CaptainFinale: each pattern alone, phase crossing under simultaneous shots, blocked charge, failed add anchor, death at extraction deadline, zero-resource terminal access with gates closed in both layouts/phases, and transition from a recently used cell terminal; all nine lanes and network-loss overlay.

## Failure and timeout semantics

Each action deadline plus one-second guard cancels a stuck action; three consecutive action initialization failures cause technical abort. Extraction expiry is ordinary failed run; no bonus. Missing visual tells block acceptance even if server damage tests pass. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-54.json, boss-phase-action-trace.json, extraction-race-table.json, hit/counter clips, solo/same-role completion and debrief results. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All mission templates reach and complete the finale; each attack has demonstrated counterplay, and both roles can recover from zero resources inside the closed arena. Death/phase/extraction races preserve one terminal payout.

## Defer / anti-goals

No second boss, cinematic campaign, new animation rig, unavoidable enrage timer, damage sponge tuning or boss-specific damage pipeline.
