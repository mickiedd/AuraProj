# In-Game Web UI HUD Plan

Status: complete. The gameplay HUD is WebUI-only and guarded against dynamic-delegate, viewport-ownership, and native-overlay regressions.

## Goal

Move the complete gameplay HUD to the existing in-game Web UI bridge: health, mana, progress, skills, attributes, spells, interaction, messages, location, and quit flow. The web surface must cover the game viewport transparently, preserve GAS press/hold/release behavior, and never leave LMB input stuck when the browser captures or loses focus.

## Contract

Unreal sends `skill_panel_ability` events for equipped slots plus `hud_vitals`, `hud_progress`, `hud_attribute`, `hud_spell_catalog`, `hud_interaction`, `hud_message`, `hud_location`, and modal state events. The page requests a replay with `hud_ready` and sends validated skill, attribute, spell, interaction, location, and quit commands.

## Implementation stages

1. Extend `UWebUIWidget` with a `UGameViewportSubsystem`-backed anchor/offset layout API.
2. Add `skill-panel.html` with transparent outer bounds, responsive sizing, slot rendering, icon/fallback rendering, and pointer hold timers.
3. Create the full-screen WebUI HUD from `AAuraHUD::InitOverlay`, then replay controller state after the browser reports ready.
4. Route browser commands through `AAuraPlayerController` wrappers so targeting, input gates, cooldown retry, and release behavior remain native; add global WebUI pointer-release and disconnect safety for gameplay LMB.
5. Encode available `UTexture2D` icon mips as cached PNG data URLs; use deterministic ability initials when a cooked texture cannot be read; apply modern framed, type-colored, readable icon treatments in `hud.html`.
6. Cover HTML, viewport, startup replay, and full Web UI bridge contracts with automation tests.
7. Mark both dynamic bridge handlers as reflected `UFUNCTION`s and assert their presence through Unreal reflection.
8. Mount and reload `hud.html` at top z-order, using the local player context in real gameplay and a world fallback for headless automation; use full-viewport transparent geometry and keep no native HUD fallback path.

## Fallback and rollout

The WebUI HUD is local-controller-only, uses z-order 200, and covers the full viewport. The existing widget controllers remain as nonvisual GAS/data/action services; `WBP_Overlay` is not instantiated by `AAuraHUD`.

## Validation

- AuraEditor Win64 DebugGame build.
- `AuraWebUI` automation suite (bridge, config, content, WebSocket loopback).
- `Aura.UI.WebSkillPanel.HUDContract` and `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding`.
- `Aura.Abilities` suite.
- `AAuraHUD` reflection checks for `HandleWebUICommand` and `HandleWebUIConnectionChanged`.
- Web-only contract: `AAuraHUD` does not construct `UUserWidget` overlays, call native widget delegates, draw Canvas HUD text, or restore native widgets on disconnect.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback`: mounts the real `hud.html` browser host, validates the loaded asset and full-viewport geometry when a viewport exists, and confirms no native action forwarding.
