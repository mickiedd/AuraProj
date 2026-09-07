# Validation Changes: connection flow and persistence recovery

Date: 2026-09-07

## Intent

Verify the attached Validation Changes review against the current source and logs, apply only confirmed fixes, and prove the rendered Login → Loading → dedicated-server flow.

## Findings and changes

- Manifest crash safety was a real evidence gap. `WriteManifest` already alternates A/B and writes only the inactive slot, so an interrupted write leaves the active manifest intact. The code now documents that invariant, and `Aura.Persistence.CheckpointIsolation` truncates the inactive manifest, restarts the subsystem, and asserts that the surviving checkpoint and checksum load.
- GSM transport fallback was a real evidence gap. A client with GSM port `19004` unavailable waited for the transport timeout, selected direct `127.0.0.1:17891`, and received a dedicated-server welcome. A separate fresh manager run on port `9000` returned `127.0.0.1:7790` and the client received a welcome without fallback.
- A complete manager line now marks `bReceivedManagerResponse` before JSON parsing. A malformed manager payload therefore remains an explicit failure on Loading/Login and cannot silently redirect to a fallback authority. The malformed-payload probe observed `Invalid JSON`, `OnCrossServerTravelFailed`, and no fallback endpoint.
- The Login/gameplay persistence boundary is explicit through `IsAuthorityWorldPersistenceEnabled()` and the new `Aura.Persistence.FrontendIsolationContract` test. Login disables authority persistence; gameplay keeps it enabled.
- Manual retry behavior was verified from source and the rendered Login UI: the command-line attempt guard is scoped to automatic dispatch, while a new/manual Login controller can issue another connect request.

## Validation

- `Build.bat AuraEditor Win64 Development -Project=C:/Git/AuraProj/Aura.uproject -WaitMutex`: passed.
- `Build.bat AuraEditor Win64 DebugGame -Project=C:/Git/AuraProj/Aura.uproject -WaitMutex`: passed.
- `Automation RunTests Aura.Persistence.CheckpointIsolation+Aura.Persistence.FrontendIsolationContract`: 2 passed, 0 failed. See [`persistence-validation-2.log`](../../../Saved/Reports/ValidationChanges-20260907/persistence-validation-2.log).
- GSM success path: [`gsm-client-isolated.log`](../../../Saved/Reports/ValidationChanges-20260907/gsm-client-isolated.log) and [`gsm-manager-isolated.stderr.log`](../../../Saved/Reports/ValidationChanges-20260907/gsm-manager-isolated.stderr.log).
- Direct transport fallback path: [`transport-fallback-client.log`](../../../Saved/Reports/ValidationChanges-20260907/transport-fallback-client.log) and [`transport-server.log`](../../../Saved/Reports/ValidationChanges-20260907/transport-server.log).
- Malformed manager response: [`malformed-manager-client.log`](../../../Saved/Reports/ValidationChanges-20260907/malformed-manager-client.log).
- Visual inspection: [`Login`](../../../Saved/Reports/ValidationChanges-20260907/visual-login-only.png), [`Loading`](../../../Saved/Reports/ValidationChanges-20260907/visual-login.png), and [`Gameplay`](../../../Saved/Reports/ValidationChanges-20260907/visual-gameplay.png).

The temporary isolated `LevelConfig.json` launch argument used to keep the existing corrupt campaign namespace untouched was restored before completion. No production campaign save was deleted, rewritten, or recovered.

[Before/after diagram](2026-09-07-validation-changes-connection-flow.svg)
