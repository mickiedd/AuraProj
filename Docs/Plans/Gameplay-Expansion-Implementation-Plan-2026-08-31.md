# Gameplay Expansion — Days 41–60

Date: 2026-08-31  
Status: **Deep contract review complete; Day 41 tooling and baseline-warning remediation validated; runtime entry BLOCKED**  
Scope: twenty logical development gates following the Days 21–40 Playable Candidate

## Mission

Make AuraProj worth replaying through decisions, counterplay, cooperation, and changing encounter combinations. The proposed product is a **12–18 minute, one- or two-player cooperative expedition** using Aura and BungeeMan. The city and its civilians become a reason to undertake missions; combat, preparation, rescue, and rewards become one connected loop.

This is a bounded design proposal for the next milestone, not a claim that adding a feature count guarantees fun. Days 41, 49, 55, and 59 contain human play checkpoints. If the basic fighting and objectives are not enjoyable, revise them before increasing content quantity.

Read together:

- [Game analysis and design rationale](Gameplay-Expansion-Game-Analysis-2026-08-31.md).
- [Architecture, authority, and persistence contracts](Gameplay-Expansion-Architecture-2026-08-31.md).
- [Shared execution, topology, command, and evidence contract](Gameplay-Expansion-Implementation/Execution-Contract.md).
- [Initial planning audit](../Reports/Gameplay-Expansion-Plan-Review-2026-08-31.md), [deep review](../Reports/Gameplay-Expansion-Deep-Review-2026-08-31.md), and [current blocker remediation](../Reports/Gameplay-Baseline-Blocker-Remediation-2026-08-31.md).
- Previous milestone: [Days 21–40](Playable-Candidate-Implementation-Plan-2026-08-29.md).

## Starting truth and entry gate

Source inspected at `3df38a9822a9cc175df167c273d5775d2193be32`, Unreal Engine 5.5. Source/configuration inspection and checked-in reports informed this plan; the game was not launched or played during planning. Recheck HEAD and working-tree changes on Day 41.

The [August 30 review](../Reports/Change-Archive/2026-08-30-playable-candidate-days-21-40-review-fixes.md) records successful local/native checks but explicitly blocks packaged multiplayer, soak, and production-provider sign-off. Later [reload verification](../Reports/Change-Archive/2026-08-31-bungeeman-reload-hud-runtime.md) demonstrates a narrower live firearm/HUD path, not the full Day 40 matrix. Therefore **do not assume Day 40 is complete**.

Day 41 may produce a Draft scope, tested evidence tooling and missing-evidence diagnostics immediately; Frozen requires measured and verified baseline evidence. Runtime implementation of Day 42 onward requires an explicit Day 40 local/LAN PASS artifact, or a separately recorded prerequisite remediation package that produces that PASS. The old contract's required Shipping identity and second-developer operations evidence must still pass; only requirements explicitly classified as separate external scope may remain independently BLOCKED. Do not weaken the old finalizer or relabel a static test as packaged evidence to start this arc.

## Player journey and minimum content

`safe hub → choose mission + optional modifier → enter combat cell → read threats / combine abilities → choose one of three augments → complete objective / optional rescue → resupply or accept extra risk → captain boss → extract → durable reward + debrief → try another combination`

| Element | Required Day 60 inventory | Why it earns its scope |
| --- | --- | --- |
| Roles | Aura and BungeeMan; solo, same-role pairs, and mixed pair | Distinct range/control/resource decisions without requiring a missing teammate class. |
| Combat verbs | Existing attacks/spells, one shared evade, Focus Shot and Shock Trap for BungeeMan | A deliberate defensive response and additional gun-role decisions. FireGun remains semi-auto. |
| Enemy roster | Raider, Lancer, Bulwark, Disruptor | Approach, sidestep, flank, and interrupt are different answers; recolored HP variants do not count. |
| Mission templates | Assault, Rescue Relay, Sabotage | Clear threats, protect movement, or hold an exposed interaction. |
| Space | One StartupMap mission overlay, two validated arena arrangements, safe hub | Reuse owned content; no new biome production, procedural geometry, or map travel dependency. |
| Build choices | Eight augments: four shared, two per role; two choices per run, one of three offers each | Change how abilities are used; no rarity treadmill. |
| World interactions | Rescue beacon, escort/shelter, sabotage relay, hazard vent, cache, resupply station | Reuse registered interaction objectives rather than special-case UI mutations. |
| Pacing | Build, Pressure, Recovery; bounded spawn budget | Space to recognize threats, recover, and make choices. |
| Cooperation | Pings, shared mission progress, teammate rescue, equal eligible completion rewards | Teamwork without last-hit currency competition. |
| Replay | Three templates, two arrangements, two opt-in mutators plus Standard; fixed reproduction seeds | Controlled recombination with explicit compatibility checks, not a claim that every product combination is unique content. |
| Finale and progression | One three-pattern captain boss; three cosmetic mastery badges | A skill check and visible record of success without mandatory stat grinding. |

