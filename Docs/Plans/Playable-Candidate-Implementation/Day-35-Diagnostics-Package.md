# Day 35 — Diagnostics Package

Status: Planned  
Depends on: Day 22 evidence format and Day 33 error paths

## Goal

Make a failure diagnosable from its artifacts without immediately reproducing it.

## Work

- Standardize `BuildRevision`, `SessionId`, `ServerInstanceId`, `WorldPersistenceId`, map, server mode, a run-scoped HMAC player identity representation plus provider type, and `RoleId` in important logs and reports. The HMAC key is supplied to cooperating processes for the run and is never written to logs or the candidate artifact, so identities correlate within a run without being dictionary-hashable or linkable across runs.
- Add action/transaction/combat correlation IDs where a request crosses process or subsystem boundaries.
- Add concise startup, readiness, rejection, persistence, and shutdown summaries with stable result codes.
- Keep credentials, raw secrets, and unnecessary personal data out of logs. Artifact and log paths are emitted relative to the run root; absolute paths and user names are not diagnostic fields.

## Detailed execution contract

### Files to inspect or modify

- **Runtime writers:** `Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Public/AutoTestLog.h`, `AutoTestResults.h`, `AutoTestReportWriter.h`, their implementations, and the project/shared result logging path.
- **Operational writers:** `Scripts/GameServerManager.py`, `RunSmokeTestSupervisor.ps1`, persistence/network runners, server/client startup/shutdown logs, and Day 38 artifact collection.
- **Correlation sources:** login/session, server instance, world persistence, role, combat, ammo, interaction, commerce, save, reconnect, and package stages.
- **New output:** `Source/Aura/Private/Tests/AuraRoleBattleDay35Tests.cpp`, `day-35-diagnostics.json`, result-code catalog, and redaction test report.

### Diagnostic record contract

Every important event has `SchemaVersion`, `TimestampUtc`, `BuildRevision`, `SessionId`, `ServerInstanceId`, `WorldPersistenceId`, `Map`, `ServerMode`, `ProviderType`, `PlayerIdentityHmac` (not raw identity), `RoleId`, `ActionCorrelationId` where applicable, `RequestId` where applicable, `ResultCode`, and a concise safe message. Player identity HMACs and correlation IDs are stable within a run and join client/server/process artifacts without exposing credentials, personal data, or absolute user paths. Non-player events explicitly use `PlayerIdentityHmac=null` rather than inventing an identity.

### Detailed steps

1. Inventory current log/report writers and normalize field names, timestamp format, severity, result-code format, and JSON encoding.
2. Add correlation propagation from login/readiness through role, combat/ammo, interaction/commerce, persistence, reconnect, and process lifecycle boundaries.
3. Define result codes for success, rejected input, invalid content, timeout, process failure, persistence rollback, readiness failure, and packaging failure; avoid using free-form text as the gate.
4. HMAC external identity at the writer boundary; add tests for provider IDs, credentials, tokens, URLs, local paths containing user names, cross-run unlinkability, and arbitrary payloads.
5. Add startup/readiness/rejection/commit/rollback/shutdown summaries that identify the relevant process and artifact paths.
6. Inject one synthetic failure into each required class: boot, role/config, combat, ammo, interaction, commerce, persistence, reconnect, and packaging.
7. Reconstruct each failure from the artifact packet alone and verify process isolation preserves correlation across server/client logs.
8. Publish the diagnostics contract and result-code catalog for Day 37/38; a missing correlation or redaction failure blocks those days.

### Named automation and commands

- Native tests: `CorrelationPropagation`, `ResultCodeStability`, `IdentityRedaction`, `SecretRedaction`, `BootFailure`, `CombatFailure`, `CommerceRollback`, `PersistenceFailure`, `ReconnectFailure`, and `PackageFailure`.
- Run the synthetic-failure matrix through the existing supervisors and confirm every row has a client symptom, server result, process, correlation ID, log path, and nonzero gate where appropriate.
- The diagnostics artifact itself must be safe to share with engineering; raw provider IDs, secrets, tokens, and unnecessary personal data are a hard failure.

## Validation and evidence

- Force one failure in boot, role/config validation, combat, ammo, interaction, commerce, persistence, reconnect, and packaging.
- Prove an engineer can identify the session, player, server, action, and failed contract from logs alone.
- Verify the diagnostics survive process isolation and candidate artifact collection.

## Deep-review closure

- **Owner surfaces:** the shared logging/result writers used by the server, client, `Scripts/GameServerManager.py`, smoke supervisors, and the candidate artifact collector.
- **Required artifacts:** `day-35-diagnostics.json`, a result-code catalog, a redaction test report, and a reconstruction packet linking one synthetic failure from client symptom to server action and persistence result.
- **Gate:** all nine forced failure classes produce a stable correlation chain and result code; raw external IDs, credentials, tokens, and unnecessary personal data are absent from logs and artifacts; an engineer can reconstruct the synthetic failure without reproducing it.

## Completion gate

Every major failure has a stable correlation path from human report to machine-readable result and relevant server/client logs.
