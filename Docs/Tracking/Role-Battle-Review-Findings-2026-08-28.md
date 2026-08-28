# Role/Battle Review Findings — 2026-08-28

Scope: full review of the 20 daily plans in `Docs/Plans/Role-Battle-Implementation/` plus the
implementation index, followed by a full test run (native automation suite + all smoke runners)
at revision `90c02cd` (clean worktree), fresh `AuraEditor Win64 Development` build.

Purpose: capture every finding with its evidence and root cause so a later session can fix
without re-diagnosing. Evidence reports/logs from this run live in `Saved/Reports/` and
`Saved/Logs/` (gitignored); the pre-review evidence backup is `Saved/Backup-preReview-2026-08-28/`.

---

## Part 1 — Plan documentation issues (plans vs. repo)

### 1.1 `Content/Maps/RoleBattleCivilianTest.umap` does not exist (Days 08–14)
- Day 08 declares it a new file and says the smoke "must use" it; Days 09–14 list it under files-to-modify.
- Reality: it was implemented as an alias — `Content/Config/LevelConfig.json` (lines 5–7) maps level ID
  `RoleBattleCivilianTest` → `/Game/Maps/StartupMap`, and runners launch StartupMap
  (`RunRoleBattleDays1012NetworkSmoke.ps1` line 63). A runtime fixture volume is registered in C++ when
  the map ID matches (`Source/Aura/Private/Game/AuraGameModeBase.cpp` ~line 775).
- **Fix**: update the day manifests to describe the alias approach, or author the real map. Deviation was
  never recorded, violating the index's own "update the day manifest" / "record deviations" rules.

### 1.2 Declared Civilian content assets were never created (Days 08–10)
- Day 08: `BP_AuraCivilian.uasset`, `WBP_CivilianDebugHealthBar.uasset` — absent. Population table spawns
  the C++ class directly (`"/Script/Aura.AuraCivilian"`, `Content/Config/PopulationSpawnTable.json` line 9).