All three templates use two combat cells and the same reusable captain finale. Assault's primary is clearing the second cell; Rescue Relay's primary is delivering two designated civilians; Sabotage's primary is disabling two relays. The optional cache challenge is a separate branch and can always be skipped. Cell boundaries supply augment choices; the second boundary also supplies resupply. Day 54 adds the boss to the earlier objective-only slice.

The run limit is 20 minutes, including combat and choice windows; the target observed completion is 12–18 minutes. The final extraction window is 30 seconds within that limit. Host selection is confirmed by both connected players in preparation; no approval response after 30 seconds cancels preparation without cost. Solo can start immediately. No new late entrant joins an active run; an existing participant can reconnect to the same server within 60 seconds.

## Delivery sequence

| Day | Player-facing outcome | Main architecture seam | Contract |
| --- | --- | --- | --- |
| 41 | A measurable definition of the game we intend to make | Scope, baseline, prerequisite evidence | [Day 41](Gameplay-Expansion-Implementation/Day-41-Gameplay-Contract-and-Baseline.md) |
| 42 | Start, finish, fail, and replay one simple mission | Run state and transient loadout isolation | [Day 42](Gameplay-Expansion-Implementation/Day-42-Mission-Vertical-Slice.md) |
| 43 | Enemies spawn once and pursue one coherent intention | Encounter leases and exclusive AI ownership | [Day 43](Gameplay-Expansion-Implementation/Day-43-Encounter-and-AI-Ownership.md) |
| 44 | Read an attack and evade it deliberately | Telegraph timeline and validated movement | [Day 44](Gameplay-Expansion-Implementation/Day-44-Telegraphs-and-Evasion.md) |
| 45 | Four enemy types demand different tactics | Data-driven enemy actions and counters | [Day 45](Gameplay-Expansion-Implementation/Day-45-Enemy-Archetypes.md) |
| 46 | Both roles have useful combos and team contributions | Existing ability graphs and bounded statuses | [Day 46](Gameplay-Expansion-Implementation/Day-46-Role-Kits-and-Combos.md) |
| 47 | Two build choices change each run | Server offers and reversible effect handles | [Day 47](Gameplay-Expansion-Implementation/Day-47-Run-Augments.md) |
| 48 | Three objective styles vary the mission | Objective reducers and validated interactions | [Day 48](Gameplay-Expansion-Implementation/Day-48-Objective-Templates.md) |
| 49 | Civilians and routes matter to play | Escort ownership and bounded world consequences | [Day 49](Gameplay-Expansion-Implementation/Day-49-Civilian-Rescue-and-Space.md) |
| 50 | Combat alternates pressure and relief | Encounter budget/pacing scheduler | [Day 50](Gameplay-Expansion-Implementation/Day-50-Encounter-Pacing.md) |
| 51 | A teammate can recover you without infinite lives | Mission recovery adapter over existing death policy | [Day 51](Gameplay-Expansion-Implementation/Day-51-Cooperative-Recovery.md) |
| 52 | Supplies create a useful preparation decision | Run inventory and resupply receipts | [Day 52](Gameplay-Expansion-Implementation/Day-52-Supplies-and-Risk.md) |
| 53 | Completion produces fair, durable rewards | Settlement and profile migration | [Day 53](Gameplay-Expansion-Implementation/Day-53-Rewards-and-Mastery.md) |
| 54 | A boss tests the learned mechanics | Phase transitions and counter windows | [Day 54](Gameplay-Expansion-Implementation/Day-54-Captain-Boss.md) |
| 55 | Replays vary while remaining reproducible | Seeded assembly and compatibility rules | [Day 55](Gameplay-Expansion-Implementation/Day-55-Replay-and-Mutators.md) |
| 56 | Players understand choices, threats, and outcomes | Replicated presentation and contextual guidance | [Day 56](Gameplay-Expansion-Implementation/Day-56-Gameplay-HUD-and-Onboarding.md) |
| 57 | Designers can add a valid encounter without new C++ | Registries, fixtures, and staged validation | [Day 57](Gameplay-Expansion-Implementation/Day-57-Content-Authoring-Gate.md) |
| 58 | Busy encounters stay responsive | Profile-driven AI/network/effect budgets | [Day 58](Gameplay-Expansion-Implementation/Day-58-Gameplay-Performance.md) |
| 59 | Observed play drives balancing decisions | Playtest telemetry and reproducible tuning | [Day 59](Gameplay-Expansion-Implementation/Day-59-Playtest-and-Balance.md) |
| 60 | Publish an honest gameplay-candidate decision | Regression, recovery, soak, and evidence binding | [Day 60](Gameplay-Expansion-Implementation/Day-60-Gameplay-Candidate-Signoff.md) |

