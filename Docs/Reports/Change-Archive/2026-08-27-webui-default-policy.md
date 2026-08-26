# WebUI as the default in-game HUD path

## Intent

Persist the project decision that new in-game HUD work should use AuraWebUI/WebUI by default.

## Decision

- New HUD panels, bars, indicators, and controls should be implemented in WebUI and mounted through the transparent, bounded browser host.
- Native WBP remains a compatibility/fallback layer. When an existing native behavior is authoritative, WebUI commands should forward to its delegate rather than duplicate gameplay logic.
- Future work should include bridge events, readiness/disconnect fallback, and runtime visual verification in its test plan.

## Validation

- The policy follows the verified WebUI skill HUD implementation: health/mana state is bridged as `hud_vitals`, action commands reach the existing WBP delegates, and native widgets restore on browser disconnect.
- The current WebUI HUD build, focused automation, server listen, PIE screenshot, and click-path evidence are recorded in [the vitals/action HUD archive](2026-08-27-webui-vitals-action-hud.md).
- This decision is indexed in `.claude/memory/visual-change-archive.md` for future HUD tasks.

## Illustration

[Open the project UI default decision diagram](2026-08-27-webui-default-policy.svg)
