# Day 23 — Canonical Boot-to-Game Flow

Status: Planned  
Depends on: Day 22 baseline

## Goal

Make launch, login, loading, server readiness, travel, and player initialization end in a bounded success state or an actionable error.

## Work

- Trace Login → Game Server Manager → Loading → world entry → role application → HUD readiness on the canonical local/LAN path.
- Make map/config/connection/player-init failures visible in the WebUI status contract and logs.
- Ensure input becomes active exactly once after the gameplay world is ready and loading cannot remain indefinitely.
- Preserve the existing server-first role validation and coordinated unhealthy/readiness behavior.

## Detailed execution contract

### Files to inspect or modify

- **Session/readiness:** `Scripts/GameServerManager.py`, `Source/Aura/Public/Game/GameServerClient.h`, `ServerTravelComponent.h`, `AuraGameInstance.h`, and matching `.cpp` files.
- **Login/loading:** `LoginPlayerController.h/.cpp`, `LoadingPlayerController.h/.cpp`, `AuraPlayerController.h/.cpp`, and the role-application handoff.
- **Presentation:** `Plugins/AuraWebUI/Content/WebUI/login.html`, `loading.html`, `index.html`, `hud-left-top.html`, `hud-right-top.html`, `hud-bottom.html`, and `WebUIBridgeSubsystem`.
- **New output:** `Saved/Reports/PlayableCandidate/<Revision>/<RunId>/day-23-boot-matrix.json` and packaged screenshots.

### State and error contract

Use one sequence: `LoginPending → ServerReady → Loading → WorldReady → RoleValidated → HUDReady → Controllable`. Each transition has a single success event, a stable failure code, the owning process, and an elapsed-time field. `HUDReady` requires initial role, identity, pawn, and presentation state; an open WebSocket alone is not success.

### Detailed steps

1. Trace current callbacks and log every transition with `SessionId` and `ServerInstanceId`; identify duplicate callbacks and missing failure exits.
2. Make map/config, server unavailable, connection rejection, invalid role, readiness delay, and pawn initialization failures terminate in stage-specific WebUI and log results.
3. Gate native input on the single role/HUD-ready event; clear browser delegates, timers, and stale state before a second launch.
4. Run both roles in packaged listen and packaged dedicated paths using the canonical StartupMap and Day 21 lane manifest.
5. Capture loading, role-ready, HUD-ready, and controllable states; retain client/server logs joined by the same session IDs.
6. Repeat after one intentional failure with a new run ID and persistence namespace to prove stale state cannot satisfy readiness.

### Required automation and gate

- Success: HUD-ready and controllable within 60 seconds after server readiness.
- Failure: a named stage error within 30 seconds after the failure is observable; 120 seconds is the hard no-hang timeout.
- Exactly one input activation and exactly one terminal result are recorded per launch.

## Validation and evidence

- Add a boot smoke for success, missing map/config, unavailable server, rejected role, and delayed readiness.
- Run a packaged client/listen and packaged client/dedicated launch, retaining screenshots and logs.
- Verify a second launch does not inherit stale browser or persistence state.

## Deep-review closure

- **Owner surfaces:** `Scripts/GameServerManager.py`, the existing WebUI `login.html`, `loading.html`, `index.html`, and HUD pages, plus the server-side role/readiness path. WebUI remains presentation and command transport only.
- **Required artifacts:** `day-23-boot-matrix.json` with success and each named failure stage, one packaged screenshot per success lane, and client/server logs joined by `SessionId` and `ServerInstanceId`.
- **Gate:** each supported launch reaches HUD-ready and controllable within 60 seconds of server readiness, or reports a stage-specific error within 30 seconds; no launch may remain pending beyond a 120-second hard timeout, and input activation is recorded exactly once.

## Completion gate

A tester can start the supported build and reach a controllable Aura or BungeeMan, or receives a bounded error naming the failed stage and evidence path.

## Defer

Do not redesign session discovery or introduce public matchmaking; local/LAN determinism is the target.
