# In-Game Web UI HUD Plan

Status: complete. The gameplay HUD is WebUI-only, split into three bounded browser panels, and guarded against dynamic-delegate, viewport-ownership, native-overlay, and level-click regressions.

## Goal

Move the complete gameplay HUD to the existing in-game Web UI bridge: health, mana, progress, skills, attributes, spells, interaction, messages, location, and quit flow. Use independent transparent bottom, left-top, and right-top browser surfaces, preserve GAS press/hold/release behavior for explicit skill controls, and keep blank level clicks in the native viewport.

## Role/battle coverage

The player-facing coverage for the completed Day 01–18 role/battle milestones is tracked in [Role/Battle UI Incremental Plan](Role-Battle-UI-Incremental-Plan.md). It maps each server-side change to a HUD signal and defines the `hud_role_state`, `hud_battle_state`, `hud_interaction`, `hud_economy`, `hud_merchant`, and `hud_merchant_result` event contract.

## Contract

Unreal sends `skill_panel_ability` events for equipped slots plus `hud_vitals`, `hud_progress`, `hud_attribute`, `hud_spell_catalog`, `hud_interaction`, `hud_message`, `hud_location`, and modal state events. The page requests a replay with `hud_ready` and sends validated skill, attribute, spell, interaction, location, and quit commands.

## Implementation stages

1. Extend `UWebUIWidget` with a `UGameViewportSubsystem`-backed anchor/offset layout API.
2. Add `skill-panel.html` with transparent outer bounds, responsive sizing, slot rendering, icon/fallback rendering, and pointer hold timers.
3. Create independent bounded bottom, left-top, and right-top WebUI HUD panels from `AAuraHUD::InitOverlay`, then replay controller state after each browser reports ready.
4. Route browser commands through `AAuraPlayerController` wrappers so targeting, input gates, cooldown retry, and release behavior remain native; keep explicit skill release and disconnect safety without forwarding blank-page pointer input as gameplay LMB.
5. Encode available `UTexture2D` icon mips as cached PNG data URLs; use deterministic ability initials when a cooked texture cannot be read; apply modern framed, type-colored, readable icon treatments in `hud-bottom.html`.
6. Cover HTML, viewport, startup replay, and full Web UI bridge contracts with automation tests.
7. Mark both dynamic bridge handlers as reflected `UFUNCTION`s and assert their presence through Unreal reflection.
8. Mount and reload all three panel pages at top z-order, using the local player context in real gameplay and a world fallback for headless automation; use bounded transparent geometry and keep no native HUD fallback path.

## Fallback and rollout

The WebUI HUD is local-controller-only, uses z-order 200, and covers only its visible panel regions during normal play. Menus and the interaction prompt temporarily expand their owning panel while active. The existing widget controllers remain as nonvisual GAS/data/action services; `WBP_Overlay` is not instantiated by `AAuraHUD`.

## Validation

- AuraEditor Win64 DebugGame build.
- `AuraWebUI` automation suite (bridge, config, content, WebSocket loopback).
- `Aura.UI.WebSkillPanel.HUDContract` and `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding`.
- `Aura.Abilities` suite.
- `AAuraHUD` reflection checks for `HandleWebUICommand` and `HandleWebUIConnectionChanged`.
- Web-only contract: `AAuraHUD` does not construct `UUserWidget` overlays, call native widget delegates, draw Canvas HUD text, or restore native widgets on disconnect.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback`: mounts all three browser hosts, validates their dedicated assets and bounded geometry when a viewport exists, and confirms no native action forwarding.