## Capacity and sequencing

These are **logical implementation contracts, not a promise of twenty calendar days**. Plan roughly 30–50 engineering days plus overlapping content/design and playtest support, then re-estimate after Day 41; Days 42, 43, 46, 49, and 53 have the greatest integration risk. A day can span multiple working days. One implementer follows the dependency order; independent content preparation can overlap only after its schema is frozen. Do not count unbuilt animations, testers, or package lanes as available capacity.

Three playable checkpoints prevent an architecture-only month: Day 42 objective-only loop, Day 49 roles/builds/three objectives, Day 55 complete replayable slice. Every feature includes its minimal HUD and network evidence when introduced; Day 56 consolidates usability rather than hiding feedback work until the end.

If capacity slips, remove the second arena arrangement, one opt-in mutator, or cosmetic badge art first through an explicit scope revision. Keep three mission verbs, four distinct counters, two meaningful build decisions, solo/same-role viability, reliable recovery/settlement, and human testing. No silent downgrading of a mandatory row to optional.

## Final acceptance

- All mandatory technical lanes in the shared execution contract pass on the exact final source/package/content hashes; no P0/P1, duplication, persistence leakage, authority/privacy violation, unbounded callback, or unexplained timeout.
- Human play evidence includes at least six participants new to this gameplay experience on the final tuned build, twelve first-role player-sessions and six within-person replay comparisons (twelve additional player-sessions), with the voluntary Stop/Again decision recorded before scheduled comparison instructions under the Day 59 protocol. Exploratory/repeat testers cannot supply final first-use evidence; changes after final first-use testing require a fresh first-use cohort on the final build. Missing participants means usability evidence is BLOCKED, not inferred from automation.
- At least five of six participants can state the current objective within 60 seconds and complete their first augment/resupply interaction without developer coaching. Median enjoyment is at least 4/5 and at least four of six voluntarily choose another run when stopping is an equally easy option. These are small-sample iteration gates, not evidence of market retention.
- Each enemy counter and both role playstyles are observed in live play; no mandatory objective requires a particular role. No single augment dominates over 70% of at least ten comparable offer opportunities without a documented rebalance and retest.
- Performance meets the Day 58 measured-hardware budgets; all three templates complete in the target median range without extending the 20-minute cap to conceal stalls.
- Local technical, usability, performance, and external-provider results are separate. A technically stable game with failed enjoyment gates is **NEEDS ITERATION**, not a completed gameplay milestone.

## Boundaries

No PvP, new playable class, open-world simulation, reputation/crime economy, crafting, randomized equipment affixes, durability, trading, monetization, matchmaking, four-player support, host migration, cloud persistence, full procedural levels, mass-entity conversion, new network backend, or wholesale GAS/BehaviorU rewrite. Reuse current assets and implement only new animation/VFX needed to communicate the specified counters. The old candidate profile, fixture reward, map identity, and archived evidence stay intact under their own explicit profile.
