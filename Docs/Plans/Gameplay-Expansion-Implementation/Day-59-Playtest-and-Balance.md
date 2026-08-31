# Day 59 — Playtest and Balance

Status: Planned; not implemented by this planning job.  
Depends on: Day 58 technically stable candidate and Days 49/55 exploratory checkpoints. Final first-use acceptance requires six participants new to this gameplay experience after the tested content is frozen; their availability is a human-evidence prerequisite, not a prohibition on implementing telemetry, tuning tools or fixing known technical defects.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Use observed decisions, confusion and voluntary replay to decide whether the slice is enjoyable and what to change.

## Exact change surfaces

- Tuning only where evidence supports it: gameplay combat/enemy/augment/pacing/reward/mission definitions, cue/HUD text; retain authority and persistence APIs unless a confirmed defect demands repair.
- New: `Docs/Reports/Gameplay-Playtest-Protocol-<run-id>.md`, `gameplay-playtest.json`, anonymized session/choice/event CSVs and `balance-change-log.md`.
- Extend diagnostic events at accepted ability/choice/objective/recovery/settlement boundaries; local consent-based capture, no external analytics service.
- Extend `Scripts/gameplay_expansion.py` and `Scripts/test_gameplay_expansion.py` for consented session import, cohort/denominator aggregation and artifact binding; these artifact checks are Python tooling, not a new native gameplay behavior.

## Data and authority contract

Recruitment/contact requires user authorization; do not message people automatically. Keep exploratory/repeat participants separate from the final first-use cohort. After freezing the final gameplay content, recruit at least six participants new to this gameplay experience, not merely new to a binary version. Each has a first role-use session for both roles (12 player-sessions), followed by an A/B pair of scheduled comparison sessions (12 more player-sessions, six within-person comparisons). For final acceptance, an A/B build factor means a run loadout or mission variant already supported by the same frozen binary/content; comparisons between different binaries or edited content are exploratory only. Only each participant's initial game session supplies first-objective/first-choice clarity, initial enjoyment and voluntary-replay acceptance; the second role-use session is not a second independent first-use observation. Counterbalance first-role/variant order, record paired game sessions separately from player-session counts, and vary solo/same-role/mixed composition. Use anonymous participant IDs; video optional. Report exact counts and no inferential retention claims.

At the end of each participant's initial run, collect enjoyment then present equally prominent Stop and Play Again choices before requesting or directing any scheduled comparison session. Record the first voluntary choice once; an instructed role-use/A/B replay never counts toward voluntary replay, and completion of scheduled sessions is not conditioned on choosing Play Again. Participation remains optional throughout. Report withdrawal/missing sessions openly without replacing an unfavorable first decision or excluding it from the original cohort denominator. Initial-session clarity requires at least five of six to state the objective within 60s and complete their first augment and resupply interaction without developer coaching; initial enjoyment median must be at least 4/5 and at least four of six must voluntarily choose another run. Missing required observations are BLOCKED, not successful by omission.

Any gameplay/content change after final first-use testing invalidates those acceptance observations; the already exposed cohort becomes exploratory/repeat evidence and cannot be relabeled fresh. Finish iteration with exploratory participants before freezing another candidate, then use a fresh uncoached first-use cohort. Different-build observations remain useful for hypotheses but never satisfy exact final-build gates. Changes limited to an evidence report still obey the shared source/package/content binding rules and cannot rewrite recorded observations.

## Numbered implementation steps

