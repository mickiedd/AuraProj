# All-changes review: Gameplay Expansion Days 42–47A

Date: 2026-09-03. Status: review recorded; source fixes were not applied.

## Request and scope

This record captures the all-changes review requested after the Days 42–47A implementation work. The review covered the current tracked and untracked gameplay-expansion source, configuration, test harnesses, launcher/parser changes, Unreal assets, and dated archive records. The implementation is intentionally inert and fail-closed; these findings are contract and future-wiring risks, not claims of a currently live player regression.

## Findings

### P1 — Reject stale mission death generations

[AuraMissionTypes.cpp:206](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraMissionTypes.cpp:206) accepts any positive Generation and stores it in the duplicate key, but does not compare it with an authority-owned active generation for the current cell. A delayed callback from an older cell generation can therefore increment current-cell progress. Store and validate the active cell or lease generation, and add a stale-generation regression.

### P1 — Make encounter death source-life safe and terminal

[AuraEncounterCoordinator.cpp:229](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraEncounterCoordinator.cpp:229) does not accept SourceLifeGeneration and leaves the live slot unchanged after recording a death. A replaced actor or callback with a new DeathSequence can increment AcceptedDeathCount again. Require source-life identity and atomically mark the slot terminal, or maintain a per-slot terminal death receipt, before counting.

The compatibility CommitSlot, ReleaseSlot, and IsDriverOwner overloads also infer source-life identity from the current lease. Those paths should require the callback's source-life generation before runtime wiring.

### P1 — Include attack instance identity in heavy replay keys

[AuraEncounterCoordinator.cpp:252](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraEncounterCoordinator.cpp:252) keys a heavy request by AttackId but omits the separate authority-issued AttackSequence carried by the attack timeline. The current Day 46 harness passes the definition ID RaiderHeavyStrike as AttackId. Repeating that definition after release can hit the remembered request and be rejected as a duplicate instead of acquiring a new lease. Include AttackSequence or an equivalent per-instance ID and add a release/reacquire regression.

### P1 — Make mission snapshot ordering run-aware

[AuraMissionState.cpp:21](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraMissionState.cpp:21) compares numeric Revision before considering RunId. FAuraMissionRunState resets Revision for a new run, so reusing the always-relevant state actor can reject the first snapshot of a new run whenever the previous run had a higher revision. Compare run identity or epoch first, then apply same-run revision ordering.

### P2 — Enforce the attack callback deadline

[AuraAttackTimelineTypes.cpp:96](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraAttackTimelineTypes.cpp:96) accepts arbitrarily late finite commits. Impact resolution rejects after the damage window, but no state or component timeout transitions a committed attack to Cancelled. A late montage callback can strand the timeline and any lease held by its caller. Enforce definition deadline plus one second and add a late-callback regression.

### P2 — Enforce the evade movement window

[AuraEvadePolicy.cpp:154](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraEvadePolicy.cpp:154) validates travel distance but never bounds AuthorityNow to the accepted request's 0.25-second active interval. An adapter can accept a sweep before the request or long after the contract window. Reject out-of-window sweeps or add explicit timeout settlement.

### P2 — Preserve an open Day 47 boundary choice

[AuraGameplayDay47Harness.cpp:223](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Tests/AuraGameplayDay47Harness.cpp:223) lacks a bChoiceOpen guard. A second boundary can call GenerateOffers, reset the current offers and deadline, and silently replace an unresolved first choice. Enforce the pause or queue rule at the boundary API itself.

### P2 — Validate definition provenance

[AuraGameplayDefinitionRegistry.cpp:133](/Volumes/M2/Works/AuraProj/Source/Aura/Private/Gameplay/AuraGameplayDefinitionRegistry.cpp:133) validates schemaVersion and gameplayProfile but not the shared definitionRevision required by the offline contract. Mixed-revision packages can therefore be accepted. The exposed DefinitionsHash is also reset but never assigned; compute the aggregate hash or remove the getter until it is implemented.

### P2 — Keep the archive index current

[Change-Archive/README.md:5](/Volumes/M2/Works/AuraProj/Docs/Reports/Change-Archive/README.md:5) still names the macOS stub repair as Latest and omits the newer same-day Day 42–47A job records. The index must include the five newer records and point Latest to this review record.

## Validation evidence

- Native UE null-RHI review run: 33 matched tests, 33 succeeded, zero warnings, zero failures, zero not-run tests. Evidence: [Review-AllGameplay-Native index](../../../Saved/Reports/Review-AllGameplay-Native.hi7OId/index.json).
- Offline contract suites passed: Day 42–43 4/4, Day 44–45 11/11, Day 46 4/4, and Day 47A 4/4.
- Targeted native completion-parser regressions passed 10/10, including Windows and macOS queue-empty cases.
- BuildEditor, Python compilation, shell syntax, JSON, SVG/XML, archive-link, and diff checks passed.

The green suites do not cover the missing cases above: stale mission source generation, encounter death with a new sequence or source life, repeated heavy definition after release, new-run snapshot ordering, late attack callback timeout, out-of-window evade settlement, or a second boundary while an offer is open.

## Handoff status

The handoff skill was used and retried. ChatGPT app inspection returned a safety restriction for the Codex app context, and the one permitted app-discovery call reported that the Mac was locked and automatic unlock failed. No consultant response was available. Local fallback review remains authoritative for the project files; no handoff response is treated as permission to modify source.

## Residual gates

The full Days 46–47 runtime consumers, packaged gameplay evidence, anchor survey, dedicated-server proof, and live player gates remain deferred or fail-closed. Git LFS is not installed on the host, so Unreal binary diffs remain limited to build/package validation.

## Visual archive

[Visual summary](2026-09-03-gameplay-expansion-review-findings.svg)
