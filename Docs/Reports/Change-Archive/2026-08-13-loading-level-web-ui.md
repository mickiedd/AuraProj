# Loading level Web UI migration

## Intent

Use the separated `AuraWebUI` plugin for the first real game screen by replacing the Loading level's `WBP_LoadingUI` presentation.

## Changed behavior

- `ALoadingPlayerController` now creates `UWebUIWidget` and loads the plugin-owned `WebUI/loading.html` page.
- Existing destination parsing, portal GSM resolution, cross-server travel, same-server travel timing, and progress interpolation remain in the controller.
- Progress is broadcast through `UWebUIBridgeSubsystem` as `loading_progress` events.
- The web page sends `ready`, reconnects if needed, and renders the progress percent/message.
- The existing `WBP_LoadingUI` asset remains untouched as a recoverable fallback/reference, but is no longer selected by the Loading controller.

## Validation

- AuraServer Win64 Development build passed.
- AuraEditor Win64 Development live-coding build passed; the normal editor build was blocked by the already-running editor Live Coding session.
- Focused `AuraWebUI` automation run passed all 3 tests.
- SVG XML/archive link validation passed.

## Illustration

[View the Loading-level migration diagram](./2026-08-13-loading-level-web-ui.svg)
