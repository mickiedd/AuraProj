# Day 37 — Dedicated-Server Operations Contract

Status: Planned  
Depends on: Day 31 persistence, Day 35 diagnostics, and the existing Game Server Manager path

## Goal

Make one deterministic dedicated server boring to start, identify, observe, stop, restart, and recover.

## Work

- Document command line, target/configuration, ports, map, persistence root/`WorldPersistenceId`, log locations, readiness signal, and graceful shutdown. The exact runbook command must pass `/Game/Maps/StartupMap`; the existing `StartDedicatedServer.bat` default is a dungeon map and cannot be used implicitly for this candidate.
- Define expected behavior for startup failure, client rejection, crash/forced kill, restart, and save recovery.
- Ensure server identity and readiness are visible to the existing manager and logs without relying on editor-only behavior. Process liveness, an open port, or the GSM startup grace period is not readiness; the server must emit the authoritative world-readiness result for the requested map/world ID.
- Provide a second-developer runbook using only checked-in scripts and documented prerequisites.

## Detailed execution contract

### Files to inspect or modify

- **Existing operations:** `Scripts/GameServerManager.py`, `BuildDedicatedServer.bat`, `StartDedicatedServer.bat`, `StopDedicatedServer.bat`, `BuildAndRunDedicatedServer.bat`, server target/config files, and `Content/Config/ServerConnection.json`.
- **Runtime readiness:** Game Server Manager client/server protocol, readiness result writers, persistence startup/shutdown hooks, and process ownership cleanup.
- **New documentation/output:** `Docs/Reference/Playable-Candidate-Server-Operations.md`, `Source/Aura/Private/Tests/AuraRoleBattleDay37Tests.cpp`, `RunPlayableCandidateDay37ServerOps.ps1`, and `day-37-server-operations.json`.

### Operations contract

The runbook defines prerequisites, exact command lines, working directory, build/configuration, map, ports, `WorldPersistenceId`/save root, readiness pattern and timeout, client connection command, log/report paths, graceful stop, forced-kill recovery, restart behavior, and expected exit codes. It uses a foreground/owned launcher or an equivalent process-group wrapper; the detached `start` path in `StartDedicatedServer.bat` is not a readiness or ownership gate. All processes are owned by the run and no editor interaction or prompt is required. The GSM control socket is loopback-only unless the runbook supplies the Day 33 allowlist and authentication token.

### Detailed steps

1. Document the existing dedicated binary selection and verify the cooked StartupMap and required staged JSON/XML/WebUI content are present. Require a packaged Shipping server for candidate/release evidence; Development/editor fallback is diagnostic evidence only.
2. Define a unique run ID, port allocation, persistence root, `WorldPersistenceId`, run-scoped log paths, readiness result code, and cleanup owner before launch; do not rely on the fixed `Saved/Logs/<level>.log` GSM path as the sole evidence file.
3. Start the server using the checked-in command, wait for readiness, and record PID, port, map, world ID, build revision, and log path.
4. Connect two clients, run the Day 21 journey through save, and verify server/client results use the same correlation/session fields.
5. Gracefully stop, verify the save/manifest commit, restart from the same world ID, and compare restored role/economy/ammo/population/merchant values.
6. Repeat with a forced kill during a safe checkpoint window; prove last-known-good recovery and no partial/cross-world state.
7. Run the exact procedure from a second developer account/environment using only the runbook and checked-in scripts; record any hidden prerequisite as a failure.
8. Verify readiness and shutdown are visible to GSM and logs in packaged server mode, not only editor mode.

### Named automation and gate

- Native/runner cases: `RunbookCommandContract`, `ReadySignal`, `TwoClientJourney`, `GracefulRestart`, `ForcedKillRecovery`, `WorldIdIsolation`, `ProcessOwnership`, and `NoInteractivePrompt`.
- Readiness must occur within 120 seconds; owned teardown must finish within 30 seconds; two clean runs plus one forced-kill recovery are mandatory.
- Any hidden editor dependency, missing log/exit record, orphaned process, port leak, or restore mismatch blocks Day 38.

## Deep-review closure

- **Owner surfaces:** `Scripts/GameServerManager.py`, `BuildDedicatedServer.bat`, `StartDedicatedServer.bat`, `StopDedicatedServer.bat`, the existing dedicated-server configuration, and the planned runbook.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Server-Operations.md` and `day-37-server-operations.json` with command lines, prerequisites, ports, readiness/result patterns, process ownership, and recovery evidence.
- **Gate:** a second developer completes two clean start/ready/play/save/stop/restart runs plus one forced-kill recovery with no hidden prompt or editor dependency; readiness is observed within 120 seconds, teardown leaves no owned process after 30 seconds, and restored values match the last committed snapshot.

## Validation and evidence

- Execute start → ready → two clients → play → save → graceful stop → restart → restore.
- Repeat with forced kill and verify no partial/cross-world state.
- Record process ownership, ports, readiness, logs, exit codes, and restored values.

## Completion gate

Someone other than the implementer can operate and diagnose the server from the runbook, with no hidden editor or interactive prompt dependency.

## Defer

Do not introduce cloud orchestration, fleet management, Kubernetes, or autoscaling.
