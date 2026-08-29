# Day 21 — Freeze the Playable Contract

Status: Planned  
Depends on: Role/Battle Day 20 local verification and the readiness review

## Goal

Define one supported local/LAN playable candidate so every later day has a stable target.

## Work

- Freeze the canonical `/Game/Maps/StartupMap` runtime-fixture path, Aura and BungeeMan roles, Civilian population, merchant, persistence scope, and listen/dedicated topology; do not leave these as implementation-time choices.
- Record the launch-to-reconnect journey as a human checklist and machine-readable scenario manifest.
- Reconcile the old `RoleBattleCivilianTest` wording with the actual `LevelConfig.json` alias; document runtime-created AI/markers as intentional.
- Define the candidate artifact fields: revision, build configuration, content manifest, test results, package hashes, logs, visual evidence, and gate disposition.
- Start the production Online Subsystem/App ID/account preflight in parallel; it is an external release gate, not a local candidate prerequisite.

## Detailed execution contract

### Files to inspect or create

- **New:** `Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json` and `Docs/Reports/Playable-Candidate-Scope-Decision-2026-08-29.md`.
- **Inspect:** `Content/Config/RoleConfig.json`, `Content/Config/ServerConnection.json`, `Config/DefaultGame.ini`, the StartupMap/level-config resolution path, and `Scripts/GameServerManager.py`.
- **Evidence:** `Saved/Reports/PlayableCandidate/<SourceRevision>/<RunId>/day-21-scope.json` plus the unchanged Day 20 baseline output.

### Contract fields

The manifest contains `schemaVersion`, `scopeRevision`, `sourceRevisionAtFreeze`, `canonicalMap`, `roles`, `topologies`, `lanes`, ordered `journeySteps`, `playerPersistencePolicy`, `worldPersistencePolicy`, `fireGunPolicy`, `rewardPolicy`, `merchantRestockPolicy`, `identityPolicy`, `contentRoots`, `runnerEntryPoints`, and `externalPreflight`. Every lane names expected role-ready, HUD-ready, combat, commerce, recovery, save, restart, and reconnect observations. Artifact paths must be unique, relative to the run root, and reject absolute paths or `..` traversal. `scopeRevision` is immutable for this candidate; source revisions may advance during implementation and are recorded by each run.

### Detailed steps

1. Compare the Day 20 report, `RoleConfig.json`, and live StartupMap resolution; record actual names and aliases rather than copying stale plan text.
2. Freeze the four required lanes: Aura/listen, BungeeMan/listen, Aura/dedicated, and BungeeMan/dedicated. Define listen as one packaged host plus one packaged remote client, and dedicated as one packaged server plus two packaged remote clients.
3. Write the ordered player journey and the expected authority/result owner for every step.
4. Copy the master-plan decisions for semi-auto FireGun, BungeeMan-only firearm state with Aura `NotApplicable` HUD state, lost-release recovery, completed-ammo-only persistence, player recovery normalization, the fixed reward/purchase pair, UTC restock, redacted diagnostics, and forced-kill rollback.
5. Assign each Day 22–40 contract an owner surface, output file, test family, and blocking condition; reject an unowned item.
6. Record provider/App ID/account readiness as an independent external row with a reason and no local-lane dependency.
7. Validate schema, map, role, topology, step ordering, content roots, and decision completeness.
8. Publish the manifest and run the existing baseline once without gameplay changes; the recorded revision becomes the input to Day 22.

### Required automation

- Planned `Scripts/ValidatePlayableCandidateScope.ps1` returns nonzero for an unknown lane, missing step, duplicate artifact, invalid reference, or unresolved decision token.
- The scope report records every command, exit code, and decision; no later day may silently redefine this contract.

## Deep-review closure

- **Owner surfaces:** `Docs/Plans/Playable-Candidate-Implementation/`, the existing `Content/Config/RoleConfig.json` and level-config path, and the current smoke/packaging entry points.
- **Required artifacts:** `playable-candidate-scope.json` with exactly four mandatory lanes (Aura/listen, BungeeMan/listen, Aura/dedicated, BungeeMan/dedicated), the launch-to-reconnect scenario, all frozen decisions from the master plan, and one owner plus output path for every later day.
- **Gate:** the manifest validates, contains no `TBD`, `choose`, `normally`, `if persistent`, or unresolved `where applicable` values, resolves the existing StartupMap alias, records the explicit role/topology applicability matrix, and records the external provider preflight as `BLOCKED` or `READY` without affecting the local candidate lanes.

## Validation and evidence

- Contract test rejects an unknown map, role, topology, or missing journey step.
- Run the existing baseline without changing gameplay and retain revision/exit status.
- Record a scope decision for every deferred system listed at the end of this plan.

## Completion gate

Everyone can state what a player must accomplish from launch through reconnect, and no new feature may enter Days 22–40 without replacing an explicitly scoped item.

## Defer

Do not create a duplicate map or authored AI assets solely to match historical plan language.