1. Use Days 49/55 and exploratory/repeat participants to resolve known issues before final cohort recruitment. Freeze the candidate hashes, cohort eligibility and protocol before sessions: explain controls only, no coaching objective/augment/resupply decisions, equally prominent Stop and Play Again choices, consent and optional video. Do not promise that a scheduled replay will count as a voluntary choice.
2. Collect initial-session first-objective understanding within60s, first action, deaths with cause explanation, choice time, self-combo use, first augment/resupply use, primary completion, elapsed time and an enjoyment1–5 response. Then record the initial voluntary Stop/Play Again decision before scheduling or instructing comparisons. Keep per-participant acceptance rows distinct from role-use and paired-game rows.
3. After the voluntary decision is recorded, run the consented second-role and A/B comparison sessions with one deliberately changed run-loadout/mission factor at a time inside the same frozen binary/content; keep other context/seed fixed where needed. Binary/content changes belong to separate exploratory comparisons and invalidate final first-use evidence. Ask each participant what changed and why they chose again or stopped. A voluntary extra run may also satisfy a predeclared comparison row only if its exact fixture matches; never double-count the player-session or substitute scheduled compliance for voluntary choice.
4. Inspect augment selection only across comparable eligible offers with at least ten opportunities; >70% pick rate flags dominance for investigation, not automatic nerf. Look for forced role dependence, no-ammo stalls, tedious escort and unreadable deaths.
5. Prioritize at most three high-impact issues per iteration, change one associated tuning surface per comparison, and log hypothesis and resulting scope/content hash. Fix bugs through their owner tests; repeat iterations until required gates are met, rather than treating three fixes as a completion limit.
6. Retest affected behavior with exploratory participants, freeze the final tuned build and collect final human gates with a fresh first-use cohort if gameplay/content changed. Retain rejected-build observations as iteration evidence; do not recruit replacement participants to erase negative results within an unchanged cohort. If insufficient participants/sessions/time, report NEEDS_ITERATION/BLOCKED and leave the corresponding final human gate open.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay59Tests.cpp`; namespace `Aura.Gameplay.Day59`. Native cases exercise actual accepted gameplay events and privacy; they cannot manufacture human observations.

- TelemetryMatchesAuthority — rejected requests cannot inflate combo/objective/reward observations.
- SessionPrivacy — no raw identities, voice or video without consent enters artifacts.
- BalanceRegression — all modified definitions pass native tests and relevant packaged lanes.

Python tooling file: `Scripts/test_gameplay_expansion.py --day 59`; use temporary consent/session manifests and real file hashes to exercise `Scripts/gameplay_expansion.py` aggregation.

- ComparableOfferDenominator — selection rates retain eligibility/offer IDs and exact counts.
- PlaytestArtifactBinding — different build/content or duplicate participant/session records fail aggregation.
- FirstUseCohortIsFresh — prior exposure to this gameplay experience or invalidated content cannot satisfy final first-use rows; second-role use is not counted as a new participant.
- VoluntaryReplayBeforeScheduledTasks — only the initial unconstrained decision counts; scheduled replays, duplicate choices and exclusion of unfavorable/withdrawn participant rows fail aggregation.
- UsabilityThresholds — initial-session rows show five/six understand the objective within60s and complete both first augment/resupply interactions unaided; initial enjoyment median>=4; four/six voluntarily choose replay before scheduled tasks.

Planned runner commands (implement the adapter before use):

```powershell
python Scripts/test_gameplay_expansion.py --day 59 --output Saved/Reports/GameplayExpansion/day59-tooling.json
./RunGameplayExpansion.ps1 -Day 59 -Stage Fast -RunId d59-fast
./RunGameplayExpansion.ps1 -Day 59 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d59-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes Python aggregation tests, actual Day 59 native result discovery and content validation. Packaged requires actual game processes and rendered evidence; neither tooling nor native telemetry checks alone satisfy human acceptance. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

Final fresh six-person protocol above, 24 distinct player-sessions minimum; exploratory/repeat sessions are retained separately. Pair co-op runs explicitly to avoid counting one shared run as two independent run observations. Include first-role use for both roles, same-role cooperation, a failure/recovery run and all three templates across the cohort. The 24-session count and the six initial voluntary decisions are separate denominators; incomplete schedules remain missing evidence even when an initial decision was favorable.

## Failure and timeout semantics

A session stops at20-minute run cap or participant request; withdrawals are reported, not fabricated as failures/successes, and unfavorable initial decisions remain in the cohort ledger. Missing fresh cohort, observations or final retests is BLOCKED. Enjoyment/clarity below threshold is NEEDS_ITERATION even when every automation test passes. These states block final usability acceptance while allowing implementation and local technical validation to continue. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-59.json, gameplay-playtest.json, consent/recording-status ledger without personal data, exploratory-versus-final cohort/first-exposure ledger, initial voluntary-decision timestamps before scheduled tasks, anonymized raw observations, positive/negative feedback excerpts, balance-change-log.md and final-build retest table. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Meet roadmap human thresholds with honest initial-participant and session denominators, both roles usable and no unresolved dominant/mandatory choice. Final first-use evidence comes from an unexposed cohort on the final content; voluntary replay decisions precede scheduled comparison tasks and cannot be inferred from their completion.

## Defer / anti-goals

No market-retention claims, coercive engagement tactics, telemetry collection without consent, large survey platform or endless content growth in place of fixing weak play.
