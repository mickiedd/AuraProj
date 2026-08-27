# Gameplay HUD input safety and modern icon pass

## Intent

Fix the fullscreen WebUI browser's mouse capture so gameplay LMB press/hold/release remains reliable, then improve the skill-slot icon treatment without reintroducing native WBP HUD presentation.

## Changed behavior

- `hud.html` forwards world LMB press and hold through the existing validated WebUI ability-input commands.
- LMB release is handled globally on `pointerup` and `pointercancel`, and is also forced on window blur, skill-button pointer capture loss, and WebUI bridge disconnect/end play.
- HUD buttons and modal controls are excluded from the world-input route; skill buttons retain their own WebUI press/hold/release behavior.
- Skill icons now use framed glass panels, type-specific offensive/passive color accents, contrast and gradient overlays, key badges, clearer labels, hover lift, and equipped-state glow.
- The gameplay HUD remains a single transparent `hud.html` presentation surface; no native WBP or Canvas fallback was added.

## Validation

- UE5.5 `AuraEditor` Mac Development build passed.
- `Aura.UI.WebSkillPanel` passed 2/2 on a fresh editor process after the final build.
- `AuraWebUI` passed 4/4, including the HUD content contract for global LMB release and modern icon treatments.
- `hud.html` JavaScript syntax check, SVG XML validation, and `git diff --check` passed.

## Illustration

[Gameplay HUD input and icon treatment](./2026-08-27-gameplay-hud-input-and-icons.svg)
