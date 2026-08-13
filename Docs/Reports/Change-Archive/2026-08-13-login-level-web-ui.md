# Login level Web UI migration

## Intent

Refactor the Login level presentation from UMG widgets to the standalone `AuraWebUI` plugin while preserving the existing native connection flow.

## Changed behavior

- `ALoginPlayerController` now creates a native `UWebUIWidget` and loads `Plugins/AuraWebUI/Content/WebUI/login.html`.
- Login targets are loaded and validated natively from `Content/Config/LevelConfig.json`.
- The page receives `login_state` and `login_status` events.
- The page sends `login_select_level` and `login_connect` commands containing only a `levelId`.
- Native code resolves the ID and invokes the existing selection, role-validation, GSM query, fallback, and Loading travel methods.
- The former `WBP_LoginMenu` and `ULoginConnectingWidget` are no longer used by the Login controller, but remain available for other legacy screens/assets.

## Validation

- `AuraServer Win64 Development` build passed.
- Focused `AuraWebUI` automation suite passed:
  - `AuraWebUI.Plugin.WebSocketLoopback`
  - `AuraWebUI.Plugin.BridgeProtocol`
  - `AuraWebUI.Plugin.ContentContract`
- Content-contract coverage verifies the Login page, bridge placeholder, Login commands, Login events, selector, connect button, and live status region.
- `git diff --check` passed.

## Visual summary

[View the change diagram](./2026-08-13-login-level-web-ui.svg)
