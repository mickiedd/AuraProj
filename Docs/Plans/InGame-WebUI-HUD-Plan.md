# In-Game Web UI HUD Plan

Status: implemented and guarded against dynamic-delegate, viewport-ownership, and native-overlay fallback regressions.

## Goal

Move the bottom skill strip to the existing in-game Web UI bridge without replacing the native health, mana, interaction, or spell-menu HUD. The web surface must stay bounded inside the game viewport and preserve GAS press/hold/release behavior.

## Contract

Unreal sends `skill_panel_ability` events with `inputTag`, `abilityTag`, `statusTag`, `abilityType`, `levelRequirement`, `icon`, and `clear`. The page requests a replay with `skill_panel_ready` and sends `skill_ability_pressed`, `skill_ability_held`, and `skill_ability_released` commands containing a validated input tag.

## Implementation stages

1. Extend `UWebUIWidget` with a `UGameViewportSubsystem`-backed anchor/offset layout API.
2. Add `skill-panel.html` with transparent outer bounds, responsive sizing, slot rendering, icon/fallback rendering, and pointer hold timers.
3. Create the web panel from `AAuraHUD::InitOverlay` after the native overlay is attached, then replay ability state after the browser reports ready.
4. Route browser commands through `AAuraPlayerController` wrappers so targeting, input gates, cooldown retry, and release behavior remain native.
5. Encode available `UTexture2D` icon mips as cached PNG data URLs; use deterministic ability initials when a cooked texture cannot be read.
6. Cover HTML, viewport, startup replay, and full Web UI bridge contracts with automation tests.
7. Mark both dynamic bridge handlers as reflected `UFUNCTION`s and assert their presence through Unreal reflection.
8. Mount and reload the skill page at top z-order, using the local player context in real gameplay and a world fallback for headless automation; use a stretched, non-zero-height bottom band; collapse only native spell globes after `skill_panel_ready`; restore them if the browser disconnects.

## Fallback and rollout

The native overlay remains instantiated and functional. The web panel is local-controller-only, uses z-order 200, and occupies only the bottom-center skill region (vertical anchors 0.72–0.98 with zero vertical offsets). If the bridge or browser is unavailable, the existing native HUD remains visible.

## Validation

- AuraEditor Win64 DebugGame build.
- `AuraWebUI` automation suite (bridge, config, content, WebSocket loopback).
- `Aura.UI.WebSkillPanel.HUDContract` and `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding`.
- `Aura.Abilities` suite.
- `AAuraHUD` reflection checks for `HandleWebUICommand` and `HandleWebUIConnectionChanged`.
- Native spell-globe replacement contract: browser readiness hides the old spell widgets while preserving native health/mana fallback; disconnect restores them.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback`: mounts the real page/browser host, validates the loaded asset and z-order when a viewport exists, and exercises the ready/disconnect visibility handoff.
