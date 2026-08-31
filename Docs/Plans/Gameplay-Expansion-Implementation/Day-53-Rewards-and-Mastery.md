# Day 53 — Rewards and Mastery Settlement

Status: Planned; not implemented by this planning job.  
Depends on: Day 52 reward flags/supplies; Day 42 hub snapshot/entry marker; existing persistence manifest and economy.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Reward objective participation fairly and preserve it through retries/restarts without civilian farming or duplicate payouts.

## Exact change surfaces

- Existing: `Source/Aura/Private/Game/AuraGameModeBase.cpp::GrantCivilianLethalReward`, `Private/Character/AuraEnemy.cpp::ApplyEnemyDeathPolicy`, currency component and `Private/Game/AuraPersistenceSubsystem.cpp`.
- Existing save contracts: `Public/Game/AuraSaveTypes.h`, `AuraPlayerSaveGame.h`, `AuraWorldSaveGame.h`, `AuraPersistenceManifestSaveGame.h` with matching implementations/checksums.
- New: `Source/Aura/Public/Gameplay/AuraMissionRewardSettlement.h` and implementation; `Content/Config/GameplayRewardDefinitions.json`, `GameplayMasteryDefinitions.json`; extend profile badge/receipt schema, versioned `GameplayGuidanceMask` and debrief UI. Integrate the existing Day 42 participant lease/forfeit and profile-generation checks.

## Data and authority contract

Per eligible participant: 50 gold primary +25 boss/extraction +25 optional cache, max100. Ordinary wipe/timeout pays only completed primary/cache; technical run abort pays zero. Explicit abandonment or lease expiry forfeits only that member, leaving survivors' earned flags intact. Readiness-at-start, >=60s connected participation and one accepted combat/support/objective action are required; a disconnected member must still have its unexpired, bound lease at settlement. No last-hit bonus. Settlement receipt key binds world/run/profile/reward version. Commit every still-bound participant's cleanup, eligible payouts, mastery badges, guidance merge and world terminal receipt in one new manifest generation before publishing. Zero-action/ineligible/dead members still require marker cleanup. Previously expired members' later hub generations are not settlement inputs. Legacy reward remains only under the old profile, never inferred from tags/client input.

Day 42's entry transaction requires capacity for the maximum 100-gold payout and freezes wallet mutation throughout the run; this prevents a legitimate near-cap wallet from entering an unrecoverable settlement. A terminal overflow remains a defensive failure, never a silent clamp. The only intentional durable changes over each hub snapshot are gold, badges and a versioned `GameplayGuidanceMask` separate from legacy tutorial bits. Reserve/migrate/checksum the guidance field now; Day 56 supplies accepted-action updates. Merge it on normal terminal cleanup, member forfeit/expiry and graceful shutdown; a forced crash restores the last committed mask. Explicit reset is a hub-only transaction.

## Numbered implementation steps

1. Implement validated versioned rewards and eligibility snapshots from server observations; display predicted earned flags but keep wallet unchanged until terminal commit.
2. Extend Day 42's batch checkpoint operation with settlement receipts/payouts: sorted profile locks, verify expected ActiveRunId and profile generation, prepare every still-bound profile plus world receipt, checksum/write, then one manifest switch. Include disconnected eligible reserved profiles and zero-payout members needing cleanup. Expired/abandoned members already atomically restored by Day 42 contribute only immutable tombstones; never overwrite their later hub state. Do not create a second transaction mechanism.
3. Preserve original hub inventory/ammo/attributes and legacy tutorial bits; apply only gold/badges/gameplay guidance, clear active-run markers and remove transient grants. Validate the 100-gold entry-capacity invariant without mutation; distinguish eligibility from cleanup membership. A loop of independent player commits is explicitly insufficient.
4. Add cosmetic first-success badges for Assault/Rescue/Sabotage; no extra permanent damage/XP progression. Expose a concise debrief showing objective, optional result, payout and why an ineligible member received zero.
5. Test crash injection before each profile write, before world receipt, before manifest flip and after flip before client acknowledgment; also expire one member, rejoin/mutate its hub profile, then finish the survivor's run. Previous generation remains valid until the new one commits, and settlement cannot revive an obsolete marker or overwrite a newer hub generation.
6. Migrate previous save versions non-destructively, initializing the versioned gameplay guidance mask without changing legacy tutorial progress; include its version/bits in validation/checksum and hub-only reset. Refuse unknown future schema/corrupt checksum and retain backups. Re-run old economy/restock/privacy tests on the legacy profile.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay53Tests.cpp`; namespace `Aura.Gameplay.Day53`.

- SharedObjectiveFairReward — support and damage contributors receive the same formula; no last-hit or civilian payout.
- SettlementIdempotent — duplicate terminal callbacks/reconnects restore exactly one payout.
- BatchCommitAllOrNothing — injected failure cannot pay only one participant.
- CrashAfterManifestNoReplay — committed receipt prevents a second award after restart.
- TransientStateExcluded — supplies/augments/normalized ammo never enter durable profile.
- CleanupIncludesIneligibleMembers — zero-action/dead/zero-payout members clear bound markers in the same terminal generation as eligible payouts.
- ExpiredMemberGenerationProtected — expiry/abandonment commits hub restoration before release; later hub mutations survive the old run's terminal commit, and member forfeit does not remove a survivor's payout.
- MaximumRewardCapacityAtEntry — capacity 100 admits entry, capacity 99 refuses before a marker/loadout grant; defensive terminal overflow fails without partial payout.
- GameplayGuidanceDurableAllowlist — normal cleanup/forfeit/graceful shutdown preserve accepted mask bits, forced crash restores only committed bits, and legacy tutorial/inventory/attributes remain unchanged.
- SaveMigrationPreservesLegacy — old role/wallet/inventory/tutorial/restock values survive migration; unknown version rejected.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 53 -Stage Fast -RunId d53-fast
./RunGameplayExpansion.ps1 -Day 53 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d53-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

SettlementFaultMatrix: two participants including one disconnected at 10s/61s, ordinary wipe after primary, member-only abandon with successful survivor, cache-only flag, zero-action member, capacity 99/100, defensive overflow, expiry followed by hub mutation/new generation, guidance commit/reset/crash and faults at every batch boundary.

## Failure and timeout semantics

Settling retries at 1/2/4s under the same RunId then SettlementBlocked; deny new run until repair/retry. Failed writes restore prepared memory and leave the old manifest authoritative. Capacity rejection occurs before entry so ordinary capped wallets cannot strand a played run; an unexpected terminal overflow or profile-generation mismatch rejects the whole settlement, never wraps/clamps or overwrites silently. A failed expiry cleanup keeps that profile locked until repair without restoring play eligibility. Crash before terminal commit restores prior committed hub state and guidance, not a promised payout. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-53.json, settlement-fault-matrix.json, migrated-save-checksums.json, pseudonymous before/after currency/receipt table, fairness/debrief captures. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All eligible participants are paid once or none are; every still-bound participant is cleaned up, and forfeited members' later hub records remain untouched. Profile migration and restart cannot leak run state, lose committed guidance or duplicate rewards; old fixture economics remain intact in their own profile.

## Defer / anti-goals

No database service, cloud saves, trade, reputation economy, permanent stat grind, ranked economy or rewriting the old candidate evidence.