- Day 09: `BP_AuraCivilianSpawnVolume.uasset` — absent (`Content/Blueprints/World/` doesn't exist).
- Day 10: `BB_Civilian.uasset`, `BT_Civilian.uasset`, three marker BPs — absent (`Content/AI/` doesn't exist).
  The behavior tree/blackboard are runtime-constructed in C++
  (`Source/Aura/Private/AI/AuraCivilianAIController.cpp` lines 54–55, objects named `BT_Civilian_Runtime`).
- Day 10's test-map marker requirements ("one work marker, one observation marker, two shelters, one
  intentionally unreachable") can no longer be traced to any checked-in asset — they must be runtime fixtures.
- **Fix**: reconcile manifests with what shipped (same rule violation as 1.1).

### 1.3 Day 07 "Exact execution commands" block cannot run as written
- `Docs/Plans/Role-Battle-Implementation/Day-07-Combat-Roles.md` (commands block) invokes
  `RunRoleBattleDay4DamageSmoke.ps1`, `RunRoleBattleDay5ConfigSmoke.ps1`, `RunRoleBattleDay6NetworkSmoke.ps1`
  **without `-Mode`**. Day 5 and Day 6 runners declare `[Parameter(Mandatory = $true)]` on `-Mode`
  (param blocks at top of those files), so the documented sequence prompts interactively or fails unattended.
- Day 4's runner tolerates an empty `-Mode` (explicit `IsNullOrWhiteSpace` handling).
- **Fix**: add explicit `-Mode Listen` / `-Mode Dedicated` calls (as every other day's command block does).

### 1.4 Day 07 completion evidence missing / Day 01 stays formally "conditional"
- Day 07's gate requires `Saved/RoleBattle/Day7/Evidence.md`, `AuraRendered.png`, `BungeeManRendered.png`,
  `PackageManifest.txt` — `Saved/RoleBattle/` is empty (also on this machine, not just in git).
- The Day 07 archive record cites `Saved/Reports/Day07-Packaged.json` (`Passed: true`) — that file does not
  exist; only `Day7-Listen.json` / `Day7-Dedicated.json` exist (also deviates from the plan's `Day07-*` naming).
- Day 01's file still says rendered confirmation is open and "Day 07 must close the rendered follow-up
  before its gate can pass". As checked in, the two plans contradict.

### 1.5 No retained evidence is in the repository at all
- `/Saved/` is gitignored, so every day's "retained" logs/JSON reports live only on one machine.
  The DayPlan-Archive inclusion rule ("the repository contains implementation evidence") is not literally
  satisfiable from a fresh clone.
- **Fix (suggestion)**: copy per-day report JSONs (small) into a tracked `Docs/Reports/DayEvidence/` folder.

### 1.6 Stale test-file manifests (Days 07–15)
- Day 07/08/09/13 plans say tests go into `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`; they actually
  live in `AuraRoleBattleDays789Tests.cpp`, `AuraRoleBattleDays1012Tests.cpp`, `AuraRoleBattleDays1315Tests.cpp`.
- Day 15 declares new file `AuraEconomyConfigTests.cpp` — does not exist; its 7 tests are in `Days1315Tests`.
- Test *names* all match the plans; counts reconcile (Day13 10 + Day14 11 + Day15 7 = the index's 28/28;
  Day 03's "fourteen Day 1–3 tests" is correct: 3 Day1 + 3 Day2 + 8 Day3).

### 1.7 Day 07 packaged XML assertion is under-inclusive and disagrees with Day 20
- `+DirectoriesToAlwaysStageAsUFS=(Path="AbilityDefinitions")` stages the whole folder — which contains
  **nine** XMLs (player: FireBolt/FireBlast/ArcaneShards/Electrocute/FireGun; enemy: EnemyHitReact/
  EnemyMeleeAttack/EnemyFireBolt/EnemyRangedAttack). Day 07 asserts "all five XML files" in the manifest,
  leaving the four enemy XMLs unverified in packaged builds. Day 20 Phase B.8 asserts "every shipped …
  required ability-definition XML" (broader). **Fix**: assert the full directory in both gates.

### 1.8 BungeeMan `UAuraFireGun`/`GA_FireGun` ambiguity slid from Day 07 to Day 20
- Day 07's checkpoint says "resolve and document whether the parallel `UAuraFireGun` helper remains unused
  or is removed/reused". Day 07 was archived complete while
  `Source/Aura/Public/AbilitySystem/Abilities/AuraFireGun.h/.cpp` still exists, and
  `Docs/Tracking/Role-Battle-Issue-Dispositions.md` item 10 carries it as an active risk deferred to
  Day 20 Phase A 4a. Tracked, but the Day 07 plan/checkpoint was never updated to say it moved.

### 1.9 DayPlan-Archive README self-contradiction and missing Day 13–15 records
- Line 11 says "Days 10–20 are intentionally absent" but the table includes Days 10–12 (line 30 corrects it).
- Days 13–15 (implemented per index, 2026-08-25) have no archive records/mind maps yet; the index's
  "Finished milestone mind maps are archived" link implies coverage that isn't there.

### 1.10 Minor bookkeeping
- Execution-status paragraphs placed inconsistently (top for Days 02–04, bottom for 05/13/14/15);
  Days 06 and 07 have no in-file status paragraph at all.
- Day 03's automation command uses `-ExecCmds="..."` (inner quotes) unlike every other day (works either way).

---

## Part 2 — Test run findings (2026-08-28, revision 90c02cd)

### 2.1 Native automation suite (`Automation RunTests Aura`)
- **Run 1 (stale Aug-27 binary)**: 154 passed / 4 failed — all four failures were **stale-binary artifacts**:
  `Aura.UI.WebSkillPanel.HUDContract`, `Aura.UI.WebSkillPanel.RuntimeMountAndFallback`,
  `AuraWebUI.Plugin.ContentContract` (old compiled test contracts still referenced the removed single-page
  `hud.html`; the three-panel migration replaced it with `hud-left-top/right-top/bottom.html`), plus
  `Day4.ProducerInventory` (see below). The first `build_test.bat` invocation had silently not run — see 2.4.
- **Run 2 (fresh binary)**: **161 passed / 1 failed**.
- **Genuine failure — `Aura.RoleBattle.Day4.ProducerInventory`**:
  `Expected 'Discovered damage-application site Source/Aura/Private/Tests/AuraGameplayDefinitionTests.cpp is
  inventoried' to be true` at `AuraRoleBattleTests.cpp(992)`.
  Cause: `AuraGameplayDefinitionTests.cpp` line 48 contains the string `"CauseDamage"` (a node-class-name
  comparison, not a real damage application), and that file is missing from the
  `DamageSymbolNonProducerFiles` allowlist (`AuraRoleBattleTests.cpp` line 821).
  **Fix**: add `Source/Aura/Private/Tests/AuraGameplayDefinitionTests.cpp` to `DamageSymbolNonProducerFiles`.

### 2.2 Day 1 runtime smoke — PASS (exit 0, ~30 s).

### 2.3 AuraAbilityGraph smoke — tests PASS, process HANGS on exit
- Log proves `[SmokeTest] Result: 26 passed, 0 failed` and "Requesting editor exit..."; shutdown completes
  in the log, but the `UnrealEditor` process never terminates (hung >60 min; log silent after
  `LogDerivedDataCache: Maintenance finished`). Had to `Stop-Process -Force` it.
- `RunSmokeTest.bat` has no timeout, so it blocks any sequential suite indefinitely.
- **Fix**: investigate exit-path hang (suspects: AuraAutoTest/AuraWebUI WebSocket/worker thread not
  exiting) and wrap `RunSmokeTest.bat` in a bounded-timeout invocation.

### 2.4 Environment/tooling gotchas discovered while running (for the next session)
- `cmd /c build_test.bat` fails on this machine ("not recognized") — cmd here does not resolve batch files
  from the current directory; always use `.\build_test.bat`.
- A PowerShell runner invoked with `& .\Runner.ps1` whose script ends in `exit N` **terminates the calling
  suite process**. Spawn each runner isolated: `cmd /c "powershell -NoProfile -ExecutionPolicy Bypass -File .\Runner.ps1 -Mode Listen"`.
- PS 5.1 `Start-Process -ArgumentList` does not quote multi-word arguments: `-TestExit=Automation Test Queue Empty`
  arrived truncated to `-TestExit=Automation`, making the engine exit on first tick
  (log: `**** TestExit: Automation ****`). Use the `&` call operator with single-quoted args and an
  **absolute** `.uproject` path.
- PS 5.1 `*>`/`2>&1` on native commands: `*>` writes UTF-16 logs (read with `tr -d '\0'`), and `2>&1` wraps
  stderr in ErrorRecords. Redirect inside `cmd /c` instead.
- Historical logs confirm the same invocation pattern worked on Aug 14 — the gotchas are tooling-side, not project regressions.

### 2.5 Network smoke sweep results (complete — 30 of 30 invocations ran)

Legend: PASS = exit 0; FAIL = exit nonzero. "BAK" = pre-review retained evidence
(`Saved/Backup-preReview-2026-08-28/Reports/`, mostly Aug 26).

| Day | Listen | Dedicated | Triage |
| --- | --- | --- | --- |
| 2 | FAIL | FAIL | ¹ ² pre-existing + window race |
| 3 | FAIL | FAIL | ² pre-existing (identical failure text in BAK: state-transition assertions time out) |
| 4 | PASS | PASS | — |
| 5 | FAIL | FAIL | ³ **NEW regression** (BAK passed): "Invalid connection role was not rejected by PreLogin" + missing client-log artifacts |
| 6 | FAIL | FAIL | ² pre-existing (identical in BAK): "Aura did not complete two ledger-safe pawn replacements" |
| 7 | FAIL | FAIL | ⁴ cascade: its runner chains the Day 6 runner as a prerequisite and inherits Day 6's failure; the Day 7 reports themselves record `Passed: true` |
| 8 | PASS | PASS | — |
| 9 | PASS | PASS | — |
| 10 | FAIL | PASS | ³ **NEW regression** (BAK passed): Listen only — "Client 1 did not observe the replicated battle director" |
| 11 | FAIL | PASS | ³ **NEW regression** (BAK passed): Listen only — "Client 2 did not observe the replicated battle director" |
| 12 | PASS | PASS | — |
| 13 | PASS | PASS | — |
| 14 | PASS | PASS | — |
| 15 | PASS | PASS | — |

¹ **Day 2 runner timeout race**: the ~40 s assertion window expires before editor-based server/clients
finish booting; the `Failure` string lists timed-out assertions that are recorded `true` in the same
report's `Assertions` block (values flipped after the timeout snapshot). Dedicated mode proves it:
six "timed out" assertions are all `true` in the final report. The window got tighter vs. BAK
(BAK failed only on `EnemyAcquiredPlayer`).
² **Pre-existing failures** — the retained Aug 26 evidence shows the same failures
(`EnemyAcquiredPlayer=false` for Day 2; identical timeout text for Day 3; identical pawn-replacement
failure for Day 6). The index's "Days 02–15 implemented with baseline evidence" does not hold for
these network gates as of the retained evidence.
³ **Genuine new regressions vs. BAK** — investigate first: Day 5 (PreLogin role rejection — suspect the
recent login/startup-map commits), Day 10/11 (listen-mode director replication observation; dedicated
passes, so likely a listen-specific readiness/replication race or a real replication break).
⁴ **Cascade, not a Day 7 defect** — `RunRoleBattleDay7ListenSmoke.ps1` /
`RunRoleBattleDay7DedicatedSmoke.ps1` invoke `RunRoleBattleDay6NetworkSmoke.ps1` first and exit on its
failure. Fix Day 6 (or decouple the prerequisite) before reading anything into Day 7's exit code.

Machine-readable: `Saved/review_smoke_suite2.log` (per-step exit codes), per-step reports under
`Saved/Reports/Day*-*.json`, per-process logs under `Saved/Logs/Day*-*.log`.
Note for reruns: the sweep's summary CSV was empty due to a `$results` scoping slip in
`RunReviewSmokeSuite2.ps1` (needs `$script:results`); the per-step `END ... EXIT=N` lines in the log are
authoritative.

---

## Part 3 — Suggested fix order for the follow-up session

1. One-liner: add `Source/Aura/Private/Tests/AuraGameplayDefinitionTests.cpp` to `DamageSymbolNonProducerFiles`
   (fixes `Aura.RoleBattle.Day4.ProducerInventory`, the native suite's only failure).
2. **Day 5 PreLogin regression** (new vs. Aug 26 evidence): "Invalid connection role was not rejected by
   PreLogin" in both modes, plus missing client-log artifacts. Prime suspects: the recent
   login/startup-map/WebUI commits (92ea3bc, b743eb6, f996a5c, 90c02cd).
3. **Day 10/11 listen-only director-observation regression** (new vs. Aug 26 evidence; dedicated passes):
   "Client N did not observe the replicated battle director" — decide readiness-race vs. real replication break.
4. Day 6 pawn-replacement failure (pre-existing, both modes) — also unblocks Day 7, whose runner chains
   Day 6 as a prerequisite (2.5 note ⁴). Then retangle the Day 2/3 timeout-window failures: raise the
   runners' assertion windows or poll until child-process exit, and separately triage the pre-existing
   `EnemyAcquiredPlayer=false`.
5. Investigate the editor exit hang after the AuraAbilityGraph smoke; add a bounded timeout to `RunSmokeTest.bat`.
6. Documentation reconciliation (Part 1 items): day-manifest updates for the map alias / runtime BT /
   C++ class spawns / test file locations; fix the Day 07 command block (`-Mode`); widen the packaged XML
   assertion to the full directory; fix the archive README; record the `UAuraFireGun` disposition decision;
   update Day 01/Day 07 status paragraphs.
7. Consider checking per-day evidence summaries into a tracked directory (1.5).

## Part 4 — Notes on the Doc review (context for Part 1)

The plans' internal logic is sound: prerequisites, authority boundaries, exactly-once death/economy
contracts, and the Day 16–20 gates chain cleanly, and the index's test-count claims reconcile with the
checked-in test files. All Part 1 issues are doc/repo drift, not design flaws.
