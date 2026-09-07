# Client connection loop and campaign persistence protection

Date: 2026-09-07. Code fixes validated; live campaign recovery remains pending the user's recovery choice.

## Intent and evidence

Investigate clients stuck connecting and fix the client/server causes without silently resetting saved progress.

- `Saved/Logs/Aura_2.log`: repeated manager query, fallback to `192.168.1.6:7790`, `PreLoginFailure`, Login reload, then another auto-login.
- `Saved/Logs/GameServerManager/RoleBattleCivilianTest.log`: both manifests reject their world record and server readiness becomes unhealthy.
- Preserved diagnostic logs: `Saved/Reports/ConnectionFix-20260907/`. Runtime audit found both manifests reference `AuraWorld_B9154B90_G179`, expected checksum `EF2B3283`; the actual record has checksum `9EB5501B` and map `None`.
- Earlier client logs show Login restoring the campaign and reaching Ready with zero live population. Login inherited the authority persistence/checkpoint path. Generation-only record names allowed stale writers to overwrite existing records.

## Changed behavior

- Login game mode opts out of campaign restore/checkpoint. Gameplay worlds retain persistence readiness checks.
- World and player record slots include unique GUIDs. Manifest publication uses a nonblocking system-wide lock and rejects a newer on-disk generation from another writer. Rejected writes roll back their own uncommitted records. The observed generation high-water mark permits recovery from an older valid world record.
- Explicit parsed manager failures cannot fall back to a static endpoint. Transport failures retain the existing standalone-server fallback.
- Failure state survives an early callback and Loading-to-Login travel. Network rejection reasons reach the Login status region.
- Command-line auto-login is once per game instance, rather than once per newly created Login controller. Manual retries remain possible.
- Login widget is focusable, resolving the accompanying UIOnly focus error.
- Added read-only, opt-in save audit and a runtime persistence regression covering stale writers and torn-checkpoint fallback.

## Validation

- `Engine/Build/BatchFiles/Build.bat AuraEditor Win64 Development -Project=C:\Git\AuraProj\Aura.uproject -WaitMutex -NoHotReloadFromIDE -NoLiveCoding`: passed. The same build also passed for DebugGame after the original editor/server processes exited. Evidence: `build-final.log`, `build-debuggame.log`.
- `Automation RunTests Aura.Persistence.CheckpointIsolation+Aura.RoleBattle.Day18.Save`: 16 passed, 0 failed. Evidence: `persistence-tests-final.log`.
- Independent local server process on UDP 17890 with `WorldPersistenceId=ConnectionFixValidation20260907`: Ready, client welcomed, server `Join succeeded: ConnectionFixHealthy`, pawn possessed. Evidence: `healthy-server.log` and `healthy-client.log`.
- Immediate loopback manager error fixture: returned to Login, retained the error, no fallback travel and no auto-login loop. Evidence: `explicit-error-client.log` and final focus/error check `final-client.log`.
- Final client probe: one auto-login dispatch, zero fallback attempts, zero focus errors; original campaign file hashes unchanged. All processes launched for validation were stopped afterward.
- The isolated server's notifications to the existing manager were rejected because its validation port differs from the configured production level port; the successful connection used a local manager-response fixture. This is not evidence of a production manager deployment.
- No Blueprint or content assets changed. No external review or upload was performed; the current handoff skill produces a local validation packet, not independent validation.

## Remaining recovery/deployment work

The active campaign is still fail-closed. No checksum was bypassed and no original save was deleted or replaced. A surviving map-specific snapshot is `Saved/SaveGames/AuraWorld_B9154B90_G139.sav`, last written September 6 at 23:02 local time, structurally valid with computed checksum `5B6CFC56`. It is a surviving record, not a current manifest-authorized checkpoint. Restoring it would roll world population/merchant state back; current player-profile references should be preserved. This requires the user's recovery choice and a full backup before mutation. By final inspection, the original editor and dedicated server had exited; the original manager remains running. Development and DebugGame binaries are rebuilt for subsequent launches.

[Before/after diagram](2026-09-07-client-connection-persistence.svg)
